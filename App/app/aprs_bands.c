/* Copyright 2026 tigfox. Licensed under the Apache License, Version 2.0. */
#include "app/aprs_bands.h"

bool APRS_FreqAllowed(uint32_t freq)
{
    return (freq >= APRS_BAND_2M_LOW && freq < APRS_BAND_2M_HIGH)
        || (freq >= APRS_BAND_70CM_LOW && freq < APRS_BAND_70CM_HIGH);
}

uint32_t APRS_FreqClamp(uint32_t freq)
{
    return APRS_FreqAllowed(freq) ? freq : APRS_BAND_2M_LOW;
}
