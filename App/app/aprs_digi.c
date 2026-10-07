/* Copyright 2024 UV-K5 Firmware Custom (ta1js APRS work)
 * Ported to the PY32F071 firmware by tigfox, 2026.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 *     Unless required by applicable law or agreed to in writing, software
 *     distributed under the License is distributed on an "AS IS" BASIS,
 *     WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *     See the License for the specific language governing permissions and
 *     limitations under the License.
 */

// ---------------------------------------------------------------------------
// Digipeater core (build flag ENABLE_APRS_DIGI).
//
// Touches no hardware, so tools/aprs/test_aprs_core.c tests the very code the
// radio runs: time comes in as 10 ms ticks and the channel state as a flag.
//
// Roles (gAPRS_DigiMode):
//   FILL  repeat only a first unused WIDE1-1, substituting MYCALL* in place,
//         so the frame keeps its length.
//   WIDE  also WIDEn-N, New-N style: MYCALL* is inserted in front of the hop
//         and the hop decremented (WIDE2-2 -> MYCALL*,WIDE2-1 and WIDE2-1 ->
//         MYCALL*,WIDE2*). A request for more than gAPRS_DigiHops hops is
//         refused, which is what keeps one WIDE7-7 from flooding every digi on
//         the network. The limit is on n, the hops *requested*, not N, the
//         hops left: a WIDE4-2 is refused even mid-flight, so a packet that
//         asked for too much is stopped by every digi alike, not let through
//         once a more permissive one has decremented it. With the path full (8 digipeaters) or the frame at the
//         TX budget there is no room to insert: the last hop is substituted,
//         any other decremented in place.
// Both roles honour an explicit hop naming this station (MYCALL-SSID).
//
// A repeat waits a 100 ms hold-off plus a random 0..DDly, and longer while the
// channel is busy, so digis that heard the same packet do not key up together.
// A FILL digi that hears another digi's copy of its queued repeat first
// cancels its own: WIDE1-1 asks for one nearby repeat, not one from every digi
// in earshot. A WIDE digi never cancels - a digi on another ridge does not
// cover our side of the mountain, and even a queued WIDE1-1 repeat of ours
// also carries the WIDEn-N hops a fill-in neighbour leaves unserved.
// ---------------------------------------------------------------------------


#include "app/aprs_digi.h"
#include "app/aprs_ax25.h"
#include "app/aprs_settings.h"
#include <string.h>

_Static_assert(DIGI_MAX_FRAME == APRS_RAWTX_MAX, "digi repeats must fit the TX budget");
_Static_assert((int)DIGI_OFF == (int)APRS_DIGI_OFF && (int)DIGI_FILL == (int)APRS_DIGI_FILL && (int)DIGI_WIDE == (int)APRS_DIGI_WIDE, "digi modes");
_Static_assert(DIGI_HOPS_MAX == APRS_DIGI_HOPS_MAX && DIGI_DELAYS == APRS_DIGI_DELAYS, "digi limits");


uint8_t  gAPRS_DigiMode;
uint8_t  gAPRS_DigiHops = DIGI_HOPS_DEFAULT;
uint8_t  gAPRS_DigiDelay;
uint16_t gAPRS_DigiStats[DIGI_ST_N];

static const uint8_t DIGI_DELAY_TICKS[DIGI_DELAYS] = { 0, 25, 50, 100 };

static uint8_t  gDigiFrame[DIGI_MAX_FRAME + 2];   // + the FCS appended at TX
static uint16_t gDigiLen;          // queued repeat, FCS excluded; 0 = none
static uint32_t gDigiKey;          // dedupe key of the queued repeat
static uint32_t gDigiQueuedAt;
static uint32_t gDigiDue;
static bool     gDigiCancellable;  // FILL role: yields to a neighbour's copy
static uint8_t  gDigiUsedHops;     // H-bit hops in the frame as we heard it
static uint32_t gDigiSeenKey[DIGI_SEEN];
static uint32_t gDigiSeenAt[DIGI_SEEN];
static uint32_t gDigiRand = 0x2545F491u;

static uint32_t DIGI_Random(void)
{
    uint32_t x = gDigiRand | 1u;   // xorshift32; never let the state reach 0
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    gDigiRand = x;
    return x;
}

// Same packet = same source, destination and payload, whatever its path says.
static uint32_t DIGI_Key(const uint8_t *frame, uint16_t info, uint16_t end)
{
    uint8_t a[14];
    memcpy(a, frame, sizeof(a));
    a[6]  &= 0x1Eu;                // SSID only: the C/H/last-address bits vary
    a[13] &= 0x1Eu;
    const uint32_t k = ((uint32_t)APRS_CalculateCRC(a, sizeof(a)) << 16) |
                       APRS_CalculateCRC(&frame[info], (uint16_t)(end - info));
    return k ? k : 1u;             // 0 marks an empty memory slot
}

