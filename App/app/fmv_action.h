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


#ifndef APP_FMV_ACTION_H
#define APP_FMV_ACTION_H

#include <stdbool.h>
#include <stdint.h>

/* FM Voice key actions and channel browsing inside the active bank (scan list). */

/* Action "BANK": go to the next scan list that has channels, land on the channel you left it at, show its name. */
void FMV_ActionBank(void);

/* UP / DOWN in channel mode: the next channel of the active bank from `from` going `dir` (+1 / -1, wrapping),
 * or 0xFFFF to let the stock code choose (bank ALL, an empty bank). Remembers the channel for the bank. */
uint16_t FMV_BrowseNext(uint16_t from, int8_t dir);

/* Action "TONE SEARCH": listen on the channel's frequency for a CTCSS tone (or a DCS code), and set what is found as
 * the transmit tone of the channel. Any key press gives up (PTT also stops it). */
void FMV_ActionToneSearch(void);

/* Call first in the main key handler: while a search runs, a key press ends it and the key is consumed (true). */
bool FMV_ToneSearchKey(bool pressed);

/* Every 500 ms: finish a search (restore the channel, apply the tone, show the result). */
void FMV_ToneTask500ms(void);

#endif
