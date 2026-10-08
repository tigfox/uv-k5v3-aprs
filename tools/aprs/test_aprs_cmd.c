/* Host test for App/app/aprs_cmd.c with stubbed radio operations. */
#include <stdio.h>
#include <string.h>
#include "app/aprs_ax25.h"
#include "app/aprs_beacon.h"
#include "app/aprs_cmd.h"
#include "app/aprs_digi.h"

static int fails, checks;
#define CHECK(c) do { checks++; if (!(c)) { fails++; printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); } } while (0)

/* ---- the stub radio ---- */
static aprs_settings_t cur;
static char q_to[10], q_text[31];
static int beacons, applied, raw_queued, monitor_calls;
static bool on_state, monitor_on, raw_busy, apply_fails;
static uint32_t monitor_port;
static uint8_t raw_copy[160];
static uint16_t raw_len;

static bool st_set_on(bool on) { on_state = on; cur.aprs_on = on; return true; }
static bool st_queue_message(const char *to, const char *text)
{
    if (!to[0] || !text[0]) return false;
    strcpy(q_to, to); strcpy(q_text, text); return true;
}
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
                                    st_stats, st_monitor };

#define TS 0xA1B2C3D4u
static uint8_t reply[APRS_CMD_REPLY_MAX];

static void reset(void)
{
    cur = APRS_SettingsDefaults();
    strcpy(cur.call, "W1ABC"); cur.ssid = 7;
    memset(q_to, 0, sizeof q_to); memset(q_text, 0, sizeof q_text);
    beacons = applied = raw_queued = monitor_calls = 0;
    on_state = monitor_on = raw_busy = apply_fails = false;
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

static void test_msg_beacon_on(void)
{
    reset();
    uint8_t d[44 + 4];
    memset(d, 0, sizeof d);
    put32(d, TS);
    strcpy((char *)d + 4, "N0CALL-7"); strcpy((char *)d + 14, "hello from the pc");
    CHECK(run(0x0700, d, 44, TS) == 8 && rid() == 0x0701 && rsize() == 4 && reply[4] == 1);
    CHECK(strcmp(q_to, "N0CALL-7") == 0 && strcmp(q_text, "hello from the pc") == 0);
    reset();
    CHECK(run(0x0700, d, 44, TS + 1) == 8 && reply[4] == 0 && q_to[0] == 0);     /* wrong session */
    CHECK(run(0x0700, d, 44, 0) == 8 && reply[4] == 0);                          /* no session at all */
    CHECK(run(0x0700, d, 43, TS) == 8 && reply[4] == 0);                         /* short */
    memset(d + 14, 0, 30);
    CHECK(run(0x0700, d, 44, TS) == 8 && reply[4] == 0);                         /* empty text */

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
    test_msg_beacon_on(); test_raw(); test_digi(); test_setup(); test_monitor_and_unknown();
    printf("%d checks, %d failed\n", checks, fails);
    return fails != 0;
}
