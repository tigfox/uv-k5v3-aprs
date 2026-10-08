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

/* Glue between the settings, the receiver and transmitter drivers, and the station state machine
 * (app/aprs_station.c). */
void APRS_TaskInit(void);                 /* once, after the settings are loaded: start the receiver if APRS is on */
void APRS_Task10ms(void);                 /* every 10 ms: decoded frames; every 500 ms: what to transmit */
bool APRS_SetOn(bool on);                 /* menu: switch APRS on/off, save, start/stop the receiver */
bool APRS_IsOn(void);                     /* the receiver is running (battery save and sleep stay off) */
void APRS_TaskSettingsChanged(void);      /* after any APRS setting was saved: re-arm the beacon timer */
/* The DStat text for a view 0..APRS_DSTAT_VIEWS-1; APRS_TaskDStatAutoView() rotates every 2 s. */
/* The main screen: "LH W1AW-9 12m*" (19 bytes), false until a packet is heard or while APRS is off. */
bool APRS_LastHeard(char *out);
/* Panel counter 0 HRD, 1 RPT, 2 DUP, 3 DRP as exactly 8 characters (+ NUL: 9 bytes). */
void APRS_PanelCount(char *out, unsigned which);
/* The packet box: the last decoded packet (30 s) or a message to us (until a key). "" when none. */
const char *APRS_BoxText(void);
bool APRS_DismissMessage(void);           /* a key closes a message box; true if it did (the key is used up) */
void APRS_DStatString(char *out, size_t n, unsigned view);
unsigned APRS_TaskDStatAutoView(void);

/* Menu Send / BEACON: queue a transmission for the next idle moment (within about half a second). */
bool APRS_TaskQueueSend(void);            /* false: no target or no text */
void APRS_TaskQueueBeacon(void);

/* The message fields, kept in RAM (the reply target follows whoever wrote last). */
const char *APRS_TaskMsgTo(void);
const char *APRS_TaskMsgText(void);
const char *APRS_TaskLastMsg(void);       /* the last message addressed to us, "" if none */
unsigned    APRS_TaskLastMsgPages(void);
void APRS_TaskSetMsgTo(const char *to);
void APRS_TaskSetMsgText(const char *text);

#endif
