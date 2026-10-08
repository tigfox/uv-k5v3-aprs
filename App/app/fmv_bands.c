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



#include "app/fmv_bands.h"

typedef struct { uint32_t low, high; } window_t;      /* low inclusive, high exclusive (10 Hz units) */

static const window_t WINDOWS[] = {
    {14400000u, 14800000u},      /* 2 m */
    {42000000u, 45000000u},      /* 70 cm */
    {46255000u, 46272501u},      /* FRS / GMRS */
    {46755000u, 46772501u},      /* FRS / GMRS (467 MHz group) */
};

static const uint32_t MURS[] = {15182000u, 15188000u, 15194000u, 15457000u, 15460000u};

bool FMV_TxAllowed(uint32_t freq)
{
    for (unsigned i = 0; i < sizeof(WINDOWS) / sizeof(WINDOWS[0]); i++)
        if (freq >= WINDOWS[i].low && freq < WINDOWS[i].high)
            return true;
    for (unsigned i = 0; i < sizeof(MURS) / sizeof(MURS[0]); i++)
        if (freq == MURS[i])
            return true;
    return false;
}
