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
#include "app/aprs_ax25.h"
#include "app/aprs_beacon.h"
#include "app/aprs_msg.h"
#include "app/aprs_reason.h"
#include "app/aprs_rxinfo.h"
#include "app/aprs_settings.h"

/* The station's message and beacon state machine (hardware-free; app/aprs_task.c drives it).
 *
 * What goes on the air is decided here, in this order: an ack for a numbered message we received
 * (up to four wait), a raw frame from a host, then the oldest queued message (up to four wait: the menu's Send
 * and the host's message command both queue), then a beacon (menu BEACON, or the interval timer). The task
 * transmits when the channel is idle and reports back with APRS_StationSent; until then the request stays
 * pending and is offered again on the next tick. A request the channel never let through is given up after a
 * minute, one at a time. What was sent and what was given up, and why, goes to the event sink (the monitor lines). */

#define APRS_MSGQ_DEPTH 4u              /* messages waiting to go out */
#define APRS_ACKQ_DEPTH 4u              /* acks waiting to go out */

typedef struct {
    char    to[10];
    char    text[APRS_MSG_TEXT_MAX + 1];
    uint8_t seq;                        /* the line number it goes out with, given when it was queued */
} aprs_qmsg_t;

typedef struct {
    char to[10];                        /* who sent the message */
    char seq[APRS_ACK_SEQ_MAX + 1];     /* its line number as received */
} aprs_qack_t;

#define APRS_BEACON_FIRST_TICKS 30u    /* first auto-beacon 15 s after enabling (500 ms ticks) */

typedef enum { APRS_ACT_NONE, APRS_ACT_ACK, APRS_ACT_RAW, APRS_ACT_MSG, APRS_ACT_BEACON } aprs_action_t;

typedef struct {
    char     msg_text[APRS_MSG_TEXT_MAX + 1];   /* the composed message (RAM only) */
    char     msgto[10];                          /* target: the stored MsgTo, then whoever wrote last */
    uint8_t  seq;                                /* the last line number given out (1..99) */
    bool     dirty;                              /* text or target changed: the next menu Send takes a new number */
    bool     beacon_pending;
    aprs_qmsg_t q[APRS_MSGQ_DEPTH];              /* outgoing messages, oldest first from q_head */
    uint8_t  q_head, q_count;
    aprs_qack_t a[APRS_ACKQ_DEPTH];              /* acks to send, oldest first from a_head */
    uint8_t  a_head, a_count;
    char     last_msg[APRS_RXTEXT_MAX + 1];      /* the last message addressed to us, for RdMsg */
    uint16_t beacon_countdown;                   /* 500 ms ticks to the next auto-beacon, 0 = none */
    uint16_t beacon_period;                      /* the interval in 500 ms ticks, for re-arming after a beacon was given up */
    uint32_t heard_msgs, sent_msgs, sent_beacons;
    uint8_t  raw[APRS_RAWTX_MAX];                /* a frame queued by the host (KISS-style raw transmit) */
    uint16_t raw_len;                            /* 0 = none */
    uint16_t pending_age;                        /* slots (500 ms) the oldest request has waited */
    uint16_t quiet;                              /* slots left before the next transmission is allowed */
} aprs_station_t;

#define APRS_PENDING_MAX_SLOTS 120u    /* a request the channel kept busy for a minute is given up (reported as `busy`) */
#define APRS_TX_MIN_GAP_SLOTS  APRS_GAP_DEFAULT_SLOTS   /* at least 5 s between our transmissions, whoever asks (the Gap setting: 2-10 s) */

/* Where SENT / DROP events go (NULL: nowhere). The task points it at the serial monitor. */
typedef void (*aprs_event_sink_t)(const aprs_event_t *e);
void APRS_StationSetEventSink(aprs_event_sink_t sink);

void APRS_StationInit(aprs_station_t *st, const aprs_settings_t *s);
/* APRS was switched off: forget everything waiting to go out. Messages and acks are reported as dropped (`off`). */
void APRS_StationClearPending(aprs_station_t *st);
/* (Re)arm the auto-beacon timer from the interval in s. Call when APRS or Intv changes. */
void APRS_StationRearm(aprs_station_t *st, const aprs_settings_t *s);
/* The operator edited MsgTo / Msg. */
void APRS_StationSetMsgTo(aprs_station_t *st, const char *to);
void APRS_StationSetMsgText(aprs_station_t *st, const char *text);
/* Queue a message. new_number: take the next line number (a host message always does); false: it is a retry of the
 * last one and keeps its number. EMPTY for no destination or no text, FULL when four are already waiting (nothing is
 * replaced). *seq_out (may be NULL) gets the line number on success. */
aprs_reason_t APRS_StationEnqueue(aprs_station_t *st, const char *to, const char *text, bool new_number, uint8_t *seq_out);
/* Free places in the message queue. */
uint8_t APRS_StationMsgFree(const aprs_station_t *st);
/* Menu Send: queue the composed message. A changed target or text takes a new line number, the same text again is a
 * retry. false (nothing queued) with no target, no text, or a full queue. */
bool APRS_StationQueueMessage(aprs_station_t *st);
/* An ack for a numbered message from `to`. The same sender and number twice is kept once. FULL when four are waiting
 * (the ack is reported as dropped). */
aprs_reason_t APRS_StationQueueAck(aprs_station_t *st, const char *to, const char *seq);
uint8_t APRS_StationAcksPending(const aprs_station_t *st);
void APRS_StationQueueBeacon(aprs_station_t *st);
/* A raw frame from the host (FCS excluded, 17..APRS_RAWTX_MAX bytes). false if one is already waiting or it does not fit. */
bool APRS_StationQueueRaw(aprs_station_t *st, const uint8_t *frame, uint16_t len);

/* A decoded frame (FCS included). Updates the reply target, the ack and RdMsg state and fills *info.
 * Returns false for a frame that carries nothing or is our own. */
bool APRS_StationOnFrame(aprs_station_t *st, const aprs_settings_t *s, const uint8_t *frame, uint16_t len,
                         aprs_rx_info_t *info);
/* Every 500 ms: counts the beacon timer down and says what to send next, if anything. */
aprs_action_t APRS_StationTick(aprs_station_t *st);
/* The frame for an action (FCS excluded, out holds APRS_RAWTX_MAX + 2 bytes: the longest is a raw frame). 0: nothing to send
 * (e.g. a beacon with no valid Loc), in which case the request is dropped. */
uint16_t APRS_StationBuild(aprs_station_t *st, const aprs_settings_t *s, aprs_action_t act, uint8_t *out);
/* The frame went out. */
void APRS_StationSent(aprs_station_t *st, const aprs_settings_t *s, aprs_action_t act);
/* The request cannot be sent (refused by the radio, nothing to build): forget it. `reason` is reported for a message or
 * an ack (a beacon or a raw frame is just forgotten). */
void APRS_StationDrop(aprs_station_t *st, const aprs_settings_t *s, aprs_action_t act, aprs_reason_t reason);

/* RdMsg paging: 16 characters a page. */
#define APRS_RDMSG_PAGE 16u
unsigned APRS_StationMsgPages(const aprs_station_t *st);

#endif
