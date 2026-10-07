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

#include "app/aprs_ax25.h"
#include <string.h>

// CRC-16 for AX.25 - minimal calculation (no lookup table to save space)
uint16_t APRS_CalculateCRC(const uint8_t *data, uint16_t length)
{
    uint16_t crc = 0xFFFF;
    for (uint16_t i = 0; i < length; i++) {
        crc ^= (uint16_t)data[i];
        for (uint8_t j = 0; j < 8; j++) {
            if (crc & 1) {
                crc = (crc >> 1) ^ 0x8408;  // Reversed polynomial
            } else {
                crc >>= 1;
            }
        }
    }
    return crc;
}

uint16_t AX25_CalculateFCS(const uint8_t *data, uint16_t length)
{
    // AX.25/HDLC FCS: init 0xFFFF, poly 0x8408 (reflected), final XOR 0xFFFF.
    return (uint16_t)(~APRS_CalculateCRC(data, length));
}

void AX25_EncodeAddress(const char *callsign, uint8_t ssid, bool last, uint8_t out7[7])
{
    bool end = false;
    for (uint8_t i = 0; i < 6; i++) {
        char c = ' ';
        if (!end) {
            if (callsign[i] == 0)
                end = true;  // stop reading past the terminator
            else
                c = callsign[i];
        }
        out7[i] = (uint8_t)(c << 1);
    }
    out7[6] = (uint8_t)(0x60 | ((ssid & 0x0F) << 1) | (last ? 0x01 : 0x00));
}

uint8_t AX25_FormatAddress(const uint8_t a7[7], char out[10])
{
    uint8_t n = 0;
    for (uint8_t k = 0; k < 6; k++) {
        const char c = (char)(a7[k] >> 1);
        if (c > ' ')
            out[n++] = c;
    }
    const uint8_t ssid = (a7[6] >> 1) & 0x0Fu;
    if (ssid > 0) {
        out[n++] = '-';
        if (ssid >= 10)
            out[n++] = '1';
        out[n++] = (char)('0' + ssid % 10u);
    }
    out[n] = 0;
    return n;
}

void HDLC_PutBit(hdlc_writer_t *w, bool bit)
{
    if (w->bits >= HDLC_BUF_SIZE * 8u)
        return;
    if (!bit)
        w->level ^= 1;  // NRZI: 0 = transition, 1 = no change
    if (w->level)
        w->buf[w->bits >> 3] |= 0x80u >> (w->bits & 7u);  // MSB first
    w->bits++;
}

void HDLC_PutByte(hdlc_writer_t *w, uint8_t b, bool stuff)
{
    for (uint8_t i = 0; i < 8; i++) {  // AX.25 sends bits LSB first
        const bool bit = (b >> i) & 1u;
        HDLC_PutBit(w, bit);
        if (stuff && bit) {
            if (++w->ones == 5) {
                HDLC_PutBit(w, false);  // stuff a 0 after 5 consecutive 1s
                w->ones = 0;
            }
        } else {
            w->ones = 0;
        }
    }
}

uint16_t HDLC_EncodeFrame(uint8_t buf[HDLC_BUF_SIZE], const uint8_t *frame, uint16_t frame_len)
{
    memset(buf, 0, HDLC_BUF_SIZE);
    if (frame_len > APRS_RAWTX_MAX + 2u)
        return 0;   // would not fit: never send a truncated frame
    hdlc_writer_t w = { buf, 0, 0, 1 };
    for (uint16_t i = 0; i < HDLC_LEAD_FLAGS; i++)
        HDLC_PutByte(&w, 0x7E, false);
    for (uint16_t i = 0; i < frame_len; i++)
        HDLC_PutByte(&w, frame[i], true);
    for (uint16_t i = 0; i < HDLC_TAIL_FLAGS; i++)
        HDLC_PutByte(&w, 0x7E, false);
    return w.bits;
}
