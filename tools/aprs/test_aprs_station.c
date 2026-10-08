/* Host test for App/app/aprs_rxinfo.c and App/app/aprs_station.c: what the station does with received
 * frames, and what it decides to send. */
#include <stdio.h>
#include <string.h>
#include "app/aprs_ax25.h"
#include "app/aprs_beacon.h"
#include "app/aprs_station.h"

static int fails, checks;
#define CHECK(c) do { checks++; if (!(c)) { fails++; printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); } } while (0)
#define CHECK_STR(got, want) do { checks++; if (strcmp((got), (want))) { fails++; \
    printf("FAIL %s:%d: got '%s' want '%s'\n", __FILE__, __LINE__, (got), (want)); } } while (0)

static aprs_settings_t me(void)
{
    aprs_settings_t s = APRS_SettingsDefaults();
    strcpy(s.call, "W1ABC"); s.ssid = 7; s.aprs_on = 1; s.interval_s = 600;
    strcpy(s.loc, "130712810599405");   /* 40.7128 N, 74.0060 W */
    strcpy(s.comment, "Ridge digi");
    return s;
}

static aprs_settings_t other(const char *call, uint8_t ssid)
{
    aprs_settings_t s = APRS_SettingsDefaults();
    strcpy(s.call, call); s.ssid = ssid;
    return s;
}

static uint16_t with_fcs(uint8_t *f, uint16_t len)
{
    const uint16_t fcs = AX25_CalculateFCS(f, len);
    f[len] = (uint8_t)(fcs & 0xFF);
    f[len + 1] = (uint8_t)(fcs >> 8);
    return (uint16_t)(len + 2);
}

/* "SRC>DST,...:info" of a frame without FCS, for comparing what we would transmit */
static const char *tnc2(const uint8_t *f, uint16_t len)
{
    static char out[200];
    char a[10], *p = out;
    uint8_t n = AX25_FormatAddress(&f[7], a); memcpy(p, a, n); p += n;
    *p++ = '>';
    n = AX25_FormatAddress(&f[0], a); memcpy(p, a, n); p += n;
    uint16_t at = 13;
    while (!(f[at] & 1u)) { at += 7; *p++ = ','; n = AX25_FormatAddress(&f[at - 6], a); memcpy(p, a, n); p += n; }
    *p++ = ':';
    memcpy(p, &f[at + 3], len - at - 3u); p += len - at - 3u;
    *p = 0;
    return out;
}

