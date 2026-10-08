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


#ifndef APP_FMV_INFO_H
#define APP_FMV_INFO_H

#include <stdbool.h>
#include <stdint.h>

/* Channel info (FM Voice build): the city / landmark text of a memory channel, as RepeaterBook writes it
 * ("Mechanicsburg, Three Square Hollow"). The callsign is the channel's name and the frequency its own, so this
 * text is the only new data. Hardware-free; the storage is app/fmv_store.c.
 *
 * Table: FMV_INFO_SLOTS records of FMV_INFO_RECORD bytes in the EEPROM address space at FMV_INFO_EEPROM_ADDR,
 * record i for memory channel i (0-based, so CHIRP's memory 1 is i = 0). Record: u16 little-endian check of the
 * channel's receive frequency, then the text, NUL-terminated, at most FMV_INFO_TEXT_MAX characters. An erased
 * slot is all 0xFF. The check makes a record that no longer belongs to its channel invisible (a channel changed by
 * a program that does not know the table) instead of showing the wrong repeater. */

#define FMV_INFO_SLOTS        256u
#define FMV_INFO_RECORD       48u
#define FMV_INFO_TEXT_MAX     45u
#define FMV_INFO_EEPROM_ADDR  0xD000u    /* 256 x 48 = 0x3000 bytes: up to the end of the 16-bit space */
#define FMV_LINE_MAX          FMV_INFO_TEXT_MAX

/* 16-bit check of a receive frequency in 10 Hz units; never 0xFFFF (erased flash). */
uint16_t FMV_FreqCheck(uint32_t rx_freq);

/* Make the record for a channel. The text is cleaned (trimmed, runs of spaces collapsed, anything not printable
 * ASCII becomes '?') and cut at FMV_INFO_TEXT_MAX; *cut says whether it was cut (cut may be NULL). Returns false
 * for an empty text, and then rec is an erased record, ready to be written to clear the slot. */
bool FMV_InfoEncode(uint8_t rec[FMV_INFO_RECORD], uint32_t rx_freq, const char *text, bool *cut);
/* The text of a record if it is valid and belongs to a channel on rx_freq; false (out empty) otherwise.
 * out needs FMV_INFO_TEXT_MAX + 1 bytes. */
bool FMV_InfoDecode(const uint8_t rec[FMV_INFO_RECORD], uint32_t rx_freq, char *out);
bool FMV_InfoIsErased(const uint8_t rec[FMV_INFO_RECORD]);

/* Split "City, Landmark" at the first comma into two lines, each trimmed and at most FMV_LINE_MAX characters.
 * city and rest need FMV_LINE_MAX + 1 bytes. */
void FMV_InfoSplit(const char *text, char *city, char *rest);

/* A marquee for text wider than the row: the window of `width` characters of text at step `tick` (one step per
 * 500 ms). Text that fits is shown as it is. Longer text rests FMV_MARQUEE_PAUSE steps at its start, scrolls one
 * character per step, rests at its end, and starts again. Returns true if the text scrolls (so the screen must be
 * redrawn every step). out needs width + 1 bytes. */
#define FMV_MARQUEE_PAUSE 4u
bool FMV_Marquee(const char *text, unsigned width, uint32_t tick, char *out);

/* EEPROM address of the record for a 0-based channel, 0 if the channel has no slot. */
uint16_t FMV_InfoAddress(uint16_t channel);

#endif
