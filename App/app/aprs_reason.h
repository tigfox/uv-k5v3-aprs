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


#ifndef APP_APRS_REASON_H
#define APP_APRS_REASON_H

#include <stddef.h>
#include <stdint.h>

/* Why a message was refused or a queued transmission was given up. One table for the DROP monitor lines, byte 7 of the
 * reply to the send command and the status command. The numbers are a wire format (claude/aprs-messaging-api.md). */
typedef enum {
    APRS_R_OK = 0,
    APRS_R_FULL,        /* the outgoing queue (or the ack list) is full */
    APRS_R_SESSION,     /* wrong or missing session timestamp (a refusal only) */
    APRS_R_EMPTY,       /* empty destination or text (a refusal only) */
    APRS_R_TOOLONG,     /* more than 67 text bytes (a refusal only) */
    APRS_R_BUSY,        /* the channel, or the radio (scanning, being configured), stayed busy for 60 s */
    APRS_R_NOCALL,      /* the callsign is the placeholder */
    APRS_R_APRSOFF,     /* APRS is switched off */
    APRS_R_BAND,        /* the TX frequency is outside the allowed bands */
    APRS_R_BATTERY,     /* battery too low */
    APRS_R_MODE,        /* the TX VFO is not FM */
    APRS_R_REPLACED,    /* a newer message took the slot (reserved: the queue never replaces) */
    APRS_R_OFF,         /* APRS was switched off with this still waiting */
    APRS_R_INVALID,     /* a value the radio does not accept (a callsign, an SSID) */
    APRS_R_STORE        /* the radio could not save it */
} aprs_reason_t;

/* "full", "busy" ... ; "" for OK and for a number that is not a reason. */
const char *APRS_ReasonText(uint8_t reason);

/* What the station reports to a monitoring host. seq: a message's two-digit line number, or an ack's number as received. */
typedef enum { APRS_EV_SENT, APRS_EV_DROP_MSG, APRS_EV_DROP_ACK } aprs_event_kind_t;
typedef struct {
    aprs_event_kind_t kind;
    char    to[10];        /* the destination of a message, or the station an ack goes to */
    char    seq[6];
    uint8_t reason;        /* aprs_reason_t, for the DROP kinds */
} aprs_event_t;

#define APRS_EVENT_LINE_MAX 48u

/* "SENT:KD2DCM-1{07}", "DROP:KD2DCM-1{07} busy", "DROP:ack KD2DCM-1{050} busy" (no line ending). Returns the length;
 * the text is cut to n - 1 characters and always terminated. */
size_t APRS_EventLine(const aprs_event_t *e, char *out, size_t n);

#endif
