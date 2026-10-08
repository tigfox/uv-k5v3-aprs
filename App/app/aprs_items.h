/* Copyright 2026 tigfox. Arrow text entry after the ta1js design.
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


#ifndef APP_APRS_ITEMS_H
#define APP_APRS_ITEMS_H

#include <stdbool.h>
#include <stdint.h>
#include "app/aprs_menu_text.h"
#include "app/aprs_settings.h"

/* The APRS menu items that are a choice or a number (the arrows step through a range). Items are
 * the APRS_MI_* numbers. */
bool    APRS_ItemIsChoice(unsigned item);
int32_t APRS_ItemMin(unsigned item);
int32_t APRS_ItemMax(unsigned item);
int32_t APRS_ItemGet(const aprs_settings_t *s, unsigned item);
/* A copy of s with the item set to selection sel (clamped to its range). */
aprs_settings_t APRS_ItemSet(const aprs_settings_t *s, unsigned item, int32_t sel);

#endif