static void test_rxinfo(void)
{
    aprs_settings_t m = me(), o = other("N0CALL", 3);
    uint8_t f[APRS_BUILD_MAX + 4];
    aprs_rx_info_t info;
    uint16_t len;

    len = with_fcs(f, APRS_BuildMessage(f, &o, "W1ABC-7", "hello there", 12));
    CHECK(APRS_RxInfo(f, len, &m, 0, 0, &info));
    CHECK(info.to_me && !info.is_ack && !info.own);
    CHECK_STR(info.from, "N0CALL-3"); CHECK_STR(info.text, "N0CALL-3>hello there"); CHECK_STR(info.ack_seq, "12");

    len = with_fcs(f, APRS_BuildMessage(f, &o, "W1ABC-7", "no number", 0));
    CHECK(APRS_RxInfo(f, len, &m, 0, 0, &info) && info.to_me && info.ack_seq[0] == 0);

    {   /* a line number with framing characters is shown but never echoed back in an ack */
        uint8_t h[APRS_BUILD_MAX + 4];
        uint16_t hl = APRS_BuildHeader(h, &o, false);
        memcpy(h + hl, ":W1ABC-7  :hi{1}2", 17); hl = (uint16_t)(hl + 17);
        len = with_fcs(h, hl);
        CHECK(APRS_RxInfo(h, len, &m, 0, 0, &info) && info.to_me && info.ack_seq[0] == 0);
        hl = APRS_BuildHeader(h, &o, false);
        memcpy(h + hl, ":W1ABC-7  :acknowledged all", 27); hl = (uint16_t)(hl + 27);
        len = with_fcs(h, hl);
        CHECK(APRS_RxInfo(h, len, &m, 0, 0, &info) && info.to_me && !info.is_ack);   /* not an ack: a word */
    }
    len = with_fcs(f, APRS_BuildAck(f, &o, "W1ABC-7", "9"));
    CHECK(APRS_RxInfo(f, len, &m, 0, 0, &info) && info.to_me && info.is_ack && info.ack_seq[0] == 0);
    CHECK_STR(info.text, "N0CALL-3>ack9");

    len = with_fcs(f, APRS_BuildMessage(f, &o, "W1ABC-1", "wrong ssid", 3));    /* not for us */
    CHECK(APRS_RxInfo(f, len, &m, 0, 0, &info) && !info.to_me);
    CHECK(strncmp(info.text, "N0CALL-3:", 9) == 0);

    /* a position gets a distance when we know where we are, the payload otherwise */
    strcpy(o.loc, "130712810599405");
    strcpy(o.loc, "129512810609405");     /* not checked by the builder path below: use a fixed beacon */
    len = with_fcs(f, APRS_BuildBeacon(f, &o, 39952600, -75165200));
    CHECK(APRS_RxInfo(f, len, &m, 40712800, -74006000, &info));
    CHECK(strncmp(info.text, "N0CALL-3 1", 10) == 0 && strstr(info.text, "km") != NULL);   /* ~130 km */
    CHECK(APRS_RxInfo(f, len, &m, 0, 0, &info) && strcmp(info.text, "N0CALL-3") == 0);   /* no own location: just the call */

    /* our own frame heard back through a digipeater */
    len = with_fcs(f, APRS_BuildBeacon(f, &m, 40712800, -74006000));
    CHECK(APRS_RxInfo(f, len, &m, 0, 0, &info) && info.own);

    CHECK(!APRS_RxInfo(f, 10, &m, 0, 0, &info));                 /* runt */
    /* non-printable payload bytes show as dots */
    uint8_t g[300];
    uint16_t hl = APRS_BuildHeader(g, &o, false);
    g[hl++] = 'T'; g[hl++] = 0x01; g[hl++] = 0xFF; g[hl++] = 'x';
    len = with_fcs(g, hl);
    CHECK(APRS_RxInfo(g, len, &m, 0, 0, &info) && strstr(info.text, ":T..x") != NULL);
    memset(g + hl, 'q', 100);                                     /* long payload is cut at 43 characters */
    len = with_fcs(g, (uint16_t)(hl + 100));
    CHECK(APRS_RxInfo(g, len, &m, 0, 0, &info) && strlen(info.text) == APRS_RXTEXT_MAX);
}

static void test_beacon_timer(void)
{
    aprs_settings_t s = me();
    aprs_station_t st;
    APRS_StationInit(&st, &s);
    unsigned ticks = 0;
    aprs_action_t a;
    while ((a = APRS_StationTick(&st)) == APRS_ACT_NONE && ticks < 100) ticks++;
    CHECK(a == APRS_ACT_BEACON && ticks + 1 == APRS_BEACON_FIRST_TICKS);
    CHECK(APRS_StationTick(&st) == APRS_ACT_BEACON);          /* stays pending until sent */
    uint8_t f[APRS_BUILD_MAX + 4];
    uint16_t n = APRS_StationBuild(&st, &s, APRS_ACT_BEACON, f);
    CHECK(n > 0);
    CHECK_STR(tnc2(f, n), "W1ABC-7>APZK5,WIDE1-1,WIDE2-1:!4042.76N/07400.36W>Ridge digi");
    APRS_StationSent(&st, &s, APRS_ACT_BEACON);
    CHECK(st.beacon_countdown == 1200 && APRS_StationTick(&st) == APRS_ACT_NONE);
    for (ticks = 0; ticks < 1198; ticks++) CHECK(APRS_StationTick(&st) == APRS_ACT_NONE);
    CHECK(APRS_StationTick(&st) == APRS_ACT_BEACON);          /* every 600 s = 1200 ticks */

    s.interval_s = 0;                                          /* auto-beacon off: only on request */
    APRS_StationInit(&st, &s);
    for (ticks = 0; ticks < 5000; ticks++) CHECK(APRS_StationTick(&st) == APRS_ACT_NONE);
    APRS_StationQueueBeacon(&st);
    CHECK(APRS_StationTick(&st) == APRS_ACT_BEACON);
    s.interval_s = 600; s.aprs_on = 0;
    APRS_StationRearm(&st, &s);
    CHECK(st.beacon_countdown == 0);                           /* APRS off: no timer */

    /* no Loc: the beacon is a status packet, so the station still identifies itself */
    s = me(); s.loc[0] = 0;
    APRS_StationInit(&st, &s);
    for (ticks = 0; ticks < 30; ticks++) APRS_StationTick(&st);
    CHECK(st.beacon_pending);
    n = APRS_StationBuild(&st, &s, APRS_ACT_BEACON, f);
    CHECK_STR(tnc2(f, n), "W1ABC-7>APZK5,WIDE1-1,WIDE2-1:>Ridge digi");
    /* a request that cannot be built is dropped and the timer re-armed for the next interval */
    APRS_StationDrop(&st, &s, APRS_ACT_BEACON);
    CHECK(!st.beacon_pending && st.beacon_countdown == 1200 && st.sent_beacons == 0);
}

