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


#include "app/fmv_card.h"
#include "app/fmv_bank.h"
#include "app/fmv_info.h"
#include "app/fmv_store.h"
#include <string.h>
#include "misc.h"
#include "radio.h"
#include "settings.h"
#include "driver/st7565.h"
#include "ui/helper.h"
#include "ui/ui.h"

#define CARD_COLS 18u            /* small-font characters in a row from x = 2 */

static uint32_t gStep;           /* marquee step, 500 ms each */
static bool     gScrolling;
static uint8_t  gFlash;          /* half-seconds left of the bank name; gFlashLabel is what it says */
static char     gFlashLabel[12];

#define FLASH_STEPS 3u

void FMV_CardFlashBank(uint8_t list)
{
    const char *name = (list >= 1 && list <= MR_CHANNELS_LIST) ? gListName[list - 1] : "";
    FMV_BankLabel(list, name, SCAN_LIST_MODE_ALL, SCAN_LIST_MODE_MIX, gFlashLabel);
    gFlash = FLASH_STEPS;
    gUpdateDisplay = true;
}      /* the last card drawn needs a step every 500 ms */

bool FMV_CardRow(uint8_t row)
{
    gScrolling = false;
    if (gFlash > 0) {
        memset(gFrameBuffer[row], 0, LCD_WIDTH);
        UI_PrintStringSmallBold(gFlashLabel, 0, LCD_WIDTH - 1, row);
        return true;
    }
    const uint16_t channel = gEeprom.ScreenChannel[gEeprom.TX_VFO];
    if (!IS_MR_CHANNEL(channel))
        return false;
    char text[FMV_INFO_TEXT_MAX + 1], window[CARD_COLS + 1];
    if (!FMV_StoreLoad(channel, gEeprom.VfoInfo[gEeprom.TX_VFO].freq_config_RX.Frequency, text))
        return false;
    gScrolling = FMV_Marquee(text, CARD_COLS, gStep, window);
    UI_PrintStringSmallNormal(window, 2, 0, row);
    return true;
}

void FMV_Task500ms(void)
{
    gStep++;
    if (gFlash > 0 && --gFlash == 0 && gScreenToDisplay == DISPLAY_MAIN)
        gUpdateDisplay = true;
    if (gScrolling && gScreenToDisplay == DISPLAY_MAIN)
        gUpdateDisplay = true;
}
