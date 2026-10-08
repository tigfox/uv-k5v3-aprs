/* Host test for the transmit side: frames from the builders go through HDLC_EncodeFrame, the tone
 * schedule the driver plays (APRS_ToneRegs / APRS_LineLevel, one tone per 833 us bit) is synthesized
 * as phase-continuous AFSK at the 9.6 kHz ADC rate, and the C demodulator must decode it. */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "app/aprs_ax25.h"
#include "app/aprs_beacon.h"
#include "app/aprs_demod.h"
#include "app/aprs_digi.h"
#include "app/aprs_modem.h"
#include "app/aprs_msg.h"

static int fails, checks;
#define CHECK(c) do { checks++; if (!(c)) { fails++; printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); } } while (0)

static aprs_demod_t dm;
static uint16_t audio[9600 * 4];

/* REG_71 is Hz x 10.32444 (scale_freq): invert it so the synthesizer plays what the chip would */
static double reg71_hz(uint16_t r) { return r / 10.32444; }

/* Phase-continuous AFSK for the frame bits. ppm skews the bit clock, gain_db the whole level,
 * noise_lsb adds white noise. Returns the number of samples. */
static size_t modulate(const uint8_t *bits, uint16_t nbits, uint8_t level, int8_t twist,
                       double ppm, double noise_lsb, unsigned seed, size_t lead_silence)
{
    size_t n = 0;
    srand(seed);
    for (size_t i = 0; i < lead_silence; i++) audio[n++] = 2048;
    double phase = 0.0;
    const double spb = 8.0 * (1.0 + ppm * 1e-6);        /* ADC samples per bit */
    double carry = 0.0;
    uint16_t r71, r70;
    for (uint16_t b = 0; b < nbits; b++) {
        const bool mark = APRS_LineLevel(bits, b);
        APRS_ToneRegs(!mark, level, twist, &r71, &r70);
        const double hz = reg71_hz(r71);
        const double amp = ((r70 >> 8) & 0x7F) * 14.0;  /* ~900 LSB p-p at gain 66, as measured on a real radio */
        double want = spb + carry;
        const int ns = (int)want;
        carry = want - ns;
        for (int s = 0; s < ns && n < sizeof audio / sizeof audio[0]; s++) {
            phase += 2.0 * M_PI * hz / 9600.0;
            double v = 2048.0 + amp * sin(phase);
            if (noise_lsb > 0.0) v += noise_lsb * ((rand() / (double)RAND_MAX) * 2.0 - 1.0) * 1.7;
            audio[n++] = (uint16_t)(v < 0 ? 0 : v > 4095 ? 4095 : v);
        }
    }
    for (size_t i = 0; i < 2000 && n < sizeof audio / sizeof audio[0]; i++) audio[n++] = 2048;
    return n;
}

/* frame (no FCS) -> audio -> decoded frame; returns the number of frames decoded */
static unsigned roundtrip(const uint8_t *frame, uint16_t len, uint8_t level, int8_t twist,
                          double ppm, double noise, unsigned seed, uint8_t *got, uint16_t *got_len)
{
    uint8_t buf[HDLC_BUF_SIZE], full[APRS_RAWTX_MAX + 2];
    memcpy(full, frame, len);
    const uint16_t fcs = AX25_CalculateFCS(frame, len);
    full[len] = (uint8_t)(fcs & 0xFF);
    full[len + 1] = (uint8_t)(fcs >> 8);
    const uint16_t nbits = HDLC_EncodeFrame(buf, full, (uint16_t)(len + 2));
    CHECK(nbits > 0);
    const size_t n = modulate(buf, nbits, level, twist, ppm, noise, seed, 1500);
    unsigned found = 0;
    APRS_DemodInit(&dm);
    for (size_t i = 0; i < n; i++) {
        const uint16_t l = APRS_DemodSample(&dm, audio[i]);
        if (l) {
            if (found == 0) { memcpy(got, APRS_DemodFrame(&dm), l); *got_len = l; }
            found++;
        }
    }
    return found;
}

