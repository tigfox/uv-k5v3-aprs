/* Host test for App/app/aprs_cmd.c with stubbed radio operations. */
#include <stdio.h>
#include <string.h>
#include "app/aprs_ax25.h"
#include "app/aprs_beacon.h"
#include "app/aprs_cmd.h"
#include "app/aprs_digi.h"
#include "app/aprs_msg.h"
#include "app/aprs_station.h"

static int fails, checks;
#define CHECK(c) do { checks++; if (!(c)) { fails++; printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); } } while (0)

/* ---- the stub radio ---- */
static aprs_settings_t cur;
static char q_to[10], q_text[APRS_MSG_TEXT_MAX + 1];
static int beacons, applied, raw_queued, monitor_calls;
static bool on_state, monitor_on, raw_busy, apply_fails;
static uint32_t monitor_port;
static uint8_t raw_copy[160];
static uint16_t raw_len;

static bool st_set_on(bool on) { on_state = on; cur.aprs_on = on; return true; }
static aprs_reason_t q_reason;          /* what the stub queue answers: OK, or FULL ... */
static uint8_t q_seq, q_free;
static aprs_reason_t st_queue_message(const char *to, const char *text, uint8_t *seq)
{
    if (q_reason != APRS_R_OK) return q_reason;
    if (!to[0] || !text[0]) return APRS_R_EMPTY;
    strcpy(q_to, to); strcpy(q_text, text); *seq = ++q_seq; return APRS_R_OK;
}
static uint8_t st_queue_free(void) { return q_free; }
static aprs_status_t status_now;
static void st_status(aprs_status_t *s) { *s = status_now; }
static bool st_monitoring(uint32_t port) { return monitor_on && port == monitor_port; }
static void st_queue_beacon(void) { beacons++; }
static bool st_queue_raw(const uint8_t *f, uint16_t n)
{
    if (raw_busy) return false;
    memcpy(raw_copy, f, n); raw_len = n; raw_queued++; return true;
}
static bool st_apply(const aprs_settings_t *s) { if (apply_fails) return false; cur = *s; applied++; return true; }
static void st_get(aprs_settings_t *s) { *s = cur; }
static void st_stats(uint16_t st[6], uint32_t *up) { for (int i = 0; i < 6; i++) st[i] = (uint16_t)(10 + i); *up = 366000; }
static void st_monitor(uint32_t p, bool on) { monitor_port = p; monitor_on = on; monitor_calls++; }

static const aprs_cmd_ops_t ops = { st_set_on, st_queue_message, st_queue_beacon, st_queue_raw, st_apply, st_get,
                                    st_stats, st_monitor, st_queue_free, st_status, st_monitoring };

#define TS 0xA1B2C3D4u
static uint8_t reply[APRS_CMD_REPLY_MAX];

static void reset(void)
{
    cur = APRS_SettingsDefaults();
    strcpy(cur.call, "W1ABC"); cur.ssid = 7;
    memset(q_to, 0, sizeof q_to); memset(q_text, 0, sizeof q_text);
    beacons = applied = raw_queued = monitor_calls = 0;
    on_state = monitor_on = raw_busy = apply_fails = false;
    q_reason = APRS_R_OK; q_seq = 0; q_free = 3;
    raw_len = 0;
}

static void put32(uint8_t *p, uint32_t v) { p[0] = v & 255; p[1] = (v >> 8) & 255; p[2] = (v >> 16) & 255; p[3] = v >> 24; }
static uint16_t run(uint16_t id, const uint8_t *d, uint16_t n, uint32_t ts)
{
    memset(reply, 0xEE, sizeof reply);
    return APRS_CmdRun(&ops, 1, id, d, n, ts, reply);
}
static uint16_t rid(void) { return (uint16_t)(reply[0] | (reply[1] << 8)); }
static uint16_t rsize(void) { return (uint16_t)(reply[2] | (reply[3] << 8)); }

