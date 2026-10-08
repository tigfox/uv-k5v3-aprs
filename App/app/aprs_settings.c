/* Copyright 2026 tigfox. Licensed under the Apache License, Version 2.0. */
#include "app/aprs_settings.h"
#include <string.h>

#define MAGIC0 'A'
#define MAGIC1 'P'
#define MAGIC2 'R'
#define MAGIC3 '1'

enum {
    OFF_MAGIC = 0, OFF_VERSION = 4, OFF_FLAGS = 5, OFF_DIGI = 6, OFF_SSID = 7,
    OFF_INTERVAL = 8, OFF_LEVEL = 10, OFF_TWIST = 11, OFF_CALL = 12, OFF_MSGTO = 20,
    OFF_LOC = 30, OFF_COMMENT = 46, OFF_CRC = 90, OFF_SPARE = 92,
};

#define DEFAULT_INTERVAL_S 600u
#define DEFAULT_TONE_LEVEL APRS_TONE_LEVEL_DEF

static uint16_t crc16(const uint8_t *p, unsigned n)   /* CCITT-FALSE */
{
    uint16_t crc = 0xFFFF;
    while (n--) {
        crc ^= (uint16_t)(*p++) << 8;
        for (int i = 0; i < 8; i++)
            crc = (crc & 0x8000) ? (uint16_t)((crc << 1) ^ 0x1021) : (uint16_t)(crc << 1);
    }
    return crc;
}

/* A NUL-terminated string within cap bytes whose chars satisfy ok(). */
static bool str_ok(const char *s, unsigned cap, unsigned max_len, unsigned min_len, bool (*ok)(char))
{
    const char *end = memchr(s, 0, cap);
    if (!end)
        return false;
    unsigned n = (unsigned)(end - s);
    if (n < min_len || n > max_len)
        return false;
    for (unsigned i = 0; i < n; i++)
        if (!ok(s[i]))
            return false;
    return true;
}

