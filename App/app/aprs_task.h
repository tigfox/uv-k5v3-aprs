/* Copyright 2026 tigfox.
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


#ifndef APP_APRS_TASK_H
#define APP_APRS_TASK_H

#include <stdbool.h>
#include <stddef.h>
#include "driver/aprs_tx.h"

/* Glue between the settings, the receiver driver and the rest of the firmware. */
void APRS_TaskInit(void);                 /* once, after the settings are loaded: start the receiver if APRS is on */
void APRS_Task10ms(void);                 /* every 10 ms: collect decoded frames */
bool APRS_SetOn(bool on);                 /* menu: switch APRS on/off, save, start/stop the receiver */
aprs_tx_result_t APRS_TxBeacon(void);     /* send the station beacon now (menu BEACON) */
bool APRS_IsOn(void);                     /* the receiver is running (battery save and sleep stay off) */
void APRS_DStatString(char *out, size_t n);

#endif
