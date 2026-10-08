/* Host test for App/app/rpt_info.c: the repeater info records (check value + city/landmark text). */
#include <stdio.h>
#include <string.h>
#include "app/rpt_info.h"

static int fails, checks;
#define CHECK(c) do { checks++; if (!(c)) { fails++; printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); } } while (0)
#define CHECK_STR(got, want) do { checks++; if (strcmp((got), (want))) { fails++; \
    printf("FAIL %s:%d: got '%s' want '%s'\n", __FILE__, __LINE__, (got), (want)); } } while (0)

#define F 14682000u    /* 146.820 MHz in 10 Hz units */

static void test_check(void)
{
    CHECK(RPT_FreqCheck(F) == RPT_FreqCheck(F));
    CHECK(RPT_FreqCheck(F) != RPT_FreqCheck(F + 1));
    CHECK(RPT_FreqCheck(14682000u) != RPT_FreqCheck(14694000u));
    int clash = 0;                                      /* neighbouring channels (5 kHz steps) must not collide */
    for (uint32_t f = 14400000u; f < 14800000u; f += 500u)
        for (uint32_t g = f + 500u; g < f + 500u * 8u; g += 500u)
            clash += RPT_FreqCheck(f) == RPT_FreqCheck(g);
    CHECK(clash == 0);
    for (uint32_t f = 0; f < 100000u; f++)              /* 0xFFFF is the erased-flash value: never a real check */
        if (RPT_FreqCheck(f * 4501u) == 0xFFFFu) { CHECK(0); break; }
}

static void test_roundtrip(void)
{
    uint8_t rec[RPT_INFO_RECORD];
    char out[RPT_INFO_TEXT_MAX + 1];
    bool cut = true;
    CHECK(RPT_InfoEncode(rec, F, "Mechanicsburg, Three Square Hollow", &cut) && !cut);
    CHECK(RPT_InfoDecode(rec, F, out));
    CHECK_STR(out, "Mechanicsburg, Three Square Hollow");
    CHECK(!RPT_InfoDecode(rec, F + 500u, out) && out[0] == 0);           /* the channel's frequency changed: not shown */
    CHECK(!RPT_InfoDecode(rec, 14694000u, out));
    /* the text may be shorter than the field and is NUL-terminated inside the record */
    CHECK(RPT_InfoEncode(rec, F, "Enola", &cut) && RPT_InfoDecode(rec, F, out));
    CHECK_STR(out, "Enola");
}

static void test_limits_and_cleaning(void)
{
    uint8_t rec[RPT_INFO_RECORD];
    char out[RPT_INFO_TEXT_MAX + 1], longtxt[80];
    bool cut = false;
    memset(longtxt, 'x', 60); longtxt[60] = 0;
    CHECK(RPT_InfoEncode(rec, F, longtxt, &cut) && cut);                  /* cut at 45 and reported */
    CHECK(RPT_InfoDecode(rec, F, out) && strlen(out) == RPT_INFO_TEXT_MAX);
    memset(longtxt, 'y', 45); longtxt[45] = 0;
    CHECK(RPT_InfoEncode(rec, F, longtxt, &cut) && !cut && RPT_InfoDecode(rec, F, out) && strlen(out) == 45);
    CHECK(RPT_InfoEncode(rec, F, "  Big  Flat \x01 Mt\xE9  ", &cut));    /* trimmed, controls and non-ASCII made safe */
    CHECK(RPT_InfoDecode(rec, F, out));
    CHECK_STR(out, "Big Flat ? Mt?");
    CHECK(RPT_InfoEncode(rec, F, "Biglerville,  Big Flat So Mt", &cut) && RPT_InfoDecode(rec, F, out));
    CHECK_STR(out, "Biglerville, Big Flat So Mt");                        /* runs of spaces collapse */
    /* an empty text is no record: the slot becomes erased */
    CHECK(!RPT_InfoEncode(rec, F, "", &cut) && !RPT_InfoEncode(rec, F, "   ", &cut) && !RPT_InfoEncode(rec, F, NULL, &cut));
    uint8_t erased[RPT_INFO_RECORD];
    memset(erased, 0xFF, sizeof erased);
    CHECK(memcmp(rec, erased, sizeof rec) == 0);                          /* encode left an erased record, never stale bytes */
    CHECK(!RPT_InfoDecode(erased, F, out) && out[0] == 0);
    CHECK(RPT_InfoIsErased(erased) && RPT_InfoIsErased(rec));
    RPT_InfoEncode(rec, F, "Enola", &cut);
    CHECK(!RPT_InfoIsErased(rec));
}

