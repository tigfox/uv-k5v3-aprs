/* Host test for the frame builders (beacon, message, ack), Loc decoding, distance and the
 * message-to-me matcher. Links the real files in App/app. */
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>

#include "app/aprs_ax25.h"
#include "app/aprs_beacon.h"
#include "app/aprs_msg.h"
#include "app/aprs_parse.h"

static int fails, checks;
#define CHECK(c) do { checks++; if (!(c)) { fails++; printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); } } while (0)
#define CHECK_STR(got, want) do { checks++; if (strcmp((got), (want))) { fails++; \
    printf("FAIL %s:%d: got '%s' want '%s'\n", __FILE__, __LINE__, (got), (want)); } } while (0)

/* "SRC>DST,VIA*,VIA:info" from a frame without FCS */
static const char *tnc2(const uint8_t *f, uint16_t len)
{
    static char out[400];
    char a[10], *p = out;
    p += AX25_FormatAddress(&f[7], a); memcpy(out, a, (size_t)(p - out));
    *p++ = '>';
    char d[10]; uint8_t n = AX25_FormatAddress(&f[0], d); memcpy(p, d, n); p += n;
    uint16_t at = 13;
    while (!(f[at] & 1u)) {
        at += 7;
        *p++ = ',';
        n = AX25_FormatAddress(&f[at - 6], d); memcpy(p, d, n); p += n;
        if (f[at] & 0x80) *p++ = '*';
    }
    *p++ = ':';
    memcpy(p, &f[at + 3], len - at - 3u); p += len - at - 3u;
    *p = 0;
    return out;
}

static aprs_settings_t station(void)
{
    aprs_settings_t s = APRS_SettingsDefaults();
    strcpy(s.call, "W1ABC"); s.ssid = 7; strcpy(s.comment, "Ridge digi");
    strcpy(s.loc, "130712810599405");
    return s;
}

static void test_addresses(void)
{
    uint8_t a[7]; char t[10];
    AX25_EncodeAddress("W1ABC", 0, false, a);
    CHECK(AX25_FormatAddress(a, t) == 5); CHECK_STR(t, "W1ABC");
    AX25_EncodeAddress("W1ABC", 12, true, a);
    CHECK(AX25_FormatAddress(a, t) == 8); CHECK_STR(t, "W1ABC-12");
    AX25_EncodeAddress("N0CALL", 3, false, a);
    CHECK_STR((AX25_FormatAddress(a, t), t), "N0CALL-3");
    CHECK(AX25_CalculateFCS((const uint8_t *)"123456789", 9) == 0x906E);   /* CRC-16/X-25 check value */
}

static void test_beacons(void)
{
    uint8_t f[APRS_BUILD_MAX];
    aprs_settings_t s = station();
    uint16_t n = APRS_BuildBeacon(f, &s, 40712800, -74006000);
    CHECK(n > 0 && n <= APRS_BUILD_MAX);
    CHECK_STR(tnc2(f, n), "W1ABC-7>APZK5,WIDE1-1,WIDE2-1:!4042.76N/07400.36W>Ridge digi");

    s.beacon_type = 1;   /* digipeater: direct, '/#' */
    n = APRS_BuildBeacon(f, &s, 40712800, -74006000);
    CHECK_STR(tnc2(f, n), "W1ABC-7>APZK5:!4042.76N/07400.36W#Ridge digi");
    CHECK(f[13] & 1u);   /* source is the last address */

    s.beacon_type = 0;
    n = APRS_BuildStationBeacon(f, &s);
    CHECK(n > 0);
    CHECK_STR(tnc2(f, n), "W1ABC-7>APZK5,WIDE1-1,WIDE2-1:!4042.76N/07400.36W>Ridge digi");

    strcpy(s.loc, "");
    CHECK(APRS_BuildStationBeacon(f, &s) == 0);          /* no Loc: nothing to beacon */
    strcpy(s.loc, "130712810599406");
    CHECK(APRS_BuildStationBeacon(f, &s) == 0);          /* bad checksum */

    s = station(); memset(s.comment, 'x', APRS_COMMENT_MAX); s.comment[APRS_COMMENT_MAX] = 0;
    n = APRS_BuildBeacon(f, &s, -33865000, 151209000);   /* S and E hemispheres */
    CHECK(n <= APRS_BUILD_MAX);
    CHECK(strstr(tnc2(f, n), "!3351.90S/15112.54E>") != NULL);
    n = APRS_BuildBeacon(f, &s, 0, 0);
    CHECK(strstr(tnc2(f, n), "!0000.00N/00000.00E>") != NULL);
}

