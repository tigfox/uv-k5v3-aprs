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


#include "app/rpt_store.h"
#include "driver/eeprom.h"
#include <string.h>

bool RPT_StoreLoad(uint16_t channel, uint32_t rx_freq, char *out)
{
    out[0] = 0;
    const uint16_t addr = RPT_InfoAddress(channel);
    if (addr == 0)
        return false;
    uint8_t rec[RPT_INFO_RECORD];
    EEPROM_ReadBuffer(addr, rec, sizeof rec);
    return RPT_InfoDecode(rec, rx_freq, out);
}

bool RPT_StoreSave(uint16_t channel, uint32_t rx_freq, const char *text, bool *cut)
{
    const uint16_t addr = RPT_InfoAddress(channel);
    if (addr == 0)
        return false;
    uint8_t rec[RPT_INFO_RECORD], have[RPT_INFO_RECORD];
    RPT_InfoEncode(rec, rx_freq, text, cut);        // an empty text leaves an erased record: that clears the slot
    EEPROM_ReadBuffer(addr, have, sizeof have);
    if (memcmp(rec, have, sizeof rec) != 0)         // never rewrite what is already there (flash wear)
        EEPROM_WriteBuffer(addr, rec, sizeof rec);
    return true;
}
