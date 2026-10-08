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


#ifndef APP_FMV_BANDS_H
#define APP_FMV_BANDS_H

#include <stdbool.h>
#include <stdint.h>

/* FM Voice build: it receives anywhere the radio can, but transmits only on
 *   2 m 144-148 MHz, 70 cm 420-450 MHz, FRS/GMRS 462.550-462.725 and 467.550-467.725 MHz,
 *   and the five MURS frequencies (151.820, 151.880, 151.940, 154.570, 154.600 MHz).
 * freq is in 10 Hz units, as everywhere in the firmware. Which of these the operator may use, and with what
 * equipment, is the operator's responsibility. */
bool FMV_TxAllowed(uint32_t freq);

#endif
