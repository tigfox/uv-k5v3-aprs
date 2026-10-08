/* Copyright 2026 tigfox. The tone method is armel's (App/apps/aprstx).
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


#ifndef APP_APRS_MODEM_H
#define APP_APRS_MODEM_H

#include <stdbool.h>
#include <stdint.h>

/* Bell 202 transmit tones (hardware-free part). The BK4829 TX tone path plays one tone from
 * REG_71 (frequency) at the gain in REG_70; the driver rewrites both at every NRZI transition,
 * 833 us (40000 CPU cycles) apart. */

#define APRS_TONE_REG70_ENABLE  0x8000u   /* TONE1 on; gain in bits 14:8 */
#define APRS_TONE_REG71_MARK    12389u    /* 1200 Hz: (f * 1353245 + 2^16) >> 17 */
#define APRS_TONE_REG71_SPACE   22714u    /* 2200 Hz */
#define APRS_TONE_LEVEL_MIN     10u
#define APRS_TONE_LEVEL_MAX     127u
#define APRS_TONE_LEVEL_DEFAULT 66u       /* the firmware's own tone gain */
#define APRS_TONE_TWIST_MIN     (-4)      /* 2200 Hz gain x 0.5 (-6 dB) */
#define APRS_TONE_TWIST_MAX     8         /* 2200 Hz gain x 2 (+6 dB) */
#define APRS_TX_CYCLES_PER_BIT  40000u    /* 48 MHz / 1200 baud, exact */

/* REG_71 and REG_70 values for the mark (1200 Hz) or space (2200 Hz) tone. The space gain is
 * level x (8 + twist) / 8, clamped to 127. level is clamped to 10..127, twist to -4..8. */
void APRS_ToneRegs(bool space, uint8_t level, int8_t twist, uint16_t *reg71, uint16_t *reg70);

/* Line level of bit i of an HDLC_EncodeFrame() buffer (1 = mark, 0 = space). */
bool APRS_LineLevel(const uint8_t *bits, uint16_t i);

#endif
