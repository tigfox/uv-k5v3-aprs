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


#include "app/rpt_info.h"
#include <string.h>

uint16_t RPT_FreqCheck(uint32_t f)
{
    uint32_t h = f * 2654435761u;           /* Fibonacci hashing: neighbouring frequencies spread */
    uint16_t c = (uint16_t)((h >> 16) ^ (h & 0xFFFFu));
    return c == 0xFFFFu ? 0xFFFEu : c;
}

bool RPT_InfoIsErased(const uint8_t rec[RPT_INFO_RECORD])
{
    for (unsigned i = 0; i < RPT_INFO_RECORD; i++)
        if (rec[i] != 0xFF)
            return false;
    return true;
}

static void erase(uint8_t rec[RPT_INFO_RECORD])
{
    memset(rec, 0xFF, RPT_INFO_RECORD);
}

bool RPT_InfoEncode(uint8_t rec[RPT_INFO_RECORD], uint32_t rx_freq, const char *text, bool *cut)
{
    if (cut)
        *cut = false;
    erase(rec);
    if (!text)
        return false;

    char clean[RPT_INFO_TEXT_MAX + 1];
    unsigned n = 0;
    bool more = false;
    for (const unsigned char *p = (const unsigned char *)text; *p; p++) {
        char c = (*p >= 0x20 && *p <= 0x7E) ? (char)*p : '?';
        if (c == ' ' && (n == 0 || clean[n - 1] == ' '))
            continue;                          /* leading spaces and runs of spaces */
        if (n == RPT_INFO_TEXT_MAX) {
            more = true;
            break;
        }
        clean[n++] = c;
    }
    while (n > 0 && clean[n - 1] == ' ')
        n--;                                    /* trailing space (also left by a cut) */
    if (n == 0)
        return false;
    if (more && cut)
        *cut = true;

    const uint16_t chk = RPT_FreqCheck(rx_freq);
    rec[0] = (uint8_t)(chk & 0xFF);
    rec[1] = (uint8_t)(chk >> 8);
    memcpy(rec + 2, clean, n);
    rec[2 + n] = 0;                              /* the rest of the record stays 0xFF */
    return true;
}

bool RPT_InfoDecode(const uint8_t rec[RPT_INFO_RECORD], uint32_t rx_freq, char *out)
{
    out[0] = 0;
    const uint16_t chk = (uint16_t)(rec[0] | (rec[1] << 8));
    if (chk != RPT_FreqCheck(rx_freq))
        return false;
    unsigned n = 0;
    while (n < RPT_INFO_RECORD - 2u && rec[2 + n] != 0)
        n++;
    if (n == 0 || n > RPT_INFO_TEXT_MAX)
        return false;                            /* empty, or no terminator inside the record */
    for (unsigned i = 0; i < n; i++)
        if (rec[2 + i] < 0x20 || rec[2 + i] > 0x7E)
            return false;
    memcpy(out, rec + 2, n);
    out[n] = 0;
    return true;
}

static void trim_copy(const char *s, size_t len, char *out)
{
    while (len > 0 && *s == ' ') { s++; len--; }
    while (len > 0 && s[len - 1] == ' ') len--;
    if (len > RPT_LINE_MAX)
        len = RPT_LINE_MAX;
    memcpy(out, s, len);
    out[len] = 0;
}

void RPT_InfoSplit(const char *text, char *city, char *rest)
{
    const char *comma = strchr(text, ',');
    if (!comma) {
        trim_copy(text, strlen(text), city);
        rest[0] = 0;
        return;
    }
    trim_copy(text, (size_t)(comma - text), city);
    trim_copy(comma + 1, strlen(comma + 1), rest);
}

uint16_t RPT_InfoAddress(uint16_t channel)
{
    return channel < RPT_INFO_SLOTS ? (uint16_t)(RPT_INFO_EEPROM_ADDR + channel * RPT_INFO_RECORD) : 0u;
}
