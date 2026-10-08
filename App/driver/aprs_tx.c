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


#include "driver/aprs_tx.h"
#include "app/aprs_ax25.h"
#include "app/aprs_modem.h"
#include "app/aprs_store.h"
#include "driver/aprs_rx.h"
#include "driver/bk4819.h"
#include "frequencies.h"
#include "misc.h"
#include "py32f0xx.h"
#include "radio.h"
#include "settings.h"
#include <string.h>

#define FRAME_MIN 17u        /* 2 addresses + control + PID */
#define SETTLE_BITS 1u       /* one bit of mark before the first flag, as armel's app */

/* SysTick cycle counter: the timer counts down 48 MHz cycles, reloading every 10 ms. Call at least
 * once per 10 ms; wraps of the counter itself are handled by comparing with a signed difference. */
typedef struct { uint32_t prev, total; } cyc_t;

static void cyc_start(cyc_t *c)
{
    c->prev = SysTick->VAL;
    c->total = 0;
}

static uint32_t cyc_now(cyc_t *c)
{
    const uint32_t v = SysTick->VAL;
    c->total += (v <= c->prev) ? c->prev - v : c->prev + (SysTick->LOAD + 1u) - v;
    c->prev = v;
    return c->total;
}

aprs_tx_result_t APRS_TxCheck(void)
{
    if (!APRS_CallIsSet(&gAprsSettings))
        return APRS_TX_NO_CALL;
    if (TX_freq_check(gTxVfo->pTX->Frequency) != 0 || gTxVfo->Modulation != MODULATION_FM)
        return APRS_TX_DENIED;
    if (gBatteryDisplayLevel == 0 || gBatteryDisplayLevel > 6)
        return APRS_TX_DENIED;
    return APRS_TX_OK;
}

static void set_tone(bool space)
{
    uint16_t r71, r70;
    APRS_ToneRegs(space, gAprsSettings.tone_level, gAprsSettings.tone_twist, &r71, &r70);
    BK4819_WriteRegister(BK4819_REG_71, r71);
    BK4819_WriteRegister(BK4819_REG_70, r70);
}

aprs_tx_result_t APRS_TxSend(const uint8_t *frame, uint16_t len)
{
    if (len < FRAME_MIN || len > APRS_RAWTX_MAX)
        return APRS_TX_BAD_FRAME;
    const aprs_tx_result_t chk = APRS_TxCheck();
    if (chk != APRS_TX_OK)
        return chk;

    static uint8_t full[APRS_RAWTX_MAX + 2u], bits[HDLC_BUF_SIZE];   // static: keeps the stack shallow (not reentrant)
    memcpy(full, frame, len);
    const uint16_t fcs = AX25_CalculateFCS(frame, len);
    full[len] = (uint8_t)(fcs & 0xFF);
    full[len + 1u] = (uint8_t)(fcs >> 8);
    const uint16_t nbits = HDLC_EncodeFrame(bits, full, (uint16_t)(len + 2u));
    if (nbits == 0)
        return APRS_TX_BAD_FRAME;

    APRS_RxPause();                              // the audio is our own signal now
    VFO_Info_t *const savedVfo = gCurrentVfo;
    gCurrentVfo = gTxVfo;                        // always the main VFO

    RADIO_SetTxParameters();                     // carrier and PA on, power from the Power menu
    BK4819_WriteRegister(BK4819_REG_51, 0);      // no CTCSS/DCS under the tones
    BK4819_TransmitTone(false, 1200);            // tone path on, microphone off, 50 ms settle
    bool space = false;
    set_tone(space);

    cyc_t clk;
    cyc_start(&clk);
    uint32_t next = APRS_TX_CYCLES_PER_BIT * SETTLE_BITS;
    for (uint16_t i = 0; i < nbits; i++) {
        while ((int32_t)(cyc_now(&clk) - next) < 0) { }
        const bool want_space = !APRS_LineLevel(bits, i);
        if (want_space != space) {               // NRZI: a transition changes the tone
            space = want_space;
            set_tone(space);
        }
        next += APRS_TX_CYCLES_PER_BIT;
    }
    while ((int32_t)(cyc_now(&clk) - next) < 0) { }   // the last bit plays out

    BK4819_EnterTxMute();
    BK4819_WriteRegister(BK4819_REG_70, 0);
    BK4819_ToggleGpioOut(BK4819_GPIO1_PIN29_PA_ENABLE, false);
    RADIO_SetupRegisters(true);                  // back to receive
    gCurrentVfo = savedVfo;
    APRS_RxResume();
    return APRS_TX_OK;
}
