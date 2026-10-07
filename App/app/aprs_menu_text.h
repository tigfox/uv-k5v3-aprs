/* Value text for the APRS menu items (hardware-free, host-tested).
 * Copyright 2026 tigfox. Licensed under the Apache License, Version 2.0.
 */
#ifndef APP_APRS_MENU_TEXT_H
#define APP_APRS_MENU_TEXT_H

#include <stddef.h>
#include "app/aprs_settings.h"

/* Item numbers, in menu order; ui/menu.h's MENU_APRS_* ids follow the same order. */
enum {
    APRS_MI_APRS, APRS_MI_DIGI, APRS_MI_DHOPS, APRS_MI_DDLY, APRS_MI_BCNTY, APRS_MI_DSTAT,
    APRS_MI_INTV, APRS_MI_CALL, APRS_MI_SSID, APRS_MI_LOC, APRS_MI_CMNT, APRS_MI_MSGTO,
    APRS_MI_MSG, APRS_MI_SEND, APRS_MI_RDMSG, APRS_MI_BEACON,
    APRS_MI_COUNT
};

/* Always NUL-terminates out (when n > 0), truncating if needed. */
void APRS_MenuText(const aprs_settings_t *s, unsigned item, char *out, size_t n);

#endif
