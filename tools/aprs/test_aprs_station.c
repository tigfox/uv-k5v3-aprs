/* Host test for App/app/aprs_rxinfo.c and App/app/aprs_station.c: what the station does with received
 * frames, and what it decides to send. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "app/aprs_ax25.h"
#include "app/aprs_beacon.h"
#include "app/aprs_reason.h"
#include "app/aprs_station.h"

static int fails, checks;
#define CHECK(c) do { checks++; if (!(c)) { fails++; printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); } } while (0)
#define CHECK_STR(got, want) do { checks++; if (strcmp((got), (want))) { fails++; \
    printf("FAIL %s:%d: got '%s' want '%s'\n", __FILE__, __LINE__, (got), (want)); } } while (0)

/* the events the station reports, as the monitor lines they become */
static char evlog[16][APRS_EVENT_LINE_MAX];
static unsigned evn;
static void sink(const aprs_event_t *e)
{
    if (evn < 16u)
        APRS_EventLine(e, evlog[evn++], APRS_EVENT_LINE_MAX);
}
static void ev_reset(void) { evn = 0; }

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
    APRS_StationDrop(&st, &s, APRS_ACT_BEACON, APRS_R_OK);
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
    CHECK(st.beacon_countdown == 1200);                               /* given up, but the next automatic beacon is still due */
    APRS_StationQueueBeacon(&st);
    uint8_t seq;
    CHECK(APRS_StationQueueAck(&st, "K1ABC", "7") == APRS_R_OK);
    CHECK(APRS_StationEnqueue(&st, "K1ABC", "hi", true, &seq) == APRS_R_OK);
    APRS_StationClearPending(&st);
    CHECK(APRS_StationTick(&st) == APRS_ACT_NONE && !st.beacon_pending && st.a_count == 0 && st.q_count == 0);
}

static void test_raw(void)
{
    aprs_settings_t s = me();
    aprs_station_t st;
    uint8_t f[APRS_RAWTX_MAX + 2], g[APRS_RAWTX_MAX + 2];
    APRS_StationInit(&st, &s);
    uint16_t n = APRS_BuildBeacon(f, &s, 40712800, -74006000);
    CHECK(APRS_StationQueueRaw(&st, f, n));
    CHECK(!APRS_StationQueueRaw(&st, f, n));                           /* one at a time */
    CHECK(APRS_StationTick(&st) == APRS_ACT_RAW);
    APRS_StationQueueBeacon(&st);
    APRS_StationSetMsgTo(&st, "K1ABC"); APRS_StationSetMsgText(&st, "hi"); APRS_StationQueueMessage(&st);
    CHECK(APRS_StationTick(&st) == APRS_ACT_RAW);                      /* raw before message and beacon */
    CHECK(APRS_StationBuild(&st, &s, APRS_ACT_RAW, g) == n && memcmp(g, f, n) == 0);
    APRS_StationSent(&st, &s, APRS_ACT_RAW);
    CHECK(st.raw_len == 0);
    st.quiet = 0;
    CHECK(APRS_StationTick(&st) == APRS_ACT_MSG);
    CHECK(!APRS_StationQueueRaw(&st, f, 16));                          /* too short */
    uint8_t big[APRS_RAWTX_MAX + 1];
    memset(big, 0, sizeof big);
    CHECK(!APRS_StationQueueRaw(&st, big, APRS_RAWTX_MAX + 1));        /* too long */
    CHECK(APRS_StationQueueRaw(&st, big, APRS_RAWTX_MAX));
    APRS_StationClearPending(&st);                                      /* APRS off forgets it */
    CHECK(st.raw_len == 0 && APRS_StationTick(&st) == APRS_ACT_NONE);
    CHECK(APRS_StationQueueRaw(&st, f, n));
    APRS_StationDrop(&st, &s, APRS_ACT_RAW, APRS_R_BAND);               /* refused by the radio */
    CHECK(st.raw_len == 0);
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
    CHECK(st.seq == 1 && st.q_count == 0 && !st.dirty);
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
    CHECK(st.a_count == 0 && strcmp(st.msgto, "K1ABC") == 0);
    CHECK_STR(st.last_msg, "N0CALL-3>ack2");

    /* a message without a line number needs no ack */
    len = with_fcs(f, APRS_BuildMessage(f, &o, "W1ABC-7", "no number", 0));
    CHECK(APRS_StationOnFrame(&st, &s, f, len, &info) && st.a_count == 0);

    /* our own transmission heard back is ignored entirely */
    len = with_fcs(f, APRS_BuildMessage(f, &s, "N0CALL-3", "echo", 5));
    st.last_msg[0] = 0;
    CHECK(!APRS_StationOnFrame(&st, &s, f, len, &info) && st.last_msg[0] == 0);

    /* a refused request is forgotten */
    APRS_StationQueueMessage(&st);
    APRS_StationDrop(&st, &s, APRS_ACT_MSG, APRS_R_BAND);
    CHECK(st.q_count == 0);
    /* message text longer than the limit is cut */
    char longtxt[100]; memset(longtxt, 'z', 99); longtxt[99] = 0;
    APRS_StationSetMsgText(&st, longtxt);
    CHECK(strlen(st.msg_text) == APRS_MSG_TEXT_MAX);
}