static bool is_call_char(char c)   { return (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9'); }
static bool is_msgto_char(char c)  { return is_call_char(c) || c == '-'; }
static bool is_digit_char(char c)  { return c >= '0' && c <= '9'; }
static bool is_print_char(char c)  { return (unsigned char)c >= 0x20 && (unsigned char)c <= 0x7E; }

/* Copy the string only, so bytes after the NUL never reach flash or the CRC. */
static void put_text(uint8_t *dst, const char *src, unsigned cap)
{
    for (unsigned i = 0; i < cap && src[i]; i++)
        dst[i] = (uint8_t)src[i];
}

aprs_settings_t APRS_SettingsDefaults(void)
{
    aprs_settings_t s;
    memset(&s, 0, sizeof s);
    memcpy(s.call, "N0CALL", 7);
    s.interval_s = DEFAULT_INTERVAL_S;
    s.digi_hops = 2;
    s.tone_level = DEFAULT_TONE_LEVEL;
    return s;
}

bool APRS_SettingsValid(const aprs_settings_t *s)
{
    if (!s)
        return false;
    return str_ok(s->call, sizeof s->call, APRS_CALL_MAX, 1, is_call_char)
        && str_ok(s->msgto, sizeof s->msgto, APRS_MSGTO_MAX, 0, is_msgto_char)
        && str_ok(s->loc, sizeof s->loc, APRS_LOC_MAX, 0, is_digit_char)
        && str_ok(s->comment, sizeof s->comment, APRS_COMMENT_MAX, 0, is_print_char)
        && s->ssid <= 15
        && (s->interval_s == 0 || s->interval_s >= APRS_INTERVAL_MIN_S)
        && s->aprs_on <= 1
        && s->digi_mode <= APRS_DIGI_WIDE
        && s->digi_hops >= 1 && s->digi_hops <= APRS_DIGI_HOPS_MAX
        && s->digi_delay < APRS_DIGI_DELAYS
        && s->beacon_type <= 1
        && s->tone_level >= APRS_TONE_LEVEL_MIN && s->tone_level <= APRS_TONE_LEVEL_MAX
        && s->tone_twist >= APRS_TWIST_MIN && s->tone_twist <= APRS_TWIST_MAX;
}

bool APRS_CallIsSet(const aprs_settings_t *s)
{
    if (!APRS_SettingsValid(s))
        return false;
    /* a real amateur callsign has at least 3 characters with a digit and a letter; the
     * placeholders N0CALL and NOCALL are refused */
    unsigned digits = 0, letters = 0;
    const size_t n = strlen(s->call);
    for (size_t i = 0; i < n; i++) {
        if (s->call[i] >= '0' && s->call[i] <= '9') digits++; else letters++;
    }
    return n >= 3 && digits > 0 && letters > 0 && strcmp(s->call, "N0CALL") != 0 && strcmp(s->call, "NOCALL") != 0;
}

uint8_t APRS_DigiByteEncode(const aprs_settings_t *s)
{
    return (uint8_t)((s->digi_mode & 3u) | ((s->digi_hops & 7u) << 2)
                     | ((s->digi_delay & 3u) << 5) | ((s->beacon_type & 1u) << 7));
}

bool APRS_DigiByteDecode(uint8_t b, aprs_settings_t *s)
{
    uint8_t mode = b & 3u, hops = (b >> 2) & 7u;
    if (mode > APRS_DIGI_WIDE || hops < 1)
        return false;
    s->digi_mode = mode;
    s->digi_hops = hops;
    s->digi_delay = (b >> 5) & 3u;
    s->beacon_type = (b >> 7) & 1u;
    return true;
}

bool APRS_SettingsEncode(const aprs_settings_t *s, uint8_t out[APRS_RECORD_SIZE])
{
    if (!APRS_SettingsValid(s))
        return false;
    uint8_t r[APRS_RECORD_SIZE];
    memset(r, 0, sizeof r);
    r[OFF_MAGIC] = MAGIC0; r[OFF_MAGIC + 1] = MAGIC1; r[OFF_MAGIC + 2] = MAGIC2; r[OFF_MAGIC + 3] = MAGIC3;
    r[OFF_VERSION] = APRS_RECORD_VERSION;
    r[OFF_FLAGS] = s->aprs_on;
    r[OFF_DIGI] = APRS_DigiByteEncode(s);
    r[OFF_SSID] = s->ssid;
    r[OFF_INTERVAL] = (uint8_t)(s->interval_s & 0xFF);
    r[OFF_INTERVAL + 1] = (uint8_t)(s->interval_s >> 8);
    r[OFF_LEVEL] = s->tone_level;
    r[OFF_TWIST] = (uint8_t)s->tone_twist;
    put_text(r + OFF_CALL, s->call, sizeof s->call);
    put_text(r + OFF_MSGTO, s->msgto, sizeof s->msgto);
    put_text(r + OFF_LOC, s->loc, sizeof s->loc);
    put_text(r + OFF_COMMENT, s->comment, sizeof s->comment);
    uint16_t crc = crc16(r, OFF_CRC);
    r[OFF_CRC] = (uint8_t)(crc & 0xFF);
    r[OFF_CRC + 1] = (uint8_t)(crc >> 8);
    memcpy(out, r, sizeof r);
    return true;
}

bool APRS_SettingsDecode(const uint8_t in[APRS_RECORD_SIZE], aprs_settings_t *out)
{
    aprs_settings_t s = APRS_SettingsDefaults();
    bool ok = in[OFF_MAGIC] == MAGIC0 && in[OFF_MAGIC + 1] == MAGIC1
           && in[OFF_MAGIC + 2] == MAGIC2 && in[OFF_MAGIC + 3] == MAGIC3
           && in[OFF_VERSION] == APRS_RECORD_VERSION;
    for (unsigned i = OFF_SPARE; ok && i < APRS_RECORD_SIZE; i++)
        ok = in[i] == 0;
    if (ok) {
        uint16_t crc = (uint16_t)(in[OFF_CRC] | (in[OFF_CRC + 1] << 8));
        ok = crc == crc16(in, OFF_CRC);
    }
    if (ok) {
        s.aprs_on = in[OFF_FLAGS];
        s.ssid = in[OFF_SSID];
        s.interval_s = (uint16_t)(in[OFF_INTERVAL] | (in[OFF_INTERVAL + 1] << 8));
        s.tone_level = in[OFF_LEVEL];
        s.tone_twist = (int8_t)in[OFF_TWIST];
        memcpy(s.call, in + OFF_CALL, sizeof s.call);
        memcpy(s.msgto, in + OFF_MSGTO, sizeof s.msgto);
        memcpy(s.loc, in + OFF_LOC, sizeof s.loc);
        memcpy(s.comment, in + OFF_COMMENT, sizeof s.comment);
        ok = APRS_DigiByteDecode(in[OFF_DIGI], &s) && APRS_SettingsValid(&s);
    }
    *out = ok ? s : APRS_SettingsDefaults();
    return ok;
}