static bool DIGI_Seen(uint32_t key, uint32_t now)
{
    for (uint8_t i = 0; i < DIGI_SEEN; i++)
        if (gDigiSeenKey[i] == key && now - gDigiSeenAt[i] < DIGI_DUPE_TICKS)
            return true;
    return false;
}

// Let a packet through again: its repeat never made it onto the air.
static void DIGI_Forget(uint32_t key)
{
    for (uint8_t i = 0; i < DIGI_SEEN; i++)
        if (gDigiSeenKey[i] == key)
            gDigiSeenKey[i] = 0;
}

// Digipeater hops already used (H bit set); s is the last address's SSID byte.
static uint8_t DIGI_UsedHops(const uint8_t *frame, uint16_t s)
{
    uint8_t used = 0;
    for (uint16_t a = 20; a <= s; a += 7u)
        used += (frame[a] >> 7) & 1u;
    return used;
}

static void DIGI_Remember(uint32_t key, uint32_t now)
{
    uint8_t oldest = 0;
    for (uint8_t i = 1; i < DIGI_SEEN; i++)
        if (now - gDigiSeenAt[i] > now - gDigiSeenAt[oldest])
            oldest = i;
    gDigiSeenKey[oldest] = key;
    gDigiSeenAt[oldest]  = now;
}

// Callsign (6 chars) and SSID match; the C/H/last-address bits are ignored.
static bool DIGI_IsCall(const uint8_t *a7, const uint8_t *me7)
{
    return memcmp(a7, me7, 6) == 0 && ((a7[6] ^ me7[6]) & 0x1Eu) == 0;
}

// The n of a WIDEn alias (1..7), or 0 for anything else.
static uint8_t DIGI_WideN(const uint8_t *a7)
{
    static const uint8_t WIDE[4] = { 'W' << 1, 'I' << 1, 'D' << 1, 'E' << 1 };
    const uint8_t n = (uint8_t)((a7[4] >> 1) - '0');
    if (memcmp(a7, WIDE, sizeof(WIDE)) != 0 || a7[5] != (' ' << 1) || n < 1u || n > 7u)
        return 0;
    return n;
}

