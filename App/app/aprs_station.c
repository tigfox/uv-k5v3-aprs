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

void APRS_StationInit(aprs_station_t *st, const aprs_settings_t *s)
{
    memset(st, 0, sizeof *st);
    copy_str(st->msgto, sizeof st->msgto, s->msgto);
    APRS_StationRearm(st, s);
}

void APRS_StationRearm(aprs_station_t *st, const aprs_settings_t *s)
{
    st->beacon_countdown = (s->aprs_on && s->interval_s > 0) ? APRS_BEACON_FIRST_TICKS : 0;
}

void APRS_StationClearPending(aprs_station_t *st)
{
    st->msg_pending = st->beacon_pending = st->ack_pending = false;
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

bool APRS_StationQueueMessage(aprs_station_t *st)
{
    if (st->msgto[0] == 0 || st->msg_text[0] == 0)
        return false;
    st->msg_pending = true;
    return true;
}

void APRS_StationQueueBeacon(aprs_station_t *st)
{
    st->beacon_pending = true;
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
            if (info->ack_seq[0]) {
                copy_str(st->ack_to, sizeof st->ack_to, info->from);
                copy_str(st->ack_seq, sizeof st->ack_seq, info->ack_seq);
                st->ack_pending = true;
            }
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
    if (!(st->ack_pending || st->msg_pending || st->beacon_pending)) {
        st->pending_age = 0;
        return APRS_ACT_NONE;
    }
    if (++st->pending_age > APRS_PENDING_MAX_SLOTS) {      // the channel never cleared: give up
        APRS_StationClearPending(st);
        return APRS_ACT_NONE;
    }
    if (st->quiet > 0)
        return APRS_ACT_NONE;                               // the minimum gap between transmissions
    if (st->ack_pending)    return APRS_ACT_ACK;
    if (st->msg_pending)    return APRS_ACT_MSG;
    return APRS_ACT_BEACON;
}

uint16_t APRS_StationBuild(aprs_station_t *st, const aprs_settings_t *s, aprs_action_t act, uint8_t *out)
{
    switch (act) {
    case APRS_ACT_ACK:
        return APRS_BuildAck(out, s, st->ack_to, st->ack_seq);
    case APRS_ACT_MSG:
        /* a changed message takes a new line number; sending the same text again is an APRS retry
         * and keeps its number, so the far end can drop the duplicate */
        return APRS_BuildMessage(out, s, st->msgto, st->msg_text, APRS_MsgNextSeq(st->seq, st->dirty));
    case APRS_ACT_BEACON:
        return APRS_BuildStationBeacon(out, s);
    default:
        return 0;
    }
}

void APRS_StationSent(aprs_station_t *st, const aprs_settings_t *s, aprs_action_t act)
{
    st->pending_age = 0;
    st->quiet = APRS_TX_MIN_GAP_SLOTS;
    switch (act) {
    case APRS_ACT_ACK:
        st->ack_pending = false;
        break;
    case APRS_ACT_MSG:
        st->seq = APRS_MsgNextSeq(st->seq, st->dirty);
        st->dirty = false;
        st->msg_pending = false;
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

void APRS_StationDrop(aprs_station_t *st, const aprs_settings_t *s, aprs_action_t act)
{
    switch (act) {
    case APRS_ACT_ACK:    st->ack_pending = false; break;
    case APRS_ACT_MSG:    st->msg_pending = false; break;
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
