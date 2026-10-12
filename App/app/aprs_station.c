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


#include "app/aprs_station.h"
#include "app/aprs_parse.h"
#include <string.h>

static void copy_str(char *dst, size_t cap, const char *src)
{
    size_t i = 0;
    for (; i + 1 < cap && src[i]; i++)
        dst[i] = src[i];
    dst[i] = 0;
}

static aprs_event_sink_t gSink;

void APRS_StationSetEventSink(aprs_event_sink_t sink)
{
    gSink = sink;
}

static void emit(aprs_event_kind_t kind, const char *to, const char *seq, aprs_reason_t reason)
{
    if (gSink == NULL)
        return;
    aprs_event_t e;
    memset(&e, 0, sizeof e);
    e.kind = kind;
    copy_str(e.to, sizeof e.to, to);
    copy_str(e.seq, sizeof e.seq, seq);
    e.reason = (uint8_t)reason;
    gSink(&e);
}

static void seq_text(char out[3], uint8_t seq)
{
    out[0] = (char)('0' + (seq / 10) % 10);
    out[1] = (char)('0' + seq % 10);
    out[2] = 0;
}

static aprs_qmsg_t *msg_head(aprs_station_t *st)
{
    return &st->q[st->q_head];
}

static void msg_pop(aprs_station_t *st, aprs_event_kind_t kind, aprs_reason_t reason)
{
    char num[3];
    seq_text(num, msg_head(st)->seq);
    emit(kind, msg_head(st)->to, num, reason);
    st->q_head = (uint8_t)((st->q_head + 1u) % APRS_MSGQ_DEPTH);
    st->q_count--;
}

static void ack_pop(aprs_station_t *st, bool report, aprs_reason_t reason)
{
    if (report)
        emit(APRS_EV_DROP_ACK, st->a[st->a_head].to, st->a[st->a_head].seq, reason);
    st->a_head = (uint8_t)((st->a_head + 1u) % APRS_ACKQ_DEPTH);
    st->a_count--;
}

void APRS_StationInit(aprs_station_t *st, const aprs_settings_t *s)
{
    memset(st, 0, sizeof *st);
    copy_str(st->msgto, sizeof st->msgto, s->msgto);
    APRS_StationRearm(st, s);
}

void APRS_StationRearm(aprs_station_t *st, const aprs_settings_t *s)
{
    st->beacon_countdown = (s->aprs_on && s->interval_s > 0) ? APRS_BEACON_FIRST_TICKS : 0;
    st->beacon_period = (s->aprs_on && s->interval_s > 0)
        ? (uint16_t)(s->interval_s * 2u > 0xFFFFu ? 0xFFFFu : s->interval_s * 2u) : 0;      /* 500 ms ticks */
}

void APRS_StationClearPending(aprs_station_t *st)
{
    while (st->q_count)
        msg_pop(st, APRS_EV_DROP_MSG, APRS_R_OFF);
    while (st->a_count)
        ack_pop(st, true, APRS_R_OFF);
    st->beacon_pending = false;
    st->raw_len = 0;
    st->pending_age = 0;
}

void APRS_StationSetMsgTo(aprs_station_t *st, const char *to)
{
    copy_str(st->msgto, sizeof st->msgto, to);
    st->dirty = true;
}

void APRS_StationSetMsgText(aprs_station_t *st, const char *text)
{
    copy_str(st->msg_text, sizeof st->msg_text, text);
    st->dirty = true;
}

aprs_reason_t APRS_StationEnqueue(aprs_station_t *st, const char *to, const char *text, bool new_number, uint8_t *seq_out)
{
    if (to[0] == 0 || text[0] == 0)
        return APRS_R_EMPTY;
    if (st->q_count >= APRS_MSGQ_DEPTH)
        return APRS_R_FULL;
    aprs_qmsg_t *m = &st->q[(st->q_head + st->q_count) % APRS_MSGQ_DEPTH];
    copy_str(m->to, sizeof m->to, to);
    copy_str(m->text, sizeof m->text, text);
    m->seq = APRS_MsgNextSeq(st->seq, new_number);
    st->seq = m->seq;
    st->q_count++;
    if (seq_out)
        *seq_out = m->seq;
    return APRS_R_OK;
}

uint8_t APRS_StationMsgFree(const aprs_station_t *st)
{
    return (uint8_t)(APRS_MSGQ_DEPTH - st->q_count);
}

bool APRS_StationQueueMessage(aprs_station_t *st)
{
    if (APRS_StationEnqueue(st, st->msgto, st->msg_text, st->dirty, NULL) != APRS_R_OK)
        return false;
    st->dirty = false;
    return true;
}

aprs_reason_t APRS_StationQueueAck(aprs_station_t *st, const char *to, const char *seq)
{
    for (uint8_t i = 0; i < st->a_count; i++) {
        const aprs_qack_t *a = &st->a[(st->a_head + i) % APRS_ACKQ_DEPTH];
        if (strcmp(a->to, to) == 0 && strcmp(a->seq, seq) == 0)
            return APRS_R_OK;                                  /* a retry heard before our ack went out: one ack is enough */
    }
    if (st->a_count >= APRS_ACKQ_DEPTH) {
        emit(APRS_EV_DROP_ACK, to, seq, APRS_R_FULL);
        return APRS_R_FULL;
    }
    aprs_qack_t *a = &st->a[(st->a_head + st->a_count) % APRS_ACKQ_DEPTH];
    copy_str(a->to, sizeof a->to, to);
    copy_str(a->seq, sizeof a->seq, seq);
    st->a_count++;
    return APRS_R_OK;
}