static void test_tone_regs(void)
{
    uint16_t r71, r70;
    APRS_ToneRegs(false, 66, 0, &r71, &r70);
    CHECK(r71 == 12389 && r70 == (0x8000 | (66 << 8)));
    APRS_ToneRegs(true, 66, 0, &r71, &r70);
    CHECK(r71 == 22714 && r70 == (0x8000 | (66 << 8)));
    APRS_ToneRegs(true, 66, 8, &r71, &r70);       /* +6 dB */
    CHECK(((r70 >> 8) & 0x7F) == 127);               /* 66 x 2 = 132, clamped */
    APRS_ToneRegs(true, 66, -4, &r71, &r70);      /* -6 dB */
    CHECK(((r70 >> 8) & 0x7F) == 33);
    APRS_ToneRegs(true, 120, 8, &r71, &r70);      /* clamped to the 7-bit gain */
    CHECK(((r70 >> 8) & 0x7F) == 127);
    APRS_ToneRegs(false, 1, 0, &r71, &r70);       /* level floor */
    CHECK(((r70 >> 8) & 0x7F) == 10);
    APRS_ToneRegs(false, 255, 0, &r71, &r70);     /* level ceiling */
    CHECK(((r70 >> 8) & 0x7F) == 127);
    APRS_ToneRegs(true, 66, 100, &r71, &r70);     /* twist clamped */
    CHECK(((r70 >> 8) & 0x7F) == 127);               /* 66 x 2 = 132, clamped */
    /* the register values are what scale_freq gives for 1200 and 2200 Hz */
    CHECK(((1200u * 1353245u) + (1u << 16)) >> 17 == APRS_TONE_REG71_MARK);
    CHECK(((2200u * 1353245u) + (1u << 16)) >> 17 == APRS_TONE_REG71_SPACE);
}

static void test_line_level(void)
{
    const uint8_t b[2] = { 0xA5, 0x0F };   /* 1010 0101 0000 1111 */
    const int want[16] = { 1,0,1,0,0,1,0,1, 0,0,0,0,1,1,1,1 };
    for (uint16_t i = 0; i < 16; i++) CHECK(APRS_LineLevel(b, i) == (bool)want[i]);
}

static void test_roundtrips(void)
{
    aprs_settings_t s = APRS_SettingsDefaults();
    strcpy(s.call, "W1ABC"); s.ssid = 7; strcpy(s.comment, "Ridge digi"); strcpy(s.loc, "130712810599405");
    uint8_t f[APRS_BUILD_MAX + 2], got[APRS_DEMOD_FRAME_MAX];
    uint16_t len, gl = 0;

    len = APRS_BuildStationBeacon(f, &s);
    CHECK(len > 0);
    CHECK(roundtrip(f, len, 66, 0, 0, 0, 1, got, &gl) == 1 && gl == len + 2 && memcmp(got, f, len) == 0);

    s.beacon_type = 1;
    len = APRS_BuildStationBeacon(f, &s);
    CHECK(roundtrip(f, len, 66, 0, 0, 0, 1, got, &gl) == 1 && memcmp(got, f, len) == 0);

    len = APRS_BuildMessage(f, &s, "N0CALL-7", "hello from the ridge", 12);
    CHECK(roundtrip(f, len, 66, 0, 0, 0, 1, got, &gl) == 1 && memcmp(got, f, len) == 0);
    len = APRS_BuildAck(f, &s, "N0CALL-7", "42");
    CHECK(roundtrip(f, len, 66, 0, 0, 0, 1, got, &gl) == 1 && memcmp(got, f, len) == 0);

    /* every level / twist the settings allow */
    len = APRS_BuildStationBeacon(f, &s);
    int ok = 0, total = 0;
    for (int lvl = 10; lvl <= 127; lvl += 39)
        for (int tw = -4; tw <= 8; tw += 4) {
            total++;
            ok += roundtrip(f, len, (uint8_t)lvl, (int8_t)tw, 0, 0, 1, got, &gl) == 1 && memcmp(got, f, len) == 0;
        }
    printf("level/twist grid: %d/%d decode\n", ok, total);
    CHECK(ok == total);

    /* bit-clock error of a cheap radio (the demodulator is verified to 1 %), and some noise */
    ok = total = 0;
    for (int ppm = -8000; ppm <= 8000; ppm += 4000) {
        total++;
        ok += roundtrip(f, len, 66, 0, ppm, 0, 1, got, &gl) == 1 && memcmp(got, f, len) == 0;
    }
    CHECK(ok == total);
    ok = total = 0;
    for (unsigned seed = 1; seed <= 6; seed++) {
        total++;
        ok += roundtrip(f, len, 66, 0, 0, 40.0, seed, got, &gl) == 1 && memcmp(got, f, len) == 0;
    }
    printf("40 LSB noise: %d/%d decode\n", ok, total);
    CHECK(ok >= 5);

    /* longest frame we send (raw TX / digi repeat): 150 bytes of path-heavy traffic */
    uint8_t big[APRS_RAWTX_MAX];
    AX25_EncodeAddress("APZK5", 0, false, &big[0]);
    AX25_EncodeAddress("W1ABC", 7, false, &big[7]);
    AX25_EncodeAddress("WIDE1", 1, false, &big[14]);
    AX25_EncodeAddress("WIDE2", 1, true, &big[21]);
    big[28] = 0x03; big[29] = 0xF0;
    memset(big + 30, 'x', sizeof big - 30);
    CHECK(roundtrip(big, APRS_RAWTX_MAX, 66, 0, 0, 0, 1, got, &gl) == 1 && gl == APRS_RAWTX_MAX + 2);

    /* a bit-flipped frame must not decode */
    uint8_t buf[HDLC_BUF_SIZE], full[APRS_BUILD_MAX + 4];
    memcpy(full, f, len);
    const uint16_t fcs = AX25_CalculateFCS(f, len);
    full[len] = (uint8_t)(fcs & 0xFF); full[len + 1] = (uint8_t)(fcs >> 8);
    full[len / 2] ^= 0x10;
    const uint16_t nbits = HDLC_EncodeFrame(buf, full, (uint16_t)(len + 2));
    const size_t n = modulate(buf, nbits, 66, 0, 0, 0, 1, 1500);
    unsigned found = 0;
    APRS_DemodInit(&dm);
    for (size_t i = 0; i < n; i++) found += APRS_DemodSample(&dm, audio[i]) != 0;
    CHECK(found == 0);
}

