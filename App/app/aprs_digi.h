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

#ifndef APP_APRS_DIGI_H
#define APP_APRS_DIGI_H

#include <stdbool.h>
#include <stdint.h>

// Digipeater settings and counters (ENABLE_APRS_DIGI), shared with the menu,
// EEPROM and UART code. The logic itself is in aprs_digi.c.

enum { DIGI_OFF, DIGI_FILL, DIGI_WIDE };   // gAPRS_DigiMode

enum {                                    // gAPRS_DigiStats[] index
    DIGI_ST_HEARD,       // valid frames decoded
    DIGI_ST_REPEATED,    // repeats transmitted
    DIGI_ST_DUP,         // already repeated within 30 s
    DIGI_ST_CANCELLED,   // fill-in yielded: a neighbour repeated it first
    DIGI_ST_TOOMANY,     // WIDEn-N above the hop limit, or malformed
    DIGI_ST_DROPPED,     // queue full, frame too long, or channel busy for 5 s
    DIGI_ST_N
};

#define DIGI_HOPS_DEFAULT 2u
#define DIGI_HOPS_MAX     7u
#define DIGI_DELAYS       4u   // DDly choices: OFF, 250, 500, 1000 ms

extern uint8_t  gAPRS_DigiMode;
extern uint8_t  gAPRS_DigiHops;    // 1..DIGI_HOPS_MAX: highest n honoured in WIDEn-N
extern uint8_t  gAPRS_DigiDelay;   // 0..DIGI_DELAYS-1
extern uint16_t gAPRS_DigiStats[DIGI_ST_N];

#define DIGI_MAX_FRAME   150u   // a repeat, FCS excluded: the TX bitstream budget (APRS_RAWTX_MAX)
#define DIGI_MAX_VIAS    8u     // AX.25 limit on digipeater addresses
#define DIGI_SEEN        16u    // recent-repeat memory
#define DIGI_DUPE_TICKS  3000u  // 30 s
#define DIGI_HOLDOFF     10u    // 100 ms after the frame before keying up
#define DIGI_BUSY_RETRY  25u    // a busy channel is re-checked after 100..340 ms
#define DIGI_GIVEUP      500u   // abandon a repeat the channel kept busy for 5 s

typedef enum {
    DIGI_IGNORED,      // not ours: digi off, own frame, no path, other alias
    DIGI_QUEUED,
    DIGI_DUP,
    DIGI_CANCELLED,
    DIGI_TOOMANY,
    DIGI_DROPPED,
} digi_result_t;


// Decide what to do with a received frame (len includes the 2-byte FCS) and queue its
// repeat, with the path rewritten, if there is one. now is in 10 ms ticks.
digi_result_t DIGI_Consider(const uint8_t *frame, uint16_t len, uint32_t now,
                            const char *mycall, uint8_t myssid);
// The queued repeat if due and the channel is clear (FCS excluded, two spare bytes for
// it), else NULL. Report the outcome with DIGI_Sent.
uint8_t *DIGI_Due(uint32_t now, bool busy, uint16_t *len);
void DIGI_Sent(bool on_air);
// Forget everything and set the mode, hop limit and delay choice (start-up and tests).
void DIGI_Reset(uint8_t mode, uint8_t hops, uint8_t delay);
const uint8_t *DIGI_QueuedFrame(void);   // the frame DIGI_Due last handed out or holds
bool DIGI_HasQueued(void);

#endif
