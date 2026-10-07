/* Persistent storage of the APRS settings record (SPI-flash emulated EEPROM).
 * Copyright 2026 tigfox. Licensed under the Apache License, Version 2.0.
 */
#ifndef APP_APRS_STORE_H
#define APP_APRS_STORE_H

#include <stdbool.h>
#include "app/aprs_settings.h"

#define APRS_EEPROM_ADDR 0x00D000u   /* EEPROM address; driver/eeprom_compat.c maps it to SPI 0x012000 */

extern aprs_settings_t gAprsSettings;

/* Load the record into gAprsSettings (defaults if blank or corrupt).
 * Returns true if a valid record was found. Never writes. */
bool APRS_StoreInit(void);
/* Validate, then write a changed record. Returns false if invalid or the
 * read-back differs. Skips the (slow, wearing) flash write if unchanged. */
bool APRS_StoreSave(const aprs_settings_t *s);

#endif
