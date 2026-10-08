/* Copyright 2024 UV-K5 Firmware Custom (ta1js APRS work), ported by tigfox 2026.
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


#ifndef APP_APRS_RXINFO_H
#define APP_APRS_RXINFO_H

#include <stdbool.h>
#include <stdint.h>
#include "app/aprs_msg.h"
#include "app/aprs_settings.h"

#define APRS_RXTEXT_MAX 43u

/* What the station makes of a received frame (hardware-free; the logic of ta1js APRS_ShowFrame). */
typedef struct {
    bool     own;                       /* sent by this station (heard back through a digipeater) */
    bool     to_me;                     /* a message addressed to our callsign-SSID */
    bool     is_ack;                    /* ... and it is an ack or a rej of something we sent */
    char     from[10];                  /* source "CALL-n" */
    char     text[APRS_RXTEXT_MAX + 1]; /* "W1ABC-7>hello" for a message, "W1ABC 12.3km" or "W1ABC:payload" */
    char     ack_seq[APRS_ACK_SEQ_MAX + 1]; /* line number to acknowledge, "" if none */
} aprs_rx_info_t;

/* frame includes its 2-byte FCS. Returns false for a frame too short to carry anything.
 * my_lat / my_lon (micro-degrees, both 0 = unknown) turn a position into a distance. */
bool APRS_RxInfo(const uint8_t *frame, uint16_t len, const aprs_settings_t *s,
                 int32_t my_lat, int32_t my_lon, aprs_rx_info_t *out);

#endif
