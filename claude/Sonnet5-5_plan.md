# APRS digipeater plan

Started 2026-10-02 from a read of `app/aprs_minimal.c`, `docs/APRS.md`,
`docs/flash-budget.md`, `Makefile`, `app/uart.c`, `firmware.ld`. The sections below are
in the order the work happened; this one is the current state.

**Not our repo.** Nothing is committed or pushed upstream; all changes are uncommitted
in the working tree, and `claude/` is untracked. Builds are local, not via CI.

## Current status (2026-10-05)

**All five plan steps are done, plus arrow text entry and the APRS panel.** The
`PROFILE=digi2m` image (`claude/build-env/out/digi2m.bin`, 60972 bytes, 468 free) has
been flashed to a radio and **the user reports it working** (2026-10-05, first
impressions; the structured bench tests below have not been reported yet).

What the digi image contains, by flag (all set by `PROFILE=digi2m`; the standard
`make` images are byte-identical to upstream HEAD `6320cda`):

| Flag | What |
|---|---|
| `ENABLE_2M_BAND_ONLY` | VFO/TX window `BAND_2M_LOWER/UPPER`, default 144-146 MHz |
| `ENABLE_APRS_DIGI` | FILL / WIDE (New-N) digi, hop limit, dedupe, delay, busy, menus, `0x070A` |
| `ENABLE_APRS_SAVE_COMMENT` | beacon comment in EEPROM 0x1BD0 |
| `ENABLE_APRS_LAST_HEARD` | last station heard + age |
| `ENABLE_APRS_ARROW_TEXT` | text fields spelled with arrows + STAR; fields open empty |
| `ENABLE_APRS_PANEL` | lower half of the screen: last heard + 4 counters (APRS on) |
| `ENABLE_RESUME_SCAN=0` | never resume a scan after power loss |
| cuts | AM fix, scan ranges, narrower, copy-chan, flashlight, wide RX, F4HWN CA/CTR/INV, extra squelch steps, RX position decoding |

Kept: RSSI meter (back in, step 4), UART remote control.

**Still to do (bench, before a mountain):** squelch > 0 busy detection; decode lag vs
random delay with two digis (collision rate); TX with the programming cable left in;
held-arrow scroll speed; panel/RSSI/packet-box layout on the real screen. Back up
each radio's EEPROM (calibration) before flashing.

**Deferred by the user:** UART commands to set callsign/SSID/comment/location/
frequency from a laptop (`aprs_pc.py setup`), estimated 250-350 bytes. Not now.

**Flashing (this Mac):** `utils/k5flash.py` with pyserial in `~/k5venv`;
the CH340 cable (`0x1A86:0x7523`) appears as `/dev/cu.usbserial-210` and works on
Apple's driver. Radio off, hold PTT, power on, then
`~/k5venv/bin/python utils/k5flash.py /dev/cu.usbserial-210 claude/build-env/out/digi2m.bin '*CLUB v1.0'`.
The README's `utils/eeprom_tool.py` does not exist; back up with k5prog or CHIRP.

**Hand-out:** `claude/radio-card.html`, the local file is the copy in use (open in a
browser, print two-sided on white paper). Not hosted.

**Verify after any change:** `sh claude/build-env/run.sh` (all variants; the
`expect fail` lines must fail), then `cmp claude/build-env/baseline/amateur.bin
claude/build-env/out/amateur.bin` and the same for `allband.bin` (upstream HEAD
`6320cda` builds: the standard images must stay byte-identical), `cc -o t
utils/aprs_hdlc_test.c && ./t` (every suite PASSED), and `cd utils && python3 -m pytest
-q test_aprs_pc.py` (19 passed).

## Starting point

A fill-in digipeater already exists (commit `d49b76e`, `ENABLE_APRS_DIGI`, off in
shipped images):

- Repeats only a first-unused `WIDE1-1` hop, substituting `MYCALL-ssid*`. The frame
  length is unchanged, so it is resent in place with the FCS recomputed
  (`APRS_DigiConsider`, `app/aprs_minimal.c:940`).
- Loop protection: 4 entries, 16-bit key (source CRC ^ info CRC), 30 s window.
- One queued frame, transmitted from `APRS_Task`'s IDLE branch.
- Menu item "Digi", persisted in EEPROM `State[14]`.
- The host test (`utils/aprs_hdlc_test.c`) tests a hand-copied mirror of the logic,
  not the real function. It passes today (`DIGIPEATER PASSED`, 2026-10-02).

