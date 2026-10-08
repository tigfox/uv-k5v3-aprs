/* Copyright 2026 tigfox, after the ta1js APRS task.
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


#ifndef APP_APRS_STATION_H
#define APP_APRS_STATION_H

#include <stdbool.h>
#include <stdint.h>
#include "app/aprs_beacon.h"
#include "app/aprs_msg.h"
#include "app/aprs_rxinfo.h"
#include "app/aprs_settings.h"

/* The station's message and beacon state machine (hardware-free; app/aprs_task.c drives it).
 *
 * What goes on the air is decided here, in this order: an ack for a numbered message we received,
 * then the message the operator queued (menu Send), then a beacon (menu BEACON, or the interval
 * timer). The task transmits when the channel is idle and reports back with APRS_StationSent;
 * until then the request stays pending and is offered again on the next tick. */

#define APRS_BEACON_FIRST_TICKS 30u    /* first auto-beacon 15 s after enabling (500 ms ticks) */

typedef enum { APRS_ACT_NONE, APRS_ACT_ACK, APRS_ACT_MSG, APRS_ACT_BEACON } aprs_action_t;

typedef struct {
    char     msg_text[APRS_MSG_TEXT_MAX + 1];   /* the composed message (RAM only) */
    char     msgto[10];                          /* target: the stored MsgTo, then whoever wrote last */
    uint8_t  seq;                                /* line number of the last message sent */
    bool     dirty;                              /* text or target changed: the next send takes a new number */
    bool     msg_pending, beacon_pending;
    char     ack_to[10], ack_seq[APRS_ACK_SEQ_MAX + 1];
    bool     ack_pending;
    char     last_msg[APRS_RXTEXT_MAX + 1];      /* the last message addressed to us, for RdMsg */
    uint16_t beacon_countdown;                   /* 500 ms ticks to the next auto-beacon, 0 = none */
    uint32_t heard_msgs, sent_msgs, sent_beacons;
} aprs_station_t;

void APRS_StationInit(aprs_station_t *st, const aprs_settings_t *s);
/* (Re)arm the auto-beacon timer from the interval in s. Call when APRS or Intv changes. */
void APRS_StationRearm(aprs_station_t *st, const aprs_settings_t *s);
/* The operator edited MsgTo / Msg. */
void APRS_StationSetMsgTo(aprs_station_t *st, const char *to);
void APRS_StationSetMsgText(aprs_station_t *st, const char *text);
/* Menu Send / BEACON. Send returns false (nothing queued) with no target or no text. */
bool APRS_StationQueueMessage(aprs_station_t *st);
void APRS_StationQueueBeacon(aprs_station_t *st);

/* A decoded frame (FCS included). Updates the reply target, the ack and RdMsg state and fills *info.
 * Returns false for a frame that carries nothing or is our own. */
bool APRS_StationOnFrame(aprs_station_t *st, const aprs_settings_t *s, const uint8_t *frame, uint16_t len,
                         aprs_rx_info_t *info);
/* Every 500 ms: counts the beacon timer down and says what to send next, if anything. */
aprs_action_t APRS_StationTick(aprs_station_t *st);
/* The frame for an action (FCS excluded, out holds APRS_BUILD_MAX + 2). 0: nothing to send
 * (e.g. a beacon with no valid Loc), in which case the request is dropped. */
uint16_t APRS_StationBuild(aprs_station_t *st, const aprs_settings_t *s, aprs_action_t act, uint8_t *out);
/* The frame went out. */
void APRS_StationSent(aprs_station_t *st, const aprs_settings_t *s, aprs_action_t act);
/* The request cannot be sent (refused by the radio, nothing to build): forget it. */
void APRS_StationDrop(aprs_station_t *st, const aprs_settings_t *s, aprs_action_t act);

/* RdMsg paging: 16 characters a page. */
#define APRS_RDMSG_PAGE 16u
unsigned APRS_StationMsgPages(const aprs_station_t *st);

#endif
