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

#ifndef APP_APRS_PARSE_H
#define APP_APRS_PARSE_H

#include <stdbool.h>
#include <stdint.h>

// Position decoders. Coordinates are micro-degrees; each returns 1 on success, 0 otherwise.
uint8_t APRS_ParseUncompressed(const uint8_t *p, uint16_t len, int32_t *lat, int32_t *lon);
uint8_t APRS_ParseMicE(const uint8_t *frame, const uint8_t *info, uint16_t ilen, int32_t *lat, int32_t *lon);
uint8_t APRS_ParseCompressed(const uint8_t *p, uint16_t len, int32_t *lat, int32_t *lon);
// Dispatch on the data type identifier at ip[0]; frame is the AX.25 frame (Mic-E needs the destination).
bool APRS_ParsePosition(const uint8_t *frame, const uint8_t *ip, uint16_t ilen, int32_t *lat, int32_t *lon);

// "41.15N": degrees with two decimals and the hemisphere letter. Returns the end of the text.
char *APRS_FmtCoord(char *p, int32_t micro, char pos, char neg);
// Distance in metres between two points (equirectangular, integer only), and "12.3km" / "850m".
uint32_t APRS_DistanceMetres(int32_t lat, int32_t lon, int32_t my_lat, int32_t my_lon);
char *APRS_FmtDistance(char *p, uint32_t metres);

// The 15-digit Loc code (see the code generator) to micro-degrees. false: malformed or bad checksum.
bool APRS_LocDecode(const char *code, int32_t *lat, int32_t *lon);

// Is the info field ip a message ":ADDRESSEE:text" for call-ssid?
bool APRS_MessageToMe(const uint8_t *ip, uint16_t ilen, const char *call, uint8_t ssid);

#endif
