/* Host test for the hardware-free APRS code: AX.25/HDLC, parsers, builders, digi core,
 * last-heard formatter, arrow text entry. Links the real files in App/app (nothing copied).
 * Ported from ta1js utils/aprs_hdlc_test.c. Run: make -C tools/aprs test */
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

#include "app/aprs_ax25.h"
#include "app/aprs_beacon.h"
#include "app/aprs_digi.h"
#include "app/aprs_lastheard.h"
#include "app/aprs_msg.h"
#include "app/aprs_parse.h"
#include "app/aprs_text.h"

#define DIGI_TEST_CALL "TA1JS"
#define DIGI_TEST_SSID 7

typedef struct { const char *call; uint8_t ssid; bool used; } via_t;
#define VIAS(...) (const via_t[]){ __VA_ARGS__ }, \
                  (uint8_t)(sizeof((const via_t[]){ __VA_ARGS__ }) / sizeof(via_t))
#define NOVIA     NULL, 0
#define DINFO      "!4059.60N/02735.98E>digi test"

// build "SRC>APZK5,<vias>:info" as an AX.25 frame, FCS included
static uint16_t MkFrameV(uint8_t *f, const char *src, uint8_t sssid,
                         const via_t *v, uint8_t nv, const char *info)
{
    uint16_t i = 0;
    AX25_EncodeAddress("APZK5", 0, false, &f[i]); i += 7;
    AX25_EncodeAddress(src, sssid, nv == 0, &f[i]); i += 7;
    for (uint8_t k = 0; k < nv; k++) {
        AX25_EncodeAddress(v[k].call, v[k].ssid, k + 1 == nv, &f[i]);
        if (v[k].used) f[i + 6] |= 0x80;
        i += 7;
    }
    f[i++] = 0x03; f[i++] = 0xF0;
    const size_t n = strlen(info);
    memcpy(&f[i], info, n); i += (uint16_t)n;
    const uint16_t fcs = AX25_CalculateFCS(f, i);
    f[i++] = fcs & 0xFF; f[i++] = fcs >> 8;
    return i;
}

static void Refcs(uint8_t *f, uint16_t len)   // after patching a frame by hand
{
    const uint16_t fcs = AX25_CalculateFCS(f, (uint16_t)(len - 2u));
    f[len - 2] = fcs & 0xFF; f[len - 1] = fcs >> 8;
}

static char *AddrStr(char *p, const uint8_t *a7, bool star)
{
    for (int k = 0; k < 6; k++) { const char c = (char)(a7[k] >> 1); if (c != ' ') *p++ = c; }
    const int ssid = (a7[6] >> 1) & 0x0F;
    if (ssid) p += snprintf(p, 8, "-%d", ssid);
    if (star && (a7[6] & 0x80)) *p++ = '*';
    return p;
}

// TNC2 monitor form of a frame (len excludes the FCS): "SRC>DST,VIA*,VIA:info".
// Parsing follows the last-address bits, so a misplaced one shows up here too.
static const char *Tnc2(const uint8_t *f, uint16_t len)
{
    static char out[512];
    char *p = AddrStr(out, &f[7], false);
    *p++ = '>';
    p = AddrStr(p, &f[0], false);
    uint16_t a = 13;
    while (!(f[a] & 1u) && a + 7u < len) {
        a += 7;
        *p++ = ',';
        p = AddrStr(p, &f[a - 6], true);
    }
    *p++ = ':';
    for (uint16_t i = a + 3; i < len; i++) *p++ = (char)f[i];
    *p = 0;
    return out;
}

static int gDigiFails;
static void DCheck(bool ok, const char *fmt, ...)
{
    if (ok) return;
    va_list ap;
    va_start(ap, fmt);
    printf("digi: ");
    vprintf(fmt, ap);
    printf("\n");
    va_end(ap);
    gDigiFails++;
}

static void DigiReset(uint8_t mode, uint8_t hops, uint8_t delay)
{
    DIGI_Reset(mode, hops, delay);
}

static digi_result_t Feed(const uint8_t *f, uint16_t len, uint32_t now)
{
    return DIGI_Consider(f, len, now, DIGI_TEST_CALL, DIGI_TEST_SSID);
}

// Poll with a clear channel until the queued repeat comes out; its TNC2 form.
static const char *Drain(uint32_t now, uint32_t *at)
{
    uint16_t n = 0;
    for (uint32_t t = now; t < now + 400u; t++) {
        uint8_t *fr = DIGI_Due(t, false, &n);
        if (fr) { if (at) *at = t; return Tnc2(fr, n); }
    }
    return "(nothing)";
}

