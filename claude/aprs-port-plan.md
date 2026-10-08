# APRS digi port to the UV-K5 V3 / UV-K1 (PY32F071)

Started 2026-10-07. Re-implements the ta1js APRS digipeater work
(`claude/Sonnet5-5_plan.md`, copied from `../uv-k5-firmware-ta1js`, kept unchanged as
the record of what was built and decided there) on armel's PY32F071 firmware
(`armel/uv-k1-k5v3-firmware-custom`, upstream HEAD `523ef042`, v6.1.0).

**Status: step 0 partly done (2026-10-07).** Branch `aprs` created; baseline images and hashes saved in `claude/baseline/` (`claude/baseline/check.sh` is the guard). Step 0 complete: fork `tigfox/uv-k5v3-aprs` is `origin` (armel is `upstream`), calibration backed up, DFU works, stock Fusion v6.1.0 is saved in multiboot **slot 1** (recovery: hold MENU at power-on, restore slot 1).

## Decisions (2026-10-07)

| Question | Decision |
|---|---|
| Radios | **UV-K5 V3 and UV-K1** (both PY32F071 + BK4829). The K1 navigates with LEFT/RIGHT (`SET_NAV`); every arrow-driven screen must handle both. **No UV-K1 is available**: K1 support is written and built, but tested on the V3 only (the V3 with `SetNav` switched exercises the LEFT/RIGHT path). |
| Architecture | **Resident**: APRS runs inside the normal firmware, in the background. Not an overlay app. |
| Scope | **Full parity** with the ta1js digi image: RX decode + display, beacon + auto-beacon, messaging (send, receive, auto-ack, RdMsg), digi OFF/FILL/WIDE with hops/delay/cancel/dedupe, BcnTy, DStat, last heard, APRS panel, arrow text entry, persisted comment, USB status/config commands. |
| TX tones | **Real Bell 202 (1200/2200 Hz)**, generated in software as armel's APRS TX app does. This removes the ta1js 1200/1800 limitation: hardware-TNC radios (Kenwood D7x/D74/D75, Yaesu) should decode us. The old decision to replace those radios is void. |
| Band | **US amateur 2 m (144-148 MHz) and 70 cm (420-450 MHz) only**, for VFO and TX. |
| Menus | Only **APRS, RF, TX power + battery, key lock** settings are in the menu. Everything else is set by **programming cable** (CHIRP / UV Studio for radio settings; `aprs_pc.py` for APRS settings). |
| Cable tool for APRS settings | **`aprs_pc.py` over USB**. The `setup` commands the old plan deferred are now in scope. |
| PC link | **USB-C virtual serial port** (VCP) for all APRS commands and monitoring. The K-plug UART keeps working too. |
| Feature cuts | **Spectrum analyzer and FM radio** compiled out. **UV Studio / K5Viewer kept.** Everything else upstream ships in the base preset stays compiled in (its settings are just not in the menu). |
| Tocall | **`APZK5`** (what armel's APRS TX app sends). |
| Upstream | **Stay mergeable.** New `APRS` CMake preset. Every existing preset stays byte-identical. APRS lives in new files; edits to upstream files are small and behind `ENABLE_APRS*` flags. |
| Overlay APRS apps | **Hidden** in the APRS build. Resident APRS pauses while any overlay app runs. (The base preset has overlay apps off already; this matters only if they are turned on.) |
| Git | **Fork to tigfox's GitHub**, work on a branch, commit there. `claude/` is committed on the branch. Nothing goes to armel's repo unless decided later. |
| Hidden service menu | **Kept** as upstream has it (power-on key combo: calibration, F Lock, Reset). |
| Site power | **The radio's own battery, possibly with USB-C power as well** (see *Site power*). |
| Plan file | This file. The copied `Sonnet5-5_plan.md` stays as reference. |

Unchanged from the ta1js plan: personal / club use; remote mountain sites; multiple digis
on the network, possibly all running this firmware; prefer `WIDEn-N` paths; the
digi behaviour rules (FILL/WIDE, New-N, hop limit on n, cancel only for FILL, 16-slot
32-bit dedupe, UI frames only, own frames and N0CALL never repeated).

## Hardware notes (PY32F071 radios vs the old DP32G030 UV-K5)

| Item | Old (UV-K5 V1, DP32G030) | New (UV-K5 V3 / UV-K1, PY32F071) |
|---|---|---|
| MCU | Cortex-M0, 48 MHz | Cortex-M0+, 48 MHz, SysTick 10 ms |
| Flash for firmware | 60 KB, ~0.5 KB free in the digi image | **118 KB limit** (`compile-firmware.sh`). Flash is no longer the constraint; none of the old flash cuts are needed. |
| RAM | 16 KB | 16 KB (`RAM_LIMIT`). Still not expected to be the constraint, but measured per build. |
| RF chip | BK4819 | **BK4829.** Its FSK block has **no Bell 202 demodulator** (armel, 2026-09-25: dead end). The ta1js RX path (BK4819 FSK engine + sync detector) and TX path (FSK FIFO, FFSK 1200/1800) **do not port**. |
| RX audio for software demod | n/a | BK4829 AF output reaches **PA4** (the voice DAC pin), read on **ADC channel 4**. PA4 must be held at mid-scale by the DAC (unbuffered). The AF tap is before the speaker amplifier, so the speaker can stay off. Proven on air by the APRS RX app. |
| ADC | n/a | Shared with battery voltage (channel 8, `BOARD_ADC_GetBatteryInfo`, every 500 ms from `helper/battery.c`). Needs arbitration. |
| Timers / DMA in use | n/a | TIM6 + DMA ch3 + DAC: voice prompts (`driver/voice.c`, voice is off in the presets). TIM7: backlight PWM. DMA ch4-7: SPI flash (`py25q16.c`). A free timer and DMA channel must be confirmed for ADC sampling. |
| "EEPROM" | I2C EEPROM | **Emulated** in the 2 MB PY25Q16 SPI flash (`driver/eeprom_compat.c`, `ADDR_MAPPINGS`). Writes erase and rewrite a 4 KB sector: slow, wear-limited. Write rarely. |
| Calibration | EEPROM | Mapped at EEPROM `0xB000` (SPI flash `0x010000`). Back it up with UV Studio before the first flash. |
| USB-C | Charging only | **Data: USB CDC virtual serial port** (`driver/vcp.c`, `ENABLE_USB`), handled next to the UART in `app/uart.c`. |
| Flashing | `k5flash.py` over the K-plug cable, PTT at power-on | **UV Studio / UVTools2** (Web Serial, Chrome/Edge) with the radio in DFU mode. `k5flash.py` does not apply. **Multiboot** keeps a protected main slot, so a bad image can be recovered. |
| Bootloader / recovery | PTT at power-on | DFU mode + UV Studio; `tools/unbrick_k5_v1` is for the V1 only. Confirm the V3 recovery path on the bench before flashing an experimental build (step 0). |

## What ports from ta1js, and what is rewritten

| Part (ta1js file) | Fate |
|---|---|
| Digi core `app/aprs_digi.c/.h` (hardware-free, host-tested) | **Ports as is.** |
| Last heard `app/aprs_lastheard.c`, arrow text `app/aprs_text.c`, `APRS_FmtCount` | **Port as is** (host-tested formatters). |
| AX.25 framing, HDLC encode, FCS, path building, beacon/message/ack builders, packet parser (standard, Mic-E, compressed) | **Port**, split out of `aprs_minimal.c` (1739 lines) into files under 800 lines each: `aprs_ax25.c`, `aprs_parse.c`, `aprs_msg.c`, `aprs_beacon.c`, `aprs_task.c`. |
| RX modem (BK4819 FSK engine, `APRS_RX_IRQ_MASK`, sync detector, 500 ms stuck-tick decode) | **Replaced** by the APRS RX app's software demodulator (`App/apps/aprsrx/aprsrx_app.c`, model `test/model_rx.py`): DC removal, 1625 Hz band-pass, 4 correlators, per-tone AGC, 3 slicers + DPLL + NRZI + HDLC + CRC + UI check. Moved from a busy loop to interrupt-driven sampling. |
| TX modem (FSK FIFO, FFSK 1200/1800, `REG_58` etc.) | **Replaced** by the APRS TX app's method (`App/apps/aprstx/aprstx_app.c`): carrier + PA on, `REG_51 = 0`, tone path on, REG_71/REG_70 rewritten at each NRZI transition, bits timed at 40000 cycles (48 MHz). Real 1200/2200 Hz, with level and twist. |
| Busy detection (`gRxCapturing`, squelch `INCOMING`/`RECEIVE`) | **Redone**: "busy" = a slicer inside a preamble or a frame (the RX app already tracks this) or squelch open. |
| Menus (`ui/menu.c`, `app/menu.c` APRS group, digi items) | **Re-done** on upstream's menu tables, then the whole menu trimmed to the APRS build list (below). |
| Main screen panel / packet box / RSSI interplay (`ui/main.c`) | **Re-done** on upstream's `ui/main.c` (it differs from ta1js). Same layout rules. |
| EEPROM layout (two 16-byte APRS rows, digi byte 14, comment at 0x1BD0) | **Re-done** as one APRS settings record in a new mapped region (below). |
| UART commands (`0x0706`, `0x0708` raw TX, `0x070A/0x070B` digi, `APRS:`/`APRSRAW:`/`DIGI:` lines) | **Port**, sent to both VCP and UART. Same command numbers and layouts so `aprs_pc.py` stays compatible, plus new `setup` commands. |
| `ENABLE_2M_BAND_ONLY` / band window | **Replaced** by a US 2 m + 70 cm band table. |
| Host tests `utils/aprs_hdlc_test.c`, `utils/test_aprs_pc.py` | **Port**, plus new demodulator/modulator tests. |
| `utils/aprs_pc.py`, web beacon, code generator | **Port**; serial over USB-C. |
| Radio card `claude/radio-card.html` | **Rewrite** for this firmware's menus, keys and flashing. |
| Build env `claude/build-env/` (Docker, gcc 10.3, Makefile) | **Not ported.** Use upstream's `compile-firmware.sh` (see *Build*). |

Licence: both projects are Apache 2.0. Ported files keep their copyright headers (ta1js
/ DualTachyon / armel / muzkr as applicable), and the fork's `NOTICE` gets a line for
the ta1js-derived code.

## Build

- Upstream's `compile-firmware.sh <Preset>` builds in Docker, with the **ARM GNU
  toolchain 13.3.rel1** pinned by the `Dockerfile` (`ARM_GCC_VERSION`). It does **not**
  prune Docker data (unlike the old repo's script), so it is safe to use as is. It
  reports flash and RAM per preset against the 118 KB / 16 KB limits.
- New preset **`APRS`** in `CMakePresets.json`, based on **Fusion** (the everyday
  reference edition), with:
  - `ENABLE_APRS` (and sub-flags mirroring the ta1js ones: `ENABLE_APRS_DIGI`,
    `ENABLE_APRS_LAST_HEARD`, `ENABLE_APRS_PANEL`, `ENABLE_APRS_ARROW_TEXT`,
    `ENABLE_APRS_SAVE_COMMENT`), `ENABLE_APRS_MENU_ONLY` (the trimmed menu),
    `ENABLE_US_2M_70CM_ONLY` (band table).
  - `ENABLE_SPECTRUM`, `ENABLE_FEAT_F4HWN_SPECTRUM`, `ENABLE_FMRADIO` off;
    `ENABLE_VOICE` off (APRS owns PA4 and the DAC); K5Viewer kept;
    `ENABLE_FEAT_F4HWN_FULL_WATCH` off (dual/full watch splits listening and misses
    packets).
  - `compile-firmware.sh` gets `APRS` added to its preset list. `All` keeps building
    only the release presets.
- `CMakeLists.txt`: the new `ENABLE_*` options default off, so every other preset
  is unaffected.
- **Byte-identical guard:** before any code change, build every release preset
  (Fusion, Transfer, FieldOps, Labs, Max) and save the `.bin` files and SHA-256 as
  `claude/baseline/`. After every change, rebuild and compare. Only `APRS` may differ. `claude/baseline/check.sh` does the comparison; it tolerates the few bytes of `__DATE__`/`__TIME__` (`App/version.c`) that change per build.
- Host tests run on the Mac with `cc` and `python3 -m pytest`, outside Docker.

## Settings storage

- One **APRS record** (about 96 bytes): magic + version, callsign, SSID, location,
  interval, MsgTo, comment, digi byte (mode, hops, delay, beacon type, same packing as
  ta1js byte 14), APRS on/off, TX tone level and twist, spare bytes.
- New `ADDR_MAPPINGS` entry in its own SPI-flash sector, outside every range upstream
  maps or documents (calibration `0x010000`, boot logo `0x011000`, RX/TX log
  `0x1E0000`), and outside multiboot's slots (check `mb_flash.c`). Picked in step 1.
- Written only on a menu save or a `setup` command, never for counters or traffic.
  A sector erase blocks for tens of ms: RX sampling must survive it (see risks).
- **Multiconfig (`SetCfg`) banks:** the APRS record is **global**, not per bank, so a
  bank switch never changes the station's callsign or digi mode. Confirm on the bench
  that a bank switch leaves the APRS sector alone.
- A blank or bad record (magic, version or printable-text checks fail) loads
  defaults: `N0CALL` (TX refused until set), APRS off, digi off.

## Menu (APRS build)

Built from upstream's `MenuList`, with items compiled out of the list (not deleted) so
other presets are untouched. Their values stay in storage and remain settable by
CHIRP / UV Studio.

| Group | Items |
|---|---|
| APRS | APRS, Digi, DHops, DDly, BcnTy, DStat, Intv, Call, SSID, Loc, Cmnt, MsgTo, Msg, Send, RdMsg, BEACON |
| RF | Power, Sql, W/N, Step, TxTOut, BusyCL |
| Battery | BatSav, BatTyp, BatTxt |
| Lock | KeyLck |

- **Fixed in the APRS build:** RxMode is forced to MAIN ONLY while APRS is on (dual
  watch was the ta1js "misses packets" trap). Battery save stays off while APRS is on,
  as in ta1js.
- **Hidden service menu** (power-on key combo: calibration, F Lock, Reset): kept as
  upstream has it (decided 2026-10-07).
- APRS text entry (Call, Cmnt, MsgTo, Msg) uses arrow entry, opening empty, with
  LEFT/RIGHT on the K1.

## Interfaces (USB-C VCP + K-plug UART)

- Every APRS reply and line goes to the port the command came from. `APRS:`,
  `APRSRAW:` and `DIGI:` lines go to whichever port is open (VCP if enumerated, UART
  otherwise, or both).
- Kept commands: `0x0706` APRS on/off, `0x0708` raw TX, `0x070A/0x070B` digi
  status/settings, `0x0B02` radio status. Layouts pinned by `_Static_assert`.
- **New `setup` commands:** read and write the whole APRS record (callsign, SSID,
  location, interval, comment, MsgTo, digi byte, APRS on, tone level/twist), validated
  in full before anything is written. `aprs_pc.py <port> setup get|set --file site.json`
  so every ridge digi is configured from one saved file.
- `aprs_pc.py` finds the radio's USB serial port (macOS `/dev/cu.usbmodem*`).
- Check that upstream's UV Studio / K5Viewer protocol and the APRS commands coexist
  on the VCP (command IDs must not collide).

## Site power

Sites run on the radio's battery pack, possibly with USB-C power connected too.

- **Draw is continuous.** APRS on disables battery save and sleep, and the demod keeps
  the MCU, ADC and DMA busy all the time. Measure RX current and per-beacon/per-repeat
  TX energy on the bench (step 10), then work out hours of runtime per pack (1600 and
  2200 mAh) at expected traffic levels. Battery-only sites may need a lower TX power or
  a longer beacon interval.
- **USB-C connected:** the VCP enumerates if a host is attached, but a plain charger
  must not stall anything (no host = no VCP traffic). Bench checks: the radio charges
  while transmitting and receiving; no USB/charger noise reaches the PA4 audio tap
  (decode rate with and without the charger); TX works with USB connected; the radio
  boots straight back into APRS + digi when USB power returns after the battery ran
  flat.
- **Low battery:** upstream refuses TX below its battery threshold. The digi must log
  that as a drop (`DIGI:NOTX`, as in ta1js), not wedge, and resume once charged.
  `0x0B02` already reports battery mV over USB.
- **Option (not in scope yet):** put the battery voltage in the digi's beacon comment
  so a site's battery can be watched on aprs.fi. Raise with the club before adding.

## Plan (in order)

Each step follows the user's TDD rule: host tests written first where the code is
host-testable, then the code, then a code review, then a commit on the fork branch.

0. **Setup.** Fork `armel/uv-k1-k5v3-firmware-custom` to tigfox's GitHub, add it as a
   remote, create branch `aprs`. Record the baseline images and hashes (see *Build*).
   Back up the V3's calibration with UV Studio. Confirm the DFU recovery path and
   multiboot's protected slot on the bench before anything experimental is flashed.
1. **Preset and skeleton.** `APRS` preset, empty `ENABLE_APRS*` flags, the US 2 m +
   70 cm band table, the APRS settings sector and record (load/save/defaults,
   host-tested), the trimmed menu with the APRS items as placeholders. Build, check the
   guard, flash, confirm the radio works normally.
2. **Port the hardware-free code + tests.** AX.25/HDLC/FCS, parser, message/ack and
   beacon builders, digi core, last heard, arrow text, counters. Port
   `aprs_hdlc_test.c` first and make it pass against the new files.
3. **Software RX modem.** Port the RX app's demodulator to a resident module:
   - Timer-triggered ADC on PA4 at 9.6 kHz into a DMA circular buffer. Half/full
     transfer interrupt at low priority runs the demod over that half, so main-loop
     stalls (display, SPI flash) don't drop samples.
   - DAC holds PA4 at mid-scale. Battery reads borrow the ADC between halves (or are
     added to the conversion sequence).
   - BK4829 AF path kept open while APRS is on; the speaker amp follows squelch as
     normal.
   - Host test: feed the RX app's synthetic audio (`test/model_rx.py`, the Flipper
     vectors) through the C demod and check the decoded frames match the model.
   - On the radio: measure CPU load (cycles per sample from the SysTick counter, budget
     5000 at 48 MHz) and check the UI stays responsive.
4. **Software TX modem.** Port the TX app's tone switching, timed by a hardware timer
   rather than a busy loop. RX sampling stopped during TX. Level and twist stored in the
   APRS record. Host test: modulate, then demodulate with the C demod (the TX app's
   `test/tx_model.py` does the same in Python).
5. **APRS task.** Beacon / auto-beacon (BcnTy MOBILE `/>` + `WIDE1-1,WIDE2-1`, DIGI
   `/#` no path), messaging with line numbers and auto-ack, RX display box, RdMsg.
   First on-air test: beacon received by an SDR/Dire Wolf and a hardware-TNC radio;
   packets from the network decoded.
6. **Digi.** Wire the digi core to the new modem: 10 ms poll, random delay, busy =
   slicer activity or squelch open, cancel rule, `DIGI:` lines, DStat.
7. **Screen.** APRS panel (last heard + HRD/RPT/DUP/DRP), packet box, RSSI bar
   interplay, re-done on upstream `ui/main.c`, for both V3 and K1 layouts.
8. **USB commands and `aprs_pc.py`.** Port the commands and pytest suite to VCP + UART;
   add `setup get/set`. Port the web beacon / code generator if still wanted.
   Companion web tools from ta1js (`utils/*.html`, Chrome Web Serial): the
   **code generator** (`aprs-location.html`, phone GPS -> the 15-digit `Loc` code plus
   two-digit codes for callsign/message text, for entering with the keypad) and the
   **web beacon** (`aprs-web-beacon.html`, beacons a laptop's or Android phone's live GPS
   position through the radio over USB, with SmartBeaconing). Port only if wanted (open
   item).
9. **Radio card and docs.** New card for this firmware (menus, arrows on K5 V3 vs K1,
   flashing with UV Studio, calibration backup). README section for the `APRS` preset.
10. **Bench, then the ta1js bench list:** squelch/busy behaviour, two-digi collision
    rate vs DDly, TX with USB-C connected, held-arrow speed, panel layout, power loss
    resume, SPI-flash write during RX, CPU headroom while a packet decodes during a menu
    save, and the *Site power* checks (current draw, runtime per pack, USB charging
    while operating, charger noise, flat-battery recovery).

## Testing strategy

1. **Host tests (Mac):** ported `aprs_hdlc_test.c` (HDLC, parser, digi 40+ checks,
   formatters), new demod/mod round-trip tests driven by the RX/TX apps' Python models,
   settings record load/save, `aprs_pc.py` pytest. Target 80 % line coverage of the
   hardware-free APRS files.
2. **Build checks:** `compile-firmware.sh APRS` within 118 KB / 16 KB; all release
   presets byte-identical to `claude/baseline/`.
3. **On air:** two radios (V3 + another) and an observer (RTL-SDR + Dire Wolf, which
   prints used-hop `*` markers), dummy load or low power on the club frequency. Raw TX
   (`0x0708`) for hand-built frames: `WIDE2-2`, full 8-hop path, own call, duplicates.
4. **Recovery:** multiboot main slot + DFU mode via UV Studio; calibration backup taken
   before the first experimental flash.

## Multiboot findings (step 0, from `mb_flash.c` / `ui/multiboot.c`)

- Slot 0 (Main) is firmware-managed and cannot be written from the host, **but a normal
  flash of a different build is adopted as Main at the next boot**, erasing the old Main.
  Main is therefore not a safe place for the known-good image. Slot 1 holds stock Fusion
  v6.1.0 for recovery.
- A build that crashes before `main.c` resolves the boot state is never adopted.
- Slots are 128 KiB each; the 118 KiB flash limit keeps every image inside.
- Config banks follow slots: settings and the APRS-record decision (global, not per bank)
  need checking against `MB_BankBase` in step 1.

## Risks

1. **CPU and timing of the resident demod.** The RX app owns the CPU and serves keys
   only every 50 ms. Resident, it shares the CPU with the UI, SPI-flash writes, USB and
   display. Interrupt-driven sampling with a DMA buffer is the mitigation. Measure in
   step 3. If 3 slicers are too heavy, drop to 1-2 (the RX app's `test/variants.py`
   quantifies the cost in decode rate).
2. **Interrupt masking.** `mb_flash.c` and `ui/lock.c` disable interrupts; SPI-flash
   writes block. Any long masked section drops samples. Check each one's duration;
   multiboot writes only happen while flashing, which is acceptable.
3. **ADC sharing** with the battery reading, and **PA4 ownership** (voice DAC). Voice
   must stay off in the APRS build.
4. **AF path with squelch closed.** The demod needs BK4829 audio whether or not the
   speaker is on. Check that upstream's squelch/mute handling leaves the AF output
   running, and that this doesn't change squelch behaviour for the user.
5. **TX timing** while interrupts run: bit edges must stay within a few µs of 833 µs.
   A timer interrupt for the edges avoids busy-loop jitter.
6. **Upstream drift.** armel ships often. Keep the diff in upstream files small; merge
   upstream releases into `aprs` periodically.
7. **Two keypads** (K5 V3 UP/DOWN vs K1 LEFT/RIGHT) and possibly different screen
   details. No K1 is available, so K1-only differences can't be tested.

## Unattended-site checks (carried over)

- APRS on and Digi mode persist; the digi returns after a power loss (resume state on,
  scan resume off).
- Nothing gets stuck: RX re-arms after every TX and after any stall.
- APRS on disables battery save and sleep: continuous RX draw. Sites run on the radio
  battery, possibly with USB-C power (see *Site power*).
- Sensible continuous-duty TX power; VFO stays on the digi frequency.
- Squelch above 0 if squelch is used for busy detection.
- Auto-beacon provides station identification.

## Step 1 status (2026-10-07)

Done: `APRS` preset, settings record (`app/aprs_settings.c`, 96 B at EEPROM `0x00D000` =
SPI `0x012000`, shared across config banks), band windows (`app/aprs_bands.c`), flat APRS-first
menu (placeholders, read-only; `MENU_CAT` off), host tests in `tools/aprs` (`make -C tools/aprs test`).
Code review fixes: per-channel `TX_LOCK` can no longer bypass the band limit
(`TX_LOCK_APPLIES`), strings zero-padded on encode, EEPROM address/size tied by constants.
Known gap: RX confinement only gates keypad entry and VFO stepping; stored VFO/channel
frequencies outside the windows still receive (TX is blocked). Decide on bench whether to clamp
with `APRS_FreqClamp` at VFO load. Flashed 2026-10-07: radio boots and operates normally; the
menu opens, the APRS-first list shows, and the RF/battery items work. The APRS items open but
cannot be changed yet (read-only placeholders by design; editing is step 5 for the text items).

## Step 2 status (2026-10-07)

Ported hardware-free code into `App/app/`: `aprs_ax25` (CRC/FCS, address, HDLC encode),
`aprs_parse` (uncompressed / Mic-E / compressed, distance, Loc code decode, message-to-me),
`aprs_beacon`, `aprs_msg` (builders take `aprs_settings_t`; tocall `APZK5`), `aprs_digi`,
`aprs_lastheard`, `aprs_text`. Host tests link the real files (nothing copied):
`make -C tools/aprs test`, `make -C tools/aprs coverage` (93.6 % lines). Not ported: the BK4819
FSK capture decoder and its streaming-RX test (replaced by the step 3 demodulator), and the
third-party `}` unwrap tests (the ta1js firmware never had that code in `APRS_ShowFrame`).
Changes vs ta1js: beacon/message comment up to 43 chars (was 31), a station with no valid Loc
sends no beacon (ta1js sent 0/0), digi core gained `DIGI_Reset`. Compiled into the APRS preset;
nothing calls them yet, so the image is unchanged.

## Step 3 status (2026-10-07)

**Works on the radio** (user, 2026-10-07): a beacon from another radio increased the heard count; DStat
showed ISR avg 16 us, max 32 us (budget 104); the screen stayed responsive. Squelch/AF behaviour held up.
Built as follows. `app/aprs_demod.c` is a C transcription of armel's demodulator;
`make -C tools/aprs test` feeds it ADC vectors generated from his Python model
(`tools/aprs/gen_vectors.py`, `vectors/`): all 15 cases decode exactly what the model decodes (13/13
true frames, the bad-FCS frame rejected), plus 30 s of noise and silence with no false frame.
`driver/aprs_rx.c`: **TIM6** update interrupt at 9.6 kHz (priority 1) reads ADC1 channel 4 (PA4) and
runs the demodulator in the ISR; frames go to a 2-slot queue for the main loop. Design change from the
plan: **no DMA and no half-buffer**; the ISR is short enough (measured on the radio: see DStat) and
interrupts stay enabled during SPI-flash writes. The battery reading shares ADC1: `board.c` brackets it
with `APRS_RxAdcAcquire/Release` (the tick is delayed, not lost). PA4 is held at mid-scale by DAC1
(buffer off); the APRS build refuses `ENABLE_VOICE`. APRS on disables battery save
(`app.c`). The menu item **APRS** now works (OFF/ON, saved, starts/stops the receiver live);
**DStat** shows `HRD n` and rotates last heard / mean ISR µs / max ISR µs. Each decoded frame beeps.
RAM: 12.9 KiB of 16 (demodulator ~1.3 KB, queue 0.7 KB).
Bench note (user, 2026-10-07): with APRS on the VFO B line is still shown; at step 7 replace it with the
ta1js panel (last heard + HRD/RPT/DUP/DRP counters, `UI_DisplayAPRSPanel`), and force the two-row layout
while APRS is on. Also force RxMode to MAIN ONLY while APRS is on (step 5/7).
Known gaps: sleep mode (SetOff) is not yet disabled while APRS runs; squelch/AF behaviour unverified.

## Step 4 status (2026-10-07)

Built, **not yet run on the radio**. `app/aprs_modem.c` (tone registers, line levels, level 10..127 /
twist -4..8 as armel's TX app; the settings record's ranges and defaults changed to match, so a record
saved by the step 3 build is rejected once and APRS comes up OFF) and `driver/aprs_tx.c`
(`APRS_TxSend`): receiver paused, `RADIO_SetTxParameters`, REG_51 = 0, `BK4819_TransmitTone(1200)`,
then REG_71/REG_70 rewritten at each NRZI transition on a SysTick cycle counter (40000 cycles/bit),
`RADIO_SetupRegisters(true)` to return to RX. Blocking (~0.8 s for a beacon), like armel's app; no
`FUNCTION_TRANSMIT` state, so no TX timers / roger / RXTX log. Host test (`test_aprs_modem.c`): builder
frames -> HDLC -> synthesized phase-continuous AFSK at the tone schedule -> the C demodulator decodes
beacon, digi beacon, message, ack, 150-byte frame; 16/16 level x twist grid, +-0.8 % bit clock, noise
6/6; a corrupted frame is rejected. The menu **BEACON** item (YES) sends the station beacon, but a
beacon needs a callsign and a Loc, which cannot be entered yet.

## Menu editing (pulled forward from step 5 for the transmit test, 2026-10-07)

Choice items edit with the arrows and save on MENU: APRS, Digi, DHops (1-7), DDly, BcnTy, Intv (OFF, 1, 2,
5, 10, 15, 30, 60 min), SSID. Text fields (**Call, Loc, Cmnt, MsgTo**) have a self-contained editor
(`app/aprs_edit.c`, hooked at the top of `MENU_ProcessKeys`): MENU opens it, the field opens empty,
UP/DOWN walk the characters at the cursor, the digit keys type a digit, STAR moves to the next position,
EXIT backspaces (and leaves when empty), MENU commits. Committing a blank field keeps the old value.
Loc is the 15-digit code (code generator), shown afterwards as latitude / longitude; a wrong checksum is
refused with a double beep. Msg, Send and RdMsg are still step 5. Digi/DHops/DDly/BcnTy/Intv are stored
only; the digi and the auto-beacon act on them in steps 5/6.

## Step 5 status (2026-10-07)

Built, **not yet run on the radio**. Hardware-free (host-tested, `test_aprs_station.c`): `app/aprs_rxinfo.c`
(what a received frame means: own / to me / ack, the text to show, the line number to acknowledge) and
`app/aprs_station.c` (the state machine: auto-beacon timer, queued Send / BEACON, ack for a numbered
message, reply target, RdMsg, line numbers with retry semantics). Priority on the air: ack, then message,
then beacon. Firmware (`app/aprs_task.c`): every 500 ms the station is asked what to send; it goes out when
the receiver reports no packet in progress, the squelch is closed and PTT is not down, else it is offered
again next slot. A refused transmission (band, battery, no call) is dropped with a double beep. The first
auto-beacon is 15 s after APRS is switched on, then every Intv. A beacon with no valid Loc is dropped and
the timer re-armed. **Main-only listening is enforced** while APRS is on (RxMode forced to MAIN ONLY at
start and every 500 ms; the VFO B panel is step 7). Menu: **Msg** (text, RAM only), **MsgTo** (stored; the
RAM copy follows whoever wrote last, as in ta1js), **Send** (NO/SEND, queues), **RdMsg** (pages of 16
characters, newest message; an incoming message beeps three times), **BEACON** now queues instead of
blocking in the key handler. Received packets are not drawn on the main screen yet (step 7): DStat shows the
last callsign heard. RAM is now 13.5 of 16 KiB (the TX bit buffers are static, so the stack is not deeper):
watch the stack in step 10.

### Step 4-5 review fixes (2026-10-07)

Fixed: the menu editor no longer wedges after a menu timeout (state resets when the sub-menu closes; a
long EXIT cancels); no transmission while scanning, a CSS scan, serial configuration, or the FM radio;
TX compander forced off; `APRS_CallIsSet` now needs >= 3 characters with a digit and a letter (NOCALL
and N0CALL refused); queued transmissions are dropped after a minute of busy channel and when APRS is
switched off; at least 5 s between our transmissions (an ack storm cannot hold the carrier); the beacon
timer re-arms only when APRS or Intv changes; ack line numbers are echoed only if alphanumeric; `is_ack`
needs the word alone; receive frame buffers cut to 256 bytes (RAM 13.4 of 16 KiB). **Frequency policy (user, 2026-10-07):**
the band windows are enough; no APRS-frequency allow-list. An automatic transmission goes out on whatever
the main VFO is tuned to inside 144-148 / 420-450 MHz. Carrier sense is the squelch only (a CTCSS-gated
signal would not be heard). No TX LED or burst counter yet.

## Step 5 bench result (user, 2026-10-07)

A message addressed to another radio was sent and its ack received. The units will have no location and
no GPS, so with no Loc the beacon is a **status packet** (`>comment`, `>UV-K5 APRS` if the comment is
empty; direct path for BcnTy DIGI) instead of being dropped: the station still identifies itself.

## Step 6 status (2026-10-07)

Built, **not yet run on the radio**. The digi core (ported in step 2) is wired in: every decoded frame goes
to `DIGI_Consider` (only with APRS on and a real callsign); a 10 ms poll hands the due repeat to
`APRS_TxSend` unless the receiver reports a packet in progress or the squelch (above 0) is open; a refused
transmission counts as dropped. Settings (Digi OFF/FILL/WIDE, DHops, DDly) apply immediately; APRS off
clears the repeat queue. DStat now has eight views you can step through with the arrows once it is open:
last heard, RPT, DUP, CNL, HOP, DRP, interrupt mean and maximum. Host test of a whole hop: a mobile's
WIDE1-1 beacon -> modulate -> demodulate -> digi queues -> repeat modulated -> second receiver decodes
`W1ABC-7*` in the path. Not yet: the `DIGI:` log lines (step 8) and the last-heard repeat marker (step 7).

## Step 7 status (2026-10-07)

Built, **not yet run on the radio**. With APRS on, the main screen keeps the two-row layout and the second
VFO's half shows the APRS panel (ta1js layout): `LH W1AW-9 12m*` (who was last heard, how long ago, `*` if we
repeated it) over `HRD`/`RPT` and `DUP`/`DRP` counters (heard, repeated, duplicate, dropped). The packet box
is a framed overlay over rows 3-6 with up to three lines of 16 characters: the last decoded packet for 30 s
(`W1ABC-7 12.3km`, or `:payload`; just the callsign when we have no location), and a message to us stays until
a key closes it (the key is used up; PTT passes through) or a newer message arrives; ordinary traffic does not
replace it. A message to us also lights the backlight. The panel and box show only while APRS is on; the
RSSI bar and the VFO A rows are untouched. Layout only: `isMainOnly()` returns false while APRS is on, listening
stays main-only (`app/aprs_task.c`). Unchanged presets are byte-identical.

