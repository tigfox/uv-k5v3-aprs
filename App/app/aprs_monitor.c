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


#include "app/aprs_monitor.h"
#include "app/aprs_ax25.h"
#include <string.h>

#define CHUNK 64u

typedef struct { aprs_out_fn out; char buf[CHUNK]; uint16_t n; } line_t;

static void put(line_t *l, const char *s, uint16_t n)
{
    while (n--) {
        if (l->n == CHUNK) { l->out(l->buf, l->n); l->n = 0; }
        l->buf[l->n++] = *s++;
    }
}

static void flush(line_t *l)
{
    put(l, "\r\n", 2);
    if (l->n) l->out(l->buf, l->n);
    l->n = 0;
}

static void put_int(line_t *l, int32_t v)
{
    char tmp[12];
    unsigned i = 0;
    uint32_t u = v < 0 ? (uint32_t)(-(int64_t)v) : (uint32_t)v;
    do { tmp[i++] = (char)('0' + u % 10u); u /= 10u; } while (u);
    if (v < 0) put(l, "-", 1);
    while (i) { const char c = tmp[--i]; put(l, &c, 1); }
}

void APRS_MonitorFrame(aprs_out_fn out, const uint8_t *frame, uint16_t len, const char *text, int16_t rssi_dbm)
{
    static const char hex[] = "0123456789ABCDEF";
    line_t l = { .out = out, .n = 0 };
    put(&l, "APRSRAW:", 8);
    for (uint16_t i = 0; i + 2u < len; i++) {            // drop the 2-byte FCS
        const char h[2] = { hex[frame[i] >> 4], hex[frame[i] & 15] };
        put(&l, h, 2);
    }
    flush(&l);
    if (rssi_dbm != APRS_NO_RSSI) {
        put(&l, "APRSRSSI:", 9);
        put_int(&l, rssi_dbm);
        flush(&l);
    }
    put(&l, "APRS:", 5);
    put(&l, text, (uint16_t)strlen(text));
    flush(&l);
}

void APRS_MonitorDigi(aprs_out_fn out, const char *what, const uint8_t *frame)
{
    line_t l = { .out = out, .n = 0 };
    char src[10];
    const uint8_t n = AX25_FormatAddress(&frame[7], src);
    put(&l, "DIGI:", 5);
    put(&l, what, (uint16_t)strlen(what));
    put(&l, " ", 1);
    put(&l, src, n);
    flush(&l);
}

void APRS_MonitorLine(aprs_out_fn out, const char *line)
{
    line_t l = { .out = out, .n = 0 };
    put(&l, line, (uint16_t)strlen(line));
    flush(&l);
}
