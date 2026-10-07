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

#ifndef APP_APRS_LASTHEARD_H
#define APP_APRS_LASTHEARD_H

#include <stdbool.h>
#include <stdint.h>

// "LH TB1AAW-9 12m*": source address a7 (AX.25 form), heard `ticks` 10 ms ticks ago, '*' if this
// station repeated it. At most 18 characters (fits one row of the small font).
void APRS_FmtLastHeard(char *out, const uint8_t *a7, uint32_t ticks, bool rpt);
// "RPT  567": 3-letter label and the count right-aligned in 5 places: always 8 characters.
void APRS_FmtCount(char *out, const char *label, uint16_t v);

#endif
