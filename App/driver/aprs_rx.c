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

#include "driver/aprs_rx.h"
#include "app/aprs_demod.h"
#include "py32f071_ll_adc.h"
#include "py32f071_ll_bus.h"
#include "py32f071_ll_dac.h"
#include "py32f071_ll_gpio.h"
#include "py32f071_ll_tim.h"
#include "py32f0xx.h"
#include <string.h>

#ifdef ENABLE_VOICE
#error "ENABLE_APRS and ENABLE_VOICE both use PA4 (the DAC): build the APRS preset without voice"
#endif

#define SAMPLE_CYCLES   (48000000u / APRS_DEMOD_FS)     /* 5000 */
#define BIAS_CODE       APRS_DEMOD_ADC_BIAS
#define TIM_IRQ_PRIO    1u
#define DAC_CHANNEL     LL_DAC_CHANNEL_1

static aprs_demod_t     gDemod;
static aprs_rx_frame_t  gQueue[APRS_RX_QUEUE];
static volatile uint8_t gHead, gTail;           /* ISR writes gHead, main writes gTail */
static volatile bool    gRunning, gBusy, gPrimed, gHeld, gPaused;
static volatile uint32_t gSamples, gFrames, gDropped;
static volatile uint32_t gCycMax, gCycSum, gCycN;

static void bias_on(void)
{
    LL_IOP_GRP1_EnableClock(LL_IOP_GRP1_PERIPH_GPIOA);
    LL_GPIO_SetPinMode(GPIOA, LL_GPIO_PIN_4, LL_GPIO_MODE_ANALOG);
    LL_APB1_GRP1_EnableClock(LL_APB1_GRP1_PERIPH_DAC1);
    LL_DAC_SetTriggerSource(DAC1, DAC_CHANNEL, LL_DAC_TRIG_SOFTWARE);
    LL_DAC_SetOutputBuffer(DAC1, DAC_CHANNEL, LL_DAC_OUTPUT_BUFFER_DISABLE);   /* unbuffered: the audio is AC-coupled in */
    LL_DAC_EnableTrigger(DAC1, DAC_CHANNEL);
    LL_DAC_Enable(DAC1, DAC_CHANNEL);
    LL_DAC_ConvertData12RightAligned(DAC1, DAC_CHANNEL, BIAS_CODE);
    LL_DAC_TrigSWConversion(DAC1, DAC_CHANNEL);
}

static void bias_off(void)
{
    LL_DAC_Disable(DAC1, DAC_CHANNEL);
    LL_DAC_DisableTrigger(DAC1, DAC_CHANNEL);
    LL_APB1_GRP1_DisableClock(LL_APB1_GRP1_PERIPH_DAC1);
}

/* Finish the conversion the last tick started and read it, which clears the end flag the
 * board code polls (it would otherwise see a stale one). Bounded: the conversion is 4.5 us. */
static void adc_drain(void)
{
    if (gPrimed) {
        for (uint32_t n = 0; n < 4000u && !LL_ADC_IsActiveFlag_EOS(ADC1); n++)
            ;
        (void)LL_ADC_REG_ReadConversionData12(ADC1);
    }
}

static void adc_select_pa4(void)
{
    LL_ADC_SetChannelSamplingTime(ADC1, LL_ADC_CHANNEL_4, LL_ADC_SAMPLINGTIME_41CYCLES_5);
    LL_ADC_REG_SetSequencerRanks(ADC1, LL_ADC_REG_RANK_1, LL_ADC_CHANNEL_4);
}

static void adc_select_battery(void)
{
    LL_ADC_REG_SetSequencerRanks(ADC1, LL_ADC_REG_RANK_1, LL_ADC_CHANNEL_8);
}

void APRS_RxStart(void)
{
    if (gRunning)
        return;
    APRS_DemodInit(&gDemod);
    gHead = gTail = 0;
    gBusy = gPrimed = gHeld = gPaused = false;
    gSamples = gFrames = gDropped = gCycMax = gCycSum = gCycN = 0;

    bias_on();
    adc_select_pa4();

    LL_APB1_GRP1_EnableClock(LL_APB1_GRP1_PERIPH_TIM6);
    LL_TIM_SetPrescaler(TIM6, 0);
    LL_TIM_SetAutoReload(TIM6, SAMPLE_CYCLES - 1u);
    LL_TIM_SetCounter(TIM6, 0);
    LL_TIM_ClearFlag_UPDATE(TIM6);
    LL_TIM_EnableIT_UPDATE(TIM6);
    NVIC_SetPriority(TIM6_LPTIM1_DAC_IRQn, TIM_IRQ_PRIO);
    NVIC_EnableIRQ(TIM6_LPTIM1_DAC_IRQn);
    gRunning = true;
    LL_TIM_EnableCounter(TIM6);
}