## Decisions made

| Question | Decision |
|---|---|
| Audience | Personal and club use, amateur radio club. Not public infrastructure. |
| Environment | Remote, mountainous, non-standard APRS frequency, low noise. |
| Image | Any. Only 2 m APRS is needed, no other use for this firmware. |
| TX tones | 1200/1800 Hz limitation accepted. Hardware-TNC radios (Kenwood D7x/D74/D75, Yaesu) will be replaced. Real Bell 202 (issue #1) is out of scope. |
| Topology | Multiple digis on the network, possibly all running this firmware. |
| `ENABLE_UART_RC` | **Keep it.** The club wants to read radio status (`0x0B02`: VFO, frequency, TX/RX, power, bandwidth, squelch, RSSI, battery mV) over USB serial. It costs 440 bytes, which is not part of the flash headroom estimate below. |
| Repo / builds | Not our repo: no commits upstream yet. Build locally with the pinned toolchain (see *Build environment*). |
| RSSI meter | Wanted, but **last priority for flash**. Cut in step 1 to fund the digi, re-added in step 4 only if it fits after the digi work. |
| Last-message line | **Stretch goal** (step 5), only if flash remains after the RSSI meter. Shows the **last packet's sender callsign and a timestamp**. Priority below the RSSI meter confirmed. |

## Build environment

`claude/build-env/` holds a local Docker build pinned to the toolchain CI uses,
**arm-none-eabi-gcc 10.3-2021.10** (ARM's official aarch64 tarball on Debian
bookworm). The compiler version matters: flash figures shift with it.

```sh
sh claude/build-env/run.sh          # from the repo root: build every variant
sh claude/build-env/run.sh clean    # remove this project's image + outputs only
```

- Builds every line of `claude/build-env/variants` (`label|make args`).
- The repo is mounted **read-only** and copied inside the container, so the working
  tree is never touched. Images go to `claude/build-env/out/` (git-ignored).
- Prints flash free and RAM (`.data` + `.bss`) for each variant, and saves the
  compiled-in defines to `out/<label>.defs`. Lines labelled `expect fail` are guard
  tests that must fail.
- Docker hygiene: it only touches its own image (`uvk5-gcc103`, ~1.3 GB) and its own
  `--rm` containers. It never prunes, so other projects' Docker data is untouched.
- Do **not** use the repo's `compile-with-docker.sh`: it runs
  `docker system prune -f --volumes`, which deletes all unused Docker data on the
  machine, and it builds with Alpine's unpinned compiler.

## Baseline measurements (2026-10-02, HEAD `6320cda`)

| Variant | Flash used | Flash free | RAM (.data+.bss) | RAM left for stack |
|---|---|---|---|---|
| amateur (default `make`) | 61268 | 172 | 3580 | 12804 |
| all-band | 61320 | 120 | 3580 | 12804 |
| all-band + digi | 61212 | 228 | 3636 | 12748 |
| amateur + digi | 61172 | 268 | 3636 | 12748 |

- The amateur figure matches `docs/flash-budget.md` exactly, so this environment
  reproduces the published numbers.
- **The digi now fits the amateur image** (268 free). The Makefile comment saying it
  overflows by ~200 bytes is out of date; the later `RESUME_STATE` coupling freed space.
  Enabling the digi still forces `ENABLE_AM_FIX=0`.
- **RAM is not a constraint.** 16 KB total, about 12.7 KB left for the stack. Growing
  the RX/digi buffers by a few hundred bytes is fine. Largest RAM users: framebuffer
  896, `gEeprom` 324, UART buffers 2 × 256, HDLC buffer 240, `gRxFrame` 80.

## Gaps in the existing digi

1. No WIDEn-N handling: `WIDE2-N` decrement, callsign insertion, explicit routing.
2. `gRxFrame` is 80 bytes. Longer frames are dropped by the decoder and never repeated.
   The TX bitstream buffer caps frames at about 170 bytes.
3. No carrier sense or backoff. `RADIO_PrepareTX` only refuses when `BUSY_CHANNEL_LOCK`
   is set and the radio is mid-receive (`radio.c:1123`), and then the repeat is silently
   dropped.
4. About 2-3 s deaf time per repeat (blocking TX ~1 s, 1 s cooldown, re-arm on the 500 ms
   tick). Single-frame queue.
5. Dedupe key is weak (16-bit, 4 slots) and does not cancel a queued repeat when another
   digi's copy is heard first.
6. Beacon is hardcoded to the `/>` car symbol with a `WIDE1-1,WIDE2-1` path. A digi should
   beacon as `#` and not ask other digis to repeat it.
7. "Digi" only works while APRS listening is also ON, and nothing says so.
8. Test mirror can drift from the real code.

## Flash

- Measured costs are in `docs/flash-budget.md` (2026-07-27, same toolchain). LTO makes
  them non-additive, so re-measure every combination with `claude/build-env/run.sh`.
- Candidate cuts for a 2 m-only image (UART_RC is deliberately *not* on the list):

  | Cut | Bytes |
  |---|---|
  | `ENABLE_AM_FIX` | 616 (already cut by the digi flag) |
  | `ENABLE_RSSI_BAR` | 564 (re-added in step 4 if it fits) |
  | `ENABLE_SCAN_RANGES` | 384 |
  | `ENABLE_FEAT_F4HWN_NARROWER` | 192 |
  | small items (copy-chan-to-VFO, flashlight, wide RX, F4HWN extras) | ~400 |
  | **Subtotal beyond AM_FIX** | **~1.5 KB**, plus the 268 already free |

- The amateur-band-only flag is replaced by a 2 m-only profile. Watch the coupling:
  `ENABLE_AMATEUR_BAND_ONLY` also turns `RESUME_STATE` off.

## Feasibility risks

None are blockers. Items 2 and 3 are where the bench time is expected to go.

1. ~~RAM unmeasured~~ **Resolved:** about 12.7 KB headroom (see baseline).
2. **Timing granularity.** `APRS_Task` runs every 500 ms, too coarse for a random delay
   of a few hundred ms. The delay and cancel-on-duplicate check need the 10 ms time slice
   or the systick counter. RX must stay armed during the delay to hear a neighbour's copy.
   The most delicate change.
3. **Busy-channel detection while FSK RX is armed.** Candidates: `gRxCapturing` (packet
   sync seen; cheapest) or squelch state (`FUNCTION_RECEIVE`). Which is reliable alongside
   APRS listening must be checked on the bench.
4. **AX.25 path limit.** At most 8 digipeater addresses. Callsign insertion must refuse a
   frame whose path is already full.
5. **Some flag combinations don't compile** (`docs/flash-budget.md` lists several). The
   2 m-only profile may hit a new one; expect a fix or two in step 1.
6. **Compiler version matters for flash numbers.** Solved by the pinned build env.

## Step 1 result (done 2026-10-02, uncommitted)

`make PROFILE=digi2m` builds the dedicated digi image. **59596 bytes, 1844 free**,
RAM 3620 (12764 left for stack).

| Variant | Flash free |
|---|---|
| `PROFILE=digi2m` (144-146 MHz) | 1844 |
| `PROFILE=digi2m BAND_2M_UPPER=14800000` (Region 2) | 1836 |
| `PROFILE=digi2m ENABLE_RSSI_BAR=1` | 1300 (RSSI bar measured at **544** bytes here) |
| default amateur / all-band | 172 / 120, **byte-identical** to before (`cmp`) |

Changes:
- `Makefile`: `PROFILE=digi2m` block at the top (sets `?=` defaults, so any flag can
  still be overridden on the command line), `ENABLE_2M_BAND_ONLY` with
  `BAND_2M_LOWER/UPPER`, guard errors for an unknown profile and for both band flags at
  once; stale digi-overflow comment corrected.
- `frequencies.c`: one-entry band table for `ENABLE_2M_BAND_ONLY`, reusing the
  amateur-band machinery (VFO snap, RX/TX checks). `#error` if the window leaves
  144-148 MHz.
- `app/action.c`, `main.c`: **pre-existing upstream bug** fixed. `ENABLE_SCAN_RANGES=0`
  with `RESUME_STATE=1` never compiled (two ungated uses of `gScanRangeStart`); the
  shipped images never combine them.

Profile contents: digi on, UART_RC kept, resume-state kept (it is on whenever the
amateur flag is off, and it brings the radio back on its last frequency after power
loss). Cut: AM_FIX, RSSI bar, scan ranges, narrower, copy-chan-to-VFO, flashlight,
wide RX, F4HWN CA.

Band default **144-146 MHz** (amateur in every ITU region, so the build can never
transmit outside 2 m). Region 2 sites widen it with `BAND_2M_UPPER=14800000`. The runtime
F LOCK menu still narrows TX further.

Verification: all variants above built; the three guard errors fire as intended; host
test still passes. Not yet run on a radio.

### Scan-resume off (done 2026-10-02)

New flag `ENABLE_RESUME_SCAN` (default 1; `PROFILE=digi2m` sets 0). With resume-state
on, the radio still comes back on its frequency/channel after a power loss, but a
scan that was running is not restarted (`main.c`, boot path). Default images remain
byte-identical; digi2m is now **59580 bytes, 1860 free**.

## Step 2 + 3 result (done 2026-10-02, uncommitted, not yet on a radio)

`make PROFILE=digi2m`: **59380 bytes, 2060 free** (after review fixes), RAM ~3.9 KB
(~12.5 KB left for stack). 144-148 MHz variant 2048 free; with the RSSI meter 1520
free. Default images still byte-identical.

**Digi core** `app/aprs_digi.c` + `app/aprs_digi.h` (hardware-free; `#include`d by
`aprs_minimal.c`, and by the host test, so the tests run the real code):
- FILL: first unused WIDE1-1, `MYCALL*` substituted in place (the old behaviour).
- WIDE: also WIDEn-N, New-N: `WIDE2-2 -> MYCALL*,WIDE2-1`, `WIDE2-1 -> MYCALL*,WIDE2*`.
  `n > DHops` or `N > n` refused (`DIGI:HOPS`). Full path (8 vias) or a frame at the
  150-byte TX budget: last hop substituted, others decremented in place.
- Explicit hop `MYCALL-SSID` honoured in both modes; a path already holding `MYCALL*`
  is ignored (loop); own frames (any SSID) and N0CALL never repeated.
- Dedupe: 16 entries, 32-bit key over source + destination + payload, 30 s. Only UI
  frames (control 0x03, PID 0xF0) are repeated.
- Hop limit is on n (hops requested), not N (hops left): `WIDE4-2` is refused even
  mid-flight, so every club digi stops an over-asking packet alike (deliberate).
- Timing on the 10 ms slice (`APRS_DigiPoll`, `app/app.c`): 100 ms hold-off + random
  0..DDly; busy = packet being captured, or squelch open (`INCOMING`/`RECEIVE`, only
  when squelch level > 0); busy postpones 100-340 ms, 5 s busy drops the repeat.
- **Cancel rule (changed from the plan):** only a FILL digi yields when it hears a
  neighbour's copy first. A WIDE digi never cancels: even its WIDE1-1 repeat carries the
  WIDEn-N hops a fill-in neighbour leaves unserved (found in review, test added first).
- RX buffer 80 -> 152 bytes in digi builds; `APRSRAW:` lines now carry the full frame.

**Beacon type** (BcnTy=DIGI): `/#` symbol, no digipeater path (source is the last
address). MOBILE is the old `/>` + `WIDE1-1,WIDE2-1`.

**Menu** (APRS group, after Digi): Digi OFF/FILL/WIDE (shows `APRS OFF` under the value
when APRS listening is off), DHops shown as `WIDE2-2`, DDly OFF/250ms/500ms/1s, BcnTy
MOBILE/DIGI, DStat (read-only, small font: HRD/RPT/DUP/CNL/HOP/DRP).

**EEPROM:** all digi settings + BcnTy packed into APRS byte 14 (bits 0-1 mode, 2-4 hops,
5-6 delay, 7 beacon type). Old images wrote 1/0, which decode as FILL/OFF; 0xFF is a
fresh radio. The three other spare bytes are still free. **Comment** persisted at
0x1BD0-0x1BEF (`ENABLE_APRS_SAVE_COMMENT`), loaded only if it is printable and
NUL-terminated.

**UART:** `DIGI:<QUE|DUP|CNCL|HOPS|DROP|RPT|BUSY> <source>` lines after the frame's
`APRSRAW:` line. Command `0x070A {Set, Mode, Hops, Delay, BcnDigi}` -> `0x070B {Ok, Mode,
Hops, Delay, BcnDigi, Uptime (10 ms), 6 counters}`; Set=1 validates all four, then
applies and saves. Layout pinned by a `_Static_assert` in `app/uart.c`.
`utils/aprs_pc.py <port> digi` / `digi set WIDE 2 500ms DIGI`; `monitor` prints the
`DIGI:` lines.

**Flash cuts added to the profile** (decided 2026-10-02): position decoding
(`ENABLE_APRS_RX_POSITION=0`, ~2 KB; received packets show `CALL:raw text` instead of
`CALL 12.3km`), display contrast + invert settings, extra squelch steps (264 bytes).

**Tests:** `cc utils/aprs_hdlc_test.c` -> 40+ digi checks (fill, New-N, hop limit,
full/long path, explicit, loop, queue, delay range + randomness, busy, cancel rules),
plus the existing HDLC/decoder/parser checks. Mutation-checked: four planted bugs each
fail the suite. `python3 -m pytest utils/test_aprs_pc.py`: 17 tests for the 0x070B
reply parser and `digi set` encoding.

**Code review (everything-claude-code:code-reviewer, 2026-10-02):** no critical
findings. Fixed, each with a test written first where it is core logic:
- HIGH: a FILL digi cancelled its repeat when the *originator* resent the packet
  (tracker retry / multipath). Now a copy cancels only if it has more used hops.
- A repeat dropped for a busy channel, or refused by the radio, no longer blocks the
  sender's retry for 30 s (its key is forgotten).
- A refused TX (`RADIO_PrepareTX`: TX lock, band, battery) is counted as dropped and
  logged `DIGI:NOTX`, not `RPT` (`DIGI_Sent`).
- `0x070A` shorter than its 5 data bytes is a status request: stale buffer bytes are
  never applied to EEPROM.
- Non-UI frames ignored; 16 dedupe slots; an empty saved comment stays empty;
  `aprs_pc.py` delay names are case-insensitive.
Kept as is (documented): hop limit on n, see above.

**Bench must-checks added by this step:**
- Squelch must be **above 0** on a digi, or open-squelch noise could count as busy;
  confirm that `INCOMING`/`RECEIVE` really tracks carrier while APRS listens.
- Deaf time after each repeat: ~1 s TX + 1 s cooldown + RX re-arm on the 500 ms tick.
- Decode often lags the packet by ~1-1.5 s (RX_FINISHED rarely fires; the 500 ms
  stuck-tick path decodes), so neighbouring digis differ by more than the 0-1 s random
  delay. Measure how often two digis still collide; DDly may need its top value raised.
- The digi transmits whatever the radio is doing (menus open, etc.); it is gated only on
  APRS being on. Fine for an unattended digi; note for anyone using it on site.
- A full-size `APRSRAW:` line takes ~80 ms of UART; watch for missed packets while a
  laptop monitors a busy channel.

## Step 4 + 5 result (done 2026-10-02, uncommitted, not yet on a radio)

`make PROFILE=digi2m`: **60508 bytes, 932 free** (144-148 MHz: 920). Default images
still byte-identical.

- **Step 4, RSSI meter:** back in the profile (no longer cut). Measured cost here 540.
- **Step 5, last-heard line** (`ENABLE_APRS_LAST_HEARD`, on in the profile): main
  screen row 5 shows `LH TB1AAW-9 12m*` - the source of the last valid frame heard,
  its age (`<1m`, minutes, hours up to 47 h, then days), `*` once this digi repeated
  that station. Redrawn whenever the age reaches a new minute; hidden while APRS is off. Shown only with a single VFO (dual watch and
  cross-band off - which a digi should use anyway) and only when row 5 is free: the RSSI
  bar takes it while receiving, the packet box for 30 s after a packet. Formatter in
  `app/aprs_lastheard.c`, `#include`d by the firmware and the host test (8 cases incl.
  the 18-character limit and the tick wrap at 497 days).
- Cost: ~590 bytes, about twice the estimate; not chased, it fits.
- Code review fixes: the prefix was `LAST `, which made the line up to 20
  characters, but the small font (7 px a character from x = 2, no clipping) fits 18,
  so the tail was written into the next row's memory - now `LH `, test limit 18. The
  per-minute redraw compares minutes instead of a 500 ms window a stalled loop could
  miss. With APRS off the line is hidden instead of showing a frozen age. While
  receiving, the RSSI bar owns row 5, so on a busy channel the line shows between
  packets only (intended).

## APRS panel (done 2026-10-02, uncommitted, not yet on a radio)

`ENABLE_APRS_PANEL` (digi profile; needs the digi and last heard; standard images
byte-identical). digi2m now **60972 bytes, 468 free** (144-148 MHz: 456).
- With APRS on, the screen keeps its two-row layout even in RxMode MAIN ONLY (layout
  only - `isMainOnly()` is used by `ui/main.c` alone), and the half of the VFO that is
  not the TX VFO shows: `LH <call> <age>[*]`, then `HRD`/`RPT` and `DUP`/`DRP`, each
  counter exactly 8 characters (`APRS_FmtCount`, host-tested) so nothing runs off the
  row. CNL/HOP stay in 06 DStat. Redrawn on every digi decision and each minute.
- The main-only last-heard line on row 5 is compiled out in panel builds (it could no
  longer show), which recovered 272 bytes.
- Found while doing this: a radio that never had RxMode set runs **dual watch**
  (factory default), which splits listening between A and B and misses packets. The
  card now says why to set 71 RxMode MAIN ONLY.

## Arrow text entry (done 2026-10-02, uncommitted, not yet on a radio)

`ENABLE_APRS_ARROW_TEXT` (on in the digi profile; standard images byte-identical).
digi2m now **60724 bytes, 716 free** (144-148 MHz: 704).
- Call / MsgTo / Cmnt / Msg: up/down step through blank, A-Z, 0-9, then
  `- . / ? ! @ : , '` (Call stops before the punctuation, MsgTo keeps `-`). Held arrows
  repeat. STAR keeps the character and moves on (it used to overwrite it). EXIT steps
  back, MENU saves. The two-digit codes still work.
- A `^` caret under the edited text shows the cursor; 31-character fields show 10 at a
  time with the position (`12/31`).
- The fields **open empty**: before, new text kept the old text's tail (W1AW entered
  over N0CALL saved as `W1AWLL`, with digit codes too - a pre-existing bug, still there
  in the standard images). MENU with nothing entered, or EXIT, keeps the old value.
