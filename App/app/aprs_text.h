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

#ifndef APP_APRS_TEXT_H
#define APP_APRS_TEXT_H

#include <stdbool.h>
#include <stdint.h>

enum {                       // how much of APRS_TEXT_CHARS a field may use
    APRS_TEXT_CALL  = 37,    // blank, A-Z, 0-9
    APRS_TEXT_MSGTO = 38,    // ... and '-'
    APRS_TEXT_ALL   = 46,    // everything: letters, digits, - . / ? ! @ : , '
};

// The character after (dir = +1) or before (-1) c among the first n. Blank, '_' and anything
// outside the set count as position 0; placeholder fields show position 0 as '_' not ' '.
char APRS_StepChar(char c, int8_t dir, uint8_t n, bool placeholder);

#endif
