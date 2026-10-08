/* Copyright 2026 tigfox. Licensed under the Apache License, Version 2.0.
 * No libc printf: the firmware links its own, and newlib's would need _sbrk. */
#include "app/aprs_menu_text.h"
#include "app/aprs_parse.h"

#define LINE_CHARS 8u   /* big-font characters per menu line */

static const char *const kOffOn[]   = { "OFF", "ON" };
static const char *const kDigi[]    = { "OFF", "FILL", "WIDE" };
static const char *const kDelay[]   = { "OFF", "250ms", "500ms", "1s" };
static const char *const kBeacon[]  = { "MOBILE", "DIGI" };

typedef struct { char *p; size_t left; } sink_t;   /* left = room excluding the NUL */

static void put_char(sink_t *k, char c)
{
    if (k->left) { *k->p++ = c; k->left--; }
}

static void put_str(sink_t *k, const char *s)
{
    while (*s) put_char(k, *s++);
}

static void put_strn(sink_t *k, const char *s, size_t n)
{
    while (n-- && *s) put_char(k, *s++);
}

static void put_uint(sink_t *k, unsigned v)
{
    char tmp[11];
    unsigned i = 0;
    do { tmp[i++] = (char)('0' + v % 10u); v /= 10u; } while (v);
    while (i) put_char(k, tmp[--i]);
}

static void put_wide(sink_t *k, unsigned hops)
{
    put_str(k, "WIDE"); put_uint(k, hops); put_char(k, '-'); put_uint(k, hops);
}

/* At most two lines of LINE_CHARS from src, split by "\n". */
static void put_two_lines(sink_t *k, const char *src)
{
    size_t len = 0;
    while (src[len] && len < 2 * LINE_CHARS) len++;
    put_strn(k, src, len < LINE_CHARS ? len : LINE_CHARS);
    if (len > LINE_CHARS) { put_char(k, '\n'); put_strn(k, src + LINE_CHARS, len - LINE_CHARS); }
}

void APRS_MenuText(const aprs_settings_t *s, unsigned item, char *out, size_t n)
{
    if (n == 0)
        return;
    sink_t k = { out, n - 1 };
    switch (item) {
    case APRS_MI_APRS:   put_str(&k, kOffOn[s->aprs_on & 1u]); break;
    case APRS_MI_DIGI:   put_str(&k, kDigi[s->digi_mode % 3u]); break;
    case APRS_MI_DHOPS:  put_wide(&k, s->digi_hops); break;
    case APRS_MI_DDLY:   put_str(&k, kDelay[s->digi_delay & 3u]); break;
    case APRS_MI_BCNTY:  put_str(&k, kBeacon[s->beacon_type & 1u]); break;
    case APRS_MI_DSTAT:  put_str(&k, "--"); break;
    case APRS_MI_INTV:
        if (s->interval_s == 0)            put_str(&k, "OFF");
        else if (s->interval_s % 60u == 0) { put_uint(&k, s->interval_s / 60u); put_str(&k, "min"); }
        else                               { put_uint(&k, s->interval_s); put_char(&k, 's'); }
        break;
    case APRS_MI_CALL:   put_str(&k, s->call); break;
    case APRS_MI_SSID:   put_uint(&k, s->ssid); break;
    case APRS_MI_LOC: {
        int32_t lat, lon;
        char tmp[16];
        if (!s->loc[0]) {
            put_str(&k, "NONE");
        } else if (APRS_LocDecode(s->loc, &lat, &lon)) {     // "40.71N" over "74.00W"
            char *e = APRS_FmtCoord(tmp, lat, 'N', 'S'); *e = 0;
            put_str(&k, tmp); put_char(&k, '\n');
            e = APRS_FmtCoord(tmp, lon, 'E', 'W'); *e = 0;
            put_str(&k, tmp);
        } else {
            put_two_lines(&k, s->loc);                         // set by cable with a bad checksum: show the digits
        }
        break;
    }
    case APRS_MI_CMNT:   put_two_lines(&k, s->comment); break;
    case APRS_MI_MSGTO:  put_str(&k, s->msgto); break;
    case APRS_MI_BEACON: put_str(&k, "NO"); break;     /* an action: the menu shows SEND while editing */
    default:             put_str(&k, "N/A"); break;   /* Msg, Send, RdMsg: not wired yet */
    }
    *k.p = '\0';
}

void APRS_DStatText(const aprs_dstat_t *d, unsigned phase, char *out, size_t n)
{
    if (n == 0)
        return;
    sink_t k = { out, n - 1 };
    if (!d->running) {
        put_str(&k, "OFF");
    } else {
        put_str(&k, "HRD "); put_uint(&k, d->heard); put_char(&k, '\n');
        switch (phase % APRS_DSTAT_VIEWS) {
        case 0:  put_str(&k, d->last[0] ? d->last : "--"); break;
        case 1:  put_str(&k, "RPT "); put_uint(&k, d->repeated); break;
        case 2:  put_str(&k, "DUP "); put_uint(&k, d->dup); break;
        case 3:  put_str(&k, "CNL "); put_uint(&k, d->cancelled); break;
        case 4:  put_str(&k, "HOP "); put_uint(&k, d->toomany); break;
        case 5:  put_str(&k, "DRP "); put_uint(&k, d->digi_dropped + d->dropped); break;
        case 6:  put_str(&k, "avg "); put_uint(&k, d->isr_avg_us); put_str(&k, "us"); break;
        case 7:  put_str(&k, "max "); put_uint(&k, d->isr_max_us); put_str(&k, "us"); break;
        default: put_str(&k, "stk "); put_uint(&k, d->stack_free_min); break;
        }
    }
    *k.p = '\0';
}

void APRS_MenuTextLines(const char *s, char *out, size_t n)
{
    if (n == 0)
        return;
    sink_t k = { out, n - 1 };
    put_two_lines(&k, s);
    *k.p = '\0';
}

void APRS_MenuMsgPage(const char *msg, unsigned page, char *out, size_t n)
{
    if (n == 0)
        return;
    size_t len = 0;
    while (msg[len]) len++;
    sink_t k = { out, n - 1 };
    if (page * 16u < len)
        put_two_lines(&k, msg + page * 16u);
    *k.p = '\0';
}

unsigned APRS_BoxLines(const char *s, char lines[APRS_BOX_ROWS][APRS_BOX_COLS + 1])
{
    size_t len = 0;
    while (s[len] && len < APRS_BOX_ROWS * APRS_BOX_COLS) len++;
    unsigned rows = 0;
    for (size_t i = 0; i < len; i += APRS_BOX_COLS, rows++) {
        size_t n = len - i < APRS_BOX_COLS ? len - i : APRS_BOX_COLS;
        for (size_t k = 0; k < n; k++)
            lines[rows][k] = s[i + k];
        lines[rows][n] = 0;
    }
    return rows;
}