static void test_queue(void)
{
    aprs_settings_t s = me();
    aprs_station_t st;
    uint8_t f[APRS_BUILD_MAX + 4], seq = 0;
    APRS_StationInit(&st, &s);
    APRS_StationSetEventSink(sink);
    ev_reset();

    CHECK(APRS_StationMsgFree(&st) == APRS_MSGQ_DEPTH && APRS_MSGQ_DEPTH == 4);
    CHECK(APRS_StationEnqueue(&st, "", "x", true, &seq) == APRS_R_EMPTY && seq == 0);
    CHECK(APRS_StationEnqueue(&st, "K1ABC", "", true, &seq) == APRS_R_EMPTY);
    for (uint8_t i = 1; i <= 4; i++) {
        char text[8];
        snprintf(text, sizeof text, "msg%u", i);
        CHECK(APRS_StationEnqueue(&st, "K1ABC-7", text, true, &seq) == APRS_R_OK && seq == i);
        CHECK(APRS_StationMsgFree(&st) == 4 - i);
    }
    seq = 77;
    CHECK(APRS_StationEnqueue(&st, "K1ABC-7", "fifth", true, &seq) == APRS_R_FULL && seq == 77);   /* refused: nothing replaced */
    CHECK(st.q_count == 4 && strcmp(st.q[st.q_head].text, "msg1") == 0);

    /* first in, first out; each sent message reports SENT with its destination and line number */
    for (uint8_t i = 1; i <= 4; i++) {
        st.quiet = 0;
        CHECK(APRS_StationTick(&st) == APRS_ACT_MSG);
        const uint16_t n = APRS_StationBuild(&st, &s, APRS_ACT_MSG, f);
        char want[16];
        snprintf(want, sizeof want, "msg%u{0%u", i, i);
        CHECK(n > 0 && strstr(tnc2(f, n), want) != NULL);
        APRS_StationSent(&st, &s, APRS_ACT_MSG);
    }
    CHECK(st.q_count == 0 && st.sent_msgs == 4 && APRS_StationTick(&st) == APRS_ACT_NONE);
    CHECK(evn == 4 && strcmp(evlog[0], "SENT:K1ABC-7{01}") == 0 && strcmp(evlog[3], "SENT:K1ABC-7{04}") == 0);

    /* line numbers run 1-99 and wrap */
    st.seq = 98;
    CHECK(APRS_StationEnqueue(&st, "K1", "a", true, &seq) == APRS_R_OK && seq == 99);
    CHECK(APRS_StationEnqueue(&st, "K1", "b", true, &seq) == APRS_R_OK && seq == 1);

    /* the operator's composer: the same text again is a retry and keeps its number; a changed one takes a new number */
    APRS_StationInit(&st, &s);
    APRS_StationSetMsgTo(&st, "K1ABC"); APRS_StationSetMsgText(&st, "hello");
    CHECK(APRS_StationQueueMessage(&st) && st.q[st.q_head].seq == 1);
    CHECK(APRS_StationQueueMessage(&st) && st.q[(st.q_head + 1) % APRS_MSGQ_DEPTH].seq == 1);
    APRS_StationSetMsgText(&st, "changed");
    CHECK(APRS_StationQueueMessage(&st) && st.q[(st.q_head + 2) % APRS_MSGQ_DEPTH].seq == 2);
    /* ... but a host message always takes a new number, even with the same text */
    CHECK(APRS_StationEnqueue(&st, "K1ABC", "changed", true, &seq) == APRS_R_OK && seq == 3);
    CHECK(!APRS_StationQueueMessage(&st));                        /* full: the menu's Send says no */
    APRS_StationSetEventSink(sink);
}