uint8_t APRS_StationAcksPending(const aprs_station_t *st)
{
    return st->a_count;
}

void APRS_StationQueueBeacon(aprs_station_t *st)
{
    st->beacon_pending = true;
}

bool APRS_StationQueueRaw(aprs_station_t *st, const uint8_t *frame, uint16_t len)
{
    if (st->raw_len != 0 || len < 17u || len > APRS_RAWTX_MAX)
        return false;
    memcpy(st->raw, frame, len);
    st->raw_len = len;
    return true;
}

bool APRS_StationOnFrame(aprs_station_t *st, const aprs_settings_t *s, const uint8_t *frame, uint16_t len,
                         aprs_rx_info_t *info)
{
    int32_t lat = 0, lon = 0;
    if (s->loc[0])
        APRS_LocDecode(s->loc, &lat, &lon);
    if (!APRS_RxInfo(frame, len, s, lat, lon, info) || info->own)
        return false;
    if (info->to_me) {
        st->heard_msgs++;
        if (!info->is_ack) {
            /* Reply target: the sender's full callsign-SSID, so the operator only types the text
             * (a wrong SSID is the usual reason the far station ignores a message). */
            copy_str(st->msgto, sizeof st->msgto, info->from);
            st->dirty = true;
            copy_str(st->last_msg, sizeof st->last_msg, info->text);
            if (info->ack_seq[0])
                APRS_StationQueueAck(st, info->from, info->ack_seq);
        } else {
            copy_str(st->last_msg, sizeof st->last_msg, info->text);
        }
    }
    return true;
}

aprs_action_t APRS_StationTick(aprs_station_t *st)
{
    if (st->quiet > 0)
        st->quiet--;
    if (st->beacon_countdown > 0 && --st->beacon_countdown == 0) {
        st->beacon_pending = true;
        /* the interval is re-armed when the beacon goes out (APRS_StationSent) */
    }
    const aprs_action_t next = st->a_count ? APRS_ACT_ACK : st->raw_len ? APRS_ACT_RAW
                             : st->q_count ? APRS_ACT_MSG : st->beacon_pending ? APRS_ACT_BEACON : APRS_ACT_NONE;
    if (next == APRS_ACT_NONE) {
        st->pending_age = 0;
        return APRS_ACT_NONE;
    }
    if (++st->pending_age > APRS_PENDING_MAX_SLOTS) {      // the channel never cleared: give up on this one, the next gets its own minute
        if (next == APRS_ACT_BEACON) {                      // forgotten, and the timer runs on so automatic beacons continue
            st->beacon_pending = false;
            st->beacon_countdown = st->beacon_period;
            st->pending_age = 0;
        } else {
            APRS_StationDrop(st, NULL, next, APRS_R_BUSY);
        }
        return APRS_ACT_NONE;
    }
    if (st->quiet > 0)
        return APRS_ACT_NONE;                               // the minimum gap between transmissions
    return next;
}

uint16_t APRS_StationBuild(aprs_station_t *st, const aprs_settings_t *s, aprs_action_t act, uint8_t *out)
{
    switch (act) {
    case APRS_ACT_ACK:
        return APRS_BuildAck(out, s, st->a[st->a_head].to, st->a[st->a_head].seq);
    case APRS_ACT_RAW:
        memcpy(out, st->raw, st->raw_len);
        return st->raw_len;
    case APRS_ACT_MSG:
        /* the line number was given when it was queued */
        return APRS_BuildMessage(out, s, msg_head(st)->to, msg_head(st)->text, msg_head(st)->seq);
    case APRS_ACT_BEACON:
        return APRS_BuildStationBeacon(out, s);
    default:
        return 0;
    }
}

void APRS_StationSent(aprs_station_t *st, const aprs_settings_t *s, aprs_action_t act)
{
    st->pending_age = 0;
    st->quiet = APRS_GapSlots(s);
    switch (act) {
    case APRS_ACT_ACK:
        if (st->a_count)
            ack_pop(st, false, APRS_R_OK);
        break;
    case APRS_ACT_RAW:
        st->raw_len = 0;
        break;
    case APRS_ACT_MSG:
        if (st->q_count)
            msg_pop(st, APRS_EV_SENT, APRS_R_OK);
        st->sent_msgs++;
        break;
    case APRS_ACT_BEACON:
        st->beacon_pending = false;
        st->sent_beacons++;
        st->beacon_countdown = (s->aprs_on && s->interval_s > 0)
            ? (uint16_t)(s->interval_s * 2u > 0xFFFFu ? 0xFFFFu : s->interval_s * 2u) : 0;   /* 500 ms ticks */
        break;
    default:
        break;
    }
}

void APRS_StationDrop(aprs_station_t *st, const aprs_settings_t *s, aprs_action_t act, aprs_reason_t reason)
{
    st->pending_age = 0;
    switch (act) {
    case APRS_ACT_ACK:    if (st->a_count) ack_pop(st, true, reason); break;
    case APRS_ACT_RAW:    st->raw_len = 0; break;
    case APRS_ACT_MSG:    if (st->q_count) msg_pop(st, APRS_EV_DROP_MSG, reason); break;
    case APRS_ACT_BEACON:                          /* e.g. no Loc yet: try again next interval */
        st->beacon_pending = false;
        APRS_StationSent(st, s, APRS_ACT_BEACON);
        st->sent_beacons--;
        break;
    default: break;
    }
}

unsigned APRS_StationMsgPages(const aprs_station_t *st)
{
    const unsigned n = (unsigned)strlen(st->last_msg);
    return n == 0 ? 0 : (n + APRS_RDMSG_PAGE - 1u) / APRS_RDMSG_PAGE;
}