/* A whole digipeater hop on the host: a mobile's WIDE1-1 beacon is "heard" through the modulator and the
 * demodulator, the digi core queues a repeat, the repeat is modulated and a second receiver decodes it
 * with our callsign marked as used. */
static void test_digi_hop(void)
{
    aprs_settings_t mobile = APRS_SettingsDefaults();
    strcpy(mobile.call, "K1MOB"); mobile.ssid = 9; strcpy(mobile.comment, "driving");
    uint8_t f[APRS_BUILD_MAX + 2], got[APRS_DEMOD_FRAME_MAX], rep[APRS_DEMOD_FRAME_MAX];
    uint16_t gl = 0;
    const uint16_t len = APRS_BuildBeacon(f, &mobile, 40712800, -74006000);
    CHECK(roundtrip(f, len, 66, 0, 0, 0, 1, got, &gl) == 1);

    DIGI_Reset(APRS_DIGI_WIDE, 2, 1);
    CHECK(DIGI_Consider(got, gl, 1000, "W1ABC", 7) == DIGI_QUEUED);
    uint16_t n = 0;
    uint8_t *out = NULL;
    uint32_t t = 1000;
    while (t < 1200 && (out = DIGI_Due(t, false, &n)) == NULL) t++;
    CHECK(out != NULL && n == len + 7);                 /* MYCALL* inserted in front of WIDE1-1 */
    memcpy(rep, out, n);
    DIGI_Sent(true);
    uint16_t gl2 = 0;
    CHECK(roundtrip(rep, n, 66, 0, 0, 0, 2, got, &gl2) == 1 && gl2 == n + 2 && memcmp(got, rep, n) == 0);
    /* W1ABC-7* is now in the path, marked used, and WIDE1 is spent */
    char a[10];
    AX25_FormatAddress(&got[14], a);
    CHECK(strcmp(a, "W1ABC-7") == 0 && (got[20] & 0x80));
    CHECK(gAPRS_DigiStats[DIGI_ST_REPEATED] == 1);
    /* hearing the same packet again within 30 s is a duplicate: nothing more to send */
    uint8_t again[APRS_DEMOD_FRAME_MAX];
    memcpy(again, f, len);
    const uint16_t fcs = AX25_CalculateFCS(f, len);
    again[len] = (uint8_t)(fcs & 0xFF); again[len + 1] = (uint8_t)(fcs >> 8);
    CHECK(DIGI_Consider(again, (uint16_t)(len + 2), 1100, "W1ABC", 7) == DIGI_DUP);
    DIGI_Reset(APRS_DIGI_OFF, 2, 0);
}

int main(void)
{
    test_digi_hop();
    test_tone_regs();
    test_line_level();
    test_roundtrips();
    printf("%d checks, %d failed\n", checks, fails);
    return fails != 0;
}
