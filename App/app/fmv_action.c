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



#include "app/fmv_action.h"
#include "app/fmv_bank.h"
#include <stddef.h>
#include "app/chFrScanner.h"
#include "app/fmv_card.h"
#include "misc.h"
#include "radio.h"
#include "settings.h"

#define BANKS (MR_CHANNELS_LIST + 2u)                  /* 24 lists, ALL, MIX; index = list value - 1 */

static uint16_t gLast[BANKS];                          /* the channel each bank was left on; not kept over a reboot */
static bool     gLastInit;

static bool in_list(uint16_t channel, void *ctx)
{
    const ChannelAttributes_t *att = MR_GetChannelAttributes(channel);
    return att != NULL && !att->exclude && att->band <= BAND7_470MHz
        && RADIO_IsChannelInScanList(att->scanlist, *(const uint8_t *)ctx);
}

static void init_last(void)
{
    if (gLastInit)
        return;
    for (uint8_t i = 0; i < BANKS; i++)
        gLast[i] = FMV_BANK_LAST_NONE;
    gLastInit = true;
}

static void remember(uint8_t list, uint16_t channel)
{
    if (list >= 1 && list <= BANKS && IS_MR_CHANNEL(channel))
        gLast[list - 1] = channel;
}

void FMV_ActionBank(void)
{
    init_last();
    const uint8_t old = gEeprom.SCAN_LIST_DEFAULT;
    remember(old, gEeprom.ScreenChannel[gEeprom.TX_VFO]);

    RADIO_NextValidList(1);
    uint8_t list = gEeprom.SCAN_LIST_DEFAULT;
    gRequestSaveSettings = true;

    if (gScanStateDir == SCAN_OFF && list != SCAN_LIST_MODE_ALL) {
        const uint16_t to = FMV_BankTarget(gLast[list - 1], MR_CHANNELS_MAX, in_list, &list);
        if (to != 0xFFFFu && to != gEeprom.ScreenChannel[gEeprom.TX_VFO]) {
            gEeprom.MrChannel[gEeprom.TX_VFO]     = to;
            gEeprom.ScreenChannel[gEeprom.TX_VFO] = to;
            gRequestSaveVFO   = true;
            gVfoConfigureMode = VFO_CONFIGURE_RELOAD;
        }
    }
    FMV_CardFlashBank(list);
}

uint16_t FMV_BrowseNext(uint16_t from, int8_t dir)
{
    uint8_t list = gEeprom.SCAN_LIST_DEFAULT;
    if (list == SCAN_LIST_MODE_ALL)
        return 0xFFFFu;
    init_last();
    const uint16_t next = FMV_NextIn(from, dir, MR_CHANNELS_MAX, in_list, &list);
    if (next != 0xFFFFu)
        remember(list, next);
    return next;
}