static void test_status(void)
{
    reset();
    memset(&status_now, 0, sizeof status_now);
    status_now.battery_mv = 7420; status_now.freq_10hz = 14439000u; status_now.decoded = 258; status_now.bad_fcs = 7;
    status_now.sent = 12; status_now.queue_free = 3; status_now.acks_pending = 1;
    status_now.would_tx = true; status_now.in_band = true; status_now.busy = false; status_now.why = APRS_R_OK;
    /* no session, no data */
    CHECK(run(0x0714, NULL, 0, 0) == 4 + 20 && rid() == 0x0715 && rsize() == 20 && reply[4] == 1);
    CHECK((reply[5] & 1) == 0);                                                  /* bit 0: monitoring on this port */
    CHECK((reply[5] & 2) && !(reply[5] & 4) && (reply[5] & 8) && reply[6] == APRS_R_OK && reply[7] == 0);
    CHECK((reply[8] | (reply[9] << 8)) == 7420 && (reply[10] | (reply[11] << 8)) == 258);
    CHECK((uint32_t)(reply[12] | (reply[13] << 8) | (reply[14] << 16) | ((uint32_t)reply[15] << 24)) == 14439000u);
    CHECK((reply[16] | (reply[17] << 8)) == 7 && (reply[18] | (reply[19] << 8)) == 12);
    CHECK(reply[20] == 3 && reply[21] == 1 && reply[22] == 0 && reply[23] == 0);

    /* it would not transmit: the reason, and the busy channel */
    status_now.would_tx = false; status_now.in_band = false; status_now.busy = true; status_now.why = APRS_R_BAND;
    CHECK(run(0x0714, NULL, 0, 0) == 4 + 20);
    CHECK(!(reply[5] & 2) && (reply[5] & 4) && !(reply[5] & 8) && reply[6] == APRS_R_BAND);

    /* monitoring is per port: this port (1) asked for it */
    monitor_on = true; monitor_port = 1;
    CHECK(run(0x0714, NULL, 0, 0) == 4 + 20 && (reply[5] & 1));
    monitor_port = 2;
    CHECK(run(0x0714, NULL, 0, 0) == 4 + 20 && !(reply[5] & 1));
}

static void test_set_call(void)
{
    uint8_t d[4 + 8 + 1];
    reset();
    strcpy(cur.comment, "keep me"); cur.interval_s = 600;
    put32(d, TS);
    memset(d + 4, 0, 9);
    memcpy(d + 4, "K1XYZ", 5); d[12] = 9;
    CHECK(run(0x0716, d, 13, TS) == 8 && rid() == 0x0717 && rsize() == 4 && reply[4] == 1 && reply[7] == APRS_R_OK);
    CHECK(applied == 1 && strcmp(cur.call, "K1XYZ") == 0 && cur.ssid == 9);
    CHECK(strcmp(cur.comment, "keep me") == 0 && cur.interval_s == 600);              /* the other settings are as they were */
    cur.ssid = 9;
    CHECK(run(0x0716, d, 13, TS) == 8 && reply[4] == 1 && applied == 1);             /* nothing changes: nothing is saved */
    d[12] = 0;
    CHECK(run(0x0716, d, 13, TS) == 8 && reply[4] == 1 && applied == 2 && cur.ssid == 0);   /* SSID alone */

    /* refusals, with the reason */
    CHECK(run(0x0716, d, 13, TS + 1) == 8 && reply[4] == 0 && reply[7] == APRS_R_SESSION && applied == 2);
    CHECK(run(0x0716, d, 12, TS) == 8 && reply[4] == 0 && reply[7] == APRS_R_INVALID);            /* too short */
    memset(d + 4, 0, 8);
    CHECK(run(0x0716, d, 13, TS) == 8 && reply[4] == 0 && reply[7] == APRS_R_INVALID);            /* no callsign */
    memcpy(d + 4, "k1xyz", 5);
    CHECK(run(0x0716, d, 13, TS) == 8 && reply[4] == 0 && reply[7] == APRS_R_INVALID);            /* lower case */
    memcpy(d + 4, "K1X-Z", 5);
    CHECK(run(0x0716, d, 13, TS) == 8 && reply[4] == 0 && reply[7] == APRS_R_INVALID);            /* a character outside A-Z 0-9 */
    memcpy(d + 4, "K1XYZAB\0", 8);
    CHECK(run(0x0716, d, 13, TS) == 8 && reply[4] == 0 && reply[7] == APRS_R_INVALID);            /* seven characters */
    memcpy(d + 4, "K1XYZAB", 7); d[11] = 'C';
    CHECK(run(0x0716, d, 13, TS) == 8 && reply[4] == 0 && reply[7] == APRS_R_INVALID);            /* eight: no terminator */
    memset(d + 4, 0, 8); memcpy(d + 4, "W1ABC", 5); d[12] = 16;
    CHECK(run(0x0716, d, 13, TS) == 8 && reply[4] == 0 && reply[7] == APRS_R_INVALID);            /* SSID 16 */
    d[12] = 3; apply_fails = true;
    CHECK(run(0x0716, d, 13, TS) == 8 && reply[4] == 0 && reply[7] == APRS_R_STORE);              /* the radio could not save it */
    CHECK(strcmp(cur.call, "K1XYZ") == 0);
}