void APRS_RxStop(void)
{
    if (!gRunning)
        return;
    LL_TIM_DisableCounter(TIM6);
    NVIC_DisableIRQ(TIM6_LPTIM1_DAC_IRQn);
    LL_TIM_DisableIT_UPDATE(TIM6);
    gRunning = false;
    gBusy = false;
    adc_drain();
    gPrimed = false;
    adc_select_battery();
    bias_off();
}

bool APRS_RxRunning(void)
{
    return gRunning;
}

void APRS_RxPause(void)
{
    if (!gRunning || gPaused)
        return;
    LL_TIM_DisableCounter(TIM6);
    NVIC_DisableIRQ(TIM6_LPTIM1_DAC_IRQn);
    adc_drain();
    gPrimed = false;
    gBusy = false;
    gPaused = true;
}

void APRS_RxResume(void)
{
    if (!gRunning || !gPaused)
        return;
    APRS_DemodInit(&gDemod);
    gPrimed = false;
    LL_TIM_SetCounter(TIM6, 0);
    LL_TIM_ClearFlag_UPDATE(TIM6);
    NVIC_ClearPendingIRQ(TIM6_LPTIM1_DAC_IRQn);
    gPaused = false;
    NVIC_EnableIRQ(TIM6_LPTIM1_DAC_IRQn);
    LL_TIM_EnableCounter(TIM6);
}

bool APRS_RxBusy(void)
{
    return gRunning && gBusy;
}

bool APRS_RxPop(aprs_rx_frame_t *out)
{
    const uint8_t tail = gTail;
    if (gHead == tail)
        return false;
    *out = gQueue[tail];
    gTail = (uint8_t)((tail + 1u) % APRS_RX_QUEUE);
    return true;
}

aprs_rx_stats_t APRS_RxStats(void)
{
    aprs_rx_stats_t s;
    __disable_irq();
    s.samples = gSamples;
    s.frames = gFrames;
    s.dropped = gDropped;
    s.isr_cycles_max = gCycMax;
    s.isr_cycles_avg = gCycN ? gCycSum / gCycN : 0;
    gCycMax = gCycSum = gCycN = 0;
    __enable_irq();
    return s;
}

void APRS_RxAdcAcquire(void)
{
    if (!gRunning)
        return;
    NVIC_DisableIRQ(TIM6_LPTIM1_DAC_IRQn);
    adc_drain();
    gPrimed = false;
    gHeld = true;
    adc_select_battery();
}

void APRS_RxAdcRelease(void)
{
    if (!gHeld)
        return;
    adc_select_pa4();
    gPrimed = false;            /* the ADC data register holds a battery reading */
    gHeld = false;
    if (!gPaused)
        NVIC_EnableIRQ(TIM6_LPTIM1_DAC_IRQn);   /* a pending tick fires now: one late sample */
}

static inline uint32_t systick_elapsed(uint32_t start, uint32_t end)
{
    return start >= end ? start - end : start + (SysTick->LOAD + 1u) - end;
}

void TIM6_LPTIM1_DAC_IRQHandler(void)
{
    if (!LL_TIM_IsActiveFlag_UPDATE(TIM6))
        return;                 /* LPTIM1 / DAC underrun share this vector: not ours */
    LL_TIM_ClearFlag_UPDATE(TIM6);
    const uint32_t t0 = SysTick->VAL;

    /* The conversion started by the previous tick has long finished (4.5 us of 104). */
    const uint16_t adc = (uint16_t)(LL_ADC_REG_ReadConversionData12(ADC1) & 0x0FFFu);
    LL_ADC_REG_StartConversionSWStart(ADC1);
    if (gPrimed) {
        gSamples++;
        const uint16_t len = APRS_DemodSample(&gDemod, adc);
        if (len) {
            gFrames++;
            const uint8_t head = gHead, next = (uint8_t)((head + 1u) % APRS_RX_QUEUE);
            if (next == gTail) {
                gDropped++;
            } else {
                gQueue[head].len = len;
                memcpy(gQueue[head].data, APRS_DemodFrame(&gDemod), len);
                __DMB();                // the frame is written before the main loop can see it
                gHead = next;
            }
        }
        gBusy = APRS_DemodBusy(&gDemod);
    }
    gPrimed = true;

    const uint32_t cyc = systick_elapsed(t0, SysTick->VAL);
    if (cyc > gCycMax)
        gCycMax = cyc;
    gCycSum += cyc;
    gCycN++;
}