static void ExpectRepeat(const uint8_t *f, uint16_t len, uint32_t now,
                         const char *want, const char *what)
{
    const digi_result_t r = Feed(f, len, now);
    DCheck(r == DIGI_QUEUED, "%s: result %d, want QUEUED", what, r);
    const char *got = Drain(now, NULL);
    DCheck(strcmp(got, want) == 0, "%s:\n    got  %s\n    want %s", what, got, want);
}

static void ExpectResult(const uint8_t *f, uint16_t len, uint32_t now,
                         digi_result_t want, const char *what)
{
    const digi_result_t r = Feed(f, len, now);
    DCheck(r == want, "%s: result %d, want %d", what, r, want);
}

static int test_digi(void)
{
    uint8_t f[300];
    uint16_t len;
    gDigiFails = 0;

    // ---- FILL: the pre-existing fill-in role, WIDE1-1 only, substituted ----
    DigiReset(DIGI_FILL, 2, 0);
    len = MkFrameV(f, "TB1AAW", 0, VIAS({"WIDE1", 1, false}, {"WIDE2", 1, false}), DINFO);
    ExpectRepeat(f, len, 1000, "TB1AAW>APZK5,TA1JS-7*,WIDE2-1:" DINFO, "fill WIDE1-1");
    ExpectResult(f, len, 1500, DIGI_DUP, "fill duplicate inside 30 s");
    ExpectRepeat(f, len, 1000 + 3001, "TB1AAW>APZK5,TA1JS-7*,WIDE2-1:" DINFO, "fill after 30 s");

    len = MkFrameV(f, "TB1AAW", 0, VIAS({"WIDE1", 1, true}, {"WIDE2", 1, false}), DINFO);
    ExpectResult(f, len, 9000, DIGI_IGNORED, "fill: WIDE1 already used");
    len = MkFrameV(f, "TB1AAW", 0, VIAS({"WIDE2", 2, false}), DINFO);
    ExpectResult(f, len, 9000, DIGI_IGNORED, "fill: WIDE2-2 is not a fill-in hop");
    len = MkFrameV(f, "TA1JS", 7, VIAS({"WIDE1", 1, false}), DINFO);
    ExpectResult(f, len, 9000, DIGI_IGNORED, "own frame (same call and SSID)");
    len = MkFrameV(f, "TA1JS", 7, VIAS({"WIDE1", 1, true}, {"WIDE2", 1, false}), DINFO);
    ExpectResult(f, len, 9000, DIGI_IGNORED, "own frame again, after another digi used a hop");
    /* the same callsign with another SSID is another station (a mobile and a digi of one operator) */
    len = MkFrameV(f, "TA1JS", 3, VIAS({"WIDE1", 1, false}), DINFO);
    ExpectRepeat(f, len, 9000, "TA1JS-3>APZK5,TA1JS-7*:" DINFO, "same call, other SSID is repeated");
    len = MkFrameV(f, "TB1AAW", 0, NOVIA, DINFO);
    ExpectResult(f, len, 9000, DIGI_IGNORED, "pathless frame");

    len = MkFrameV(f, "TB1AAW", 0, VIAS({"WIDE1", 1, false}), "!4059.60N/02735.98E>one");
    ExpectRepeat(f, len, 9100, "TB1AAW>APZK5,TA1JS-7*:!4059.60N/02735.98E>one", "distinct payload 1");
    len = MkFrameV(f, "TB1AAW", 0, VIAS({"WIDE1", 1, false}), "!4059.60N/02735.98E>two");
    ExpectRepeat(f, len, 9600, "TB1AAW>APZK5,TA1JS-7*:!4059.60N/02735.98E>two", "distinct payload 2");

    len = MkFrameV(f, "TB1AAW", 0, VIAS({"TA1JS", 7, false}), DINFO);
    ExpectRepeat(f, len, 10000, "TB1AAW>APZK5,TA1JS-7*:" DINFO, "fill: explicit hop to us");

    // ---- WIDE: New-N, MYCALL* inserted, hop decremented ----
    DigiReset(DIGI_WIDE, 2, 0);
    len = MkFrameV(f, "TB1AAW", 0, VIAS({"WIDE2", 2, false}), DINFO);
    ExpectRepeat(f, len, 20000, "TB1AAW>APZK5,TA1JS-7*,WIDE2-1:" DINFO, "WIDE2-2");
    len = MkFrameV(f, "TB1AAW", 1, VIAS({"WIDE2", 1, false}), DINFO);
    ExpectRepeat(f, len, 20500, "TB1AAW-1>APZK5,TA1JS-7*,WIDE2*:" DINFO, "WIDE2-1, last hop");
    len = MkFrameV(f, "TB1AAW", 2, VIAS({"WIDE1", 1, false}, {"WIDE2", 1, false}), DINFO);
    ExpectRepeat(f, len, 21000, "TB1AAW-2>APZK5,TA1JS-7*,WIDE1*,WIDE2-1:" DINFO, "WIDE1-1,WIDE2-1");
    len = MkFrameV(f, "TB1AAW", 3, VIAS({"NB1XX", 0, true}, {"WIDE1", 0, true}, {"WIDE2", 2, false}), DINFO);
    ExpectRepeat(f, len, 21500, "TB1AAW-3>APZK5,NB1XX*,WIDE1*,TA1JS-7*,WIDE2-1:" DINFO,
                 "second hop after another digi");

    len = MkFrameV(f, "TB1AAW", 4, VIAS({"WIDE3", 3, false}), DINFO);
    ExpectResult(f, len, 22000, DIGI_TOOMANY, "WIDE3-3 above the hop limit 2");
    len = MkFrameV(f, "TB1AAW", 4, VIAS({"WIDE2", 3, false}), DINFO);
    ExpectResult(f, len, 22000, DIGI_TOOMANY, "WIDE2-3 malformed (N > n)");
    DCheck(gAPRS_DigiStats[DIGI_ST_TOOMANY] == 2, "toomany counter %u, want 2",
           gAPRS_DigiStats[DIGI_ST_TOOMANY]);
    gAPRS_DigiHops = 7;
    len = MkFrameV(f, "TB1AAW", 4, VIAS({"WIDE3", 3, false}), DINFO);
    ExpectRepeat(f, len, 22500, "TB1AAW-4>APZK5,TA1JS-7*,WIDE3-2:" DINFO, "WIDE3-3 with hop limit 7");
    gAPRS_DigiHops = 2;

    // a full path (8 digipeaters): no room to insert
    len = MkFrameV(f, "TB1AAW", 5, VIAS({"NB1AA", 0, true}, {"NB1AB", 0, true}, {"NB1AC", 0, true},
                                        {"NB1AD", 0, true}, {"NB1AE", 0, true}, {"NB1AF", 0, true},
                                        {"NB1AG", 0, true}, {"WIDE2", 2, false}), DINFO);
    ExpectRepeat(f, len, 23000,
                 "TB1AAW-5>APZK5,NB1AA*,NB1AB*,NB1AC*,NB1AD*,NB1AE*,NB1AF*,NB1AG*,WIDE2-1:" DINFO,
                 "full path, WIDE2-2 decremented in place");
    len = MkFrameV(f, "TB1AAW", 6, VIAS({"NB1AA", 0, true}, {"NB1AB", 0, true}, {"NB1AC", 0, true},
                                        {"NB1AD", 0, true}, {"NB1AE", 0, true}, {"NB1AF", 0, true},
                                        {"NB1AG", 0, true}, {"WIDE2", 1, false}), DINFO);
    ExpectRepeat(f, len, 23500,
                 "TB1AAW-6>APZK5,NB1AA*,NB1AB*,NB1AC*,NB1AD*,NB1AE*,NB1AF*,NB1AG*,TA1JS-7*:" DINFO,
                 "full path, WIDE2-1 substituted");

    // a long frame: inserting 7 bytes would exceed the TX budget
    {
        // dst + src + one via (21) + control/PID (2) + 127 info = 150, the limit;
        // inserting MYCALL would make 157
        char big[160];
        memset(big, 'x', sizeof(big));
        big[0] = '>';
        big[127] = 0;
        len = MkFrameV(f, "TB1AAW", 7, VIAS({"WIDE2", 2, false}), big);
        DCheck(len - 2u == DIGI_MAX_FRAME, "long frame is %u bytes, want %u", len - 2u, DIGI_MAX_FRAME);
        char want[400];
        snprintf(want, sizeof(want), "TB1AAW-7>APZK5,WIDE2-1:%s", big);
        ExpectRepeat(f, len, 24000, want, "max-size frame, decremented in place");
        big[127] = 'z'; big[128] = 0;      // one byte over: cannot be held at all
        len = MkFrameV(f, "TB1AAW", 7, VIAS({"WIDE2", 2, false}), big);
        ExpectResult(f, len, 24500, DIGI_DROPPED, "frame over the TX budget");
    }

    len = MkFrameV(f, "TB1AAW", 8, VIAS({"TA1JS", 7, false}, {"WIDE2", 1, false}), DINFO);
    ExpectRepeat(f, len, 25000, "TB1AAW-8>APZK5,TA1JS-7*,WIDE2-1:" DINFO, "explicit hop to us");
    len = MkFrameV(f, "TB1AAW", 8, VIAS({"TA1JS", 3, false}), DINFO);
    ExpectResult(f, len, 25500, DIGI_IGNORED, "explicit hop to our other SSID");
    len = MkFrameV(f, "TB1AAW", 9, VIAS({"TA1JS", 7, true}, {"WIDE2", 1, false}), DINFO);
    ExpectResult(f, len, 26000, DIGI_IGNORED, "we already repeated it (loop)");

    // only UI frames carry APRS; connected-mode AX.25 is not ours to repeat
    len = MkFrameV(f, "TB1AAW", 12, VIAS({"WIDE2", 1, false}), DINFO);
    f[21] = 0x3F; Refcs(f, len);           // control: SABM, not UI
    ExpectResult(f, len, 26500, DIGI_IGNORED, "non-UI control byte");
    len = MkFrameV(f, "TB1AAW", 12, VIAS({"WIDE2", 1, false}), DINFO);
    f[22] = 0xCF; Refcs(f, len);           // PID: not "no layer 3"
    ExpectResult(f, len, 26600, DIGI_IGNORED, "non-APRS PID");

    // a repeat counts only once it is really on the air
    {
        uint16_t n;
        len = MkFrameV(f, "TB1AAW", 13, VIAS({"WIDE2", 1, false}), DINFO);
        Feed(f, len, 26700);
        while (!DIGI_Due(26700u + 200u, false, &n)) {}
        const uint16_t rep0 = gAPRS_DigiStats[DIGI_ST_REPEATED], drop0 = gAPRS_DigiStats[DIGI_ST_DROPPED];
        DIGI_Sent(false);                  // the radio refused to key up
        DCheck(gAPRS_DigiStats[DIGI_ST_REPEATED] == rep0 &&
               gAPRS_DigiStats[DIGI_ST_DROPPED] == drop0 + 1u, "refused TX counted as repeated");
        len = MkFrameV(f, "TB1AAW", 13, VIAS({"WIDE2", 1, false}), DINFO);
        ExpectResult(f, len, 26800, DIGI_QUEUED, "a repeat that never went out may be retried");
        while (!DIGI_Due(26800u + 200u, false, &n)) {}
        DIGI_Sent(true);
        DCheck(gAPRS_DigiStats[DIGI_ST_REPEATED] == rep0 + 1u, "sent repeat not counted");
    }

    // queue holds one repeat
    len = MkFrameV(f, "TB1AAW", 10, VIAS({"WIDE2", 1, false}), DINFO);
    ExpectResult(f, len, 27000, DIGI_QUEUED, "queue first");
    len = MkFrameV(f, "TB1AAW", 11, VIAS({"WIDE2", 1, false}), DINFO);
    ExpectResult(f, len, 27001, DIGI_DROPPED, "queue already holds a repeat");
    DCheck(strcmp(Drain(27002, NULL), "TB1AAW-10>APZK5,TA1JS-7*,WIDE2*:" DINFO) == 0, "queue: first kept");

    DigiReset(DIGI_OFF, 2, 0);
    len = MkFrameV(f, "TB1AAW", 0, VIAS({"WIDE1", 1, false}), DINFO);
    ExpectResult(f, len, 28000, DIGI_IGNORED, "mode OFF");
    DCheck(gAPRS_DigiStats[DIGI_ST_HEARD] == 1, "heard counts with digi off too");

    // ---- timing: hold-off + random delay, busy channel, cancellation ----
    DigiReset(DIGI_WIDE, 2, 3);             // up to 100 ticks of random delay
    {
        uint32_t lo = 0xFFFFFFFFu, hi = 0;
        for (int k = 0; k < 40; k++) {
            char info[40];
            snprintf(info, sizeof(info), ">delay %d", k);
            const uint32_t t0 = 30000u + (uint32_t)k * 4000u;
            len = MkFrameV(f, "TB1AAW", 0, VIAS({"WIDE2", 1, false}), info);
            Feed(f, len, t0);
            uint32_t at = 0;
            Drain(t0, &at);
            const uint32_t d = at - t0;
            if (d < lo) lo = d;
            if (d > hi) hi = d;
        }
        DCheck(lo >= DIGI_HOLDOFF, "delay %u below the hold-off", lo);
        DCheck(hi <= DIGI_HOLDOFF + 100u, "delay %u above hold-off + DDly", hi);
        DCheck(hi - lo >= 20u, "delays not random: %u..%u", lo, hi);
    }

    DigiReset(DIGI_WIDE, 2, 0);
    {
        uint16_t n;
        len = MkFrameV(f, "TB1AAW", 0, VIAS({"WIDE2", 1, false}), DINFO);
        Feed(f, len, 300000);
        bool early = false;
        for (uint32_t t = 300000; t < 300100; t++)       // busy for 1 s
            early |= DIGI_Due(t, true, &n) != NULL;
        DCheck(!early, "transmitted while the channel was busy");
        uint32_t at = 0;
        Drain(300100, &at);
        DCheck(at >= 300100 && at < 300100 + DIGI_HOLDOFF + DIGI_BUSY_RETRY + 1u,
               "busy: repeat came at %u, want soon after 300100", at);

        len = MkFrameV(f, "TB1AAW", 1, VIAS({"WIDE2", 1, false}), DINFO);
        Feed(f, len, 310000);
        for (uint32_t t = 310000; t < 310000 + DIGI_GIVEUP + 50u; t++)
            DIGI_Due(t, true, &n);
        DCheck(gAPRS_DigiStats[DIGI_ST_DROPPED] == 1, "busy 5 s: not dropped");
        DCheck(strcmp(Drain(311000, NULL), "(nothing)") == 0, "busy 5 s: still queued");
        ExpectResult(f, len, 311100, DIGI_QUEUED, "busy 5 s: the sender's retry is not a duplicate");
    }

    // a neighbour's copy of a queued fill-in cancels ours; of a WIDEn-N it does not
    DigiReset(DIGI_FILL, 2, 3);
    len = MkFrameV(f, "TB1AAW", 0, VIAS({"WIDE1", 1, false}, {"WIDE2", 1, false}), DINFO);
    ExpectResult(f, len, 400000, DIGI_QUEUED, "cancel: queue fill-in");
    len = MkFrameV(f, "TB1AAW", 0, VIAS({"NB1XX", 0, true}, {"WIDE2", 1, false}), DINFO);
    ExpectResult(f, len, 400005, DIGI_CANCELLED, "cancel: neighbour repeated it first");
    DCheck(strcmp(Drain(400006, NULL), "(nothing)") == 0, "cancel: repeat still sent");
    DCheck(gAPRS_DigiStats[DIGI_ST_CANCELLED] == 1, "cancel counter");

    // the originator sending again (its WIDE1-1 still unused) is no neighbour's
    // repeat: keep ours queued
    DigiReset(DIGI_FILL, 2, 3);
    len = MkFrameV(f, "TB1AAW", 0, VIAS({"WIDE1", 1, false}, {"WIDE2", 1, false}), DINFO);
    ExpectResult(f, len, 420000, DIGI_QUEUED, "fill retry: queue");
    ExpectResult(f, len, 420003, DIGI_DUP, "fill retry: originator's second copy");
    DCheck(strcmp(Drain(420004, NULL), "TB1AAW>APZK5,TA1JS-7*,WIDE2-1:" DINFO) == 0,
           "fill retry: our repeat must still go out");

    // A WIDE digi never yields, even on a WIDE1-1: its queued copy also carries
    // the WIDE2-1 hop the neighbouring fill-in did not serve.
    DigiReset(DIGI_WIDE, 2, 3);
    len = MkFrameV(f, "TB1AAW", 0, VIAS({"WIDE1", 1, false}, {"WIDE2", 1, false}), DINFO);
    ExpectResult(f, len, 450000, DIGI_QUEUED, "wide no-cancel: queue WIDE1-1,WIDE2-1");
    len = MkFrameV(f, "TB1AAW", 0, VIAS({"NB1XX", 0, true}, {"WIDE2", 1, false}), DINFO);
    ExpectResult(f, len, 450005, DIGI_DUP, "wide no-cancel: fill-in neighbour copy is a duplicate");
    DCheck(strcmp(Drain(450006, NULL), "TB1AAW>APZK5,TA1JS-7*,WIDE1*,WIDE2-1:" DINFO) == 0,
           "wide no-cancel: our repeat (with the WIDE2-1 hop) must still go out");

    DigiReset(DIGI_WIDE, 2, 3);
    len = MkFrameV(f, "TB1AAW", 0, VIAS({"WIDE2", 2, false}), DINFO);
    ExpectResult(f, len, 500000, DIGI_QUEUED, "no-cancel: queue WIDE2-2");
    len = MkFrameV(f, "TB1AAW", 0, VIAS({"NB1XX", 0, true}, {"WIDE2", 1, false}), DINFO);
    ExpectResult(f, len, 500005, DIGI_DUP, "no-cancel: neighbour copy is a duplicate");
    DCheck(strcmp(Drain(500006, NULL), "TB1AAW>APZK5,TA1JS-7*,WIDE2-1:" DINFO) == 0,
           "no-cancel: our WIDE2-2 repeat must still go out");

    printf(gDigiFails ? "DIGIPEATER FAILED (%d)\n"
                      : "DIGIPEATER PASSED (fill, New-N, hop limit, full/long path, explicit, "
                        "loop, queue, delay, busy, cancel)\n", gDigiFails);
    return gDigiFails != 0;
}