static void test_capabilities(void)
{
    reset();
    CHECK(run(0x0712, NULL, 0, 0) == 4 + 12 && rid() == 0x0713 && rsize() == 12);      /* no session needed */
    CHECK(reply[4] == 1 && reply[5] == APRS_PROTOCOL_REVISION && reply[6] == 0 && reply[7] == 0);
    const uint32_t features = (uint32_t)reply[8] | ((uint32_t)reply[9] << 8) | ((uint32_t)reply[10] << 16) | ((uint32_t)reply[11] << 24);
    CHECK(features == APRS_CAPS_FEATURES && (features & APRS_CAP_SEND_FIELDS) && (features & APRS_CAP_EVENTS));
    CHECK((features & APRS_CAP_ACKQ) && (features & APRS_CAP_QUEUE) && (features & APRS_CAP_STATUS) && (features & APRS_CAP_COUNTERS) && (features & APRS_CAP_SET_CALL)
          && (features & APRS_CAP_PATH) && (features & APRS_CAP_GAP) && (features & APRS_CAP_RSSI));
    CHECK(features == 0x3FFu);                                        /* bits 0-9: everything this revision has */
    CHECK(reply[12] == APRS_MSGQ_DEPTH && reply[13] == APRS_ACKQ_DEPTH && reply[14] == APRS_MSG_TEXT_MAX && reply[15] == 0);
}

