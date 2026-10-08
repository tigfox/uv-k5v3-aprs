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


#ifndef APP_FMV_TONE_H
#define APP_FMV_TONE_H

#include <stdbool.h>
#include <stdint.h>

#define FMV_TONE_LABEL_MAX 16

/* The text for a tone the search found and set as the transmit tone: "TX PL 100.0" (value = tenths of Hz) or
 * "TX DCS 023" (value = the code as a number whose octal digits are the code). A value that cannot be a tone
 * prints "???". */
void FMV_ToneLabel(bool dcs, uint16_t value, char out[FMV_TONE_LABEL_MAX]);

#endif
