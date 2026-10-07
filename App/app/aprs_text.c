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

// Arrow text entry: the character stepper (hardware-free).
// The arrows walk the characters the two-digit codes reach, letters first because a callsign
// starts with one. Callsign fields stop before the punctuation; MsgTo keeps '-' for the SSID.

#include "app/aprs_text.h"

static const char APRS_TEXT_CHARS[] = " ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789-./?!@:,'";


// The character after (dir = +1) or before (-1) c among the first n. Blank,
// '_' and anything outside the set count as position 0; placeholder fields
// (the 9-character callsign ones) show position 0 as '_' rather than ' '.
char APRS_StepChar(char c, int8_t dir, uint8_t n, bool placeholder)
{
    if (c >= 'a' && c <= 'z')
        c = (char)(c - 32);
    int idx = 0;
    for (uint8_t i = 1; i < n; i++)
        if (APRS_TEXT_CHARS[i] == c)
            idx = i;
    idx += dir;
    if (idx < 0)
        idx = n - 1;
    else if (idx >= n)
        idx = 0;
    return (idx == 0 && placeholder) ? '_' : APRS_TEXT_CHARS[idx];
}

_Static_assert(sizeof(APRS_TEXT_CHARS) - 1 == APRS_TEXT_ALL, "APRS_TEXT_ALL must match the character set");