static int test_rate_cap(void)
{
    int fail = 0;
    DIGI_Reset(DIGI_WIDE, 2, 0);
    for (unsigned i = 0; i < DIGI_RATE_MAX; i++) {
        if (!DIGI_RateOk(1000u + i * 10u)) { printf("rate: refused repeat %u of %u\n", i, DIGI_RATE_MAX); fail = 1; }
        DIGI_RateNote(1000u + i * 10u);
    }
    if (DIGI_RateOk(1000u + DIGI_RATE_MAX * 10u)) { printf("rate: the cap did not hold\n"); fail = 1; }
    if (DIGI_RateOk(1000u + DIGI_RATE_WINDOW - 1u)) { printf("rate: allowed before the oldest left the window\n"); fail = 1; }
    if (!DIGI_RateOk(1000u + DIGI_RATE_WINDOW)) { printf("rate: still refused after the window\n"); fail = 1; }
    DIGI_RateNote(1000u + DIGI_RATE_WINDOW);              // one more: the next oldest now counts
    if (DIGI_RateOk(1000u + DIGI_RATE_WINDOW + 5u)) { printf("rate: the window did not slide\n"); fail = 1; }
    if (!DIGI_RateOk(1010u + DIGI_RATE_WINDOW)) { printf("rate: the window did not slide forward\n"); fail = 1; }
    DIGI_Reset(DIGI_WIDE, 2, 0);                            // switching APRS off clears it
    if (!DIGI_RateOk(0)) { printf("rate: reset did not clear it\n"); fail = 1; }
    printf(fail ? "RATE CAP FAILED\n" : "RATE CAP PASSED (20 repeats per 60 s, sliding window)\n");
    return fail;
}