static void test_msg_beacon_on(void)
{
    reset();
    uint8_t d[44 + 4];
    memset(d, 0, sizeof d);
    put32(d, TS);
    strcpy((char *)d + 4, "N0CALL-7"); strcpy((char *)d + 14, "hello from the pc");
    CHECK(run(0x0700, d, 44, TS) == 8 && rid() == 0x0701 && rsize() == 4 && reply[4] == 1);
    CHECK(reply[5] == 1 && reply[6] == 3 && reply[7] == APRS_R_OK);              /* line number, free places, no reason */
    CHECK(strcmp(q_to, "N0CALL-7") == 0 && strcmp(q_text, "hello from the pc") == 0);
    CHECK(run(0x0700, d, 44, TS) == 8 && reply[4] == 1 && reply[5] == 2);        /* the next message, the next number */
    reset();
    CHECK(run(0x0700, d, 44, TS + 1) == 8 && reply[4] == 0 && q_to[0] == 0);     /* wrong session */
    CHECK(reply[5] == 0 && reply[6] == 3 && reply[7] == APRS_R_SESSION);
    CHECK(run(0x0700, d, 44, 0) == 8 && reply[4] == 0 && reply[7] == APRS_R_SESSION);   /* no session at all */
    CHECK(run(0x0700, d, 13, TS) == 8 && reply[4] == 0 && reply[7] == APRS_R_EMPTY);    /* too short for a destination and text */
    CHECK(run(0x0700, d, 14, TS) == 8 && reply[4] == 0 && reply[7] == APRS_R_EMPTY);    /* no text at all */
    CHECK(run(0x0700, d, 43, TS) == 8 && reply[4] == 1);                         /* a 29-character text is fine */
    reset();
    memset(d + 14, 0, 30);
    CHECK(run(0x0700, d, 44, TS) == 8 && reply[4] == 0 && reply[7] == APRS_R_EMPTY);    /* empty text */
    reset();
    q_reason = APRS_R_FULL; q_free = 0;                                           /* queue full: refused with the reason */
    memset(d + 14, 'x', 5);
    CHECK(run(0x0700, d, 44, TS) == 8 && reply[4] == 0 && reply[5] == 0 && reply[6] == 0 && reply[7] == APRS_R_FULL);
    CHECK(q_to[0] == 0);
    reset();

    {   /* the text is whatever follows the destination, up to 67 characters (a 30-byte field from older hosts still works) */
        uint8_t big[4 + 10 + 80];
        memset(big, 0, sizeof big);
        put32(big, TS);
        strcpy((char *)big + 4, "N0CALL-7");
        memset(big + 14, 'x', 67);
        reset();
        CHECK(run(0x0700, big, 4 + 10 + 67, TS) == 8 && reply[4] == 1 && strlen(q_text) == 67 && q_text[66] == 'x');
        reset();
        CHECK(run(0x0700, big, 4 + 10 + 68, TS) == 8 && reply[4] == 0 && q_to[0] == 0);   /* over 67: refused, never cut */
        CHECK(reply[7] == APRS_R_TOOLONG);
        reset();
        memset(big + 14, 0, 70);
        big[14] = 'o'; big[15] = 'k';
        CHECK(run(0x0700, big, 4 + 10 + 2, TS) == 8 && reply[4] == 1 && strcmp(q_text, "ok") == 0);   /* no padding needed */
        reset();
        memset(big + 14, 'y', 40); big[14 + 12] = 0;                                  /* a NUL ends the text early */
        CHECK(run(0x0700, big, 4 + 10 + 40, TS) == 8 && reply[4] == 1 && strlen(q_text) == 12);
        reset();
        CHECK(run(0x0700, big, 4 + 10, TS) == 8 && reply[4] == 0);                    /* no text field at all */
    }

    put32(d, TS);
    CHECK(run(0x0702, d, 4, TS) == 8 && rid() == 0x0703 && reply[4] == 1 && beacons == 1);
    CHECK(run(0x0702, d, 4, TS + 1) == 8 && reply[4] == 0 && beacons == 1);
    CHECK(run(0x0702, d, 3, TS) == 8 && reply[4] == 0 && beacons == 1);

    d[4] = 1;
    CHECK(run(0x0706, d, 5, TS) == 8 && rid() == 0x0707 && reply[4] == 1 && on_state);
    d[4] = 0;
    CHECK(run(0x0706, d, 5, TS) == 8 && reply[4] == 1 && !on_state);
    d[4] = 1;
    CHECK(run(0x0706, d, 5, TS + 1) == 8 && reply[4] == 0 && !on_state);
    CHECK(run(0x0706, d, 4, TS) == 8 && reply[4] == 0);
}

