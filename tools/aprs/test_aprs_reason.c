/* Host test for App/app/aprs_reason.c: the reason codes shared by DROP lines, the send reply and the status command,
 * and the text of the SENT / DROP monitor lines. */
#include <stdio.h>
#include <string.h>
#include "app/aprs_reason.h"

static int fails, checks;
#define CHECK(c) do { checks++; if (!(c)) { fails++; printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); } } while (0)
#define CHECK_STR(got, want) do { checks++; if (strcmp((got), (want))) { fails++; \
    printf("FAIL %s:%d: got '%s' want '%s'\n", __FILE__, __LINE__, (got), (want)); } } while (0)

static void test_codes(void)
{
    /* the numbers are a wire format: pinned */
    CHECK(APRS_R_OK == 0 && APRS_R_FULL == 1 && APRS_R_SESSION == 2 && APRS_R_EMPTY == 3 && APRS_R_TOOLONG == 4);
    CHECK(APRS_R_BUSY == 5 && APRS_R_NOCALL == 6 && APRS_R_APRSOFF == 7 && APRS_R_BAND == 8 && APRS_R_BATTERY == 9);
    CHECK(APRS_R_MODE == 10 && APRS_R_REPLACED == 11 && APRS_R_OFF == 12 && APRS_R_INVALID == 13 && APRS_R_STORE == 14);
    CHECK_STR(APRS_ReasonText(APRS_R_OK), "");
    CHECK_STR(APRS_ReasonText(APRS_R_FULL), "full");
    CHECK_STR(APRS_ReasonText(APRS_R_SESSION), "session");
    CHECK_STR(APRS_ReasonText(APRS_R_EMPTY), "empty");
    CHECK_STR(APRS_ReasonText(APRS_R_TOOLONG), "toolong");
    CHECK_STR(APRS_ReasonText(APRS_R_BUSY), "busy");
    CHECK_STR(APRS_ReasonText(APRS_R_NOCALL), "nocall");
    CHECK_STR(APRS_ReasonText(APRS_R_APRSOFF), "aprsoff");
    CHECK_STR(APRS_ReasonText(APRS_R_BAND), "band");
    CHECK_STR(APRS_ReasonText(APRS_R_BATTERY), "battery");
    CHECK_STR(APRS_ReasonText(APRS_R_MODE), "mode");
    CHECK_STR(APRS_ReasonText(APRS_R_REPLACED), "replaced");
    CHECK_STR(APRS_ReasonText(APRS_R_OFF), "off");
    CHECK_STR(APRS_ReasonText(APRS_R_INVALID), "invalid");
    CHECK_STR(APRS_ReasonText(APRS_R_STORE), "store");
    CHECK_STR(APRS_ReasonText(15), "");                 /* unknown: nothing */
    CHECK_STR(APRS_ReasonText(255), "");
}

static aprs_event_t ev(aprs_event_kind_t k, const char *to, const char *seq, uint8_t reason)
{
    aprs_event_t e;
    memset(&e, 0, sizeof e);
    e.kind = k;
    strcpy(e.to, to);
    strcpy(e.seq, seq);
    e.reason = reason;
    return e;
}

static void test_lines(void)
{
    char line[APRS_EVENT_LINE_MAX];
    aprs_event_t e = ev(APRS_EV_SENT, "KD2DCM-1", "07", 0);
    CHECK(APRS_EventLine(&e, line, sizeof line) == strlen("SENT:KD2DCM-1{07}"));
    CHECK_STR(line, "SENT:KD2DCM-1{07}");
    e = ev(APRS_EV_DROP_MSG, "KD2DCM-1", "07", APRS_R_BUSY);
    APRS_EventLine(&e, line, sizeof line); CHECK_STR(line, "DROP:KD2DCM-1{07} busy");
    e = ev(APRS_EV_DROP_ACK, "KD2DCM-1", "050", APRS_R_BUSY);
    APRS_EventLine(&e, line, sizeof line); CHECK_STR(line, "DROP:ack KD2DCM-1{050} busy");
    e = ev(APRS_EV_DROP_ACK, "N0CALL", "abcde", APRS_R_FULL);
    APRS_EventLine(&e, line, sizeof line); CHECK_STR(line, "DROP:ack N0CALL{abcde} full");
    e = ev(APRS_EV_DROP_MSG, "K1ABC", "99", APRS_R_OFF);
    APRS_EventLine(&e, line, sizeof line); CHECK_STR(line, "DROP:K1ABC{99} off");
    /* the longest line: a 9-character destination, a 5-character ack number, the longest reason */
    e = ev(APRS_EV_DROP_ACK, "ABCDEFGH-1", "abcde", APRS_R_APRSOFF);
    CHECK(APRS_EventLine(&e, line, sizeof line) < APRS_EVENT_LINE_MAX);
    /* a buffer that is too small is cut, never overrun */
    char small[8];
    e = ev(APRS_EV_SENT, "KD2DCM-1", "07", 0);
    CHECK(APRS_EventLine(&e, small, sizeof small) == 7 && strcmp(small, "SENT:KD") == 0);
}

int main(void)
{
    test_codes();
    test_lines();
    printf("%d checks, %d failed\n", checks, fails);
    return fails != 0;
}
