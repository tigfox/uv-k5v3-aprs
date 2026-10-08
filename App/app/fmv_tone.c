/* Copyright 2026 tigfox.
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



#include "app/fmv_tone.h"
#include <string.h>

#define MAX_CTCSS_TENTHS 9999u        /* 999.9 Hz: the highest the label can show */
#define MAX_DCS_CODE     0777u

static char *put(char *p, const char *s)
{
    while (*s)
        *p++ = *s++;
    return p;
}

void FMV_ToneLabel(bool dcs, uint16_t value, char out[FMV_TONE_LABEL_MAX])
{
    char *p = put(out, dcs ? "TX DCS " : "TX PL ");
    if (dcs) {
        if (value > MAX_DCS_CODE) {
            p = put(p, "???");
        } else {
            *p++ = (char)('0' + ((value >> 6) & 7u));
            *p++ = (char)('0' + ((value >> 3) & 7u));
            *p++ = (char)('0' + (value & 7u));
        }
    } else if (value > MAX_CTCSS_TENTHS) {
        p = put(p, "???");
    } else {
        char digits[4];
        unsigned n = 0, whole = value / 10u;
        do { digits[n++] = (char)('0' + whole % 10u); whole /= 10u; } while (whole);
        while (n)
            *p++ = digits[--n];
        *p++ = '.';
        *p++ = (char)('0' + value % 10u);
    }
    *p = 0;
}