static void test_raw(void)
{
    reset();
    uint8_t d[4 + 40], *f = d + 4;
    put32(d, TS);
    AX25_EncodeAddress("APZK5", 0, false, f);
    AX25_EncodeAddress("W1ABC", 3, true, f + 7);        /* our call, any SSID */
    f[14] = 0x03; f[15] = 0xF0; memcpy(f + 16, ">raw test", 9);
    const uint16_t n = 4 + 25;
    CHECK(run(0x0708, d, n, TS) == 8 && rid() == 0x0709 && reply[4] == 1 && raw_queued == 1 && raw_len == 25);
    CHECK(memcmp(raw_copy, f, 25) == 0);
    raw_busy = true;
    CHECK(run(0x0708, d, n, TS) == 8 && reply[4] == 0);                          /* one frame waiting already */
    raw_busy = false;
    CHECK(run(0x0708, d, n, TS + 1) == 8 && reply[4] == 0 && raw_queued == 1);   /* not authorised */
    AX25_EncodeAddress("K1ABC", 3, true, f + 7);                                  /* somebody else's call */
    CHECK(run(0x0708, d, n, TS) == 8 && reply[4] == 0 && raw_queued == 1);
    AX25_EncodeAddress("W1ABC", 3, true, f + 7);
    f[14] = 0x13;                                                                 /* not a UI frame */
    CHECK(run(0x0708, d, n, TS) == 8 && reply[4] == 0);
    f[14] = 0x03; f[15] = 0xCC;
    CHECK(run(0x0708, d, n, TS) == 8 && reply[4] == 0);
    f[15] = 0xF0;
    CHECK(run(0x0708, d, 4 + 16, TS) == 8 && reply[4] == 0);                      /* too short */
    f[13] &= 0xFE;                                                                /* address block never ends */
    CHECK(run(0x0708, d, n, TS) == 8 && reply[4] == 0);
    cur = APRS_SettingsDefaults();                                                /* no callsign set */
    f[13] |= 1;
    CHECK(run(0x0708, d, n, TS) == 8 && reply[4] == 0);
    cur = APRS_SettingsDefaults(); strcpy(cur.call, "W1ABC");
    uint8_t big[4 + APRS_RAWTX_MAX + 1];
    memset(big, 'x', sizeof big);
    put32(big, TS);
    memcpy(big + 4, f, 16);
    CHECK(run(0x0708, big, 4 + APRS_RAWTX_MAX, TS) == 8 && reply[4] == 1);        /* the longest allowed */
    CHECK(run(0x0708, big, 4 + APRS_RAWTX_MAX + 1, TS) == 8 && reply[4] == 0);    /* one byte too many */
}

static void test_digi(void)
{
    reset();
    uint8_t d[9];
    memset(d, 0, sizeof d);
    CHECK(run(0x070A, d, 5, 0) == 28 && rid() == 0x070B && rsize() == 24 && reply[4] == 1);   /* status needs no session */
    CHECK(reply[5] == 0 && reply[6] == 2 && reply[7] == 0 && reply[8] == 0);
    CHECK(reply[12] == (366000 & 255) && reply[16] == 10 && reply[17] == 0 && reply[26] == 15);   /* uptime, stats */
    CHECK(applied == 0);

    d[0] = 1; d[1] = APRS_DIGI_WIDE; d[2] = 3; d[3] = 2; d[4] = 1; put32(d + 5, TS);
    CHECK(run(0x070A, d, 9, TS) == 28 && reply[4] == 1 && reply[5] == 2 && reply[6] == 3 && reply[7] == 2 && reply[8] == 1);
    CHECK(applied == 1 && cur.digi_mode == APRS_DIGI_WIDE && cur.digi_hops == 3 && cur.digi_delay == 2 && cur.beacon_type == 1);
    const aprs_settings_t before = cur;
    CHECK(run(0x070A, d, 9, TS + 1) == 28 && reply[4] == 0 && applied == 1);      /* wrong session: unchanged, current shown */
    CHECK(run(0x070A, d, 5, TS) == 28 && reply[4] == 0 && applied == 1);          /* set without the session bytes */
    d[2] = 8;
    CHECK(run(0x070A, d, 9, TS) == 28 && reply[4] == 0 && memcmp(&cur, &before, sizeof cur) == 0);   /* hops out of range */
    d[2] = 0;
    CHECK(run(0x070A, d, 9, TS) == 28 && reply[4] == 0);
    d[2] = 2; d[1] = 3;
    CHECK(run(0x070A, d, 9, TS) == 28 && reply[4] == 0);
    d[1] = 1; d[3] = 4;
    CHECK(run(0x070A, d, 9, TS) == 28 && reply[4] == 0);
    d[3] = 1; d[4] = 2;
    CHECK(run(0x070A, d, 9, TS) == 28 && reply[4] == 0 && applied == 1);
    d[4] = 0; apply_fails = true;
    CHECK(run(0x070A, d, 9, TS) == 28 && reply[4] == 0);                          /* flash write failed */
    /* a stale set flag in the buffer with a short command is a status request */
    apply_fails = false;
    CHECK(run(0x070A, d, 4, TS) == 28 && applied == 1);
}

