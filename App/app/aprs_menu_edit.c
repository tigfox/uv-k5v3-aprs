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


#include "app/aprs_menu_edit.h"
#include "app/aprs_edit.h"
#include "app/aprs_store.h"
#include "app/aprs_task.h"
#include "audio.h"
#include "misc.h"
#include "ui/menu.h"
#include "ui/ui.h"
#include <string.h>

static aprs_edit_t gE;
static bool        gActive;

static int editable_kind(int menu_id)
{
    switch (menu_id) {
    case MENU_APRS_CALL:  return APRS_EDIT_CALL;
    case MENU_APRS_LOC:   return APRS_EDIT_LOC;
    case MENU_APRS_CMNT:  return APRS_EDIT_CMNT;
    case MENU_APRS_MSGTO: return APRS_EDIT_MSGTO;
    case MENU_APRS_MSG:   return APRS_EDIT_MSG;
    default:              return -1;
    }
}

bool APRS_MenuEditing(void)
{
    return gActive;
}

static void touch(void)
{
    gFlagRefreshSetting = true;          // re-arms the menu timeout
    gRequestDisplayScreen = DISPLAY_MENU;
}

static void leave(void)
{
    gActive = false;
    gIsInSubMenu = false;
    touch();
}

static void commit(void)
{
    if (gE.kind == APRS_EDIT_MSG) {              // the message text lives in RAM only
        char txt[APRS_COMMENT_MAX + 1];
        if (APRS_EditText(&gE, txt))
            APRS_TaskSetMsgText(txt);
        gBeepToPlay = BEEP_1KHZ_60MS_OPTIONAL;
        leave();
        return;
    }
    aprs_settings_t s = gAprsSettings;
    switch (APRS_EditCommit(&gE, &s)) {
    case APRS_EDIT_OK:
        if (APRS_StoreSave(&s)) {
            if (gE.kind == APRS_EDIT_MSGTO)
                APRS_TaskSetMsgTo(s.msgto);      // also the current reply target
            APRS_TaskSettingsChanged();
            gBeepToPlay = BEEP_1KHZ_60MS_OPTIONAL;
            leave();
        } else {
            gBeepToPlay = BEEP_500HZ_60MS_DOUBLE_BEEP_OPTIONAL;   // flash write failed: keep editing
        }
        break;
    case APRS_EDIT_UNCHANGED:
        leave();                          // nothing entered: the old value stays
        break;
    default:
        gBeepToPlay = BEEP_500HZ_60MS_DOUBLE_BEEP_OPTIONAL;       // bad Loc code, gap in a callsign
        touch();
        break;
    }
}

bool APRS_MenuEditKey(KEY_Code_t key, bool pressed, bool held)
{
    if (!gActive) {
        const int kind = editable_kind(UI_MENU_GetCurrentMenuId());
        if (kind < 0 || gIsInSubMenu || key != KEY_MENU || !pressed || held)
            return false;
        APRS_EditBegin(&gE, (aprs_edit_kind_t)kind);
        gActive = true;
        gIsInSubMenu = true;
        gBeepToPlay = BEEP_1KHZ_60MS_OPTIONAL;
        touch();
        return true;
    }

    if (key == KEY_PTT)
        return false;                     // the radio's own PTT handling
    if (!pressed)
        return true;                      // swallow the releases

    switch (key) {
    case KEY_UP:
    case KEY_DOWN:
        APRS_EditArrow(&gE, key == KEY_UP ? +1 : -1);     // held keys repeat
        break;
    case KEY_0 ... KEY_9:
        if (held) return true;
        APRS_EditDigit(&gE, (uint8_t)(key - KEY_0));
        break;
    case KEY_STAR:
        if (held) return true;
        APRS_EditNext(&gE);
        break;
    case KEY_EXIT:
        if (held) return true;
        if (!APRS_EditBack(&gE))
            leave();                      // empty field: EXIT leaves the editor
        break;
    case KEY_MENU:
        if (held) return true;
        commit();
        return true;
    default:
        return true;
    }
    if (!held)
        gBeepToPlay = BEEP_1KHZ_60MS_OPTIONAL;
    touch();
    return true;
}

void APRS_MenuEditView(char *out, size_t n)
{
    char tmp[2 * 9 + 2];
    APRS_EditView(&gE, gE.kind == APRS_EDIT_CALL ? 6u : 8u, tmp);
    strncpy(out, tmp, n);
    out[n - 1] = 0;
}
