/* Host test for App/app/aprs_settings.c.  Run: make -C tools/aprs test */
#include "app/aprs_settings.h"
#include <stdio.h>
#include <string.h>

static int fails, checks;
#define CHECK(c) do { checks++; if (!(c)) { fails++; printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); } } while (0)

static aprs_settings_t sample(void)
{
    aprs_settings_t s = APRS_SettingsDefaults();
    strcpy(s.call, "W1ABC");
    s.ssid = 5;
    strcpy(s.loc, "123456789012345");
    s.interval_s = 900;
    strcpy(s.msgto, "N0CALL-7");
    strcpy(s.comment, "Ridge digi");
    s.aprs_on = 1;
    s.digi_mode = APRS_DIGI_WIDE;
    s.digi_hops = 3;
    s.digi_delay = 2;
    s.beacon_type = 1;
    s.tone_level = 100;
    s.tone_twist = -3;
    return s;
}

static void test_defaults(void)
{
    aprs_settings_t d = APRS_SettingsDefaults();
    CHECK(strcmp(d.call, "N0CALL") == 0);
    CHECK(d.aprs_on == 0 && d.digi_mode == APRS_DIGI_OFF);
    CHECK(!APRS_CallIsSet(&d));
    CHECK(APRS_SettingsValid(&d));
    aprs_settings_t s = sample();
    CHECK(APRS_CallIsSet(&s));
}

static void test_roundtrip(void)
{
    aprs_settings_t s = sample(), out;
    uint8_t buf[APRS_RECORD_SIZE];
    CHECK(APRS_SettingsEncode(&s, buf));
    CHECK(APRS_SettingsDecode(buf, &out));
    CHECK(memcmp(&s, &out, sizeof s) == 0);
}

static void test_digi_byte(void)
{
    aprs_settings_t s = sample();
    uint8_t b = APRS_DigiByteEncode(&s);
    CHECK(b == (2 | (3 << 2) | (2 << 5) | (1 << 7)));
    aprs_settings_t o = APRS_SettingsDefaults();
    CHECK(APRS_DigiByteDecode(b, &o));
    CHECK(o.digi_mode == 2 && o.digi_hops == 3 && o.digi_delay == 2 && o.beacon_type == 1);
    CHECK(!APRS_DigiByteDecode(0xFF, &o));   /* mode 3 invalid (fresh/blank) */
    CHECK(!APRS_DigiByteDecode(3, &o));
    CHECK(!APRS_DigiByteDecode(0, &o));      /* hops 0 invalid */
}

static void test_blank_and_corrupt(void)
{
    uint8_t buf[APRS_RECORD_SIZE], good[APRS_RECORD_SIZE];
    aprs_settings_t s = sample(), out;
    memset(buf, 0xFF, sizeof buf);
    CHECK(!APRS_SettingsDecode(buf, &out));       /* blank flash -> defaults */
    CHECK(strcmp(out.call, "N0CALL") == 0 && !out.aprs_on);
    CHECK(APRS_SettingsEncode(&s, good));
    for (unsigned i = 0; i < APRS_RECORD_SIZE; i++) {   /* any single bit flip is caught */
        memcpy(buf, good, sizeof buf);
        buf[i] ^= 0x10;
        CHECK(!APRS_SettingsDecode(buf, &out));
        CHECK(strcmp(out.call, "N0CALL") == 0);
    }
    memcpy(buf, good, sizeof buf);
    buf[4] = 99;                                   /* unknown version */
    CHECK(!APRS_SettingsDecode(buf, &out));
}

static void test_validation(void)
{
    aprs_settings_t s = sample();
    uint8_t buf[APRS_RECORD_SIZE];
    s.ssid = 16;               CHECK(!APRS_SettingsValid(&s)); CHECK(!APRS_SettingsEncode(&s, buf));
    s = sample(); strcpy(s.call, "w1abc");   CHECK(!APRS_SettingsValid(&s));  /* lower case */
    s = sample(); memcpy(s.call, "TOOLONG", 7); CHECK(!APRS_SettingsValid(&s));
    s = sample(); s.call[0] = 0;             CHECK(!APRS_SettingsValid(&s));
    s = sample(); strcpy(s.loc, "12AB");     CHECK(!APRS_SettingsValid(&s));
    s = sample(); s.loc[0] = 0;              CHECK(APRS_SettingsValid(&s));    /* empty ok */
    s = sample(); memset(s.comment, 'x', 43); s.comment[43] = 0; CHECK(APRS_SettingsValid(&s));
    s = sample(); memset(s.comment, 'x', 44);                   CHECK(!APRS_SettingsValid(&s)); /* unterminated */
    s = sample(); s.comment[2] = 0x07;       CHECK(!APRS_SettingsValid(&s));   /* non-printable */
    s = sample(); s.interval_s = 30;         CHECK(!APRS_SettingsValid(&s));
    s = sample(); s.interval_s = 0;          CHECK(APRS_SettingsValid(&s));    /* 0 = no auto beacon */
    s = sample(); s.digi_mode = 3;           CHECK(!APRS_SettingsValid(&s));
    s = sample(); s.digi_hops = 8;           CHECK(!APRS_SettingsValid(&s));
    s = sample(); s.digi_hops = 0;           CHECK(!APRS_SettingsValid(&s));
    s = sample(); s.digi_delay = 4;          CHECK(!APRS_SettingsValid(&s));
    s = sample(); s.tone_twist = 9;          CHECK(!APRS_SettingsValid(&s));
    s = sample(); s.tone_twist = -5;         CHECK(!APRS_SettingsValid(&s));
    s = sample(); s.tone_level = 9;          CHECK(!APRS_SettingsValid(&s));
    s = sample(); s.tone_level = 128;        CHECK(!APRS_SettingsValid(&s));
    s = sample(); s.tone_level = 127; s.tone_twist = 8; CHECK(APRS_SettingsValid(&s));
    s = sample(); strcpy(s.msgto, "bad call"); CHECK(!APRS_SettingsValid(&s));
    CHECK(!APRS_SettingsValid(NULL));
}

static void test_stale_bytes(void)
{
    /* Bytes after the NUL must not reach the record, so equal settings encode equally. */
    aprs_settings_t a = sample(), b = sample();
    uint8_t ra[APRS_RECORD_SIZE], rb[APRS_RECORD_SIZE];
    strcpy(a.comment, "hello"); memset(a.comment + 6, 'z', 20);
    strcpy(b.comment, "hello");
    CHECK(APRS_SettingsEncode(&a, ra) && APRS_SettingsEncode(&b, rb));
    CHECK(memcmp(ra, rb, sizeof ra) == 0);
}

int main(void)
{
    test_stale_bytes();
    test_defaults(); test_roundtrip(); test_digi_byte(); test_blank_and_corrupt(); test_validation();
    printf("%d checks, %d failed\n", checks, fails);
    return fails != 0;
}