static void test_limits(void)
{
    aprs_settings_t s = me();
    aprs_station_t st;
    APRS_StationInit(&st, &s);
    /* the minimum gap: three acks queued back to back go out 5 s apart */
    aprs_rx_info_t info;
    aprs_settings_t o = other("N0CALL", 3);
    uint8_t f[APRS_BUILD_MAX + 4];
    unsigned sent = 0, slots = 0;
    for (int i = 0; i < 3; i++) {
        const uint16_t len = with_fcs(f, APRS_BuildMessage(f, &o, "W1ABC-7", "x", (uint8_t)(i + 1)));
        CHECK(APRS_StationOnFrame(&st, &s, f, len, &info));
        for (unsigned guard = 0; guard < 1000; guard++) {
            slots++;
            const aprs_action_t a = APRS_StationTick(&st);
            if (a == APRS_ACT_ACK) { APRS_StationSent(&st, &s, a); sent++; break; }
        }
    }
    CHECK(sent == 3 && slots == 1 + 2 * APRS_TX_MIN_GAP_SLOTS);   /* the first at once, then 5 s apart */

    /* a request the channel never let through expires after a minute, and APRS off clears everything */
    APRS_StationInit(&st, &s);
    APRS_StationQueueBeacon(&st);
    unsigned n = 0;
    while (APRS_StationTick(&st) == APRS_ACT_BEACON && n < 1000) n++;   // never sent: the channel is busy
    CHECK(n == APRS_PENDING_MAX_SLOTS && !st.beacon_pending);
    APRS_StationQueueBeacon(&st);
    st.ack_pending = true; st.msg_pending = true;
    APRS_StationClearPending(&st);
    CHECK(APRS_StationTick(&st) == APRS_ACT_NONE && !st.beacon_pending && !st.ack_pending && !st.msg_pending);
}