// Decide what to do with a received frame (len includes the 2-byte FCS) and
// queue its repeat, with the path rewritten, if there is one.
digi_result_t DIGI_Consider(const uint8_t *frame, uint16_t len, uint32_t now,
                                   const char *mycall, uint8_t myssid)
{
    gAPRS_DigiStats[DIGI_ST_HEARD]++;
    if (gAPRS_DigiMode == DIGI_OFF || len < 25u || (frame[13] & 1u))
        return DIGI_IGNORED;               // off, runt, or no digipeater path

    uint8_t me[7];
    AX25_EncodeAddress(mycall, myssid, false, me);
    if (memcmp(&frame[7], me, 6) == 0)
        return DIGI_IGNORED;               // our own transmission, any SSID

    const uint16_t end = (uint16_t)(len - 2u);   // FCS excluded
    uint16_t s = 20;                       // SSID byte of the last address
    while (!(frame[s] & 1u)) {
        s += 7u;
        if (s >= end)
            return DIGI_IGNORED;           // the address block never ends
    }
    const uint16_t info = (uint16_t)(s + 3u);    // past control + PID
    if (info >= end || frame[s + 1u] != 0x03u || frame[s + 2u] != 0xF0u)
        return DIGI_IGNORED;               // no payload, or not an APRS UI frame

    const uint32_t key = DIGI_Key(frame, info, end);
    gDigiRand ^= key ^ now;                // packet arrival times are the entropy

    const uint8_t used = DIGI_UsedHops(frame, s);
    if (gDigiLen != 0 && key == gDigiKey) {      // a copy of the repeat we hold
        // Only a copy with more hops used is another digi's repeat; the same
        // hops means the originator retrying, or multipath - keep ours.
        if (gDigiCancellable && used > gDigiUsedHops) {
            gDigiLen = 0;
            gAPRS_DigiStats[DIGI_ST_CANCELLED]++;
            return DIGI_CANCELLED;
        }
        gAPRS_DigiStats[DIGI_ST_DUP]++;
        return DIGI_DUP;
    }

    // Only the first unused hop may claim the frame.
    uint16_t a = 20;
    while (frame[a] & 0x80u) {             // H bit: this hop is used up
        if (DIGI_IsCall(&frame[a - 6u], me))
            return DIGI_IGNORED;           // we repeated it already: a loop
        if (a == s)
            return DIGI_IGNORED;           // every hop used
        a += 7u;
    }
    const uint8_t N    = (uint8_t)((frame[a] >> 1) & 0x0Fu);
    const uint8_t n    = DIGI_WideN(&frame[a - 6u]);
    const bool    mine = DIGI_IsCall(&frame[a - 6u], me);

    if (!mine) {
        if (n == 0 || N == 0 || (gAPRS_DigiMode == DIGI_FILL && (n != 1u || N != 1u)))
            return DIGI_IGNORED;           // another alias, or not our role
        if (N > n || n > gAPRS_DigiHops) {
            gAPRS_DigiStats[DIGI_ST_TOOMANY]++;
            return DIGI_TOOMANY;
        }
    }
    if (DIGI_Seen(key, now)) {
        gAPRS_DigiStats[DIGI_ST_DUP]++;
        return DIGI_DUP;
    }
    if (gDigiLen != 0 || end > DIGI_MAX_FRAME) {
        gAPRS_DigiStats[DIGI_ST_DROPPED]++;
        return DIGI_DROPPED;
    }

    memcpy(gDigiFrame, frame, end);
    uint8_t *h   = &gDigiFrame[a - 6u];    // the hop being used
    uint16_t out = end;
    if (mine) {
        h[6] |= 0x80u;                     // explicit hop: just mark it used
    } else if (gAPRS_DigiMode == DIGI_WIDE &&
               (s - 13u) / 7u < DIGI_MAX_VIAS && end + 7u <= DIGI_MAX_FRAME) {
        memmove(h + 7, h, (size_t)(end - (a - 6u)));
        memcpy(h, me, 6);
        h[6]  = (uint8_t)(0x80u | me[6]);                  // MYCALL*, not last
        h[13] = (uint8_t)((h[13] & ~0x1Eu) | ((N - 1u) << 1u) | (N == 1u ? 0x80u : 0u));
        out   = (uint16_t)(end + 7u);
    } else if (N == 1u) {
        memcpy(h, me, 6);                  // last hop: MYCALL* in its place
        h[6] = (uint8_t)(0x80u | me[6] | (h[6] & 1u));
    } else {
        h[6] = (uint8_t)((h[6] & ~0x1Eu) | ((N - 1u) << 1u));   // untraced
    }

    DIGI_Remember(key, now);
    gDigiLen         = out;
    gDigiKey         = key;
    gDigiQueuedAt    = now;
    gDigiCancellable = !mine && gAPRS_DigiMode == DIGI_FILL;
    gDigiUsedHops    = used;
    gDigiDue = now + DIGI_HOLDOFF +
               DIGI_Random() % (DIGI_DELAY_TICKS[gAPRS_DigiDelay % DIGI_DELAYS] + 1u);
    return DIGI_QUEUED;
}

// The queued repeat if it is due and the channel is clear (FCS excluded, with
// two spare bytes for it), else NULL. Report the outcome with DIGI_Sent. A channel that stays busy pushes the
// repeat back, and after DIGI_GIVEUP drops it.
uint8_t *DIGI_Due(uint32_t now, bool busy, uint16_t *len)
{
    if (gDigiLen == 0 || (int32_t)(now - gDigiDue) < 0)
        return NULL;
    if (busy) {
        if (now - gDigiQueuedAt > DIGI_GIVEUP) {
            gDigiLen = 0;
            DIGI_Forget(gDigiKey);         // the sender's retry may still get through
            gAPRS_DigiStats[DIGI_ST_DROPPED]++;
        } else {
            gDigiDue = now + DIGI_HOLDOFF + DIGI_Random() % DIGI_BUSY_RETRY;
        }
        return NULL;
    }
    *len     = gDigiLen;
    gDigiLen = 0;
    return gDigiFrame;
}

// After transmitting what DIGI_Due handed out: did it really go on the air?
// The radio can refuse to key up (TX lock, band, battery); then the repeat is
// counted as dropped and the packet forgotten, so a retry can be repeated.
void DIGI_Sent(bool on_air)
{
    if (on_air) {
        gAPRS_DigiStats[DIGI_ST_REPEATED]++;
    } else {
        DIGI_Forget(gDigiKey);
        gAPRS_DigiStats[DIGI_ST_DROPPED]++;
    }
}

void DIGI_Reset(uint8_t mode, uint8_t hops, uint8_t delay)
{
    memset(gDigiSeenKey, 0, sizeof(gDigiSeenKey));
    memset(gDigiSeenAt, 0, sizeof(gDigiSeenAt));
    memset(gAPRS_DigiStats, 0, sizeof(gAPRS_DigiStats));
    gDigiLen        = 0;
    gAPRS_DigiMode  = mode;
    gAPRS_DigiHops  = hops;
    gAPRS_DigiDelay = delay;
}

const uint8_t *DIGI_QueuedFrame(void)
{
    return gDigiFrame;
}

bool DIGI_HasQueued(void)
{
    return gDigiLen != 0;
}
