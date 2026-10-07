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
// Last-heard line and panel counters: the formatters (hardware-free).
// The radio has no clock, so the line shows how long ago, not when.
// ---------------------------------------------------------------------------

#include "app/aprs_lastheard.h"
#include <string.h>

// "LH TB1AAW-9 12m*" (last heard): the source address a7 (AX.25 form), heard
// `ticks` 10 ms ticks ago; '*' if this station repeated it. At most 18
// characters (callsign 9, age 4 up to 497 days, where the tick counter wraps):
// all the small font fits on a row from x = 2, and it does not clip.
void APRS_FmtLastHeard(char *out, const uint8_t *a7, uint32_t ticks, bool rpt)
{
    char *p = out;
    memcpy(p, "LH ", 3);
    p += 3;
    for (uint8_t k = 0; k < 6; k++) {
        const char c = (char)(a7[k] >> 1);
        if (c > ' ')
            *p++ = c;
    }
    const uint8_t ssid = (a7[6] >> 1) & 0x0Fu;
    if (ssid > 0) {
        *p++ = '-';
        if (ssid >= 10)
            *p++ = '1';
        *p++ = (char)('0' + ssid % 10u);
    }
    *p++ = ' ';

    const uint32_t min = ticks / 6000u;
    if (min == 0) {
        *p++ = '<';
        *p++ = '1';
        *p++ = 'm';
    } else {
        uint32_t v = min;
        char unit = 'm';
        if (min >= 48u * 60u) {
            v = min / (24u * 60u);
            unit = 'd';
        } else if (min >= 60u) {
            v = min / 60u;
            unit = 'h';
        }
        char d[4];
        uint8_t n = 0;
        do {
            d[n++] = (char)('0' + v % 10u);
            v /= 10u;
        } while (v > 0 && n < sizeof(d));
        while (n > 0)
            *p++ = d[--n];
        *p++ = unit;
    }
    if (rpt)
        *p++ = '*';
    *p = 0;
}

// "RPT  567": a 3-letter label and the count right-aligned in 5 places, so
// always 8 characters - two fit one small-font row (at x = 2 and x = 66).
// Used by the APRS panel (ENABLE_APRS_PANEL); unused otherwise.
void APRS_FmtCount(char *out, const char *label, uint16_t v)
{
    memcpy(out, label, 3);
    for (int i = 7; i >= 3; i--) {
        out[i] = (i == 7 || v > 0) ? (char)('0' + v % 10u) : ' ';
        v /= 10u;
    }
    out[8] = 0;
}
