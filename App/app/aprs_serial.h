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


#ifndef APP_APRS_SERIAL_H
#define APP_APRS_SERIAL_H

#include <stdbool.h>
#include <stdint.h>

/* The plain-text monitor lines for a laptop on site, on each serial port that asked for them with command
 * 0x0710 (aprs_pc.py monitor). Off by default: UV Studio and K5Viewer share the USB port and its binary
 * protocol, and never see a text line. All no-ops while no port is monitoring.
 *   APRSRAW:<hex>          every decoded frame, FCS stripped
 *   APRS:<text>            what the radio shows for it
 *   DIGI:<what> <source>   a digipeater decision (QUE DUP CNCL HOPS DROP RPT NOTX BUSY) */
void APRS_SerialSetMonitor(uint32_t port, bool on);
bool APRS_SerialMonitoring(void);
void APRS_SerialFrame(const uint8_t *frame, uint16_t len, const char *text);   /* len includes the FCS */
void APRS_SerialDigi(const char *what, const uint8_t *frame);

#endif
