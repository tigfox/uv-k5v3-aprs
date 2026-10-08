/* Copyright 2026 tigfox. The tone method is armel's (App/apps/aprstx).
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


#include "app/aprs_modem.h"

void APRS_ToneRegs(bool space, uint8_t level, int8_t twist, uint16_t *reg71, uint16_t *reg70)
{
    uint32_t v = level < APRS_TONE_LEVEL_MIN ? APRS_TONE_LEVEL_MIN
               : level > APRS_TONE_LEVEL_MAX ? APRS_TONE_LEVEL_MAX : level;
    if (space) {
        const int32_t tw = twist < APRS_TONE_TWIST_MIN ? APRS_TONE_TWIST_MIN
                         : twist > APRS_TONE_TWIST_MAX ? APRS_TONE_TWIST_MAX : twist;
        v = (v * (uint32_t)(8 + tw)) >> 3;
        if (v > APRS_TONE_LEVEL_MAX)
            v = APRS_TONE_LEVEL_MAX;
    }
    *reg71 = space ? APRS_TONE_REG71_SPACE : APRS_TONE_REG71_MARK;
    *reg70 = (uint16_t)(APRS_TONE_REG70_ENABLE | (v << 8));
}

bool APRS_LineLevel(const uint8_t *bits, uint16_t i)
{
    return (bits[i >> 3] >> (7u - (i & 7u))) & 1u;
}
