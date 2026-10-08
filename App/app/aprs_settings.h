/* APRS station settings record: defaults, validation and (de)serialisation.
 * Hardware-free so it can be unit-tested on the host (tools/aprs).
 *
 * Copyright 2026 tigfox. Licensed under the Apache License, Version 2.0.
 */
#ifndef APP_APRS_SETTINGS_H
#define APP_APRS_SETTINGS_H

#include <stdbool.h>
#include <stdint.h>

#define APRS_RECORD_SIZE      96u
#define APRS_RECORD_VERSION   1u
#define APRS_CALL_MAX         6u
#define APRS_MSGTO_MAX        9u
#define APRS_LOC_MAX          15u
#define APRS_COMMENT_MAX      43u
#define APRS_INTERVAL_MIN_S   60u
#define APRS_DIGI_HOPS_MAX    7u
#define APRS_DIGI_DELAYS      4u
#define APRS_TONE_LEVEL_MIN   10u    /* REG_70 tone gain; see aprs_modem.h */
#define APRS_TONE_LEVEL_MAX   127u
#define APRS_TONE_LEVEL_DEF   66u
#define APRS_TWIST_MIN        (-4)
#define APRS_TWIST_MAX        8

enum { APRS_DIGI_OFF, APRS_DIGI_FILL, APRS_DIGI_WIDE };   /* same values as DIGI_* in aprs_digi.h */

/* Fields are ordered so the struct has no padding (memcmp-safe). */
typedef struct {
    uint16_t interval_s;     /* auto-beacon interval, 0 = off */
    uint8_t  ssid;           /* 0..15 */
    uint8_t  aprs_on;
    uint8_t  digi_mode;      /* APRS_DIGI_* */
    uint8_t  digi_hops;      /* 1..7 */
    uint8_t  digi_delay;     /* 0..3 */
    uint8_t  beacon_type;    /* 0 mobile, 1 digi */
    uint8_t  tone_level;     /* 10..127 */
    int8_t   tone_twist;     /* -4..8: 2200 Hz gain = level x (8 + twist) / 8 */
    char     call[APRS_CALL_MAX + 1];
    char     msgto[APRS_MSGTO_MAX + 1];
    char     loc[APRS_LOC_MAX + 1];
    char     comment[APRS_COMMENT_MAX + 1];
} aprs_settings_t;

aprs_settings_t APRS_SettingsDefaults(void);
bool APRS_SettingsValid(const aprs_settings_t *s);
bool APRS_CallIsSet(const aprs_settings_t *s);   /* valid and plausible: >= 3 characters, a digit and a letter, not N0CALL / NOCALL */

/* Encode fails (returns false, out untouched) if s is invalid. */
bool APRS_SettingsEncode(const aprs_settings_t *s, uint8_t out[APRS_RECORD_SIZE]);
/* Decode fills *out with the record, or with defaults and returns false when
 * the record is blank, corrupt or unknown. */
bool APRS_SettingsDecode(const uint8_t in[APRS_RECORD_SIZE], aprs_settings_t *out);

/* digi byte: bits 0-1 mode, 2-4 hops, 5-6 delay, 7 beacon type (as ta1js). */
uint8_t APRS_DigiByteEncode(const aprs_settings_t *s);
bool    APRS_DigiByteDecode(uint8_t b, aprs_settings_t *s);   /* false: s untouched */

#endif
