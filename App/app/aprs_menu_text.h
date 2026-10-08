/* Value text for the APRS menu items (hardware-free, host-tested).
 * Copyright 2026 tigfox. Licensed under the Apache License, Version 2.0.
 */
#ifndef APP_APRS_MENU_TEXT_H
#define APP_APRS_MENU_TEXT_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "app/aprs_settings.h"

/* Item numbers, in menu order; ui/menu.h's MENU_APRS_* ids follow the same order. */
enum {
    APRS_MI_APRS, APRS_MI_DIGI, APRS_MI_DHOPS, APRS_MI_DDLY, APRS_MI_BCNTY, APRS_MI_DSTAT,
    APRS_MI_INTV, APRS_MI_CALL, APRS_MI_SSID, APRS_MI_LOC, APRS_MI_CMNT, APRS_MI_MSGTO,
    APRS_MI_MSG, APRS_MI_SEND, APRS_MI_RDMSG, APRS_MI_BEACON,
    APRS_MI_COUNT
};

/* Two lines of 8 characters from s, for a text field shown outside the editor. */
void APRS_MenuTextLines(const char *s, char *out, size_t n);
/* Page `page` (16 characters, two lines of 8) of a received message, for RdMsg. */
void APRS_MenuMsgPage(const char *msg, unsigned page, char *out, size_t n);

/* What the DStat item shows: the receiver's counters, rotating through three views. */
typedef struct {
    uint32_t heard;          /* good frames decoded */
    uint32_t dropped;        /* decoded but lost (queue full) */
    uint32_t isr_avg_us;     /* demodulator interrupt, mean and longest, microseconds */
    uint32_t isr_max_us;
    char     last[10];       /* source of the last frame heard, "" if none */
    bool     running;
} aprs_dstat_t;

/* Two lines: "HRD n", then view (phase / 2 s mod 3): last heard, "avg Nus", "max Nus".
 * "OFF" when the receiver is not running. Always NUL-terminates (when n > 0). */
void APRS_DStatText(const aprs_dstat_t *d, unsigned phase, char *out, size_t n);

/* Always NUL-terminates out (when n > 0), truncating if needed. */
void APRS_MenuText(const aprs_settings_t *s, unsigned item, char *out, size_t n);

#endif
