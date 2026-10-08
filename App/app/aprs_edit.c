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


#include "app/aprs_edit.h"
#include "app/aprs_msg.h"
#include "app/aprs_parse.h"
#include "app/aprs_text.h"
#include <string.h>

static bool is_call(const aprs_edit_t *e) { return e->kind == APRS_EDIT_CALL || e->kind == APRS_EDIT_MSGTO; }
static bool is_blank(char c) { return c == ' ' || c == '_'; }
static char blank_char(const aprs_edit_t *e) { return is_call(e) ? '_' : ' '; }

void APRS_EditBegin(aprs_edit_t *e, aprs_edit_kind_t kind)
{
    memset(e, 0, sizeof *e);
    e->kind = kind;
    switch (kind) {
    case APRS_EDIT_CALL:  e->max = APRS_CALL_MAX; break;
    case APRS_EDIT_LOC:   e->max = APRS_LOC_MAX; break;
    case APRS_EDIT_CMNT:  e->max = APRS_COMMENT_MAX; break;
    case APRS_EDIT_MSG:   e->max = APRS_MSG_TEXT_MAX; break;
    default:              e->max = APRS_MSGTO_MAX; break;
    }
    memset(e->buf, kind == APRS_EDIT_LOC ? '0' : blank_char(e), e->max);
}

void APRS_EditArrow(aprs_edit_t *e, int8_t dir)
{
    char *c = &e->buf[e->pos];
    if (e->kind == APRS_EDIT_LOC) {
        int d = (*c >= '0' && *c <= '9') ? *c - '0' : 0;
        d += dir;
        *c = (char)('0' + (d < 0 ? 9 : d > 9 ? 0 : d));
        return;
    }
    const uint8_t n = e->kind == APRS_EDIT_CALL ? APRS_TEXT_CALL
                    : e->kind == APRS_EDIT_MSGTO ? APRS_TEXT_MSGTO : APRS_TEXT_ALL;
    *c = APRS_StepChar(*c, dir, n, is_call(e));
}

void APRS_EditDigit(aprs_edit_t *e, uint8_t digit)
{
    if (digit > 9)
        return;
    e->buf[e->pos] = (char)('0' + digit);
    APRS_EditNext(e);
}

void APRS_EditNext(aprs_edit_t *e)
{
    if (e->pos + 1u < e->max)
        e->pos++;
}

bool APRS_EditBack(aprs_edit_t *e)
{
    const char blank = e->kind == APRS_EDIT_LOC ? '0' : blank_char(e);
    bool any = false;
    for (uint8_t i = 0; i < e->max; i++)
        any = any || !(e->kind == APRS_EDIT_LOC ? e->buf[i] == '0' : is_blank(e->buf[i]));
    if (!any && e->pos == 0)
        return false;
    if (e->buf[e->pos] != blank && !(e->kind != APRS_EDIT_LOC && is_blank(e->buf[e->pos]))) {
        e->buf[e->pos] = blank;        // clear what is under the cursor first
        return true;
    }
    if (e->pos > 0)
        e->pos--;
    e->buf[e->pos] = blank;
    return true;
}

aprs_edit_result_t APRS_EditCommit(const aprs_edit_t *e, aprs_settings_t *s)
{
    aprs_settings_t n = *s;
    if (e->kind == APRS_EDIT_MSG)
        return APRS_EDIT_OK;                   /* not a setting: the caller takes the text with APRS_EditText */
    if (e->kind == APRS_EDIT_LOC) {
        int32_t lat, lon;
        bool entered = false;
        for (uint8_t i = 0; i < e->max; i++)
            entered = entered || e->buf[i] != '0';
        if (!entered)
            return APRS_EDIT_UNCHANGED;
        char code[APRS_LOC_MAX + 1];
        memcpy(code, e->buf, APRS_LOC_MAX);
        code[APRS_LOC_MAX] = 0;
        if (!APRS_LocDecode(code, &lat, &lon))
            return APRS_EDIT_INVALID;
        memcpy(n.loc, code, sizeof code);
    } else {
        uint8_t len = e->max;
        while (len > 0 && is_blank(e->buf[len - 1]))
            len--;
        if (len == 0)
            return APRS_EDIT_UNCHANGED;
        char *dst = e->kind == APRS_EDIT_CALL ? n.call : e->kind == APRS_EDIT_MSGTO ? n.msgto : n.comment;
        const size_t cap = e->kind == APRS_EDIT_CALL ? sizeof n.call : e->kind == APRS_EDIT_MSGTO ? sizeof n.msgto
                                                                                                   : sizeof n.comment;
        memset(dst, 0, cap);
        for (uint8_t i = 0; i < len; i++) {
            if (is_call(e) && is_blank(e->buf[i]))
                return APRS_EDIT_INVALID;          // a gap inside a callsign
            dst[i] = e->buf[i] == '_' ? ' ' : e->buf[i];
        }
    }
    if (!APRS_SettingsValid(&n))
        return APRS_EDIT_INVALID;
    *s = n;
    return APRS_EDIT_OK;
}

bool APRS_EditText(const aprs_edit_t *e, char *out)
{
    uint8_t len = e->max;
    while (len > 0 && is_blank(e->buf[len - 1]))
        len--;
    for (uint8_t i = 0; i < len; i++)
        out[i] = e->buf[i] == '_' ? ' ' : e->buf[i];
    out[len] = 0;
    return len > 0;
}

void APRS_EditView(const aprs_edit_t *e, unsigned width, char *out)
{
    const unsigned start = ((unsigned)e->pos / width) * width;
    unsigned o = 0;
    for (unsigned i = 0; i < width; i++)
        out[o++] = start + i < e->max ? e->buf[start + i] : ' ';
    out[o++] = '\n';
    for (unsigned i = 0; i < width; i++)
        out[o++] = start + i == e->pos ? '^' : ' ';
    out[o] = 0;
}
