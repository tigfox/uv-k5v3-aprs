/* Host test for App/app/aprs_monitor.c: the plain-text monitor lines a host reads (APRSRAW, APRSRSSI, APRS, DIGI and the
 * lines for SENT / DROP), in the order and the chunks they are written. */
#include <stdio.h>
#include <string.h>
#include "app/aprs_ax25.h"
#include "app/aprs_monitor.h"

static int fails, checks;
#define CHECK(c) do { checks++; if (!(c)) { fails++; printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); } } while (0)
#define CHECK_STR(got, want) do { checks++; if (strcmp((got), (want))) { fails++; \
    printf("FAIL %s:%d: got '%s' want '%s'\n", __FILE__, __LINE__, (got), (want)); } } while (0)

static char out[4096];
static unsigned out_len, out_calls, biggest;
static void sink(const char *s, uint16_t n)
{
    CHECK(out_len + n < sizeof out);
    memcpy(out + out_len, s, n);
    out_len += n;
    out[out_len] = 0;
    out_calls++;
    if (n > biggest) biggest = n;
}
static void reset(void) { out_len = out_calls = biggest = 0; out[0] = 0; }

/* a minimal UI frame: N0CALL-3 > APZK5, info "!x", then a 2-byte FCS */
static uint16_t frame(uint8_t *f)
{
    uint16_t n = 0;
    AX25_EncodeAddress("APZK5", 0, false, &f[n]); n += 7;
    AX25_EncodeAddress("N0CALL", 3, true, &f[n]); n += 7;
    f[n++] = 0x03; f[n++] = 0xF0; f[n++] = '!'; f[n++] = 'x';
    f[n++] = 0xAB; f[n++] = 0xCD;                      /* the FCS: dropped from the hex line */
    return n;
}

static void test_frame_lines(void)
{
    uint8_t f[64];
    const uint16_t n = frame(f);
    reset();
    APRS_MonitorFrame(sink, f, n, "N0CALL-3:!x", -87);
    const char *want_raw = "APRSRAW:";
    CHECK(strncmp(out, want_raw, 8) == 0);
    /* hex of the frame without its FCS: 7 + 7 + 4 bytes = 36 characters */
    char *first_end = strstr(out, "\r\n");
    CHECK(first_end != NULL && (size_t)(first_end - out) == 8 + 2 * (n - 2u));
    /* the signal strength line comes right after the raw line, then the text line */
    CHECK(strstr(first_end + 2, "APRSRSSI:-87\r\nAPRS:N0CALL-3:!x\r\n") == first_end + 2);
    CHECK(strstr(out, "\r\n") + 2 + strlen("APRSRSSI:-87\r\nAPRS:N0CALL-3:!x\r\n") == out + out_len);   /* and nothing after */

    reset();
    APRS_MonitorFrame(sink, f, n, "N0CALL-3:!x", APRS_NO_RSSI);        /* no reading: no line */
    CHECK(strstr(out, "APRSRSSI") == NULL && strstr(out, "\r\nAPRS:N0CALL-3:!x\r\n") != NULL);

    reset();
    APRS_MonitorFrame(sink, f, n, "x", -120);
    CHECK(strstr(out, "APRSRSSI:-120\r\n") != NULL);
    reset();
    APRS_MonitorFrame(sink, f, n, "x", 0);                            /* a reading of 0 dBm is a reading */
    CHECK(strstr(out, "APRSRSSI:0\r\n") != NULL);
    reset();
    APRS_MonitorFrame(sink, f, n, "x", -5);
    CHECK(strstr(out, "APRSRSSI:-5\r\n") != NULL);
}

static void test_chunks(void)
{
    /* a long frame is written in pieces of at most 64 bytes, never one big buffer, and joins up correctly */
    uint8_t f[200];
    uint16_t n = frame(f);
    memset(f + 18, 'q', 150);
    n = (uint16_t)(18 + 150 + 2);
    reset();
    APRS_MonitorFrame(sink, f, n, "long", -70);
    CHECK(biggest <= 64u && out_calls > 4u);
    CHECK(strncmp(out, "APRSRAW:", 8) == 0 && strstr(out, "\r\nAPRSRSSI:-70\r\nAPRS:long\r\n") != NULL);
    CHECK(strlen(out) == 8 + 2 * (n - 2u) + 2 + strlen("APRSRSSI:-70\r\nAPRS:long\r\n"));
}

static void test_digi_and_lines(void)
{
    uint8_t f[64];
    frame(f);
    reset();
    APRS_MonitorDigi(sink, "RPT", f);
    CHECK_STR(out, "DIGI:RPT N0CALL-3\r\n");
    reset();
    APRS_MonitorLine(sink, "SENT:K1ABC-7{01}");
    CHECK_STR(out, "SENT:K1ABC-7{01}\r\n");
    reset();
    APRS_MonitorLine(sink, "DROP:ack K1ABC-7{050} busy");
    CHECK_STR(out, "DROP:ack K1ABC-7{050} busy\r\n");
}

int main(void)
{
    test_frame_lines();
    test_chunks();
    test_digi_and_lines();
    printf("%d checks, %d failed\n", checks, fails);
    return fails != 0;
}
