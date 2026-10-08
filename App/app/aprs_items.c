/* Copyright 2026 tigfox. Arrow text entry after the ta1js design.
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


#include "app/aprs_items.h"

/* Beacon interval choices (seconds); 0 = no automatic beacon. */
static const uint16_t kInterval[] = { 0, 60, 120, 300, 600, 900, 1800, 3600 };
#define N_INTERVAL (sizeof kInterval / sizeof kInterval[0])

bool APRS_ItemIsChoice(unsigned item)
{
    switch (item) {
    case APRS_MI_APRS: case APRS_MI_DIGI: case APRS_MI_DHOPS: case APRS_MI_DDLY:
    case APRS_MI_BCNTY: case APRS_MI_INTV: case APRS_MI_SSID:
        return true;
    default:
        return false;
    }
}

int32_t APRS_ItemMin(unsigned item)
{
    return item == APRS_MI_DHOPS ? 1 : 0;
}

int32_t APRS_ItemMax(unsigned item)
{
    switch (item) {
    case APRS_MI_APRS: case APRS_MI_BCNTY: return 1;
    case APRS_MI_DIGI:  return APRS_DIGI_WIDE;
    case APRS_MI_DHOPS: return APRS_DIGI_HOPS_MAX;
    case APRS_MI_DDLY:  return APRS_DIGI_DELAYS - 1;
    case APRS_MI_INTV:  return N_INTERVAL - 1;
    case APRS_MI_SSID:  return 15;
    default:            return 0;
    }
}

int32_t APRS_ItemGet(const aprs_settings_t *s, unsigned item)
{
    switch (item) {
    case APRS_MI_APRS:  return s->aprs_on;
    case APRS_MI_DIGI:  return s->digi_mode;
    case APRS_MI_DHOPS: return s->digi_hops;
    case APRS_MI_DDLY:  return s->digi_delay;
    case APRS_MI_BCNTY: return s->beacon_type;
    case APRS_MI_SSID:  return s->ssid;
    case APRS_MI_INTV: {
        int32_t best = 0;                      // the nearest choice, for a value set by cable
        for (unsigned i = 0; i < N_INTERVAL; i++)
            if (kInterval[i] <= s->interval_s)
                best = (int32_t)i;
        return best;
    }
    default:            return 0;
    }
}

aprs_settings_t APRS_ItemSet(const aprs_settings_t *s, unsigned item, int32_t sel)
{
    aprs_settings_t n = *s;
    if (!APRS_ItemIsChoice(item))
        return n;
    if (sel < APRS_ItemMin(item)) sel = APRS_ItemMin(item);
    if (sel > APRS_ItemMax(item)) sel = APRS_ItemMax(item);
    switch (item) {
    case APRS_MI_APRS:  n.aprs_on = (uint8_t)sel; break;
    case APRS_MI_DIGI:  n.digi_mode = (uint8_t)sel; break;
    case APRS_MI_DHOPS: n.digi_hops = (uint8_t)sel; break;
    case APRS_MI_DDLY:  n.digi_delay = (uint8_t)sel; break;
    case APRS_MI_BCNTY: n.beacon_type = (uint8_t)sel; break;
    case APRS_MI_SSID:  n.ssid = (uint8_t)sel; break;
    case APRS_MI_INTV:  n.interval_s = kInterval[sel]; break;
    default: break;
    }
    return n;
}