static void test_drops(void)
{
    aprs_settings_t s = me();
    aprs_station_t st;
    uint8_t seq;
    APRS_StationInit(&st, &s);
    APRS_StationSetEventSink(sink);

    /* the head is refused by the radio: it is dropped with the reason, the next one is tried */
    ev_reset();
    APRS_StationEnqueue(&st, "K1ABC", "one", true, &seq);
    APRS_StationEnqueue(&st, "K2DEF", "two", true, &seq);
    st.quiet = 0;
    CHECK(APRS_StationTick(&st) == APRS_ACT_MSG);
    APRS_StationDrop(&st, &s, APRS_ACT_MSG, APRS_R_BATTERY);
    CHECK(evn == 1 && strcmp(evlog[0], "DROP:K1ABC{01} battery") == 0 && st.q_count == 1);
    CHECK(strcmp(st.q[st.q_head].to, "K2DEF") == 0);

    /* a channel that never clears: each queued message gets its own minute, then goes with `busy` */
    APRS_StationInit(&st, &s);
    ev_reset();
    APRS_StationEnqueue(&st, "K1ABC", "one", true, &seq);
    APRS_StationEnqueue(&st, "K2DEF", "two", true, &seq);
    unsigned slots = 0;
    while (evn < 1 && slots < 1000) { APRS_StationTick(&st); slots++; }
    CHECK(slots == APRS_PENDING_MAX_SLOTS + 1 && strcmp(evlog[0], "DROP:K1ABC{01} busy") == 0 && st.q_count == 1);
    slots = 0;
    while (evn < 2 && slots < 1000) { APRS_StationTick(&st); slots++; }
    CHECK(slots == APRS_PENDING_MAX_SLOTS + 1 && strcmp(evlog[1], "DROP:K2DEF{02} busy") == 0 && st.q_count == 0);

    /* APRS switched off: everything waiting is reported as `off`, beacons and raw frames are just forgotten */
    APRS_StationInit(&st, &s);
    ev_reset();
    APRS_StationEnqueue(&st, "K1ABC", "one", true, &seq);
    APRS_StationEnqueue(&st, "K2DEF", "two", true, &seq);
    APRS_StationQueueAck(&st, "N0CALL-3", "050");
    APRS_StationQueueBeacon(&st);
    APRS_StationClearPending(&st);
    CHECK(evn == 3 && st.q_count == 0 && st.a_count == 0 && !st.beacon_pending);
    CHECK(strcmp(evlog[0], "DROP:K1ABC{01} off") == 0 && strcmp(evlog[1], "DROP:K2DEF{02} off") == 0);
    CHECK(strcmp(evlog[2], "DROP:ack N0CALL-3{050} off") == 0);

    /* an ack the radio refuses or gives up on is reported with the number as received */
    ev_reset();
    APRS_StationQueueAck(&st, "N0CALL-3", "050");
    st.quiet = 0;
    CHECK(APRS_StationTick(&st) == APRS_ACT_ACK);
    APRS_StationDrop(&st, &s, APRS_ACT_ACK, APRS_R_BAND);
    CHECK(evn == 1 && strcmp(evlog[0], "DROP:ack N0CALL-3{050} band") == 0 && st.a_count == 0);
    ev_reset();
    APRS_StationQueueAck(&st, "N0CALL-3", "051");
    slots = 0;
    while (evn < 1 && slots < 1000) { APRS_StationTick(&st); slots++; }
    CHECK(strcmp(evlog[0], "DROP:ack N0CALL-3{051} busy") == 0 && st.a_count == 0);

    /* no sink: nothing happens, nothing crashes */
    APRS_StationSetEventSink(NULL);
    APRS_StationEnqueue(&st, "K1ABC", "x", true, &seq);
    APRS_StationClearPending(&st);
    CHECK(st.q_count == 0);
    APRS_StationSetEventSink(sink);
}

