/* Copyright 2026 tigfox. Arrow text entry after the ta1js design.
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


#ifndef APP_APRS_EDIT_H
#define APP_APRS_EDIT_H

#include <stdbool.h>
#include <stdint.h>
#include "app/aprs_settings.h"

/* Text entry for the APRS menu fields with the arrow keys, hardware-free.
 * A field opens empty: UP/DOWN walk the characters at the cursor (aprs_text.c), the digit keys
 * type a digit, STAR moves to the next position, EXIT backspaces, MENU commits. Committing a
 * blank field keeps the old value, so a stray open never erases a setting. */

typedef enum {
    APRS_EDIT_CALL,      /* 1..6 of A-Z 0-9 */
    APRS_EDIT_LOC,       /* the 15-digit Loc code, checksum verified */
    APRS_EDIT_CMNT,      /* up to 43 characters */
    APRS_EDIT_MSGTO,     /* up to 9 of A-Z 0-9 - (a call with its SSID) */
} aprs_edit_kind_t;

typedef struct {
    aprs_edit_kind_t kind;
    uint8_t          max;                       /* positions in the field */
    uint8_t          pos;                       /* cursor */
    char             buf[APRS_COMMENT_MAX + 1]; /* NUL-terminated; blanks are ' ' ('_' for call fields) */
} aprs_edit_t;

void APRS_EditBegin(aprs_edit_t *e, aprs_edit_kind_t kind);
void APRS_EditArrow(aprs_edit_t *e, int8_t dir);        /* UP = +1, DOWN = -1 */
void APRS_EditDigit(aprs_edit_t *e, uint8_t digit);     /* 0..9: type it, move on */
void APRS_EditNext(aprs_edit_t *e);
/* Backspace: clear the character before the cursor (or under it at the end of the field).
 * Returns false when the field is already empty, i.e. EXIT should leave the editor. */
bool APRS_EditBack(aprs_edit_t *e);

typedef enum { APRS_EDIT_OK, APRS_EDIT_UNCHANGED, APRS_EDIT_INVALID } aprs_edit_result_t;
/* Apply the field to *s (left alone unless OK). UNCHANGED: nothing was entered. INVALID: bad Loc
 * code or an interior blank in a call. */
aprs_edit_result_t APRS_EditCommit(const aprs_edit_t *e, aprs_settings_t *s);

/* The page of `width` characters holding the cursor and a caret line under it ("ABC\n ^ "); out
 * needs 2 * width + 2 bytes. */
void APRS_EditView(const aprs_edit_t *e, unsigned width, char *out);

#endif
