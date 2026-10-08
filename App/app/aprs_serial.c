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


#include "app/aprs_serial.h"
#include "app/aprs_ax25.h"
#include "app/uart.h"
#include "driver/vcp.h"
#include "driver/uart.h"
#include <string.h>

#define CHUNK 64u

static bool gMon[2];            /* by port: UART, VCP */

static int slot(uint32_t port)
{
#if defined(ENABLE_UART)
    if (port == UART_PORT_UART) return 0;
#endif
#if defined(ENABLE_USB)
    if (port == UART_PORT_VCP) return 1;
#endif
    return -1;
}

void APRS_SerialSetMonitor(uint32_t port, bool on)
{
    const int i = slot(port);
    if (i >= 0)
        gMon[i] = on;
}

bool APRS_SerialMonitoring(void)
{
    return gMon[0] || gMon[1];
}

static void out(const char *s, uint16_t n)
{
#if defined(ENABLE_UART)
    if (gMon[0])
        UART_Send(s, n);
#endif
#if defined(ENABLE_USB)
    if (gMon[1])
        VCP_Send((const uint8_t *)s, n);       // waits for the host; nothing if no terminal is open
#endif
}

/* a line is sent in chunks so no large buffer is needed */
typedef struct { char buf[CHUNK]; uint16_t n; } line_t;

static void put(line_t *l, const char *s, uint16_t n)
{
    while (n--) {
        if (l->n == CHUNK) { out(l->buf, l->n); l->n = 0; }
        l->buf[l->n++] = *s++;
    }
}

static void flush(line_t *l)
{
    put(l, "\r\n", 2);
    if (l->n) out(l->buf, l->n);
    l->n = 0;
}

void APRS_SerialFrame(const uint8_t *frame, uint16_t len, const char *text)
{
    if (!APRS_SerialMonitoring())
        return;
    static const char hex[] = "0123456789ABCDEF";
    line_t l = { .n = 0 };
    put(&l, "APRSRAW:", 8);
    for (uint16_t i = 0; i + 2u < len; i++) {            // drop the 2-byte FCS
        const char h[2] = { hex[frame[i] >> 4], hex[frame[i] & 15] };
        put(&l, h, 2);
    }
    flush(&l);
    put(&l, "APRS:", 5);
    put(&l, text, (uint16_t)strlen(text));
    flush(&l);
}

void APRS_SerialDigi(const char *what, const uint8_t *frame)
{
    if (!APRS_SerialMonitoring())
        return;
    line_t l = { .n = 0 };
    char src[10];
    const uint8_t n = AX25_FormatAddress(&frame[7], src);
    put(&l, "DIGI:", 5);
    put(&l, what, (uint16_t)strlen(what));
    put(&l, " ", 1);
    put(&l, src, n);
    flush(&l);
}
