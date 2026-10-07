/* US amateur 2 m / 70 cm band windows (APRS build). Frequencies in 10 Hz units.
 * Copyright 2026 tigfox. Licensed under the Apache License, Version 2.0.
 */
#ifndef APP_APRS_BANDS_H
#define APP_APRS_BANDS_H

#include <stdbool.h>
#include <stdint.h>

#define APRS_BAND_2M_LOW     14400000u   /* 144.000 MHz */
#define APRS_BAND_2M_HIGH    14800000u   /* 148.000 MHz, exclusive */
#define APRS_BAND_70CM_LOW   42000000u   /* 420.000 MHz */
#define APRS_BAND_70CM_HIGH  45000000u   /* 450.000 MHz, exclusive */

bool     APRS_FreqAllowed(uint32_t freq);
/* Allowed frequencies pass through; others map to the 2 m lower edge. */
uint32_t APRS_FreqClamp(uint32_t freq);

#endif
