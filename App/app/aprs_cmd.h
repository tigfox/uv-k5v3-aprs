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


#ifndef APP_APRS_CMD_H
#define APP_APRS_CMD_H

#include <stdbool.h>
#include <stdint.h>
#include "app/aprs_reason.h"
#include "app/aprs_settings.h"

/* The APRS serial commands (USB-C serial port and the K-plug cable use the same framing as the radio's other
 * commands: obfuscated, CRC16). Hardware-free: this module parses and validates a command and builds the reply;
 * the radio side is behind aprs_cmd_ops_t, which the host tests stub.
 *
 * Every command that changes anything or transmits starts with the u32 session timestamp the host sent in its
 * 0x0514 handshake, as the radio's other write commands do (otherwise Ok = 0). Replies are 4 bytes of header
 * (id + 1, size) then the body; the first byte of the body is Ok (1) unless noted.
 *
 *   0x0700 msg      [ts u32][to 10][text 1..67]   -> 0x0701   queue a message (new line number); the text is the rest
 *                                                         of the data (older hosts send a 30-byte padded field), refused above 67.
 *                                                         Reply: Ok, line number (0 if refused), free queue places, reason code
 *   0x0712 capabilities (no session)              -> 0x0713   revision, feature bits, queue depth, ack depth, longest text
 *   0x0714 status       (no session)              -> 0x0715   battery, frequency, can it transmit and why not, counters
 *   0x0716 set call [ts u32][call 8][ssid u8]     -> 0x0717   the callsign (A-Z 0-9, 1-6, NUL padded) and SSID 0-15; the rest is kept; byte 7 = reason
 *   0x0702 beacon   [ts u32]                      -> 0x0703   queue the station beacon
 *   0x0706 on/off   [ts u32][on u8]               -> 0x0707
 *   0x0708 raw tx   [ts u32][AX.25 frame, no FCS] -> 0x0709   our own callsign as source only
 *   0x070A digi     [set u8][mode][hops][delay][bcn]([ts u32] when set) -> 0x070B (28 bytes, see below)
 *   0x070C setup get                              -> 0x070D   Ok + the 96-byte settings record
 *   0x070E setup set [ts u32][record 96]          -> 0x070F   validated in full, then saved
 *   0x0710 monitor  [on u8]                       -> 0x0711   APRSRAW: / APRS: / DIGI: lines on this port
 */

#define APRS_CMD_FIRST 0x0700u
#define APRS_CMD_LAST  0x0716u
#define APRS_CMD_REPLY_MAX 112u

/* Protocol revision and feature bits of the host API (claude/aprs-messaging-api.md). A host reads them with 0x0712. */
#define APRS_PROTOCOL_REVISION 1u
#define APRS_CAP_SEND_FIELDS   (1u << 0)   /* the send reply carries the line number, free places and a reason */
#define APRS_CAP_EVENTS        (1u << 1)   /* SENT: / DROP: monitor lines */
#define APRS_CAP_ACKQ          (1u << 2)   /* several acks can wait */
#define APRS_CAP_QUEUE         (1u << 3)   /* several messages can wait; a full queue refuses */
#define APRS_CAP_STATUS        (1u << 4)   /* the status command */
#define APRS_CAP_COUNTERS      (1u << 9)   /* frame counters in the status reply (bit 5 is the set callsign command, 6-8 the path, gap and signal strength) */
#define APRS_CAP_SET_CALL      (1u << 5)   /* the set callsign command */
#define APRS_CAP_PATH          (1u << 6)   /* the digipeater path setting for messages and acks */
#define APRS_CAP_GAP           (1u << 7)   /* the setting for the minimum gap between transmissions */
#define APRS_CAP_RSSI          (1u << 8)   /* APRSRSSI: lines */
#define APRS_CAPS_FEATURES     (APRS_CAP_SEND_FIELDS | APRS_CAP_EVENTS | APRS_CAP_ACKQ | APRS_CAP_QUEUE | APRS_CAP_STATUS | \
                                APRS_CAP_COUNTERS | APRS_CAP_SET_CALL | APRS_CAP_PATH | APRS_CAP_GAP | APRS_CAP_RSSI)

/* What the status command reports; the radio side fills it (aprs_task.c). */
typedef struct {
    uint16_t battery_mv;
    uint32_t freq_10hz;       /* the main VFO's receive frequency, 10 Hz units */
    uint16_t decoded;         /* good frames decoded since APRS was switched on (wraps) */
    uint16_t bad_fcs;         /* packet-shaped bursts that failed the checksum and were not rescued (wraps) */
    uint16_t sent;            /* packets transmitted by this station: messages, acks, beacons, digipeater repeats (wraps) */
    uint8_t  queue_free;      /* free places in the message queue */
    uint8_t  acks_pending;
    bool     would_tx;        /* it would transmit now */
    bool     busy;            /* the channel is busy (a packet arriving, or squelch open) */
    bool     in_band;         /* the TX frequency is inside the allowed bands */
    aprs_reason_t why;        /* why it would not transmit (APRS_R_OK if it would) */
} aprs_status_t;

typedef struct {
    bool     (*set_on)(bool on);
    aprs_reason_t (*queue_message)(const char *to, const char *text, uint8_t *seq);   /* queues, gives its line number */
    void     (*queue_beacon)(void);
    bool     (*queue_raw)(const uint8_t *frame, uint16_t len);        /* false if a raw frame is already waiting */
    bool     (*apply_settings)(const aprs_settings_t *s);             /* save, restart the receiver as needed */
    void     (*get_settings)(aprs_settings_t *s);
    void     (*get_digi_stats)(uint16_t stats[6], uint32_t *uptime_ticks);
    void     (*set_monitor)(uint32_t port, bool on);
    uint8_t  (*queue_free)(void);                                     /* free places in the message queue */
    void     (*get_status)(aprs_status_t *st);
    bool     (*monitoring)(uint32_t port);                            /* is this port receiving the monitor lines */
} aprs_cmd_ops_t;

/* Run command `id` (data: the bytes after the header; size: how many). port_ts is the timestamp this port
 * latched at its handshake. Fills reply and returns its length, or 0 if id is not an APRS command. */
uint16_t APRS_CmdRun(const aprs_cmd_ops_t *ops, uint32_t port, uint16_t id, const uint8_t *data, uint16_t size,
                     uint32_t port_ts, uint8_t *reply);

/* A raw frame (no FCS) we are willing to transmit: 17..150 bytes, an address block that ends, UI frame, and
 * the source callsign is ours (any SSID) -- a host may not send under somebody else's call. */
bool APRS_CmdRawFrameOk(const uint8_t *frame, uint16_t len, const aprs_settings_t *s);

#endif
