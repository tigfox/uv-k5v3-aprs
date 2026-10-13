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
#include "app/aprs_monitor.h"
#include "app/uart.h"
#include "driver/vcp.h"
#include "driver/uart.h"
#include <string.h>

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
#if defined(ENABLE_USB)
    /* a host that asks for monitor lines is reading them, whatever its DTR line says (a send timeout
     * clears dtr_enable; asking again brings the lines back) */
    if (on && port == UART_PORT_VCP)
        dtr_enable = 1;
#endif
}

bool APRS_SerialMonitoringPort(uint32_t port)
{
    const int i = slot(port);
    return i >= 0 && gMon[i];
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

void APRS_SerialLine(const char *line)
{
    if (!APRS_SerialMonitoring())
        return;
    APRS_MonitorLine(out, line);
}

void APRS_SerialFrame(const uint8_t *frame, uint16_t len, const char *text, int16_t rssi_dbm)
{
    if (!APRS_SerialMonitoring())
        return;
    APRS_MonitorFrame(out, frame, len, text, rssi_dbm);
}

void APRS_SerialDigi(const char *what, const uint8_t *frame)
{
    if (!APRS_SerialMonitoring())
        return;
    APRS_MonitorDigi(out, what, frame);
}