static void test_garbage_records(void)
{
    uint8_t rec[RPT_INFO_RECORD];
    char out[RPT_INFO_TEXT_MAX + 1];
    bool cut;
    RPT_InfoEncode(rec, F, "Poughkeepsie", &cut);
    uint8_t bad[RPT_INFO_RECORD];
    memcpy(bad, rec, sizeof bad);
    memset(bad + 2 + 5, 'z', sizeof bad - 7);                              /* no NUL inside the record */
    CHECK(!RPT_InfoDecode(bad, F, out) && out[0] == 0);
    memcpy(bad, rec, sizeof bad);
    bad[3] = 0x07;                                                          /* a control character in the text */
    CHECK(!RPT_InfoDecode(bad, F, out));
    memcpy(bad, rec, sizeof bad);
    bad[2] = 0;                                                             /* empty text with a matching check */
    CHECK(!RPT_InfoDecode(bad, F, out));
    uint8_t zero[RPT_INFO_RECORD] = { 0 };
    CHECK(!RPT_InfoDecode(zero, F, out));
}

static void test_split(void)
{
    char city[RPT_LINE_MAX + 1], rest[RPT_LINE_MAX + 1];
    RPT_InfoSplit("Mechanicsburg, Three Square Hollow", city, rest);
    CHECK_STR(city, "Mechanicsburg"); CHECK_STR(rest, "Three Square Hollow");
    RPT_InfoSplit("Enola", city, rest);
    CHECK_STR(city, "Enola"); CHECK_STR(rest, "");
    RPT_InfoSplit("Mount Holly Springs, South Mountain, linked", city, rest);      /* the landmark may hold commas */
    CHECK_STR(city, "Mount Holly Springs"); CHECK_STR(rest, "South Mountain, linked");
    RPT_InfoSplit(", just a landmark", city, rest);
    CHECK_STR(city, ""); CHECK_STR(rest, "just a landmark");
    char big[200];
    memset(big, 'c', 70); big[70] = ','; memset(big + 71, 'l', 80); big[151] = 0;
    RPT_InfoSplit(big, city, rest);
    CHECK(strlen(city) == RPT_LINE_MAX && strlen(rest) == RPT_LINE_MAX);            /* cut to the line, never overflows */
    RPT_InfoSplit("", city, rest);
    CHECK_STR(city, ""); CHECK_STR(rest, "");
}

static void test_addresses(void)
{
    CHECK(RPT_InfoAddress(0) == RPT_INFO_EEPROM_ADDR);
    CHECK(RPT_InfoAddress(1) == RPT_INFO_EEPROM_ADDR + 48u);
    CHECK(RPT_InfoAddress(255) == 0xD000u + 255u * 48u);
    CHECK(RPT_InfoAddress(255) + RPT_INFO_RECORD == 0x10000u);          /* the table ends exactly at the end of the 16-bit space */
    CHECK(RPT_InfoAddress(256) == 0 && RPT_InfoAddress(1023) == 0);     /* no record for channels above 256 */
    CHECK(RPT_INFO_SLOTS * RPT_INFO_RECORD == 0x3000u);
}

int main(void)
{
    test_check(); test_roundtrip(); test_limits_and_cleaning(); test_garbage_records(); test_split(); test_addresses();
    printf("%d checks, %d failed\n", checks, fails);
    return fails != 0;
}
