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
#include "app/aprs_settings.h"

/* The APRS serial commands (USB-C serial port and the K-plug cable use the same framing as the radio's other
 * commands: obfuscated, CRC16). Hardware-free: this module parses and validates a command and builds the reply;
 * the radio side is behind aprs_cmd_ops_t, which the host tests stub.
 *
 * Every command that changes anything or transmits starts with the u32 session timestamp the host sent in its
 * 0x0514 handshake, as the radio's other write commands do (otherwise Ok = 0). Replies are 4 bytes of header
 * (id + 1, size) then the body; the first byte of the body is Ok (1) unless noted.
 *
 *   0x0700 msg      [ts u32][to 10][text 30]      -> 0x0701   queue a message (new line number)
 *   0x0702 beacon   [ts u32]                      -> 0x0703   queue the station beacon
 *   0x0706 on/off   [ts u32][on u8]               -> 0x0707
 *   0x0708 raw tx   [ts u32][AX.25 frame, no FCS] -> 0x0709   our own callsign as source only
 *   0x070A digi     [set u8][mode][hops][delay][bcn]([ts u32] when set) -> 0x070B (28 bytes, see below)
 *   0x070C setup get                              -> 0x070D   Ok + the 96-byte settings record
 *   0x070E setup set [ts u32][record 96]          -> 0x070F   validated in full, then saved
 *   0x0710 monitor  [on u8]                       -> 0x0711   APRSRAW: / APRS: / DIGI: lines on this port
 */

#define APRS_CMD_FIRST 0x0700u
#define APRS_CMD_LAST  0x0710u
#define APRS_CMD_REPLY_MAX 112u

typedef struct {
    bool     (*set_on)(bool on);
    bool     (*queue_message)(const char *to, const char *text);      /* sets target and text, queues; false if empty */
    void     (*queue_beacon)(void);
    bool     (*queue_raw)(const uint8_t *frame, uint16_t len);        /* false if a raw frame is already waiting */
    bool     (*apply_settings)(const aprs_settings_t *s);             /* save, restart the receiver as needed */
    void     (*get_settings)(aprs_settings_t *s);
    void     (*get_digi_stats)(uint16_t stats[6], uint32_t *uptime_ticks);
    void     (*set_monitor)(uint32_t port, bool on);
} aprs_cmd_ops_t;

/* Run command `id` (data: the bytes after the header; size: how many). port_ts is the timestamp this port
 * latched at its handshake. Fills reply and returns its length, or 0 if id is not an APRS command. */
uint16_t APRS_CmdRun(const aprs_cmd_ops_t *ops, uint32_t port, uint16_t id, const uint8_t *data, uint16_t size,
                     uint32_t port_ts, uint8_t *reply);

/* A raw frame (no FCS) we are willing to transmit: 17..150 bytes, an address block that ends, UI frame, and
 * the source callsign is ours (any SSID) -- a host may not send under somebody else's call. */
bool APRS_CmdRawFrameOk(const uint8_t *frame, uint16_t len, const aprs_settings_t *s);

#endif