static void test_messages(void)
{
    aprs_settings_t s = me(), o = other("N0CALL", 3);
    aprs_station_t st;
    uint8_t f[APRS_BUILD_MAX + 4], g[APRS_BUILD_MAX + 4];
    aprs_rx_info_t info;
    APRS_StationInit(&st, &s);

    CHECK(!APRS_StationQueueMessage(&st));                      /* no target, no text */
    APRS_StationSetMsgText(&st, "hello");
    CHECK(!APRS_StationQueueMessage(&st));                      /* still no target */
    APRS_StationSetMsgTo(&st, "K1ABC");
    CHECK(APRS_StationQueueMessage(&st));
    uint16_t n = APRS_StationBuild(&st, &s, APRS_ACT_MSG, g);
    CHECK_STR(tnc2(g, n), "W1ABC-7>APZK5,WIDE1-1,WIDE2-1::K1ABC    :hello{01");
    APRS_StationSent(&st, &s, APRS_ACT_MSG);
    CHECK(st.seq == 1 && !st.msg_pending && !st.dirty);
    CHECK(APRS_StationQueueMessage(&st));                       /* the same text again: a retry */
    n = APRS_StationBuild(&st, &s, APRS_ACT_MSG, g);
    CHECK(strstr(tnc2(g, n), "hello{01") != NULL);
    APRS_StationSent(&st, &s, APRS_ACT_MSG);
    CHECK(st.seq == 1);
    APRS_StationSetMsgText(&st, "second");                       /* changed: next line number */
    APRS_StationQueueMessage(&st);
    n = APRS_StationBuild(&st, &s, APRS_ACT_MSG, g);
    CHECK(strstr(tnc2(g, n), "second{02") != NULL);
    APRS_StationSent(&st, &s, APRS_ACT_MSG);
    CHECK(st.seq == 2 && st.sent_msgs == 3);

    /* a numbered message to us: reply target, RdMsg, and an ack that takes priority */
    uint16_t len = with_fcs(f, APRS_BuildMessage(f, &o, "W1ABC-7", "meet at the ridge", 42));
    APRS_StationQueueBeacon(&st);
    APRS_StationQueueMessage(&st);
    CHECK(APRS_StationOnFrame(&st, &s, f, len, &info));
    CHECK_STR(st.msgto, "N0CALL-3"); CHECK(st.dirty);
    CHECK_STR(st.last_msg, "N0CALL-3>meet at the ridge");
    CHECK(APRS_StationMsgPages(&st) == 2);                       /* 26 characters, 16 a page */
    st.quiet = 0;                                                /* (the minimum gap has its own test) */
    CHECK(APRS_StationTick(&st) == APRS_ACT_ACK);
    n = APRS_StationBuild(&st, &s, APRS_ACT_ACK, g);
    CHECK_STR(tnc2(g, n), "W1ABC-7>APZK5,WIDE1-1,WIDE2-1::N0CALL-3 :ack42");
    APRS_StationSent(&st, &s, APRS_ACT_ACK);
    st.quiet = 0;
    CHECK(APRS_StationTick(&st) == APRS_ACT_MSG);                /* then the message, then the beacon */
    APRS_StationSent(&st, &s, APRS_ACT_MSG);
    st.quiet = 0;
    CHECK(APRS_StationTick(&st) == APRS_ACT_BEACON);
    APRS_StationSent(&st, &s, APRS_ACT_BEACON);
    CHECK(APRS_StationTick(&st) == APRS_ACT_NONE);

    /* an ack of ours is shown but never acknowledged, and does not move the reply target */
    APRS_StationSetMsgTo(&st, "K1ABC");
    len = with_fcs(f, APRS_BuildAck(f, &o, "W1ABC-7", "2"));
    CHECK(APRS_StationOnFrame(&st, &s, f, len, &info) && info.is_ack);
    CHECK(!st.ack_pending && strcmp(st.msgto, "K1ABC") == 0);
    CHECK_STR(st.last_msg, "N0CALL-3>ack2");

    /* a message without a line number needs no ack */
    len = with_fcs(f, APRS_BuildMessage(f, &o, "W1ABC-7", "no number", 0));
    CHECK(APRS_StationOnFrame(&st, &s, f, len, &info) && !st.ack_pending);

    /* our own transmission heard back is ignored entirely */
    len = with_fcs(f, APRS_BuildMessage(f, &s, "N0CALL-3", "echo", 5));
    st.last_msg[0] = 0;
    CHECK(!APRS_StationOnFrame(&st, &s, f, len, &info) && st.last_msg[0] == 0);

    /* a refused request is forgotten */
    APRS_StationQueueMessage(&st);
    APRS_StationDrop(&st, &s, APRS_ACT_MSG);
    CHECK(!st.msg_pending);
    /* message text longer than the limit is cut */
    char longtxt[60]; memset(longtxt, 'z', 59); longtxt[59] = 0;
    APRS_StationSetMsgText(&st, longtxt);
    CHECK(strlen(st.msg_text) == APRS_MSG_TEXT_MAX);
}

int main(void)
{
    test_rxinfo(); test_beacon_timer(); test_limits(); test_messages();
    printf("%d checks, %d failed\n", checks, fails);
    return fails != 0;
}
