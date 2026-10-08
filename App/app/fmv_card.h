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


#ifndef APP_FMV_CARD_H
#define APP_FMV_CARD_H

#include <stdbool.h>
#include <stdint.h>

/* The channel card on the main screen (FM Voice build): the city / landmark text of the channel on the screen,
 * in the row the RSSI bar uses while receiving, so it shows whenever the radio is idle. Text wider than the row
 * scrolls (the callsign is the channel name and the frequency the screen's own: both are already there). */

/* Draw the text of the channel the main VFO is on at text row `row` (0-7). Returns false (nothing drawn) for a
 * frequency channel, a channel above FMV_INFO_SLOTS, or one with no text that belongs to it. */
bool FMV_CardRow(uint8_t row);
/* Show "BANK name" in the card row for about a second and a half (list: scan-list value). */
void FMV_CardFlashBank(uint8_t list);
/* Show text (up to 19 characters) in the card row for `steps` half-seconds; steps 0 clears it. */
void FMV_CardFlashText(const char *text, uint8_t steps);
/* Every 500 ms: advance the marquee and ask for a redraw while it scrolls. */
void FMV_Task500ms(void);

#endif
