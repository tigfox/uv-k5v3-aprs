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


#include "app/aprs_msg.h"
#include "app/aprs_beacon.h"

uint8_t APRS_MsgNextSeq(uint8_t seq, bool changed)
{
    if (!changed && seq != 0)
        return seq;
    return (uint8_t)(seq >= 99 ? 1 : seq + 1);
}

static uint16_t build_addressed(uint8_t *out, const aprs_settings_t *s, const char *to,
                                const char *text, uint8_t text_len)
{
    uint16_t idx = APRS_BuildHeader(out, s, false);
    out[idx++] = ':';
    bool end = false;
    for (uint8_t i = 0; i < 9; i++) {
        if (!end && to[i] == 0)
            end = true;
        out[idx++] = end ? ' ' : (uint8_t)to[i];
    }
    out[idx++] = ':';
    for (uint8_t i = 0; i < text_len; i++)
        out[idx++] = (uint8_t)text[i];
    return idx;
}

uint16_t APRS_BuildMessage(uint8_t *out, const aprs_settings_t *s, const char *to,
                           const char *text, uint8_t seq)
{
    if (to[0] == 0 || text[0] == 0)
        return 0;
    char body[APRS_MSG_MAX + 1];
    uint8_t k = 0;
    for (; k < APRS_MSG_TEXT_MAX && text[k]; k++)
        body[k] = text[k];
    if (seq != 0) {
        body[k++] = '{';
        body[k++] = (char)('0' + (seq / 10) % 10);
        body[k++] = (char)('0' + seq % 10);
    }
    return build_addressed(out, s, to, body, k);
}

uint16_t APRS_BuildAck(uint8_t *out, const aprs_settings_t *s, const char *to, const char *seq)
{
    if (to[0] == 0)
        return 0;
    char body[3 + APRS_ACK_SEQ_MAX];
    body[0] = 'a'; body[1] = 'c'; body[2] = 'k';
    uint8_t k = 3;
    for (uint8_t i = 0; i < APRS_ACK_SEQ_MAX && seq[i]; i++)
        body[k++] = seq[i];
    return build_addressed(out, s, to, body, k);
}
