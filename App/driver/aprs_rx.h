/* Copyright 2026 tigfox. Licensed under the Apache License, Version 2.0 (the "License");
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
 *
 * The sampling method (RX audio on PA4, held at mid-scale by the DAC, read on ADC channel 4 at
 * 9.6 kHz) is armel's: App/apps/aprsrx and EPIRB 406.
 */

#ifndef DRIVER_APRS_RX_H
#define DRIVER_APRS_RX_H

#include <stdbool.h>
#include <stdint.h>
#include "app/aprs_demod.h"

/* Resident APRS receiver. A 9.6 kHz timer interrupt (TIM6) reads the BK4829 audio on PA4 with
 * ADC1 channel 4 and runs the software demodulator on each sample (app/aprs_demod.c); decoded
 * frames are queued for the main loop. No DMA channel is used.
 *
 * PA4 is the voice DAC pin, so ENABLE_VOICE cannot be on in the same build. The ADC is shared
 * with the battery reading: board.c brackets it with APRS_RxAdcAcquire / APRS_RxAdcRelease. */

#define APRS_RX_QUEUE 2u

typedef struct {
    uint16_t len;                           /* FCS included */
    uint8_t  data[APRS_DEMOD_FRAME_MAX];
} aprs_rx_frame_t;

typedef struct {
    uint32_t samples;       /* ADC samples demodulated */
    uint32_t frames;        /* good frames decoded */
    uint32_t dropped;       /* decoded but the queue was full */
    uint32_t isr_cycles_max;/* longest interrupt, in CPU cycles (48 per microsecond) since the last read */
    uint32_t isr_cycles_avg;/* mean over the same period */
} aprs_rx_stats_t;

void APRS_RxStart(void);    /* bias PA4, switch the ADC to it and start sampling; idempotent */
void APRS_RxStop(void);     /* stop sampling and give PA4 and the ADC back */
bool APRS_RxRunning(void);
/* Stop sampling without giving PA4 and the ADC back (a transmission, whose audio is not ours to
 * decode); Resume restarts it with a fresh demodulator. No-ops when not running. */
void APRS_RxPause(void);
void APRS_RxResume(void);
/* Next decoded frame, oldest first; false when none. Main context only. */
bool APRS_RxPop(aprs_rx_frame_t *out);
/* A packet is arriving (preamble or a frame that looks like one): do not transmit now. */
bool APRS_RxBusy(void);
/* Counters; resets the cycle maximum and mean. */
aprs_rx_stats_t APRS_RxStats(void);

/* Hold the ADC for a conversion on another channel (the battery). The sampling interrupt is
 * held off between the two calls (one sample is delayed, not lost). No-ops when stopped. */
void APRS_RxAdcAcquire(void);
void APRS_RxAdcRelease(void);

#endif
