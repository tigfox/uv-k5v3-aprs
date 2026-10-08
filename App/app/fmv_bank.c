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


#include "app/fmv_bank.h"
#include <string.h>

uint16_t FMV_NextIn(uint16_t from, int8_t dir, uint16_t total, fmv_pred_t ok, void *ctx)
{
    if (total == 0 || (dir != 1 && dir != -1))
        return 0xFFFFu;
    int32_t ch = from;
    if (from >= total)                           /* off either end (0 - 1 arrives as 0xFFFF): start from the far side */
        ch = dir > 0 ? 0 : total - 1;
    for (uint16_t i = 0; i < total; i++, ch += dir) {
        if (ch < 0)
            ch = total - 1;
        else if (ch >= (int32_t)total)
            ch = 0;
        if (ok((uint16_t)ch, ctx))
            return (uint16_t)ch;
    }
    return 0xFFFFu;
}

uint16_t FMV_BankTarget(uint16_t last, uint16_t total, fmv_pred_t ok, void *ctx)
{
    if (last < total && ok(last, ctx))
        return last;
    return FMV_NextIn(0, 1, total, ok, ctx);
}

static bool printable_name(const char *n)
{
    bool any = false;
    for (unsigned i = 0; i < 4 && n[i] != 0; i++) {
        if ((unsigned char)n[i] < 0x20 || (unsigned char)n[i] > 0x7E)
            return false;
        if (n[i] != ' ')
            any = true;
    }
    return any;
}

void FMV_BankLabel(uint8_t list, const char name4[4], uint8_t all_value, uint8_t mix_value, char *out)
{
    char *p = out;
    memcpy(p, "BANK ", 5);
    p += 5;
    if (list == all_value) {
        memcpy(p, "ALL", 4);
    } else if (list == mix_value) {
        memcpy(p, "MIX", 4);
    } else if (printable_name(name4)) {
        unsigned n = 0;
        while (n < 4 && name4[n] != 0 && name4[n] != ' ')
            *p++ = name4[n++];
        *p = 0;                                  /* "HO ME" would show as "HO": a name ends at its first space */
    } else {
        *p++ = (char)('0' + (list / 10) % 10);
        *p++ = (char)('0' + list % 10);
        *p = 0;
    }
}

bool FMV_BankNameClean(const uint8_t raw[FMV_BANKNAME_MAX], char *out)
{
    unsigned n = 0;
    while (n < FMV_BANKNAME_MAX && raw[n] != 0x00 && raw[n] != 0xFF) {
        if (raw[n] < 0x20 || raw[n] > 0x7E) {
            out[0] = 0;
            return false;
        }
        out[n] = (char)raw[n];
        n++;
    }
    while (n > 0 && out[n - 1] == ' ')
        n--;
    out[n] = 0;
    return n > 0;
}

void FMV_BankLabelLong(uint8_t list, const char *long_name, const char name4[4], uint8_t all_value, uint8_t mix_value,
                       char *out)
{
    if (list != all_value && list != mix_value && long_name[0] != 0) {
        memcpy(out, long_name, strlen(long_name) + 1);
        return;
    }
    FMV_BankLabel(list, name4, all_value, mix_value, out);
}

void FMV_BankNameWrap(const char *name, unsigned width, char *line1, char *line2)
{
    const unsigned len = (unsigned)strlen(name);
    line2[0] = 0;
    if (len <= width) {
        memcpy(line1, name, len + 1);
        return;
    }
    unsigned cut = width;                         /* hard split unless a space gives a nicer one */
    unsigned skip = 0;
    for (unsigned i = width; i > 0; i--) {
        if (name[i] == ' ') {
            cut = i;
            skip = 1;
            break;
        }
    }
    memcpy(line1, name, cut);
    line1[cut] = 0;
    unsigned rest = len - cut - skip;
    if (rest > width)
        rest = width;
    memcpy(line2, name + cut + skip, rest);
    line2[rest] = 0;
}
