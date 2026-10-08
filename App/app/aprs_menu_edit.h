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


#ifndef APP_APRS_MENU_EDIT_H
#define APP_APRS_MENU_EDIT_H

#include <stdbool.h>
#include <stddef.h>
#include "driver/keyboard.h"

/* The menu key hook for the APRS text fields (Call, Loc, Cmnt, MsgTo): MENU opens the editor,
 * the arrows / digits / STAR / EXIT / MENU drive app/aprs_edit.c. Returns true when it used the key. */
bool APRS_MenuEditKey(KEY_Code_t key, bool pressed, bool held);
bool APRS_MenuEditing(void);
void APRS_MenuEditView(char *out, size_t n);   /* the field being edited, with its caret line */

#endif
