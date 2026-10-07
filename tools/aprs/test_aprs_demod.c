/* Host test for App/app/aprs_demod.c: the C demodulator must decode exactly what armel's Python
 * model decodes from the same ADC samples (vectors from gen_vectors.py), plus noise, silence,
 * back-to-back frames and the busy flag. Usage: test_aprs_demod <vector dir> */
#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "app/aprs_demod.h"
#include "app/aprs_ax25.h"

static int fails, checks;
#define CHECK(c) do { checks++; if (!(c)) { fails++; printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); } } while (0)

static aprs_demod_t dm;   /* ~1.4 KB: keep it off the stack under the sanitizers */

typedef struct { char hex[2 * APRS_DEMOD_FRAME_MAX + 2]; } hexline_t;

static void to_hex(const uint8_t *f, uint16_t n, char *out)
{
    for (uint16_t i = 0; i < n; i++)
        snprintf(out + 2 * i, 3, "%02x", f[i]);
}

/* Run samples through a fresh demodulator, collecting hex frames. Returns the count. */
static unsigned run(const uint16_t *s, size_t n, hexline_t *out, unsigned max, unsigned *busy_samples)
{
    unsigned found = 0, busy = 0;
    APRS_DemodInit(&dm);
    for (size_t i = 0; i < n; i++) {
        const uint16_t len = APRS_DemodSample(&dm, s[i]);
        if (len) {
            CHECK(AX25_CalculateFCS(APRS_DemodFrame(&dm), (uint16_t)(len - 2)) ==
                  (uint16_t)(APRS_DemodFrame(&dm)[len - 2] | (APRS_DemodFrame(&dm)[len - 1] << 8)));
            if (found < max)
                to_hex(APRS_DemodFrame(&dm), len, out[found].hex);
            found++;
        }
        if (APRS_DemodBusy(&dm))
            busy++;
    }
    if (busy_samples)
        *busy_samples = busy;
    return found;
}

static uint16_t *load(const char *path, size_t *n)
{
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);
    uint16_t *buf = malloc((size_t)sz);
    if (!buf || fread(buf, 1, (size_t)sz, f) != (size_t)sz) { fclose(f); free(buf); return NULL; }
    fclose(f);
    *n = (size_t)sz / 2;
    return buf;   /* little-endian host assumed (macOS / Linux x86-64, arm64) */
}

static int vectors(const char *dir)
{
    DIR *d = opendir(dir);
    if (!d) { printf("cannot open vector dir %s (run: make vectors)\n", dir); return 0; }
    int cases = 0, truth_ok = 0, truth_total = 0;
    struct dirent *e;
    while ((e = readdir(d))) {
        const size_t L = strlen(e->d_name);
        if (L < 5 || strcmp(e->d_name + L - 4, ".exp")) continue;
        char base[300], path[400];
        snprintf(base, sizeof base, "%s/%.*s", dir, (int)(L - 4), e->d_name);
        snprintf(path, sizeof path, "%s.s16", base);
        size_t n;
        uint16_t *s = load(path, &n);
        CHECK(s != NULL);
        if (!s) continue;

        hexline_t got[8];
        const unsigned found = run(s, n, got, 8, NULL);
        free(s);

        snprintf(path, sizeof path, "%s.exp", base);
        FILE *f = fopen(path, "r");
        char line[2 * APRS_DEMOD_FRAME_MAX + 8], truth[2 * APRS_DEMOD_FRAME_MAX + 8] = "";
        unsigned want = 0;
        hexline_t exp[8];
        while (f && fgets(line, sizeof line, f)) {
            line[strcspn(line, "\r\n")] = 0;
            if (line[0] == '#') { sscanf(line, "# truth: %s", truth); continue; }
            if (want < 8) strcpy(exp[want].hex, line);
            want++;
        }
        if (f) fclose(f);

        int same = found == want;
        for (unsigned i = 0; same && i < want && i < 8; i++)
            same = strcmp(got[i].hex, exp[i].hex) == 0;
        checks++;
        if (!same) { fails++; printf("FAIL %s: C decoded %u frame(s), the Python model %u\n", e->d_name, found, want); }
        cases++;
        if (strcmp(truth, "none") && want) {
            truth_total++;
            if (found == 1 && !strcmp(got[0].hex, truth)) truth_ok++;
        }
        if (!strcmp(truth, "none"))
            CHECK(found == 0);   /* a bad FCS must never be shown */
    }
    closedir(d);
    printf("vectors: %d cases match the Python model; %d/%d decode the true frame\n", cases, truth_ok, truth_total);
    CHECK(cases >= 10);
    return cases;
}

/* xorshift noise: no frame may ever come out of noise or silence */
static void noise_and_silence(void)
{
    static uint16_t s[9600 * 30];
    uint32_t x = 12345;
    for (size_t i = 0; i < sizeof s / sizeof s[0]; i++) {
        x ^= x << 13; x ^= x >> 17; x ^= x << 5;
        s[i] = (uint16_t)(2048 + (int)(x % 801) - 400);
    }
    hexline_t out[2];
    unsigned busy = 0;
    CHECK(run(s, sizeof s / sizeof s[0], out, 2, &busy) == 0);
    CHECK(busy < sizeof s / sizeof s[0] / 4);    /* noise is busy only a small part of the time */
    for (size_t i = 0; i < sizeof s / sizeof s[0]; i++) s[i] = 2048;
    CHECK(run(s, sizeof s / sizeof s[0], out, 2, &busy) == 0 && busy == 0);
    for (size_t i = 0; i < sizeof s / sizeof s[0]; i++) s[i] = (uint16_t)(i & 1 ? 4095 : 0);   /* rail to rail */
    CHECK(run(s, sizeof s / sizeof s[0], out, 2, &busy) == 0);
}

static void back_to_back_and_busy(const char *dir)
{
    char path[400];
    snprintf(path, sizeof path, "%s/flipper_pos_raw.s16", dir);
    size_t n1;
    uint16_t *a = load(path, &n1);
    if (!a) { CHECK(a != NULL); return; }
    snprintf(path, sizeof path, "%s/sine_twist_0.s16", dir);
    size_t n2;
    uint16_t *b = load(path, &n2);
    if (!b) { free(a); CHECK(b != NULL); return; }
    uint16_t *both = malloc((n1 + n2) * 2);
    memcpy(both, a, n1 * 2);
    memcpy(both + n1, b, n2 * 2);
    hexline_t out[4];
    unsigned busy = 0;
    CHECK(run(both, n1 + n2, out, 4, &busy) == 2);          /* two frames in one stream */
    CHECK(busy > 5000 && busy < n1 + n2 - 2000);   /* busy through the packets (most of these vectors), idle in the gaps */
    /* the same frame twice within a second is shown once; after a gap it is shown again */
    uint16_t *twice = malloc((n1 * 2 + 9600 * 2) * 2);
    memcpy(twice, a, n1 * 2);
    for (size_t i = 0; i < 9600u * 2u; i++) twice[n1 + i] = 2048;
    memcpy(twice + n1 + 9600u * 2u, a, n1 * 2);
    CHECK(run(twice, n1 * 2 + 9600u * 2u, out, 4, NULL) == 2);
    free(twice); free(both); free(a); free(b);
}

int main(int argc, char **argv)
{
    const char *dir = argc > 1 ? argv[1] : "vectors";
    vectors(dir);
    noise_and_silence();
    back_to_back_and_busy(dir);
    printf("%d checks, %d failed\n", checks, fails);
    return fails != 0;
}