static void test_acks(void)
{
    aprs_settings_t s = me();
    aprs_station_t st;
    uint8_t f[APRS_BUILD_MAX + 4], g[APRS_BUILD_MAX + 4];
    aprs_rx_info_t info;
    APRS_StationInit(&st, &s);
    APRS_StationSetEventSink(sink);
    ev_reset();

    /* four messages to us within the 5 s gap: all four are acknowledged, oldest first, none replaces another */
    const char *from[4] = { "N0CALL-3", "N1CALL-4", "N2CALL-5", "N3CALL-6" };
    for (int i = 0; i < 4; i++) {
        aprs_settings_t sender = other(from[i], 0);
        char *dash = strchr(from[i], '-');
        strncpy(sender.call, from[i], (size_t)(dash - from[i])); sender.ssid = (uint8_t)atoi(dash + 1);
        const uint16_t len = with_fcs(f, APRS_BuildMessage(f, &sender, "W1ABC-7", "hi", (uint8_t)(40 + i)));
        CHECK(APRS_StationOnFrame(&st, &s, f, len, &info));
    }
    CHECK(st.a_count == 4 && APRS_StationAcksPending(&st) == 4);
    for (int i = 0; i < 4; i++) {
        st.quiet = 0;
        CHECK(APRS_StationTick(&st) == APRS_ACT_ACK);
        const uint16_t n = APRS_StationBuild(&st, &s, APRS_ACT_ACK, g);
        char want[40];
        snprintf(want, sizeof want, ":%-9s:ack%d", from[i], 40 + i);
        CHECK(strstr(tnc2(g, n), want) != NULL);
        APRS_StationSent(&st, &s, APRS_ACT_ACK);
    }
    CHECK(st.a_count == 0 && evn == 0);

    /* the same sender and number twice (a retry heard before our ack went out) is acknowledged once */
    CHECK(APRS_StationQueueAck(&st, "N0CALL-3", "42") == APRS_R_OK);
    CHECK(APRS_StationQueueAck(&st, "N0CALL-3", "42") == APRS_R_OK && st.a_count == 1);
    CHECK(APRS_StationQueueAck(&st, "N0CALL-3", "43") == APRS_R_OK && st.a_count == 2);   /* another number: another ack */
    CHECK(APRS_StationQueueAck(&st, "N9XYZ", "42") == APRS_R_OK && st.a_count == 3);      /* another sender: another ack */

    /* a fifth distinct ack does not fit: it is reported, the four waiting are kept */
    CHECK(APRS_StationQueueAck(&st, "N8XYZ", "1") == APRS_R_OK && st.a_count == 4);
    ev_reset();
    CHECK(APRS_StationQueueAck(&st, "N7XYZ", "9") == APRS_R_FULL && st.a_count == 4);
    CHECK(evn == 1 && strcmp(evlog[0], "DROP:ack N7XYZ{9} full") == 0);

    /* acks go before queued messages, a message goes before a beacon */
    uint8_t seq;
    APRS_StationEnqueue(&st, "K1ABC", "x", true, &seq);
    APRS_StationQueueBeacon(&st);
    st.quiet = 0;
    CHECK(APRS_StationTick(&st) == APRS_ACT_ACK);
}

int main(void)
{
    test_rxinfo(); test_beacon_timer(); test_limits(); test_raw(); test_messages();
    test_queue(); test_drops(); test_acks();
    printf("%d checks, %d failed\n", checks, fails);
    return fails != 0;
}
