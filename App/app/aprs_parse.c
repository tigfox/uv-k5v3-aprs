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


#include "app/aprs_parse.h"
#include <string.h>

static uint8_t MIN100_TO_MICRO(uint32_t deg, uint32_t min100, int32_t *out)
{
    // deg + (min100 / 100) minutes -> micro-degrees. 1' = 1000000/60 udeg.
    if (deg > 180u || min100 >= 6000u)
        return 0;
    *out = (int32_t)(deg * 1000000u + (min100 * 500u) / 3u);
    return 1;
}

uint8_t APRS_ParseUncompressed(const uint8_t *p, uint16_t len, int32_t *lat, int32_t *lon)
{
    // "ddmm.hhN/dddmm.hhE" - position-ambiguity spaces count as zeros
    if (len < 19)
        return 0;
    const uint8_t *q = p;
    uint32_t latd, latm;
    uint32_t lond, lonm;
    #define DIGIT(c) (((c) == ' ') ? 0u : (uint32_t)((c) - '0'))
    #define ISDIG(c) (((c) >= '0' && (c) <= '9') || (c) == ' ')
    if (!ISDIG(q[0]) || !ISDIG(q[1]) || !ISDIG(q[2]) || !ISDIG(q[3]) ||
        q[4] != '.' || !ISDIG(q[5]) || !ISDIG(q[6]))
        return 0;
    latd = DIGIT(q[0]) * 10u + DIGIT(q[1]);
    latm = DIGIT(q[2]) * 1000u + DIGIT(q[3]) * 100u + DIGIT(q[5]) * 10u + DIGIT(q[6]);
    if (latd > 90u || (latd == 90u && latm > 0u) || !MIN100_TO_MICRO(latd, latm, lat))
        return 0;
    if (q[7] == 'S')
        *lat = -*lat;
    else if (q[7] != 'N')
        return 0;
    q += 9;  // skip hemisphere + symbol table char
    if (!ISDIG(q[0]) || !ISDIG(q[1]) || !ISDIG(q[2]) || !ISDIG(q[3]) ||
        !ISDIG(q[4]) || q[5] != '.' || !ISDIG(q[6]) || !ISDIG(q[7]))
        return 0;
    lond = DIGIT(q[0]) * 100u + DIGIT(q[1]) * 10u + DIGIT(q[2]);
    lonm = DIGIT(q[3]) * 1000u + DIGIT(q[4]) * 100u + DIGIT(q[6]) * 10u + DIGIT(q[7]);
    if (!MIN100_TO_MICRO(lond, lonm, lon))
        return 0;
    if (q[8] == 'W')
        *lon = -*lon;
    else if (q[8] != 'E')
        return 0;
    #undef DIGIT
    #undef ISDIG
    return 1;
}

uint8_t APRS_ParseMicE(const uint8_t *frame, const uint8_t *info, uint16_t ilen, int32_t *lat, int32_t *lon)
{
    if (ilen < 9)
        return 0;
    const uint8_t t = info[0];
    if (t != 0x60 && t != 0x27 && t != 0x1C && t != 0x1D)  // ` ' and old GPS types
        return 0;

    // latitude digits + flags live in the AX.25 destination address
    uint8_t  dig[6], bit[6];
    for (uint8_t i = 0; i < 6; i++) {
        const char c = (char)(frame[i] >> 1);
        if (c >= '0' && c <= '9')      { dig[i] = (uint8_t)(c - '0'); bit[i] = 0; }
        else if (c >= 'A' && c <= 'J') { dig[i] = (uint8_t)(c - 'A'); bit[i] = 1; }
        else if (c >= 'P' && c <= 'Y') { dig[i] = (uint8_t)(c - 'P'); bit[i] = 1; }
        else if (c == 'L')             { dig[i] = 0; bit[i] = 0; }  // ambiguity space
        else if (c == 'K' || c == 'Z') { dig[i] = 0; bit[i] = 1; }
        else return 0;
    }
    const uint32_t latd = (uint32_t)dig[0] * 10u + dig[1];
    const uint32_t latm = (uint32_t)dig[2] * 1000u + (uint32_t)dig[3] * 100u
                        + (uint32_t)dig[4] * 10u + dig[5];
    if (latd > 90u || !MIN100_TO_MICRO(latd, latm, lat))
        return 0;
    if (!bit[3])  // 1 = north
        *lat = -*lat;

    // longitude from info bytes 1..3
    int32_t d = (int32_t)info[1] - 28;
    int32_t m = (int32_t)info[2] - 28;
    int32_t h = (int32_t)info[3] - 28;
    if (d < 0 || m < 0 || h < 0 || h > 99)
        return 0;
    if (bit[4])
        d += 100;
    if (d >= 180 && d <= 189) d -= 80;
    else if (d >= 190 && d <= 199) d -= 190;
    if (m >= 60)
        m -= 60;
    if (d > 179 || m > 59)
        return 0;
    if (!MIN100_TO_MICRO((uint32_t)d, (uint32_t)(m * 100 + h), lon))
        return 0;
    if (bit[5])  // 1 = west
        *lon = -*lon;
    return 1;
}

