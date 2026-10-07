/* Copyright 2026 tigfox. Licensed under the Apache License, Version 2.0. */
#include "app/aprs_store.h"
#include "driver/eeprom.h"
#include <string.h>

aprs_settings_t gAprsSettings;

bool APRS_StoreInit(void)
{
    uint8_t buf[APRS_RECORD_SIZE];
    EEPROM_ReadBuffer(APRS_EEPROM_ADDR, buf, sizeof buf);
    return APRS_SettingsDecode(buf, &gAprsSettings);
}

bool APRS_StoreSave(const aprs_settings_t *s)
{
    uint8_t want[APRS_RECORD_SIZE], have[APRS_RECORD_SIZE];
    if (!APRS_SettingsEncode(s, want))
        return false;
    EEPROM_ReadBuffer(APRS_EEPROM_ADDR, have, sizeof have);
    if (memcmp(want, have, sizeof want) != 0) {
        EEPROM_WriteBuffer(APRS_EEPROM_ADDR, want, sizeof want);
        EEPROM_ReadBuffer(APRS_EEPROM_ADDR, have, sizeof have);
        if (memcmp(want, have, sizeof want) != 0)
            return false;
    }
    gAprsSettings = *s;
    return true;
}