static int test_lastheard(void)
{
    int fail = 0;
    char out[32];
    uint8_t a[7];
    const struct { const char *call; uint8_t ssid; uint32_t ticks; bool rpt; const char *want; } T[] = {
        { "TB1AAW", 9,  0,                false, "LH TB1AAW-9 <1m"   },
        { "TB1AAW", 9,  5999,             false, "LH TB1AAW-9 <1m"   },  // 59.99 s
        { "TB1AAW", 9,  6000,             true,  "LH TB1AAW-9 1m*"   },
        { "TB1AAW", 15, 59u * 6000u + 99, false, "LH TB1AAW-15 59m"  },
        { "TB1AAW", 0,  60u * 6000u,      false, "LH TB1AAW 1h"      },
        { "K1A",    0,  47u * 360000u,    true,  "LH K1A 47h*"       },
        { "K1A",    0,  48u * 360000u,    false, "LH K1A 2d"         },
        { "TB1AAW", 15, 0xFFFFFFFFu,      true,  "LH TB1AAW-15 497d*"},  // tick wrap limit
    };
    for (unsigned i = 0; i < sizeof(T) / sizeof(T[0]); i++) {
        AX25_EncodeAddress(T[i].call, T[i].ssid, false, a);
        APRS_FmtLastHeard(out, a, T[i].ticks, T[i].rpt);
        if (strcmp(out, T[i].want) != 0) {
            printf("lastheard %u: got '%s' want '%s'\n", i, out, T[i].want);
            fail = 1;
        }
        // small font: 6 px glyph + 1 px gap from x=2, so 18 characters reach
        // x=127; a 19th would be written past the row into the next one
        if (strlen(out) > 18) {
            printf("lastheard %u: '%s' wider than the 18 characters row 5 holds\n", i, out);
            fail = 1;
        }
    }
    // panel counters: always 8 characters, so two fit a row from x = 2 and x = 66
    {
        const struct { const char *label; uint16_t v; const char *want; } C[] = {
            { "HRD", 0,     "HRD    0" },
            { "RPT", 567,   "RPT  567" },
            { "DUP", 65535, "DUP65535" },
        };
        for (unsigned i = 0; i < sizeof(C) / sizeof(C[0]); i++) {
            APRS_FmtCount(out, C[i].label, C[i].v);
            if (strcmp(out, C[i].want) != 0 || strlen(out) != 8) {
                printf("panel count %u: got '%s' want '%s'\n", i, out, C[i].want);
                fail = 1;
            }
        }
    }
    printf(fail ? "LAST-HEARD FAILED\n" : "LAST-HEARD PASSED (ages <1m..days, SSID, repeated mark, panel counters)\n");
    return fail;
}

