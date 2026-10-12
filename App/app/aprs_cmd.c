/* Copyright 2026 tigfox. Command numbers and the digi reply layout follow ta1js (utils/aprs_pc.py).
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


#include "app/aprs_cmd.h"
#include "app/aprs_msg.h"
#include "app/aprs_station.h"
#include "app/aprs_ax25.h"
#include "app/aprs_digi.h"
#include <string.h>

static uint32_t rd32(const uint8_t *p)
{
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static void wr16(uint8_t *p, uint16_t v) { p[0] = (uint8_t)v; p[1] = (uint8_t)(v >> 8); }
static void wr32(uint8_t *p, uint32_t v) { wr16(p, (uint16_t)v); wr16(p + 2, (uint16_t)(v >> 16)); }

/* 4-byte header, Ok, 3 padding */
static uint16_t ack(uint8_t *reply, uint16_t id, bool ok)
{
    wr16(reply, id);
    wr16(reply + 2, 4);
    reply[4] = ok ? 1u : 0u;
    reply[5] = reply[6] = reply[7] = 0;
    return 8;
}

/* the reply to a send: [Ok][line number][free places][reason] */
static uint16_t msg_reply(uint8_t *reply, const aprs_cmd_ops_t *ops, aprs_reason_t reason, uint8_t seq)
{
    wr16(reply, 0x0701);
    wr16(reply + 2, 4);
    reply[4] = reason == APRS_R_OK ? 1u : 0u;
    reply[5] = reason == APRS_R_OK ? seq : 0u;
    reply[6] = ops->queue_free();
    reply[7] = (uint8_t)reason;
    return 8;
}

static bool authed(const uint8_t *data, uint16_t size, uint32_t port_ts)
{
    return size >= 4u && port_ts != 0u && rd32(data) == port_ts;
}

bool APRS_CmdRawFrameOk(const uint8_t *f, uint16_t len, const aprs_settings_t *s)
{
    if (len < 17u || len > APRS_RAWTX_MAX)
        return false;
    uint16_t a = 13;                        /* SSID byte of the last address: bit 0 set */
    while (!(f[a] & 1u)) {
        a = (uint16_t)(a + 7u);
        if (a + 3u > len)
            return false;                   /* the address block never ends */
    }
    if (a + 3u > len || f[a + 1] != 0x03u || f[a + 2] != 0xF0u)
        return false;                       /* not an APRS UI frame */
    uint8_t me[7];
    AX25_EncodeAddress(s->call, s->ssid, false, me);
    return APRS_CallIsSet(s) && memcmp(&f[7], me, 6) == 0;
}

static uint16_t run_digi(const aprs_cmd_ops_t *ops, uint32_t port_ts, const uint8_t *d, uint16_t size, uint8_t *reply)
{
    aprs_settings_t s;
    ops->get_settings(&s);
    bool ok = true;
    if (size >= 5u && d[0]) {               /* a short command is a status request: never apply stale bytes */
        ok = size >= 9u && authed(d + 5, size - 5u, port_ts)
          && d[1] <= APRS_DIGI_WIDE && d[2] >= 1u && d[2] <= APRS_DIGI_HOPS_MAX && d[3] < APRS_DIGI_DELAYS && d[4] <= 1u;
        if (ok) {
            aprs_settings_t n = s;
            n.digi_mode = d[1]; n.digi_hops = d[2]; n.digi_delay = d[3]; n.beacon_type = d[4];
            ok = ops->apply_settings(&n);
            ops->get_settings(&s);
        }
    }
    uint16_t stats[6];
    uint32_t uptime;
    ops->get_digi_stats(stats, &uptime);
    memset(reply, 0, 28);
    wr16(reply, 0x070B);
    wr16(reply + 2, 24);
    reply[4] = ok ? 1u : 0u;
    reply[5] = s.digi_mode; reply[6] = s.digi_hops; reply[7] = s.digi_delay; reply[8] = s.beacon_type;
    wr32(reply + 12, uptime);
    for (unsigned i = 0; i < 6; i++)
        wr16(reply + 16 + 2 * i, stats[i]);
    return 28;
}

