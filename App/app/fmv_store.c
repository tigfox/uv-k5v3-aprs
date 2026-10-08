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


#include "app/fmv_store.h"
#include "driver/eeprom.h"
#include <string.h>

bool FMV_StoreLoad(uint16_t channel, uint32_t rx_freq, char *out)
{
    out[0] = 0;
    const uint16_t addr = FMV_InfoAddress(channel);
    if (addr == 0)
        return false;
    uint8_t rec[FMV_INFO_RECORD];
    EEPROM_ReadBuffer(addr, rec, sizeof rec);
    return FMV_InfoDecode(rec, rx_freq, out);
}

bool FMV_StoreSave(uint16_t channel, uint32_t rx_freq, const char *text, bool *cut)
{
    const uint16_t addr = FMV_InfoAddress(channel);
    if (addr == 0)
        return false;
    uint8_t rec[FMV_INFO_RECORD], have[FMV_INFO_RECORD];
    FMV_InfoEncode(rec, rx_freq, text, cut);        // an empty text leaves an erased record: that clears the slot
    EEPROM_ReadBuffer(addr, have, sizeof have);
    if (memcmp(rec, have, sizeof rec) != 0)         // never rewrite what is already there (flash wear)
        EEPROM_WriteBuffer(addr, rec, sizeof rec);
    return true;
}

bool FMV_StoreBankName(uint8_t list, char *out)
{
    out[0] = 0;
    if (list < 1 || list > FMV_BANKNAME_LISTS)
        return false;
    uint8_t raw[FMV_BANKNAME_MAX];
    EEPROM_ReadBuffer((uint16_t)(FMV_BANKNAME_ADDR + (list - 1u) * FMV_BANKNAME_MAX), raw, sizeof raw);
    return FMV_BankNameClean(raw, out);
}