static int test_textentry(void)
{
    int fail = 0;
    const struct { char c; int8_t dir; uint8_t n; bool ph; char want; const char *what; } T[] = {
        { ' ',  +1, APRS_TEXT_ALL,   false, 'A',  "blank up -> A"               },
        { '_',  +1, APRS_TEXT_CALL,  true,  'A',  "placeholder up -> A"         },
        { 'A',  -1, APRS_TEXT_CALL,  true,  '_',  "A down -> placeholder"       },
        { 'A',  -1, APRS_TEXT_ALL,   false, ' ',  "A down -> blank"             },
        { 'Z',  +1, APRS_TEXT_ALL,   false, '0',  "Z up -> 0"                   },
        { '9',  +1, APRS_TEXT_CALL,  true,  '_',  "callsign wraps after 9"      },
        { '9',  +1, APRS_TEXT_MSGTO, true,  '-',  "MsgTo reaches '-' for SSID"  },
        { '-',  +1, APRS_TEXT_MSGTO, true,  '_',  "MsgTo wraps after '-'"       },
        { '9',  +1, APRS_TEXT_ALL,   false, '-',  "comment reaches punctuation" },
        { '\'', +1, APRS_TEXT_ALL,  false, ' ',  "comment wraps after '"       },
        { '_',  -1, APRS_TEXT_CALL,  true,  '9',  "callsign wraps down to 9"    },
        { ' ',  -1, APRS_TEXT_ALL,   false, '\'', "comment wraps down to '"    },
        { 'q',  +1, APRS_TEXT_ALL,   false, 'R',  "lowercase is read as upper"  },
        { '#',  +1, APRS_TEXT_ALL,   false, 'A',  "unknown char counts as blank"},
    };
    for (unsigned i = 0; i < sizeof(T) / sizeof(T[0]); i++) {
        const char got = APRS_StepChar(T[i].c, T[i].dir, T[i].n, T[i].ph);
        if (got != T[i].want) {
            printf("text entry: %s: got '%c' want '%c'\n", T[i].what, got, T[i].want);
            fail = 1;
        }
    }
    printf(fail ? "ARROW TEXT ENTRY FAILED\n" : "ARROW TEXT ENTRY PASSED (order, wraps, callsign/MsgTo limits)\n");
    return fail;
}