## Splash (scope addition, user, 2026-10-07)

The boot screen's version line (`F4HWN v6.1.0`) reads **`KD2DCM-APRS`** in the APRS preset (CMake variable
`DISPLAY_VERSION_OVERRIDE` in `CMakePresets.json`; change it there). Only the splash text changes: the
version string UV Studio reads over the cable stays `EGZUMER+F4HWN v6.1.0` so its firmware detection is
unaffected. The line below it still reads `APRS Edition`.

## Step 8 status (2026-10-07)

Built, **not yet run on the radio**. Commands 0x0700-0x0710 on the USB-C serial port and the K-plug UART
(`app/aprs_cmd.c`, hardware-free, host-tested with stubbed radio operations; layouts in `app/aprs_cmd.h`):
message, beacon, APRS on/off, raw AX.25 transmit (our own callsign as the source only), digi get/set (the
ta1js 0x070A/0x070B layout), **setup get/set of the whole 96-byte settings record** (validated in full, all or
nothing), and a per-port monitor switch. Everything that changes a setting or transmits must repeat the
0x0514 session timestamp. Monitor lines `APRSRAW:` / `APRS:` / `DIGI:` (`app/aprs_serial.c`) go only to a port
that sent 0x0710, so UV Studio / K5Viewer never see text on the shared USB port. `tools/aprs/aprs_pc.py`
(+ `test_aprs_pc.py`, 69 tests; its record packing is pinned byte for byte to the C output) does
`status`, `msg`, `beacon`, `on/off`, `raw`, `digi`, `setup get/set file.json`, `monitor` and `loc LAT LON`.
`msg` is the serial-to-APRS-message path asked about earlier. A transmission refused only because the radio
is scanning or being configured stays queued (up to a minute) instead of being dropped. DStat has a ninth
view, `stk N`: the least stack ever left free, to size the RAM margin (APRS RAM is now 14.0 of 16 KiB).
Not ported: the web beacon (mobile GPS), the code generator page (`aprs_pc.py loc` replaces it).

## Open items

- Whether the code generator and web beacon tools are wanted (step 8). With the arrow
  entry and `aprs_pc.py setup`, the code generator is mostly redundant; the web beacon
  is only useful for a mobile station, not a digi.
- Battery voltage in the digi beacon comment (see *Site power*): ask the club.
- 70 cm APRS has no common US frequency; the band is allowed but the club decides any
  use.
