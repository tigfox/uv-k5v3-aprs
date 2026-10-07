/* Copyright 2026 Armel F4HWN (demodulator, from App/apps/aprsrx)
 * Resident port by tigfox, 2026.
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


#include "app/aprs_demod.h"
#include <string.h>

#define BP_B0       5356   /* band-pass 1625 Hz, Q 0.9, Q14 (b2 = -b0, b1 = 0) */
#define BP_A1     (-10714)
#define BP_A2       5673
#define AGC_ATT     5      /* peak tracker: attack 1/32 ... */
#define AGC_DEC     10     /* ... decay 1/1024 per sample */
#define PK_MIN      16
#define PLL_STEP    8192   /* 65536 per bit / 8 samples per bit */
#define PLL_CTR     36864  /* transitions pulled to mid-bit + half a sample */
#define PLL_SHIFT   2      /* 1/4 of the error per transition */
#define PRE_BITS    24u    /* busy for 3 bytes after two back-to-back flags */
#define FRAME_MIN   18u    /* 2 addresses + control + PID + FCS */
#define FCS_GOOD    0xF0B8u
#define DUP_SAMPLES APRS_DEMOD_FS   /* the same frame from another slicer within 1 s */

/* 127 * cos(2 pi f n / 9600): one period of 1200 Hz (8 entries) and 11 cycles of 2200 Hz (48).
 * The sine is the table read 3/4 period ahead (+6 of 8, +12 of 48). */
static const int8_t COS1200[8] = { 127, 90, 0, -90, -127, -90, 0, 90 };
static const int8_t COS2200[48] = {
    127, 17, -123, -49, 110, 77, -90, -101, 64, 117, -33, -126, 0, 126, 33, -117,
    -63, 101, 90, -77, -110, 49, 123, -17, -127, -17, 123, 49, -110, -77, 90, 101,
    -64, -117, 33, 126, 0, -126, -33, 117, 63, -101, -90, 77, 110, -49, -123, 17,
};

void APRS_DemodInit(aprs_demod_t *d)
{
    memset(d, 0, sizeof *d);
    d->dc = (int32_t)APRS_DEMOD_ADC_BIAS << 4;
    d->pm = d->ps = 64;
    d->sl[0].wa = 2; d->sl[0].wb = 3;   /* favours space */
    d->sl[1].wa = 1; d->sl[1].wb = 1;
    d->sl[2].wa = 3; d->sl[2].wb = 2;   /* favours mark */
}

