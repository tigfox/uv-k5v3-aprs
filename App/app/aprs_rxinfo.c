/* Copyright 2024 UV-K5 Firmware Custom (ta1js APRS work), ported by tigfox 2026.
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


#include "app/aprs_rxinfo.h"
#include "app/aprs_ax25.h"
#include "app/aprs_parse.h"
#include <string.h>

static char printable(uint8_t c)
{
    return (c >= 32 && c < 127) ? (char)c : '.';
}

bool APRS_RxInfo(const uint8_t *frame, uint16_t len, const aprs_settings_t *s,
                 int32_t my_lat, int32_t my_lon, aprs_rx_info_t *out)
{
    memset(out, 0, sizeof *out);
    if (len < 18u)
        return false;
    /* the last address field is the one whose SSID byte has bit 0 set */
    uint16_t a = 6;
    while (a + 7 < len && (frame[a] & 1u) == 0)
        a += 7;
    const uint16_t info = (uint16_t)(a + 3);       /* past the last SSID byte, control and PID */
    if (a < 13 || info >= len - 2u)
        return false;
    const uint8_t *ip = &frame[info];
    const uint16_t ilen = (uint16_t)(len - 2u - info);

    uint8_t me[7];
    AX25_EncodeAddress(s->call, s->ssid, false, me);
    out->own = memcmp(&frame[7], me, 6) == 0 && ((frame[13] ^ me[6]) & 0x1Eu) == 0;

    const uint8_t n = AX25_FormatAddress(&frame[7], out->from);
    out->to_me = APRS_MessageToMe(ip, ilen, s->call, s->ssid);

    uint8_t o = 0;
    if (out->to_me) {
        /* "ackNN" / "rejNN": the word and at most 5 characters of line number, nothing more */
        out->is_ack = ilen >= 14 && ilen <= 11u + 3u + APRS_ACK_SEQ_MAX &&
                      ((ip[11] == 'a' && ip[12] == 'c' && ip[13] == 'k') ||
                       (ip[11] == 'r' && ip[12] == 'e' && ip[13] == 'j'));
        memcpy(out->text, out->from, n);
        o = n;
        out->text[o++] = '>';
        for (uint16_t i = 11; i < ilen && o < APRS_RXTEXT_MAX; i++) {
            if (ip[i] == '{') {                    /* "{nn" line number: acknowledge it, do not show it */
                if (!out->is_ack) {
                    /* the line number is sent straight back in the ack: letters and digits only,
                     * otherwise do not acknowledge (never echo control or framing characters) */
                    uint8_t k = 0;
                    bool clean = true;
                    for (uint16_t j = i + 1u; j < ilen && k < APRS_ACK_SEQ_MAX; j++) {
                        const char c = (char)ip[j];
                        if (!((c >= '0' && c <= '9') || (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z')))
                            clean = false;
                        out->ack_seq[k++] = c;
                    }
                    out->ack_seq[clean ? k : 0] = 0;
                }
                break;
            }
            out->text[o++] = printable(ip[i]);
        }
        out->text[o] = 0;
        return true;
    }

    memcpy(out->text, out->from, n);
    o = n;
    int32_t lat = 0, lon = 0;
    if (APRS_ParsePosition(frame, ip, ilen, &lat, &lon)) {
        if (my_lat != 0 || my_lon != 0) {          /* a distance; with no location of our own it is meaningless */
            out->text[o++] = ' ';
            char *e = APRS_FmtDistance(&out->text[o], APRS_DistanceMetres(lat, lon, my_lat, my_lon));
            o = (uint8_t)(e - out->text);
        }
    } else {
        out->text[o++] = ':';
        for (uint16_t i = 0; i < ilen && o < APRS_RXTEXT_MAX; i++)
            out->text[o++] = printable(ip[i]);
    }
    out->text[o] = 0;
    return true;
}
