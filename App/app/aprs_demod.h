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


#ifndef APP_APRS_DEMOD_H
#define APP_APRS_DEMOD_H

#include <stdbool.h>
#include <stdint.h>

/* Software Bell 202 (1200/2200 Hz AFSK, 1200 baud) demodulator for the BK4829 audio sampled
 * on PA4 by the ADC at 9.6 kHz (8 samples per bit). Hardware-free: the sampling driver feeds
 * it 12-bit samples; tools/aprs/test_aprs_demod.c checks it against armel's Python model
 * (App/apps/aprsrx/test/model_rx.py), of which this is a transcription. Keep the two in step.
 *
 * Per sample: DC removal, 1625 Hz band-pass, 1200/2200 Hz sliding correlators over one bit,
 * per-tone AGC, then 3 slicers (mark/space weights 2:3, 1:1, 3:2), each with its own DPLL,
 * NRZI, HDLC and FCS. A frame counts only with a good FCS and as a UI frame. */

#define APRS_DEMOD_FRAME_MAX 256u   /* 8 digipeaters + control + PID + 200 info + FCS: RAM (3 slicers + queue) is tight; armel's app allows 330 */
#define APRS_DEMOD_FS        9600u
#define APRS_DEMOD_SLICERS   3u
#define APRS_DEMOD_ADC_BIAS  2048u  /* PA4 is held at mid-scale by the DAC */

typedef struct {
    int32_t  dprev, phase;             /* last decision, DPLL phase (65536 per bit) */
    uint16_t n, crc;                   /* bytes, running FCS */
    uint8_t  wa, wb, last;             /* mark/space weights, last level */
    uint8_t  sr, ones, byte, nb;       /* HDLC: shift register, run of ones, byte, bit count */
    uint8_t  inframe, hdr;             /* between flags, first bytes look like a callsign */
    uint8_t  bits;                     /* bits since the last flag */
    bool     pre;                      /* this flag came right after another one */
    uint8_t  buf[APRS_DEMOD_FRAME_MAX];
} aprs_slicer_t;

typedef struct {
    int32_t  dc;                       /* DC tracker, Q4 */
    int32_t  x1, x2, y1, y2;           /* band-pass state */
    int32_t  im, qm, is, qs;           /* sliding correlator sums */
    int32_t  pm, ps;                   /* per-tone magnitude peaks (AGC) */
    uint32_t r, ks;                    /* ring slot = 1200 Hz phase; 2200 Hz phase */
    int16_t  ring[8][4];               /* the products of the last bit */
    aprs_slicer_t sl[APRS_DEMOD_SLICERS];
    uint32_t samples;                  /* sample counter (wraps; used for the duplicate window) */
    uint32_t last_at;                  /* sample number of the last accepted frame */
    uint16_t last_len;                 /* ... and its length and FCS, to drop another slicer's copy */
    uint8_t  last_fcs[2];
    uint16_t out_len;                  /* frame completed by the last call, 0 = none */
    const uint8_t *out;                /* ... and where it is (a slicer's buffer) */
} aprs_demod_t;

void APRS_DemodInit(aprs_demod_t *d);
/* Feed one 12-bit ADC sample. Returns the length (FCS included) of a good frame completed by
 * this sample, else 0. The frame is at APRS_DemodFrame() until the next call. */
uint16_t APRS_DemodSample(aprs_demod_t *d, uint16_t adc);
const uint8_t *APRS_DemodFrame(const aprs_demod_t *d);
/* True while a slicer is inside a preamble (two back-to-back flags, then 3 bytes) or a frame
 * whose first bytes look like a callsign: transmitting now would step on a packet. Noise
 * is busy about 4 % of the time. */
bool APRS_DemodBusy(const aprs_demod_t *d);

#endif