static bool call_char(uint8_t c) { return (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == ' '; }

/* An APRS frame is a UI frame: the address field ends (bit 0 of an SSID byte) and is followed
 * by control 0x03 and PID 0xF0. Rejects the noise burst that passes the 16-bit FCS about once a
 * day in continuous decoding. */
static bool is_ui(const uint8_t *f, uint16_t n)
{
    uint16_t end = (uint16_t)(n - 2u), e = 13;
    while (!(f[e] & 1u) && e + 7u < end)
        e += 7;
    return (f[e] & 1u) && e + 2u < end && f[e + 1] == 0x03u && f[e + 2] == 0xF0u;
}

/* A good frame from slicer m: pending unless another slicer just gave it. */
static void accept(aprs_demod_t *d, const aprs_slicer_t *m)
{
    const uint16_t n = m->n;
    const bool dup = d->last_len == n && d->last_fcs[0] == m->buf[n - 2] && d->last_fcs[1] == m->buf[n - 1]
                  && (d->samples - d->last_at) < DUP_SAMPLES;
    d->last_at = d->samples;
    if (dup || d->out_len)
        return;
    d->last_len = n;
    d->last_fcs[0] = m->buf[n - 2];
    d->last_fcs[1] = m->buf[n - 1];
    d->out_len = n;
    d->out = m->buf;
}

/* HDLC: one bit after NRZI. A UI frame with a good FCS goes to accept() before the closing
 * flag opens the next frame. */
static void hdlc_bit(aprs_demod_t *d, aprs_slicer_t *m, uint8_t b)
{
    if (m->bits < 255u)
        m->bits++;
    m->sr = (uint8_t)((m->sr >> 1) | (b << 7));
    if (m->sr == 0x7Eu) {
        const bool ok = m->inframe && m->n >= FRAME_MIN && m->crc == FCS_GOOD && is_ui(m->buf, m->n);
        m->pre = m->bits == 8u;
        m->bits = 0;
        if (ok)
            accept(d, m);
        m->inframe = 1; m->hdr = 1; m->n = 0; m->crc = 0xFFFFu;
        m->ones = m->byte = m->nb = 0;
        return;
    }
    if (b) {
        if (++m->ones > 6u) { m->ones = 7; m->inframe = 0; return; }   /* abort / noise */
    } else {
        if (m->ones == 5u) { m->ones = 0; return; }                    /* stuffed zero */
        m->ones = 0;
    }
    if (!m->inframe)
        return;
    m->byte |= (uint8_t)(b << m->nb);
    if (++m->nb == 8u) {
        if (m->n < APRS_DEMOD_FRAME_MAX) {
            uint16_t c = m->crc ^ m->byte;        /* CRC-16/X.25, reflected 0x1021 */
            for (uint8_t i = 0; i < 8u; i++)
                c = (c & 1u) ? (uint16_t)((c >> 1) ^ 0x8408u) : (uint16_t)(c >> 1);
            m->crc = c;
            if (m->n < 6u && ((m->byte & 1u) || !call_char((uint8_t)(m->byte >> 1))))
                m->hdr = 0;
            m->buf[m->n++] = m->byte;
        } else {
            m->inframe = 0;
        }
        m->byte = m->nb = 0;
    }
}

/* |(i, q)| ~ max + 3/8 min */
static int32_t mag(int32_t i, int32_t q)
{
    if (i < 0) i = -i;
    if (q < 0) q = -q;
    if (i < q) { int32_t t = i; i = q; q = t; }
    return i + (q >> 2) + (q >> 3);
}

/* Magnitude of one tone and its peak tracker (AGC). */
static int32_t tone(int32_t i, int32_t q, int32_t *pk)
{
    const int32_t mt = mag(i, q);
    int32_t p = *pk;
    p += mt > p ? (mt - p) >> AGC_ATT : -(p >> AGC_DEC);
    if (p < PK_MIN)
        p = PK_MIN;
    *pk = p;
    return mt;
}

/* Front end, one ADC sample: *a = Mm*Ps, *b = Ms*Pm (each tone over its own peak, without
 * division; both stay below ~2^21). */
static void front(aprs_demod_t *m, int32_t adc, int32_t *a, int32_t *b)
{
    m->dc += ((adc << 4) - m->dc) >> 6;
    const int32_t x = adc - (m->dc >> 4);
    const int32_t y = (BP_B0 * (x - m->x2) - BP_A1 * m->y1 - BP_A2 * m->y2) >> 14;
    m->x2 = m->x1; m->x1 = x; m->y2 = m->y1; m->y1 = y;

    /* Sliding correlators over one bit. At 9.6 kHz one bit and one 1200 Hz period are both 8
     * samples: the ring slot is also the 1200 Hz table phase. */
    int16_t *o = m->ring[m->r];
    uint32_t k = m->ks + 12u;
    if (k >= 48u) k -= 48u;
    int32_t p;
    p = (y * COS1200[m->r]) >> 8;               m->im += p - o[0]; o[0] = (int16_t)p;
    p = (y * COS1200[(m->r + 6u) & 7u]) >> 8;   m->qm += p - o[1]; o[1] = (int16_t)p;
    p = (y * COS2200[m->ks]) >> 8;              m->is += p - o[2]; o[2] = (int16_t)p;
    p = (y * COS2200[k]) >> 8;                  m->qs += p - o[3]; o[3] = (int16_t)p;
    m->r = (m->r + 1u) & 7u;
    if (++m->ks >= 48u)
        m->ks = 0;

    const int32_t mm = tone(m->im, m->qm, &m->pm), ms = tone(m->is, m->qs, &m->ps);
    *a = mm * m->ps;
    *b = ms * m->pm;
}

/* Slicer: decision sign(wa*a - wb*b), DPLL, NRZI, HDLC. */
static void slice(aprs_demod_t *d, aprs_slicer_t *m, int32_t a, int32_t b)
{
    const int32_t dec = a * m->wa - b * m->wb;
    /* DPLL: a bit at each phase wrap (mid-bit), transitions pulled to mid-phase */
    m->phase += PLL_STEP;
    if ((dec > 0) != (m->dprev > 0))
        m->phase += (PLL_CTR - m->phase) >> PLL_SHIFT;
    m->dprev = dec;
    if (m->phase >= 65536) {
        m->phase -= 65536;
        const uint8_t lv = dec > 0;
        hdlc_bit(d, m, lv == m->last);            /* NRZI: no change = 1 */
        m->last = lv;
    }
}

uint16_t APRS_DemodSample(aprs_demod_t *d, uint16_t adc)
{
    int32_t a, b;
    d->out_len = 0;
    d->samples++;
    front(d, (int32_t)(adc & 0x0FFFu), &a, &b);
    for (uint8_t k = 0; k < APRS_DEMOD_SLICERS; k++)
        slice(d, &d->sl[k], a, b);
    return d->out_len;
}

const uint8_t *APRS_DemodFrame(const aprs_demod_t *d)
{
    return d->out;
}

bool APRS_DemodBusy(const aprs_demod_t *d)
{
    for (uint8_t k = 0; k < APRS_DEMOD_SLICERS; k++) {
        const aprs_slicer_t *m = &d->sl[k];
        if ((m->pre && m->bits < PRE_BITS) || (m->inframe && m->n && m->hdr))
            return true;
    }
    return false;
}
