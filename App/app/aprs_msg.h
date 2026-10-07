/* Copyright 2024 UV-K5 Firmware Custom (ta1js APRS work)
 * Ported to the PY32F071 firmware by tigfox, 2026.
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

#ifndef APP_APRS_MSG_H
#define APP_APRS_MSG_H

#include <stdbool.h>
#include <stdint.h>
#include "app/aprs_settings.h"

#define APRS_MSG_TEXT_MAX  30u   // characters the operator may enter
#define APRS_MSG_MAX       34u   // text + "{nn" / "ack" + line number
#define APRS_ACK_SEQ_MAX    5u   // the spec allows up to 5 characters

// The next line number (1..99). A message that changed gets a new one; sending the same text
// again is an APRS retry and keeps its number, so the far end can drop the duplicate.
uint8_t APRS_MsgNextSeq(uint8_t seq, bool changed);

// ":TO       :text{nn" to the addressee `to` (up to 9 characters). seq 0: no line number.
// Returns the frame length (FCS excluded) or 0 if to or text is empty. out: APRS_BUILD_MAX bytes.
uint16_t APRS_BuildMessage(uint8_t *out, const aprs_settings_t *s, const char *to,
                           const char *text, uint8_t seq);
// ":TO       :ackNN" acknowledging line number `seq` (as received, up to APRS_ACK_SEQ_MAX chars).
uint16_t APRS_BuildAck(uint8_t *out, const aprs_settings_t *s, const char *to, const char *seq);

#endif
