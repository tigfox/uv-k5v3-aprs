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


#ifndef APP_FMV_STORE_H
#define APP_FMV_STORE_H

#include <stdbool.h>
#include <stdint.h>
#include "app/fmv_info.h"

/* The channel info table in the external flash (through the EEPROM layer). */

/* The city / landmark text of memory channel `channel` (0-based) if it has a record that belongs to a channel
 * receiving on rx_freq (10 Hz units). out needs FMV_INFO_TEXT_MAX + 1 bytes. */
bool FMV_StoreLoad(uint16_t channel, uint32_t rx_freq, char *out);
/* Write (or, for an empty text, clear) the record. Returns false for a channel with no slot. *cut is set if the
 * text did not fit (cut may be NULL). Writing flash is slow: not for use in a loop. */
bool FMV_StoreSave(uint16_t channel, uint32_t rx_freq, const char *text, bool *cut);

#endif