- Character stepping is `app/aprs_text.c`, shared with the host test (14 cases).
- Radio card (`claude/radio-card.html`) updated: arrows first, digit codes as shortcut.

## Menu and monitoring (decided 2026-10-02, build with step 2)

Confirmed: Digi mode, DHops, DDly, BcnTy, DStat, **persist the comment**, plus UART
monitoring options (below). The table further down is the agreed item list.

### UART monitoring (proposed detail, confirm before building)

Already there: `APRS:<text>` and `APRSRAW:<hex>` on every decoded frame (needs only
`ENABLE_UART`), `0x0B02` radio status (VFO, RSSI, battery mV; UART_RC), `0x0706` APRS
listening on/off. The display mirror `0x0A03` is paused while APRS listens (it blocks
interrupts ~270 ms and would drop packets), so it is no use on a digi.

Missing: anything about what the digi *decided*. Proposed:

1. **Digi decision lines**, one per considered frame, e.g. `DIGI:RPT TB1AAW-9`,
   `DIGI:DUP`, `DIGI:CNCL` (neighbour repeated first), `DIGI:HOPS` (N above DHops),
   `DIGI:BUSY`. Emitted from the main loop, never the decode/IRQ path.
2. **Digi status command** (e.g. `0x070A` -> `0x070B`): DStat counters (heard,
   repeated, duplicate, cancelled, dropped), uptime, current digi settings.