uint8_t APRS_ParseCompressed(const uint8_t *p, uint16_t len, int32_t *lat, int32_t *lon)
{
    // symbol-table char + 4 base-91 lat chars + 4 base-91 lon chars
    if (len < 10)
        return 0;
    for (uint8_t i = 1; i <= 8; i++)
        if (p[i] < '!' || p[i] > '{')
            return 0;
    uint32_t y = 0, x = 0;
    for (uint8_t i = 1; i <= 4; i++)
        y = y * 91u + (uint32_t)(p[i] - 33);
    for (uint8_t i = 5; i <= 8; i++)
        x = x * 91u + (uint32_t)(p[i] - 33);
    // lat = 90 - y/380926, lon = -180 + x/190463 (fraction via ~21/8 and ~21/4)
    {
        const uint32_t dd = y / 380926u, rr = y % 380926u;
        if (dd > 180u) return 0;
        *lat = 90000000 - (int32_t)(dd * 1000000u + (rr * 21u) / 8u);
    }
    {
        const uint32_t dd = x / 190463u, rr = x % 190463u;
        if (dd > 360u) return 0;
        *lon = -180000000 + (int32_t)(dd * 1000000u + (rr * 21u) / 4u);
    }
    if (*lat < -90000000 || *lon > 180000000)
        return 0;   // the top of the base-91 range lies beyond the poles / date line
    return 1;
}


char *APRS_FmtCoord(char *p, int32_t micro, char pos, char neg)
{
    char hemi = pos;
    uint32_t v = (uint32_t)micro;
    if (micro < 0) {
        v = (uint32_t)-micro;
        hemi = neg;
    }
    const uint32_t deg  = v / 1000000u;
    const uint32_t frac = (v % 1000000u) / 10000u;  // 2 decimals
    if (deg >= 100u)
        *p++ = (char)('0' + (deg / 100u) % 10u);
    *p++ = (char)('0' + (deg / 10u) % 10u);
    *p++ = (char)('0' + deg % 10u);
    *p++ = '.';
    *p++ = (char)('0' + (frac / 10u) % 10u);
    *p++ = (char)('0' + frac % 10u);
    *p++ = hemi;
    return p;
}


static const uint16_t APRS_CosTable[19] = {   // cos(0..90 deg step 5) * 32768
    32767, 32643, 32270, 31651, 30791, 29698, 28378, 26841, 25100, 23170,
    21063, 18795, 16384, 13848, 11207,  8481,  5690,  2856,     0,
};

// cos(latitude) scaled by 32768, latitude given in micro-degrees.
static int32_t APRS_CosScaled(int32_t lat_udeg)
{
    uint32_t a = (uint32_t)(lat_udeg < 0 ? -lat_udeg : lat_udeg);
    if (a >= 90000000u)
        return 0;
    const uint32_t seg    = a / 5000000u;          // 0..17
    const uint32_t within = (a % 5000000u) / 1000u; // 0..4999 (0.001 deg units)
    const int32_t c0 = APRS_CosTable[seg];
    const int32_t c1 = APRS_CosTable[seg + 1];
    return c0 + ((c1 - c0) * (int32_t)within) / 5000;
}

static uint32_t APRS_ISqrt64(uint64_t n)
{
    uint64_t res = 0;
    uint64_t bit = (uint64_t)1 << 62;
    while (bit > n)
        bit >>= 2;
    while (bit) {
        if (n >= res + bit) {
            n   -= res + bit;
            res  = (res >> 1) + bit;
        } else {
            res >>= 1;
        }
        bit >>= 2;
    }
    return (uint32_t)res;
}