static int test_hdlc_and_parsers(void)
{
    // Build the frame exactly like APRS_BuildHeader plus a hand-made info field
    static const uint8_t INFO[] = "!1000.00N/02000.00E>UV-K5 APRS";
    uint8_t frame[7 * 4 + 2 + (sizeof(INFO) - 1) + 2];
    uint16_t idx = 0;
    AX25_EncodeAddress("APZK5", 0, false, &frame[idx]); idx += 7;  // mirrors APRS_TOCALL
    AX25_EncodeAddress("N0CALL", 0, false, &frame[idx]); idx += 7;
    AX25_EncodeAddress("WIDE1", 1, false, &frame[idx]); idx += 7;
    AX25_EncodeAddress("WIDE2", 1, true,  &frame[idx]); idx += 7;
    frame[idx++] = 0x03;
    frame[idx++] = 0xF0;
    memcpy(&frame[idx], INFO, sizeof(INFO) - 1);
    idx += sizeof(INFO) - 1;
    uint16_t fcs = AX25_CalculateFCS(frame, idx);
    frame[idx++] = fcs & 0xFF;
    frame[idx++] = fcs >> 8;

    // Encode to on-air bitstream
    static uint8_t hdlcbuf[HDLC_BUF_SIZE];
    struct { uint16_t bits; } w = { HDLC_EncodeFrame(hdlcbuf, frame, idx) };
    printf("frame=%u bytes, stream=%u bits (%u bytes)\n", idx, w.bits, (w.bits + 7) / 8);

    // 1) unpack MSB-first to line levels; 2) NRZI decode (transition=0)
    static uint8_t nrz[HDLC_BUF_SIZE * 8];
    uint8_t prev = 1;  // arbitrary receiver start state
    for (uint32_t i = 0; i < w.bits; i++) {
        uint8_t level = (hdlcbuf[i >> 3] >> (7 - (i & 7))) & 1;
        nrz[i] = (level == prev) ? 1 : 0;
        prev = level;
    }

    // 3) flag hunt + destuff, collecting frames between flags
    uint8_t rxframe[128];
    int best_len = 0;
    uint32_t i = 0;
    while (i + 8 <= w.bits) {
        // match flag 0x7E = bits 0,1,1,1,1,1,1,0 (LSB first)
        static const uint8_t flagbits[8] = {0,1,1,1,1,1,1,0};
        bool is_flag = true;
        for (int k = 0; k < 8; k++) if (nrz[i + k] != flagbits[k]) { is_flag = false; break; }
        if (!is_flag) { i++; continue; }
        // found flag; skip consecutive flags
        uint32_t j = i + 8;
        while (j + 8 <= w.bits) {
            bool again = true;
            for (int k = 0; k < 8; k++) if (nrz[j + k] != flagbits[k]) { again = false; break; }
            if (!again) break;
            j += 8;
        }
        // collect destuffed bits until next flag
        uint8_t bitbuf[2048]; int nb = 0; int ones = 0; bool closed = false;
        uint32_t p = j;
        while (p < w.bits) {
            // check for closing flag at p
            if (p + 8 <= w.bits) {
                bool f = true;
                for (int k = 0; k < 8; k++) if (nrz[p + k] != flagbits[k]) { f = false; break; }
                if (f) { closed = true; break; }
            }
            uint8_t b = nrz[p++];
            if (ones == 5) { // stuffed zero expected
                if (b == 0) { ones = 0; continue; }   // drop stuffed bit
                else break;                            // 6 ones = abort/flag part
            }
            if (nb < 2048) bitbuf[nb++] = b;
            ones = b ? ones + 1 : 0;
        }
        if (closed && nb >= 136 && (nb % 8) == 0) {
            int len = nb / 8;
            for (int k = 0; k < len; k++) {
                uint8_t byte = 0;
                for (int m = 0; m < 8; m++) byte |= bitbuf[k * 8 + m] << m;  // LSB first
                rxframe[k] = byte;
            }
            best_len = len;
            break;
        }
        i = j;
    }

    if (!best_len) { printf("FAIL: no frame decoded\n"); return 1; }
    printf("decoded frame: %d bytes\n", best_len);

    // FCS check
    uint16_t want = AX25_CalculateFCS(rxframe, best_len - 2);
    uint16_t got  = rxframe[best_len - 2] | (rxframe[best_len - 1] << 8);
    printf("FCS: calc=%04X rx=%04X -> %s\n", want, got, want == got ? "OK" : "FAIL");

    // parse addresses
    char dst[8] = {0}, src[8] = {0}, via[8] = {0}, via2[8] = {0};
    for (int k = 0; k < 6; k++) { dst[k] = rxframe[k] >> 1; src[k] = rxframe[7 + k] >> 1; via[k] = rxframe[14 + k] >> 1; via2[k] = rxframe[21 + k] >> 1; }
    printf("dst=%.6s ssid=%d  src=%.6s ssid=%d  via=%.6s-%d(l%d)  via2=%.6s-%d(l%d)\n",
        dst, (rxframe[6] >> 1) & 0xF, src, (rxframe[13] >> 1) & 0xF,
        via, (rxframe[20] >> 1) & 0xF, rxframe[20] & 1,
        via2, (rxframe[27] >> 1) & 0xF, rxframe[27] & 1);
    printf("ctrl=%02X pid=%02X\n", rxframe[28], rxframe[29]);
    printf("info=\"%.*s\"\n", best_len - 2 - 30, &rxframe[30]);

    bool ok = (want == got) && rxframe[28] == 0x03 && rxframe[29] == 0xF0 &&
              !memcmp(dst, "APZK5 ", 6) && !memcmp(src, "N0CALL", 6) &&
              !memcmp(via, "WIDE1 ", 6) && !memcmp(via2, "WIDE2 ", 6) &&
              (rxframe[20] & 1) == 0 && (rxframe[27] & 1) == 1;
    printf(ok ? "ALL CHECKS PASSED\n" : "CHECKS FAILED\n");
    if (!ok) return 1;

    // ---- position parsers ----
    int pfail = 0;
    int32_t lat, lon;

    // uncompressed: our own beacon text
    if (!APRS_ParseUncompressed((const uint8_t *)"1000.00N/02000.00E>", 19, &lat, &lon) ||
        lat != 10000000 || lon != 20000000) {
        printf("uncompressed FAIL: %d %d\n", lat, lon);
        pfail = 1;
    }

    // Mic-E: 33 deg 25.64' N, 112 deg 07.35' W (APRS 1.01 spec example values)
    // dest "S32UVT" (digits 3,3,2,5,6,4; N=1, lon offset=1, W=1), AX.25-shifted
    {
        uint8_t dest[7];
        const char *dc = "S32UVT";
        for (int k = 0; k < 6; k++) dest[k] = (uint8_t)(dc[k] << 1);
        dest[6] = 0x60;
        const uint8_t micinfo[9] = { 0x60, 12 + 28, 7 + 28, 35 + 28, 'x', 'x', 'x', '/', '>' };
        if (!APRS_ParseMicE(dest, micinfo, 9, &lat, &lon) ||
            lat != 33427333 || lon != -112122500) {
            printf("mic-e FAIL: %d %d\n", lat, lon);
            pfail = 1;
        }
    }

    // compressed: APRS 1.01 spec example "/5L!!<*e7>" = 49.5000N 72.7500W
    if (!APRS_ParseCompressed((const uint8_t *)"/5L!!<*e7>{?!", 13, &lat, &lon) ||
        abs(lat - 49500000) > 120 || abs(lon - -72750000) > 120) {
        printf("compressed FAIL: %d %d\n", lat, lon);
        pfail = 1;
    }

    printf(pfail ? "POSITION PARSERS FAILED\n" : "POSITION PARSERS PASSED (uncompressed, Mic-E, base-91)\n");


    return pfail;
}

int main(void)
{
    const int hfail = test_hdlc_and_parsers();
    const int dfail = test_digi();
    const int rfail = test_rate_cap();
    const int lfail = test_lastheard();
    const int efail = test_textentry();
    return hfail || dfail || rfail || lfail || efail;
}