3. **Digi settings over UART** (**confirmed** 2026-10-02): read/write mode, DHops, DDly,
   BcnTy so a laptop can configure every ridge digi identically instead of keying menus.
4. `utils/aprs_pc.py`: a `status` command, and `monitor` also prints `DIGI:` lines.

Physical link: the UART is on the 2-pin Kenwood-style K-plug (speaker/mic jacks), via a
USB-serial programming cable (CH340/CP210x/PL2303/FTDI), 38400 baud. It is **not**
on the USB-C port: on the DP32G030 UV-K5 this firmware targets, USB-C (where fitted)
is charging only. The same cable is used to flash (`utils/k5flash.py`). Bench check: if
a cable will stay plugged in at a site, confirm TX still works with it connected
(`utils/aprs-web-beacon.html` warns that a plain 3-wire cable can block TX).

Constraint: UART commands run with interrupts disabled, so replies must stay short
(the ~30-byte `0x0B02` reply is ~8 ms at 38400 baud, which is fine).

### Comment persistence

Needs ~31 bytes of EEPROM outside the two APRS rows (which have only 3 spare bytes, and
those are taken by the digi settings). Find a provably unused EEPROM region the way
`settings.c:601` documents 0x0E30/0x0F20, outside every reset-protected range.

## Menu items