static void hex_of(const uint8_t *p, unsigned n, char *out)
{
    for (unsigned i = 0; i < n; i++) snprintf(out + 2 * i, 3, "%02x", p[i]);
}

static void test_setup(void)
{
    reset();
    strcpy(cur.call, "W1ABC"); cur.ssid = 7; cur.aprs_on = 1; cur.interval_s = 600;
    cur.digi_mode = APRS_DIGI_WIDE; cur.digi_hops = 2; cur.digi_delay = 0; cur.beacon_type = 1;
    cur.tone_level = 66; cur.tone_twist = -2;
    strcpy(cur.msgto, "N0CALL-7"); strcpy(cur.loc, "130712810599405"); strcpy(cur.comment, "Ridge digi");
    uint16_t n = run(0x070C, NULL, 0, 0);
    CHECK(n == 8 + APRS_RECORD_SIZE && rid() == 0x070D && rsize() == 4 + APRS_RECORD_SIZE && reply[4] == 1);
    aprs_settings_t back;
    CHECK(APRS_SettingsDecode(reply + 8, &back) && memcmp(&back, &cur, sizeof cur) == 0);
    char hex[2 * APRS_RECORD_SIZE + 1];
    hex_of(reply + 8, APRS_RECORD_SIZE, hex);
    printf("sample record: %s\n", hex);                                     /* copied into test_aprs_pc.py */

    uint8_t d[4 + APRS_RECORD_SIZE];
    put32(d, TS);
    memcpy(d + 4, reply + 8, APRS_RECORD_SIZE);
    reset();
    CHECK(run(0x070E, d, sizeof d, TS) == 8 && rid() == 0x070F && reply[4] == 1 && applied == 1);
    CHECK(strcmp(cur.call, "W1ABC") == 0 && cur.interval_s == 600 && cur.tone_twist == -2);
    reset();
    CHECK(run(0x070E, d, sizeof d, TS + 1) == 8 && reply[4] == 0 && applied == 0);   /* not authorised */
    CHECK(run(0x070E, d, sizeof d - 1, TS) == 8 && reply[4] == 0 && applied == 0);   /* short */
    d[4 + 40] ^= 0x01;                                                                /* corrupt: CRC catches it */
    CHECK(run(0x070E, d, sizeof d, TS) == 8 && reply[4] == 0 && applied == 0);
    d[4 + 40] ^= 0x01;
    apply_fails = true;
    CHECK(run(0x070E, d, sizeof d, TS) == 8 && reply[4] == 0);
}

static void test_monitor_and_unknown(void)
{
    reset();
    uint8_t one = 1;
    CHECK(run(0x0710, &one, 1, 0) == 8 && rid() == 0x0711 && reply[4] == 1 && monitor_on && monitor_port == 1);
    one = 0;
    CHECK(run(0x0710, &one, 1, 0) == 8 && !monitor_on);
    CHECK(run(0x0710, &one, 0, 0) == 8 && reply[4] == 0 && monitor_calls == 2);
    CHECK(run(0x0701, NULL, 0, 0) == 0);          /* not a command: the radio's own dispatcher goes on */
    CHECK(run(0x0514, NULL, 0, 0) == 0);
    CHECK(run(0x0720, NULL, 0, 0) == 0);
}

int main(void)
{
    test_capabilities(); test_set_call(); test_status(); test_msg_beacon_on(); test_raw(); test_digi(); test_setup(); test_monitor_and_unknown();
    printf("%d checks, %d failed\n", checks, fails);
    return fails != 0;
}
