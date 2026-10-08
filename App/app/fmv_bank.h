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


#ifndef APP_FMV_BANK_H
#define APP_FMV_BANK_H

#include <stdbool.h>
#include <stdint.h>

/* Banks for the FM Voice build: a bank is a scan list. This is the hardware-free part: moving through the channels
 * of a bank, remembering the last channel of each bank, and the name shown when the bank changes. */

typedef bool (*fmv_pred_t)(uint16_t channel, void *ctx);   /* is this channel usable in the bank */

/* The first channel at or after `from` (stepping by dir, +1 or -1, wrapping at 0..total-1) for which ok() is true;
 * 0xFFFF if there is none. A bank with one channel returns it from any start. */
uint16_t FMV_NextIn(uint16_t from, int8_t dir, uint16_t total, fmv_pred_t ok, void *ctx);

/* Where to land when switching to a bank: `last` (the channel it was left on) if that is still in the bank,
 * else its first channel, else 0xFFFF (an empty bank). */
uint16_t FMV_BankTarget(uint16_t last, uint16_t total, fmv_pred_t ok, void *ctx);

/* "BANK HOME", "BANK 07", "BANK ALL", "BANK MIX" for the screen. list is a scan-list value (1..24, then ALL = 25,
 * MIX = 26); name4 is the list's 4-character name from the radio (may be blank or unprintable). out needs 12 bytes. */
void FMV_BankLabel(uint8_t list, const char name4[4], uint8_t all_value, uint8_t mix_value, char *out);

/* Long bank names: 16 bytes per list in the radio (FMV_BANKNAME_ADDR, see fmv_store.h), padded with NUL, 0xFF or spaces. */
#define FMV_BANKNAME_MAX    16u
#define FMV_BANKLABEL_MAX   20u          /* room for "BANK " + a short name, or a 16-character long name, and the NUL */

/* The text of a stored long name into out (FMV_BANKNAME_MAX + 1 bytes). false, and "", if there is none: never written
 * (all 0xFF), only spaces, or any byte outside printable ASCII before the padding. Trailing spaces are dropped. */
bool FMV_BankNameClean(const uint8_t raw[FMV_BANKNAME_MAX], char *out);

/* What the card shows when the bank changes: "BANK ALL" / "BANK MIX", else the long name alone (it fills the row), else
 * the short name as FMV_BankLabel does, else the number. long_name is what FMV_BankNameClean returned ("" if none). */
void FMV_BankLabelLong(uint8_t list, const char *long_name, const char name4[4], uint8_t all_value, uint8_t mix_value,
                       char *out);

/* Splits a long name into two display lines of at most `width` characters (width <= FMV_BANKNAME_MAX; each out needs
 * FMV_BANKNAME_MAX + 1 bytes): after the last space that leaves the first line within the width, else after `width`
 * characters. A name that fits is all in line1, line2 is "". Text past two lines is dropped. */
void FMV_BankNameWrap(const char *name, unsigned width, char *line1, char *line2);

#define FMV_BANK_LAST_NONE 0xFFFFu

#endif