// Distance in metres between my location and (lat,lon). Micro-degrees in.
uint32_t APRS_DistanceMetres(int32_t lat, int32_t lon, int32_t my_lat, int32_t my_lon)
{
    const int32_t dlat = lat - my_lat;
    const int32_t dlon = lon - my_lon;
    const int32_t meanlat = (lat + my_lat) / 2;

    int64_t dy = ((int64_t)dlat * 116728) >> 20;                       // metres
    int64_t dx = ((int64_t)dlon * 116728) >> 20;                       // metres @ equator
    dx = (dx * APRS_CosScaled(meanlat)) >> 15;                         // scale by cos(lat)

    return APRS_ISqrt64((uint64_t)(dx * dx) + (uint64_t)(dy * dy));
}

// "12.3km" / "850m" for the distance readout. Returns end of string.
char *APRS_FmtDistance(char *p, uint32_t metres)
{
    uint32_t v;
    if (metres < 1000u) {                 // metres
        v = metres;
        if (v >= 100u) *p++ = (char)('0' + (v / 100u) % 10u);
        if (v >= 10u)  *p++ = (char)('0' + (v / 10u) % 10u);
        *p++ = (char)('0' + v % 10u);
        *p++ = 'm';
        return p;
    }
    if (metres < 100000u) {               // km with one decimal
        const uint32_t km  = metres / 1000u;
        const uint32_t dec = (metres % 1000u) / 100u;
        if (km >= 10u) *p++ = (char)('0' + (km / 10u) % 10u);
        *p++ = (char)('0' + km % 10u);
        *p++ = '.';
        *p++ = (char)('0' + dec);
    } else {                              // whole km
        v = metres / 1000u;
        if (v > 9999u) v = 9999u;
        if (v >= 1000u) *p++ = (char)('0' + (v / 1000u) % 10u);
        if (v >= 100u)  *p++ = (char)('0' + (v / 100u) % 10u);
        if (v >= 10u)   *p++ = (char)('0' + (v / 10u) % 10u);
        *p++ = (char)('0' + v % 10u);
    }
    *p++ = 'k';
    *p++ = 'm';
    return p;
}


bool APRS_ParsePosition(const uint8_t *frame, const uint8_t *ip, uint16_t ilen, int32_t *lat, int32_t *lon)
{
    if (ilen < 2)
        return false;
    const uint8_t t = ip[0];
    if (t == '!' || t == '=' || t == '@' || t == '/') {
        const uint8_t *p2 = ip + 1;
        uint16_t l2 = (uint16_t)(ilen - 1);
        if ((t == '@' || t == '/') && l2 > 7) {  // skip timestamp
            p2 += 7;
            l2 -= 7;
        }
        if (l2 >= 1 && ((p2[0] >= '0' && p2[0] <= '9') || p2[0] == ' '))
            return APRS_ParseUncompressed(p2, l2, lat, lon) != 0;
        return APRS_ParseCompressed(p2, l2, lat, lon) != 0;
    }
    return APRS_ParseMicE(frame, ip, ilen, lat, lon) != 0;
}

// 15-digit phone code: (lat+90)*1e4 [7] + (lon+180)*1e4 [7] + checksum [1], where the
// checksum is the sum of position*digit over the first 14 digits, mod 10.
bool APRS_LocDecode(const char *code, int32_t *lat, int32_t *lon)
{
    uint32_t sum = 0, la = 0, lo = 0;
    for (uint8_t i = 0; i < 15; i++) {
        const char c = code[i];
        if (c < '0' || c > '9')
            return false;
        const uint8_t d = (uint8_t)(c - '0');
        if (i < 7)
            la = la * 10u + d;
        else if (i < 14)
            lo = lo * 10u + d;
        if (i < 14)
            sum += (uint32_t)(i + 1) * d;
        else if ((sum % 10u) != d)
            return false;
    }
    if (code[15] != 0 || la > 1800000u || lo > 3600000u || (la == 0 && lo == 0))
        return false;
    *lat = (int32_t)(la * 100u) - 90000000;
    *lon = (int32_t)(lo * 100u) - 180000000;
    return true;
}

bool APRS_MessageToMe(const uint8_t *ip, uint16_t ilen, const char *call, uint8_t ssid)
{
    // ":ADDRESSEE:text" with the addressee padded to 9 characters
    if (ilen < 11 || ip[0] != ':' || ip[10] != ':')
        return false;
    char me[10];
    uint8_t k = 0;
    for (uint8_t i = 0; i < 6 && call[i] > ' '; i++)
        me[k++] = call[i];
    if (k == 0)
        return false;
    ssid &= 0x0F;
    if (ssid > 0) {
        me[k++] = '-';
        if (ssid >= 10)
            me[k++] = '1';
        me[k++] = (char)('0' + (ssid % 10));
    }
    while (k < 9)
        me[k++] = ' ';
    return memcmp(&ip[1], me, 9) == 0;
}
