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


#include "app/aprs_beacon.h"
#include "app/aprs_ax25.h"
#include "app/aprs_parse.h"
#include <string.h>

uint16_t APRS_BuildHeader(uint8_t *frame, const aprs_settings_t *s, bool direct)
{
    uint16_t idx = 0;
    AX25_EncodeAddress(APRS_TOCALL, 0, false, &frame[idx]); idx += 7;
    AX25_EncodeAddress(s->call, s->ssid, direct, &frame[idx]); idx += 7;
    if (!direct) {
        AX25_EncodeAddress("WIDE1", 1, false, &frame[idx]); idx += 7;
        AX25_EncodeAddress("WIDE2", 1, true,  &frame[idx]); idx += 7;
    }
    frame[idx++] = 0x03; // Control: UI frame
    frame[idx++] = 0xF0; // PID: no layer 3
    return idx;
}

// micro-degrees -> "DDMM.mm" / "DDDMM.mm"
static uint16_t pos_digits(uint8_t *out, uint32_t v, bool deg3)
{
    const uint32_t deg    = v / 1000000u;
    const uint32_t min100 = ((v % 1000000u) * 6u) / 1000u;
    uint16_t n = 0;
    if (deg3)
        out[n++] = (uint8_t)('0' + (deg / 100u) % 10u);
    out[n++] = (uint8_t)('0' + (deg / 10u) % 10u);
    out[n++] = (uint8_t)('0' + deg % 10u);
    out[n++] = (uint8_t)('0' + (min100 / 1000u) % 10u);
    out[n++] = (uint8_t)('0' + (min100 / 100u) % 10u);
    out[n++] = '.';
    out[n++] = (uint8_t)('0' + (min100 / 10u) % 10u);
    out[n++] = (uint8_t)('0' + min100 % 10u);
    return n;
}

uint16_t APRS_BuildBeacon(uint8_t *out, const aprs_settings_t *s, int32_t lat_udeg, int32_t lon_udeg)
{
    const bool digi = s->beacon_type != 0;
    uint16_t idx = APRS_BuildHeader(out, s, digi);

    out[idx++] = '!';
    int32_t v = lat_udeg;
    char hemi = 'N';
    if (v < 0) { v = -v; hemi = 'S'; }
    idx += pos_digits(&out[idx], (uint32_t)v, false);
    out[idx++] = (uint8_t)hemi;
    out[idx++] = '/';
    v = lon_udeg;
    hemi = 'E';
    if (v < 0) { v = -v; hemi = 'W'; }
    idx += pos_digits(&out[idx], (uint32_t)v, true);
    out[idx++] = (uint8_t)hemi;
    out[idx++] = digi ? '#' : '>';   // '/#' digipeater, '/>' car
    for (uint8_t i = 0; i < APRS_COMMENT_MAX && s->comment[i]; i++)
        out[idx++] = (uint8_t)s->comment[i];
    return idx;
}

uint16_t APRS_BuildStationBeacon(uint8_t *out, const aprs_settings_t *s)
{
    int32_t lat, lon;
    if (!APRS_LocDecode(s->loc, &lat, &lon))
        return 0;
    return APRS_BuildBeacon(out, s, lat, lon);
}
