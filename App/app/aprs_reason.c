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


#include "app/aprs_reason.h"
#include <string.h>

static const char *const TEXT[] = {
    "", "full", "session", "empty", "toolong", "busy", "nocall", "aprsoff", "band", "battery", "mode", "replaced", "off"
};

const char *APRS_ReasonText(uint8_t reason)
{
    return reason < sizeof TEXT / sizeof TEXT[0] ? TEXT[reason] : "";
}

typedef struct { char *p; size_t left; size_t total; } sink_t;

static void put(sink_t *k, const char *s)
{
    for (; *s; s++) {
        if (k->left > 1) {
            *k->p++ = *s;
            k->left--;
        }
        k->total++;
    }
}

size_t APRS_EventLine(const aprs_event_t *e, char *out, size_t n)
{
    if (n == 0)
        return 0;
    sink_t k = { out, n, 0 };
    put(&k, e->kind == APRS_EV_SENT ? "SENT:" : e->kind == APRS_EV_DROP_ACK ? "DROP:ack " : "DROP:");
    put(&k, e->to);
    put(&k, "{");
    put(&k, e->seq);
    put(&k, "}");
    if (e->kind != APRS_EV_SENT) {
        const char *why = APRS_ReasonText(e->reason);
        if (why[0]) {
            put(&k, " ");
            put(&k, why);
        }
    }
    *k.p = '\0';
    return k.total < n ? k.total : n - 1u;
}
