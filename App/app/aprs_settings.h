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

/* The digipeater path of messages and acks (the beacon has its own rule). 0 is the default and what every earlier firmware sent. */
enum { APRS_PATH_WIDE12, APRS_PATH_WIDE1, APRS_PATH_DIRECT };   /* WIDE1-1,WIDE2-1 / WIDE1-1 / no digipeater */

/* The shortest time between two of our transmissions, in half-seconds: 0 = the default (5 s), else 2 s (4) to 10 s (20). */
#define APRS_GAP_DEFAULT_SLOTS 10u
#define APRS_GAP_MIN_SLOTS      4u
#define APRS_GAP_MAX_SLOTS     20u

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
    uint8_t  msg_path;       /* APRS_PATH_*: record byte 92 */
    uint8_t  tx_gap;         /* 0, or 4..20 half-seconds: record byte 93 */
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
/* The minimum gap between our transmissions in 500 ms slots (the default when tx_gap is 0). */
uint8_t APRS_GapSlots(const aprs_settings_t *s);
bool    APRS_DigiByteDecode(uint8_t b, aprs_settings_t *s);   /* false: s untouched */

#endif
