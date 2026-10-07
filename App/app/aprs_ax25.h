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

#ifndef APP_APRS_AX25_H
#define APP_APRS_AX25_H

#include <stdbool.h>
#include <stdint.h>

/* Longest frame we build, repeat or send raw (FCS excluded). HDLC_BUF_SIZE is sized
 * for it: 32 lead flags plus worst-case bit stuffing leave room for roughly 170 bytes. */
#define APRS_RAWTX_MAX    150u
#define HDLC_BUF_SIZE     240u   /* encoded TX bitstream */
#define HDLC_LEAD_FLAGS   32u    /* ~213 ms of 0x7E preamble */
#define HDLC_TAIL_FLAGS   3u

uint16_t APRS_CalculateCRC(const uint8_t *data, uint16_t length);
uint16_t AX25_CalculateFCS(const uint8_t *data, uint16_t length);
void     AX25_EncodeAddress(const char *callsign, uint8_t ssid, bool last, uint8_t out7[7]);
/* "CALL" or "CALL-n" from an AX.25 address; returns the length (at most 9). out gets a NUL. */
uint8_t  AX25_FormatAddress(const uint8_t a7[7], char out[10]);

typedef struct {
    uint8_t *buf;
    uint16_t bits;
    uint8_t  ones;
    uint8_t  level;  /* current NRZI line level */
} hdlc_writer_t;

void HDLC_PutBit(hdlc_writer_t *w, bool bit);
void HDLC_PutByte(hdlc_writer_t *w, uint8_t b, bool stuff);
/* Lead flags + frame (FCS included, bit-stuffed) + tail flags as NRZI line levels, MSB
 * first, into buf[HDLC_BUF_SIZE] (cleared first). Returns the number of bits, or 0 if the
 * frame is longer than APRS_RAWTX_MAX plus its 2-byte FCS (never a truncated stream). */
uint16_t HDLC_EncodeFrame(uint8_t buf[HDLC_BUF_SIZE], const uint8_t *frame, uint16_t frame_len);

#endif
