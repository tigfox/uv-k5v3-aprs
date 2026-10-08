/* Copyright 2023 Dual Tachyon
 * https://github.com/DualTachyon
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

#include <assert.h>
#include <string.h>

#include "app/chFrScanner.h"
#include "app/dtmf.h"
#ifdef ENABLE_FMRADIO_EMBEDDED
    #include "app/fm.h"
#endif
#include "driver/keyboard.h"
#include "misc.h"
#ifdef ENABLE_AIRCOPY
    #include "ui/aircopy.h"
#endif
#ifdef ENABLE_FMRADIO_EMBEDDED
    #include "ui/fmradio.h"
#endif
#ifdef ENABLE_FEAT_F4HWN_RXTX_LOG
    #include "app/rxtx_log.h"
#endif
#include "ui/inputbox.h"
#include "ui/main.h"
#include "ui/menu.h"
#include "ui/scanner.h"
#include "ui/ui.h"
#ifdef ENABLE_APRS
    #include "app/aprs_menu_text.h"
    #include "driver/st7565.h"
    #include "ui/helper.h"
    #include "app/aprs_task.h"
#endif
#include "../misc.h"

GUI_DisplayType_t gScreenToDisplay;
GUI_DisplayType_t gRequestDisplayScreen = DISPLAY_INVALID;

uint8_t           gAskForConfirmation;
bool              gAskToSave;
bool              gAskToDelete;


void (*const UI_DisplayFunctions[])(void) = {
    [DISPLAY_MAIN] = &UI_DisplayMain,
    [DISPLAY_MENU] = &UI_DisplayMenu,
    [DISPLAY_SCANNER] = &UI_DisplayScanner,

#ifdef ENABLE_FMRADIO_EMBEDDED
    [DISPLAY_FM] = &UI_DisplayFM,
#elif defined(ENABLE_FMRADIO)
    /* The FM overlay app replaces the resident FM screen (ui/fmradio.c is not
       compiled). DISPLAY_FM still exists in the enum, so the slot must stay
       initialised to keep ARRAY_SIZE == DISPLAY_N_ELEM; point it at a
       never-reached stub - the resident FM screen can no longer open. */
    [DISPLAY_FM] = &UI_DisplayMain,
#endif

#ifdef ENABLE_AIRCOPY
    [DISPLAY_AIRCOPY] = &UI_DisplayAircopy,
#endif

#ifdef ENABLE_FEAT_F4HWN_RXTX_LOG
    [DISPLAY_RXTX_LOG] = &UI_DisplayRxTxLog,
#endif
};

static_assert(ARRAY_SIZE(UI_DisplayFunctions) == DISPLAY_N_ELEM);

#ifdef ENABLE_APRS
// The packet box: a framed overlay over the lower half of the main screen (rows 3..6), covering the
// APRS panel, with the decoded packet in up to three lines of 16 characters. Drawn after the screen, so
// a long packet can never garble the VFO rows or the status line.
static void UI_DrawAPRSBox(const char *text)
{
    for (unsigned int r = 3; r <= 6; r++) {
        memset(gFrameBuffer[r], 0, LCD_WIDTH);
        gFrameBuffer[r][2]   = 0xFF;   // left frame
        gFrameBuffer[r][3]   = 0xFF;
        gFrameBuffer[r][124] = 0xFF;   // right frame
        gFrameBuffer[r][125] = 0xFF;
    }
    for (unsigned int x = 2; x < 126; x++) {
        gFrameBuffer[3][x] |= 0x01;    // top edge
        gFrameBuffer[6][x] |= 0x80;    // bottom edge
    }
    char lines[APRS_BOX_ROWS][APRS_BOX_COLS + 1];
    const unsigned int n = APRS_BoxLines(text, lines);
    const unsigned int start = 4 + (APRS_BOX_ROWS - (n ? n : 1)) / 2;   // centred in rows 4..6
    for (unsigned int r = 0; r < n; r++)
        UI_PrintStringSmallNormal(lines[r], 8, 0, start + r);
}
#endif

void GUI_DisplayScreen(void)
{
    if (gScreenToDisplay != DISPLAY_INVALID) {
        UI_DisplayFunctions[gScreenToDisplay]();
    }
#ifdef ENABLE_APRS
    if (gScreenToDisplay == DISPLAY_MAIN && APRS_BoxText()[0]) {
        UI_DrawAPRSBox(APRS_BoxText());
        ST7565_BlitFullScreen();
    }
#endif
}

void GUI_SelectNextDisplay(GUI_DisplayType_t Display)
{
    if (Display == DISPLAY_INVALID)
        return;

    if (gScreenToDisplay != Display)
    {
        DTMF_clear_input_box();

        gInputBoxIndex       = 0;
        gIsInSubMenu         = false;
        gCssBackgroundScan   = false;
        gScanStateDir        = SCAN_OFF;
        #ifdef ENABLE_FMRADIO_EMBEDDED
            gFM_ScanState    = FM_SCAN_OFF;
        #endif
        gAskForConfirmation  = 0;
        gAskToSave           = false;
        gAskToDelete         = false;

        HideFKeyIcon();
    }

    gScreenToDisplay = Display;
    gUpdateDisplay   = true;
}