uint16_t APRS_CmdRun(const aprs_cmd_ops_t *ops, uint32_t port, uint16_t id, const uint8_t *d, uint16_t size,
                     uint32_t port_ts, uint8_t *reply)
{
    aprs_settings_t s;
    switch (id) {
    case 0x0700: {
        char to[10], text[APRS_MSG_TEXT_MAX + 1];
        const uint16_t text_len = size > 14u ? (uint16_t)(size - 14u) : 0u;    // whatever follows the destination
        uint8_t seq = 0;
        aprs_reason_t why = APRS_R_OK;
        if (!authed(d, size, port_ts))
            why = APRS_R_SESSION;
        else if (text_len == 0u)
            why = APRS_R_EMPTY;
        else if (text_len > APRS_MSG_TEXT_MAX)
            why = APRS_R_TOOLONG;                                              // refused, never cut
        else {
            memcpy(to, d + 4, 9);   to[9] = 0;
            memcpy(text, d + 14, text_len); text[text_len] = 0;    // a NUL inside ends the text early
            why = ops->queue_message(to, text, &seq);
        }
        return msg_reply(reply, ops, why, seq);
    }
    case 0x0702: {
        const bool ok = authed(d, size, port_ts);
        if (ok)
            ops->queue_beacon();
        return ack(reply, 0x0703, ok);
    }
    case 0x0706: {
        const bool ok = size >= 5u && authed(d, size, port_ts) && ops->set_on(d[4] != 0);
        return ack(reply, 0x0707, ok);
    }
    case 0x0708: {
        bool ok = size > 4u && authed(d, size, port_ts);
        if (ok) {
            ops->get_settings(&s);
            ok = APRS_CmdRawFrameOk(d + 4, (uint16_t)(size - 4u), &s) && ops->queue_raw(d + 4, (uint16_t)(size - 4u));
        }
        return ack(reply, 0x0709, ok);
    }
    case 0x070A:
        return run_digi(ops, port_ts, d, size, reply);
    case 0x070C: {
        ops->get_settings(&s);
        wr16(reply, 0x070D);
        wr16(reply + 2, 4 + APRS_RECORD_SIZE);
        const bool ok = APRS_SettingsEncode(&s, reply + 8);
        reply[4] = ok ? 1u : 0u;
        reply[5] = reply[6] = reply[7] = 0;
        return 8 + APRS_RECORD_SIZE;
    }
    case 0x070E: {
        bool ok = size >= 4u + APRS_RECORD_SIZE && authed(d, size, port_ts);
        if (ok) {
            aprs_settings_t n;
            ok = APRS_SettingsDecode(d + 4, &n) && ops->apply_settings(&n);   /* all of it or nothing */
        }
        return ack(reply, 0x070F, ok);
    }
    case 0x0712: {                  // what this firmware can do: no session, read-only
        wr16(reply, 0x0713);
        wr16(reply + 2, 12);
        reply[4] = 1;
        reply[5] = APRS_PROTOCOL_REVISION;
        reply[6] = reply[7] = 0;
        wr32(reply + 8, APRS_CAPS_FEATURES);
        reply[12] = APRS_MSGQ_DEPTH;
        reply[13] = APRS_ACKQ_DEPTH;
        reply[14] = APRS_MSG_TEXT_MAX;
        reply[15] = 0;
        return 16;
    }
    case 0x0714: {                  // the radio's state: no session, read-only
        aprs_status_t st;
        ops->get_status(&st);
        wr16(reply, 0x0715);
        wr16(reply + 2, 20);
        reply[4] = 1;
        reply[5] = (uint8_t)((ops->monitoring(port) ? 1u : 0u) | (st.would_tx ? 2u : 0u) | (st.busy ? 4u : 0u)
                             | (st.in_band ? 8u : 0u));
        reply[6] = (uint8_t)st.why;
        reply[7] = 0;
        wr16(reply + 8, st.battery_mv);
        wr16(reply + 10, st.decoded);
        wr32(reply + 12, st.freq_10hz);
        wr16(reply + 16, st.bad_fcs);
        wr16(reply + 18, st.sent);
        reply[20] = st.queue_free;
        reply[21] = st.acks_pending;
        reply[22] = reply[23] = 0;
        return 24;
    }
    case 0x0716: {                  // the callsign and SSID alone, so a host need not round-trip the whole record
        aprs_reason_t why = APRS_R_OK;
        aprs_settings_t s;
        if (!authed(d, size, port_ts)) {
            why = APRS_R_SESSION;
        } else if (size != 4u + 8u + 1u) {
            why = APRS_R_INVALID;
        } else {
            ops->get_settings(&s);
            memset(s.call, 0, sizeof s.call);
            memcpy(s.call, d + 4, APRS_CALL_MAX);                   // six characters; the two bytes after them must be padding
            const bool padded = d[4 + APRS_CALL_MAX] == 0 && d[4 + APRS_CALL_MAX + 1] == 0;
            s.ssid = d[12];
            if (!padded || !APRS_SettingsValid(&s) || s.call[0] == 0) {
                why = APRS_R_INVALID;
            } else {
                aprs_settings_t cur;
                ops->get_settings(&cur);
                if (strcmp(cur.call, s.call) != 0 || cur.ssid != s.ssid)       // save only what changes: flash wears
                    why = ops->apply_settings(&s) ? APRS_R_OK : APRS_R_STORE;
            }
        }
        const uint16_t n = ack(reply, 0x0717, why == APRS_R_OK);
        reply[7] = (uint8_t)why;
        return n;
    }
    case 0x0710:
        if (size >= 1u)
            ops->set_monitor(port, d[0] != 0);
        return ack(reply, 0x0711, size >= 1u);
    default:
        return 0;
    }
}
