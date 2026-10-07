/* Copyright 2024 UV-K5 Firmware Custom (ta1js APRS work)
 * Ported to the PY32F071 firmware by tigfox, 2026.
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

#ifndef APP_APRS_BEACON_H
#define APP_APRS_BEACON_H

#include <stdbool.h>
#include <stdint.h>
#include "app/aprs_settings.h"

// AX.25 destination of every frame we send: it names the firmware to the APRS network
// (aprs.fi and friends resolve tocalls to a device name).
#define APRS_TOCALL "APZK5"

// Longest frame the builders make (FCS excluded). The FCS is appended at TX in two spare bytes.
#define APRS_BUILD_MAX 100u

// Destination, source and path, control and PID. A MOBILE station asks for WIDE1-1,WIDE2-1;
// a digipeater beacons direct (source is the last address). Returns the length.
uint16_t APRS_BuildHeader(uint8_t *frame, const aprs_settings_t *s, bool direct);

// "!DDMM.mmN/DDDMM.mmE>comment" ('/>' car, '/#' digipeater) for lat/lon in micro-degrees.
// Returns the frame length (FCS excluded). out must hold APRS_BUILD_MAX bytes.
uint16_t APRS_BuildBeacon(uint8_t *out, const aprs_settings_t *s, int32_t lat_udeg, int32_t lon_udeg);
// The same for the Loc stored in s; 0 if s has no valid Loc.
uint16_t APRS_BuildStationBeacon(uint8_t *out, const aprs_settings_t *s);

#endif