static void test_messages(void)
{
    uint8_t f[APRS_BUILD_MAX];
    aprs_settings_t s = station();
    uint16_t n = APRS_BuildMessage(f, &s, "N0CALL-7", "hello", 5);
    CHECK_STR(tnc2(f, n), "W1ABC-7>APZK5,WIDE1-1,WIDE2-1::N0CALL-7 :hello{05");
    n = APRS_BuildMessage(f, &s, "K1A", "hi", 0);
    CHECK_STR(tnc2(f, n), "W1ABC-7>APZK5,WIDE1-1,WIDE2-1::K1A      :hi");
    n = APRS_BuildMessage(f, &s, "K1A", "x", 99);
    CHECK(strstr(tnc2(f, n), ":x{99") != NULL);
    CHECK(APRS_BuildMessage(f, &s, "", "hi", 1) == 0);
    CHECK(APRS_BuildMessage(f, &s, "K1A", "", 1) == 0);
    {   /* longest text: 30 chars + {nn */
        char t[40]; memset(t, 'y', 39); t[39] = 0;
        n = APRS_BuildMessage(f, &s, "ABCDEFGHI", t, 12);
        CHECK(n <= APRS_BUILD_MAX);
        CHECK(strstr(tnc2(f, n), "::ABCDEFGHI:yyyyyyyyyyyyyyyyyyyyyyyyyyyyyy{12") != NULL);
    }
    n = APRS_BuildAck(f, &s, "N0CALL-7", "42");
    CHECK_STR(tnc2(f, n), "W1ABC-7>APZK5,WIDE1-1,WIDE2-1::N0CALL-7 :ack42");
    n = APRS_BuildAck(f, &s, "N0CALL-7", "abcdefgh");   /* at most 5 characters */
    CHECK(strstr(tnc2(f, n), ":ackabcde") != NULL && strstr(tnc2(f, n), "abcdef") == NULL);
    CHECK(APRS_BuildAck(f, &s, "", "1") == 0);

    CHECK(APRS_MsgNextSeq(0, false) == 1);   /* first message */
    CHECK(APRS_MsgNextSeq(5, true) == 6);    /* changed: new line number */
    CHECK(APRS_MsgNextSeq(5, false) == 5);   /* same text again: a retry */
    CHECK(APRS_MsgNextSeq(99, true) == 1);   /* wraps, never 0 */
}

static void test_loc_distance_match(void)
{
    int32_t lat = 0, lon = 0;
    CHECK(APRS_LocDecode("130712810599405", &lat, &lon));
    CHECK(lat == 40712800 && lon == -74006000);
    CHECK(!APRS_LocDecode("130712810599406", &lat, &lon));    /* checksum */
    CHECK(!APRS_LocDecode("13071281059940", &lat, &lon));     /* too short */
    CHECK(!APRS_LocDecode("1307128105994051", &lat, &lon));   /* too long */
    CHECK(!APRS_LocDecode("13071281059940A", &lat, &lon));    /* not digits */
    CHECK(!APRS_LocDecode("000000000000000", &lat, &lon));    /* 0/0 is "unset" */

    /* New York to Philadelphia is about 130 km; one degree of latitude is 111.3 km */
    const uint32_t ny_phl = APRS_DistanceMetres(39952600, -75165200, 40712800, -74006000);
    CHECK(ny_phl > 125000 && ny_phl < 135000);
    const uint32_t deg = APRS_DistanceMetres(41000000, 0, 40000000, 0);
    CHECK(deg > 111000 && deg < 111600);
    CHECK(APRS_DistanceMetres(1, 1, 1, 1) == 0);

    char b[16], *e;
    e = APRS_FmtDistance(b, 850); *e = 0;      CHECK_STR(b, "850m");
    e = APRS_FmtDistance(b, 12300); *e = 0;    CHECK_STR(b, "12.3km");
    e = APRS_FmtDistance(b, 250000); *e = 0;   CHECK_STR(b, "250km");
    e = APRS_FmtCoord(b, 41150000, 'N', 'S'); *e = 0;    CHECK_STR(b, "41.15N");
    e = APRS_FmtCoord(b, -27840000, 'E', 'W'); *e = 0;   CHECK_STR(b, "27.84W");

    const uint8_t *m = (const uint8_t *)":W1ABC-7  :hi{3";
    CHECK(APRS_MessageToMe(m, 15, "W1ABC", 7));
    CHECK(!APRS_MessageToMe(m, 15, "W1ABC", 0));
    CHECK(!APRS_MessageToMe(m, 15, "W1AB", 7));
    CHECK(!APRS_MessageToMe(m, 10, "W1ABC", 7));              /* too short */
    CHECK(!APRS_MessageToMe((const uint8_t *)"!4042.76N/07400.36W>", 19, "W1ABC", 7));
    CHECK(!APRS_MessageToMe(m, 15, "", 0));                   /* no callsign set */
    CHECK(APRS_MessageToMe((const uint8_t *)":W1ABC-12 :x", 12, "W1ABC", 12));
}

int main(void)
{
    test_addresses(); test_beacons(); test_messages(); test_loc_distance_match();
    printf("%d checks, %d failed\n", checks, fails);
    return fails != 0;
}
