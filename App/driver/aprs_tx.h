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


#ifndef DRIVER_APRS_TX_H
#define DRIVER_APRS_TX_H

#include <stdbool.h>
#include <stdint.h>

typedef enum {
    APRS_TX_OK,
    APRS_TX_BAD_FRAME,    /* empty, too short or longer than APRS_RAWTX_MAX */
    APRS_TX_NO_CALL,      /* callsign not set (N0CALL) */
    APRS_TX_DENIED,       /* band, battery or modulation refuses transmitting */
} aprs_tx_result_t;

/* Send one AX.25 frame (FCS excluded; it is appended) as Bell 202 on the TX VFO.
 * Blocking, about 0.8 s for a beacon: the receiver is paused and the keys are not served meanwhile.
 * The tones are the BK4829 TX tone path, REG_71 / REG_70 rewritten at each NRZI transition, 833 us
 * apart on the SysTick cycle counter (armel's method, App/apps/aprstx). */
aprs_tx_result_t APRS_TxSend(const uint8_t *frame, uint16_t len);
/* True if a transmission would currently be refused for a reason other than the frame. */
aprs_tx_result_t APRS_TxCheck(void);

#endif
