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


#ifndef APP_APRS_MONITOR_H
#define APP_APRS_MONITOR_H

#include <stdint.h>

/* The plain-text monitor lines (hardware-free: app/aprs_serial.c gives them a port). Each line ends "\r\n". A line is
 * written in pieces of at most 64 bytes so no large buffer is needed. Lines of one call follow each other with nothing
 * in between.
 *   APRSRAW:<hex>        every decoded frame, FCS stripped
 *   APRSRSSI:<dBm>       the signal strength of that frame, right after its APRSRAW line (not written without a reading)
 *   APRS:<text>          what the radio shows for it
 *   DIGI:<what> <source> a digipeater decision
 * and any other line a caller hands over (SENT: / DROP:). */
typedef void (*aprs_out_fn)(const char *s, uint16_t n);

#define APRS_NO_RSSI INT16_MIN      /* no signal strength reading for this frame */

void APRS_MonitorFrame(aprs_out_fn out, const uint8_t *frame, uint16_t len, const char *text, int16_t rssi_dbm);
void APRS_MonitorDigi(aprs_out_fn out, const char *what, const uint8_t *frame);
void APRS_MonitorLine(aprs_out_fn out, const char *line);

#endif
