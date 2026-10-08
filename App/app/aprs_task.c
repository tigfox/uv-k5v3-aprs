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
#include "app/aprs_menu_text.h"
#include "app/aprs_station.h"
#include "app/aprs_store.h"
#include "audio.h"
#include "driver/aprs_rx.h"
#include "functions.h"
#include "misc.h"
#include "settings.h"
#include "ui/menu.h"
#include "ui/ui.h"
#include <string.h>

#define TICKS_PER_SLOT 50u       /* 10 ms ticks in a 500 ms station slot */

static aprs_station_t gStation;
static aprs_dstat_t   gStat;
static uint32_t       gTicks;

/* The radio listens on the main VFO only while APRS is on: a dual watch alternates to VFO B and
 * misses packets. The settings are changed the way the RxMode menu does it. */
static void force_main_only(void)
{
    if (gEeprom.DUAL_WATCH == DUAL_WATCH_OFF && gEeprom.CROSS_BAND_RX_TX == CROSS_BAND_OFF)
        return;
    gEeprom.DUAL_WATCH = DUAL_WATCH_OFF;
    gEeprom.CROSS_BAND_RX_TX = CROSS_BAND_OFF;
    gDW = gEeprom.DUAL_WATCH;
    gCB = gEeprom.CROSS_BAND_RX_TX;
    gSaveRxMode = true;
    gFlagReconfigureVfos = true;
    gUpdateStatus = true;
}

void APRS_TaskInit(void)
{
    APRS_StationInit(&gStation, &gAprsSettings);
    if (gAprsSettings.aprs_on) {
        force_main_only();
        APRS_RxStart();
    }
}

bool APRS_IsOn(void)
{
    return APRS_RxRunning();
}

void APRS_TaskSettingsChanged(void)
{
    APRS_StationRearm(&gStation, &gAprsSettings);
}

bool APRS_SetOn(bool on)
{
    aprs_settings_t s = gAprsSettings;
    s.aprs_on = on ? 1u : 0u;
    if (!APRS_StoreSave(&s))
        return false;
    if (on) {
        force_main_only();
        APRS_RxStart();
    } else {
        APRS_RxStop();
    }
    APRS_TaskSettingsChanged();
    return true;
}

bool APRS_TaskQueueSend(void)
{
    return APRS_StationQueueMessage(&gStation);
}

void APRS_TaskQueueBeacon(void)
{
    APRS_StationQueueBeacon(&gStation);
}

const char *APRS_TaskMsgTo(void)        { return gStation.msgto; }
const char *APRS_TaskMsgText(void)      { return gStation.msg_text; }
const char *APRS_TaskLastMsg(void)      { return gStation.last_msg; }
unsigned    APRS_TaskLastMsgPages(void) { return APRS_StationMsgPages(&gStation); }
void APRS_TaskSetMsgTo(const char *to)  { APRS_StationSetMsgTo(&gStation, to); }
void APRS_TaskSetMsgText(const char *t) { APRS_StationSetMsgText(&gStation, t); }

/* One decoded frame: count it, remember who, tell the station, and beep (twice for a message to us). */
static void on_frame(const aprs_rx_frame_t *f)
{
    aprs_rx_info_t info;
    gStat.heard++;
    if (f->len >= 17u)
        AX25_FormatAddress(&f->data[7], gStat.last);
    if (APRS_StationOnFrame(&gStation, &gAprsSettings, f->data, f->len, &info))
        AUDIO_PlayBeep(info.to_me ? BEEP_880HZ_60MS_TRIPLE_BEEP : BEEP_1KHZ_60MS_OPTIONAL);
    gUpdateDisplay = true;
}

/* Every 500 ms: transmit what the station wants if the channel is quiet; otherwise try again next slot. */
static void transmit_slot(void)
{
    if (gAprsSettings.aprs_on)
        force_main_only();

    const aprs_action_t act = APRS_StationTick(&gStation);
    if (act == APRS_ACT_NONE)
        return;
    if (APRS_RxBusy() || FUNCTION_IsRx() || gCurrentFunction == FUNCTION_TRANSMIT || gPttIsPressed)
        return;                                    // a packet or a conversation is under way

    static uint8_t frame[APRS_BUILD_MAX + 2];
    const uint16_t len = APRS_StationBuild(&gStation, &gAprsSettings, act, frame);
    if (len == 0) {
        APRS_StationDrop(&gStation, &gAprsSettings, act);      // e.g. a beacon with no Loc
        return;
    }
    if (APRS_TxSend(frame, len) == APRS_TX_OK) {
        APRS_StationSent(&gStation, &gAprsSettings, act);
    } else {
        APRS_StationDrop(&gStation, &gAprsSettings, act);      // not allowed here: do not retry forever
        AUDIO_PlayBeep(BEEP_500HZ_60MS_DOUBLE_BEEP_OPTIONAL);
    }
    gUpdateDisplay = true;
}

void APRS_Task10ms(void)
{
    aprs_rx_frame_t f;
    while (APRS_RxPop(&f))
        on_frame(&f);

    gTicks++;
    if (gTicks % TICKS_PER_SLOT == 0) {
        transmit_slot();
#ifdef ENABLE_APRS_MENU_ONLY
        // keep the DStat counters live while that menu item is open
        if (gScreenToDisplay == DISPLAY_MENU && UI_MENU_GetCurrentMenuId() == MENU_APRS_DSTAT)
            gUpdateDisplay = true;
#endif
    }
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
    APRS_DStatText(&gStat, gTicks / 200u, out, n);   /* a new view every 2 s */
}
