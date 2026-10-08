/* Copyright 2026 tigfox.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 *     Unless required by applicable law or agreed to in writing, software
 *     distributed under the License is distributed on an "AS IS" BASIS,
 *     WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *     See the License for the specific language governing permissions and
 *     limitations under the License.
 */


#include "app/aprs_task.h"
#include "app/aprs_ax25.h"
#include "app/aprs_beacon.h"
#include "app/aprs_menu_text.h"
#include "app/aprs_store.h"
#include "audio.h"
#include "driver/aprs_rx.h"
#include "misc.h"
#include "ui/menu.h"
#include "ui/ui.h"
#include <string.h>

static aprs_dstat_t gStat;
static uint32_t     gStatTicks;

void APRS_TaskInit(void)
{
    if (gAprsSettings.aprs_on)
        APRS_RxStart();
}

bool APRS_IsOn(void)
{
    return APRS_RxRunning();
}

bool APRS_SetOn(bool on)
{
    aprs_settings_t s = gAprsSettings;
    s.aprs_on = on ? 1u : 0u;
    if (!APRS_StoreSave(&s))
        return false;
    if (on)
        APRS_RxStart();
    else
        APRS_RxStop();
    return true;
}

aprs_tx_result_t APRS_TxBeacon(void)
{
    uint8_t frame[APRS_BUILD_MAX + 2];
    const uint16_t len = APRS_BuildStationBeacon(frame, &gAprsSettings);
    if (len == 0)
        return APRS_TX_BAD_FRAME;       // no valid Loc
    return APRS_TxSend(frame, len);
}

void APRS_Task10ms(void)
{
    aprs_rx_frame_t f;
    while (APRS_RxPop(&f)) {
        gStat.heard++;
        if (f.len >= 17u)
            AX25_FormatAddress(&f.data[7], gStat.last);
        AUDIO_PlayBeep(BEEP_1KHZ_60MS_OPTIONAL);
        gUpdateDisplay = true;
    }
    gStatTicks++;
#ifdef ENABLE_APRS_MENU_ONLY
    // keep the DStat counters live while that menu item is open
    if (gStatTicks % 50u == 0 && gScreenToDisplay == DISPLAY_MENU && UI_MENU_GetCurrentMenuId() == MENU_APRS_DSTAT)
        gUpdateDisplay = true;
#endif
}

void APRS_DStatString(char *out, size_t n)
{
    const aprs_rx_stats_t rs = APRS_RxStats();
    gStat.running = APRS_RxRunning();
    gStat.dropped = rs.dropped;
    /* cycles to microseconds: the CPU runs at 48 MHz */
    if (rs.isr_cycles_max) {
        gStat.isr_avg_us = rs.isr_cycles_avg / 48u;
        gStat.isr_max_us = rs.isr_cycles_max / 48u;
    }
    APRS_DStatText(&gStat, gStatTicks / 200u, out, n);   /* a new view every 2 s */
}