What already exists, in the regular menu (APRS group first, `ui/menu.c:48`): APRS
(on/off), **Digi** (on/off), Intv, Call, SSID, Loc, Cmnt, MsgTo, Msg, Send, RdMsg,
BEACON. All persist in EEPROM except Cmnt and Msg.

Agreed changes for the digi profile:

| Item | Values | Notes |
|---|---|---|
| **Digi** (existing) | OFF / FILL / WIDE | FILL = WIDE1-1 only (today's behaviour); WIDE adds WIDEn-N handling. Same EEPROM byte. |
| **DHops** | 1-7, default 2 | Highest N honoured in a WIDEn-N request. Larger requests are ignored (flood control across several digis). |
| **DDly** | OFF / 250 / 500 / 1000 ms | Maximum random delay before repeating. Tunable on the bench without reflashing. |
| **BcnTy** | MOBILE / DIGI | DIGI beacons the `#` digipeater symbol with a path that does not ask other digis to repeat it. MOBILE is today's `/>` + WIDE1-1,WIDE2-1. |
| **DStat** | read-only | Packets heard / repeated since boot, for site visits. Same counters as the UART status command. |

- Storage: the 3 spare bytes in the APRS EEPROM rows (logical 15, 30, 31) hold all of
  these packed; byte 14 stays Digi.
- Cost: each menu item touches four switch statements plus strings; rough guess
  60-120 bytes each. Measure.
- **Cmnt (beacon comment)** resets to "UV-K5 APRS" on every boot today; decided to
  persist it (see *Comment persistence*).
- "Digi" only works while APRS is ON; the menu should show that (e.g. value text).

## Plan (in order)

1. ~~**2 m-only build profile.**~~ Done, see *Step 1 result*. New flag/profile with the cuts above, UART_RC and the digi
   kept. Measure with `claude/build-env/run.sh` and record the real free flash. This sets
   what fits in step 2.
2. ~~**Digi logic**~~ Done, see *Step 2 + 3 result*. Funded by that headroom (menu items, UART monitoring and comment
   persistence per *Menu and monitoring*):
   - WIDE2-N decrement; callsign insertion (frame grows 7 bytes, refuse if 8 digis
     already); explicit-routing support (own callsign unused in the path).
   - Random delay (a few hundred ms to about 1 s, on the 10 ms slice) plus a busy-channel
     check, so two digis hearing one packet do not key up together.
   - Cancel the queued repeat if another digi's copy of the same packet is heard first.
   - Stronger dedupe: 32-bit key, a few more entries.
   - Gate the "Digi" menu item on APRS being on, or document it.
   - Larger RX/digi buffers (~170 bytes); RAM allows it.
   - Digipeater beacon symbol (`#`) and a path that does not ask other digis to repeat it.
   - Update the stale Makefile comment about the amateur image overflowing.
3. ~~**Shared header + tests.**~~ Done (`app/aprs_digi.c` included by both). Move the digi logic into a header so
   `utils/aprs_hdlc_test.c` exercises the real code, and extend the tests to cover the new
   cases. Write the delay/cancel logic as a pure state machine (fake time, fake frames) so
   it is host-testable too.
4. ~~**Re-add the RSSI meter**~~ Done, see *Step 4 + 5 result*. (`ENABLE_RSSI_BAR`, ~564 bytes). Lowest-priority planned
   item: only once steps 1-3 are done and measured, and only if the image keeps a safe
   margin (CI warns under 128 bytes free). It draws on the centre line while receiving,
   but only when no APRS packet box is up (`ui/main.c:1398` takes precedence over
   `:1432`). Re-measure: LTO means the cost may differ from 564.
5. ~~**Stretch: last-message line on the display.**~~ Done, see *Step 4 + 5 result*. A persistent one-line readout that
   stays after the 30 s packet box (`gAPRS_RxDisplay`, `ui/ui.c:124`) expires.
   - Scope (decided): the **sender callsign-SSID of the last packet heard**, any type,
     plus a timestamp.
   - Timestamp: the radio has **no real-time clock**, so wall-clock time is not
     available. Planned form is **elapsed time since heard** (`TB1AAW-9 12m`), counted on
     the 500 ms tick, which survives indefinitely and needs no setup. Alternatives if
     wall-clock is required: set the time over UART at each site visit (lost on power
     loss, drifts), or copy the timestamp from packets that carry one (most don't).
   - Optional: mark whether we repeated it (e.g. `*`), if it costs little.
   - Placement: the VFO-B half of the main screen is the obvious spot on a
     single-frequency digi; the centre line is shared with the packet box and RSSI meter.
   - Cost: a guess of 150-300 bytes (copy the decoded text into a persistent buffer, plus
     a draw call). The RdMsg RAM slot cost 156 bytes, which is the closest precedent.

## Testing strategy

There is no emulator for this radio: the Cortex-M0 core is generic, but nothing models
the BK4819 RF chip, and building that model is not worth it. Testing is layered instead.

1. **Host unit tests.** `cc -o t utils/aprs_hdlc_test.c && ./t` on the Mac. Covers path
   rewriting, dedupe and frame encoding; after step 3, the real code and the delay/cancel
   state machine. Most bugs should be caught here.
2. **Build and size checks.** `claude/build-env/run.sh` after every change: flash must
   stay under 61440 with margin, RAM reported per variant.
3. **Bench test on air** (dummy load or lowest power, on the club's frequency):
   - *Packet source:* a second UV-K5 running this firmware, driven from a PC. Raw TX
     (UART `0x0708`) sends hand-built frames: `WIDE2-2`, a full 8-hop path, our own
     callsign, duplicates.
   - *Observer:* RTL-SDR + Dire Wolf (prints the path with `*` used-hop markers), or a
     third K5's `APRSRAW:` serial output.
   - *Multi-digi:* two digis on the bench to check collisions and cancel-on-duplicate
     before anything goes up a mountain.
   - *Status:* read battery/RSSI over `0x0B02` (UART_RC) to confirm it works on the digi.
4. **Recovery.** The UV-K5 bootloader is separate from the firmware: hold PTT at power-on
   for flash mode, reflash with `utils/k5flash.py`. A bad build is very unlikely to brick
   the radio. Calibration lives in EEPROM, which firmware can write, so back it up first
   (e.g. CHIRP or a K5 EEPROM tool).

## Unattended-site checks

- APRS_ON and Digi both persist in EEPROM: confirm the digi returns after a power loss.
- Confirm nothing sticks in a stuck state (RX re-arm depends on the 500 ms tick;
  `APRS_Task` comments already note stalls when the carrier drops).
- APRS_ON disables battery-save and sleep (`app/app.c:1097`), so RX draws continuously.
  Power supply (solar/battery) still to be confirmed.
- Pick a sensible continuous-duty TX power.
- Keep the VFO on the digi frequency (lock, or come up on it after reset).
- ~~Resume-state also resumes a scan~~ Fixed: the digi profile sets
  `ENABLE_RESUME_SCAN=0`.
- Squelch level above 0 (busy-channel detection relies on it).
- The auto-beacon (Intv) provides station identification.

## Multi-digi design notes

- Prefer hop-count paths (`WIDE2-N`) over a WIDE1-1 fill-in. Fill-in would have every digi
  repeat the same packet.
- Aliases: explicit callsign hops can route through a specific digi. The existing code
  already respects an explicit callsign in the path.

## Open items

- Power source for the sites.
- Which busy-channel signal works (bench).
- Whether any cut item turns out to be needed by the club.
- Calibration/EEPROM backup of each radio before first flash.
- Last-message line timestamp: elapsed time (planned) unless wall-clock is required.
