# FM Voice build for the UV-K5 V3 / UV-K1 (planning)

Started 2026-10-08. A second image from the same fork, built from the `FMVoice` preset ("FM Voice"), for using
analog FM voice repeaters on 2 m and 70 cm: the radio is a handheld that *uses* repeaters, it is not one (renamed
from "Repeater" at the user's request, 2026-10-08). Branch `fmvoice` (from `aprs`, so it can reuse the tested
modules; the APRS preset does not change). Build it with `./compile-firmware.sh FMVoice`.

## Decisions (user, 2026-10-08)

| Question | Decision |
|---|---|
| Banks | The firmware's **24 named scan lists** (plus ALL and MIX). Quick switching between them. |
| Data entry | **CHIRP**, after a **CSV import**. |
| Info storage | **Per channel**. |
| Screen | No dual watch, no VFO B in this build: the screen is free for a channel card. |
| Location | **Place names** (the radio has no GPS). No lat/lon, so no distance sorting. |
| Bands / mode | **FM only** (no digital voice). Changed 2026-10-08 (user, who holds a GMRS licence): **receive anywhere the radio can; transmit only on 2 m, 70 cm, FRS/GMRS (462.550-462.725, 467.550-467.725) and the five MURS frequencies** (`app/fmv_bands.c`, host tested; a hard limit, the per-channel TX lock does not lift it). The APRS build keeps its 2 m / 70 cm limit. |
| DCS | CTCSS is the focus; DCS keeps working on channels (the scanner finds both for free). |
| Menu | **Keep most of the stock menu** (voice use needs offset, tones, scan, power). |
| Card content | **Callsign (the channel name), frequency, and city / landmark.** No club field (user, 2026-10-08). |
| Info layout | **256 entries x 48 bytes**: a check value and one text (see below). |
| Keys | The two new actions are **assignable** in the F1/F2/M key settings. |
| Splash | **`KD2DCM-VOICE`**. |
| Source CSV | A RepeaterBook export in CHIRP format (example: `RepeaterBook_CHIRP_SHIP_2m440.csv`, 81 rows). |

## What the firmware already has (checked in the code)

- 1024 memory channels, 16 bytes each, 16-character names; **24 named scan lists + ALL + MIX**, a channel can be in
  several; direct list selection while scanning; scan lockout; per-channel scan-list membership.
- **CTCSS/DCS scan** (RxCTCS / RxDCS menu items, `*`), also used for "found a tone" in the scanner.
- Menu items for offset, tones, power, step, bandwidth, reverse, PTT-id, priority channels.
- Multiboot slots with a config bank each, so this image can live in its own slot beside the APRS one.

## The design

### 1. Repeater info record, per channel
The **callsign is the channel name** (RepeaterBook's CHIRP export puts it there) and the **frequency** is the
channel's own, so both already exist. The only new data is the **place text: city, landmark** as RepeaterBook
writes it (`Mechanicsburg, Three Square Hollow`). The channel record has no room for it, so a table of fixed
records goes in free space of each bank, mapped into the 16-bit EEPROM address space so CHIRP and the serial
tools can reach it (logical 0xD000-0xFFFF, 12 KiB, to bank-relative flash 0xB000; other firmwares ignore it).

**256 records (channels 1-256, direct index, no lookup table) of 48 bytes:** a 16-bit check of the channel's
receive frequency, then the text, NUL-terminated, up to **45 characters** (the rest is cut and the upload says so).
Measured on the example export, the whole `City, Landmark` fits in 45 characters for nearly every row. An empty
slot is all 0xFF (erased flash).

The **check value** matters: if a channel is changed by something that does not know about the table (an unpatched
CHIRP driver, another firmware, the radio's own save in a different build), the record no longer matches and is
not shown. Without it a repeater's place would end up on the wrong channel.

### 2. CHIRP and the CSV
CHIRP stores a free-text **Comment** per memory and imports it from CSV. The patched driver (armel's 6.1.0 driver
plus this) reads and writes the table through that comment. For a RepeaterBook export nothing has to be
converted: **its Comment (`City, Landmark`) is the place text as it stands**, and its Name is already the
callsign. Upload writes each memory's comment into the table; download shows it again. CSV import, RepeaterBook
exports (CHIRP can also query RepeaterBook directly) and manual editing all work with no new CHIRP columns.

The example export (`RepeaterBook_CHIRP_SHIP_2m440.csv`, July 2020, 81 rows) is only a sample; the user will make a
new one for the local area. Things in it the driver and a small checker (`tools/fmvoice/check_csv.py`) report
rather than hide: a `split` row with an empty offset (the transmit frequency is lost), rows with no tone (maybe open
repeaters), text over 45 characters, names over the radio's name length, more than 256 channels with text.

CHIRP is not installed here as a Python package, so the driver is tested against its harness only if the CHIRP app
bundle can be used; otherwise the byte layout and the text handling (one Python module, shared with the checker)
are tested and the driver is checked by hand against a downloaded `.img`.

### 3. Screen
With the channel selected and the radio idle or receiving, the lower half of the main screen shows the card:

    W2XYZ  146.820 -
    Mechanicsburg
    Three Square Hollow

The text is split at its first comma: the city on one line, the landmark on the next (21 characters a line,
the rest scrolls). The callsign (name) and frequency are the screen's own and stay where the stock screen has
them, as does the RSSI bar. A channel with no record shows the stock screen. The card hides while transmitting (the TX state needs the
room).

### 4. Banks (scan lists)
- The **active list's name** is shown on the main screen and in the scan screen.
- **One key cycles the lists** (an action added to the F1/F2/M key actions, so you assign it where you like), with
  the name flashed on screen. Lists with no channel are skipped.
- Scanning already honours the list; the cycle works both idle and while scanning.
- Per-list "last channel" memory so switching back returns to where you were.

### 5. PL scanner and quick save
- A key (a second new action) starts a **tone search on the current frequency**: it listens for a carrier, runs the
  CTCSS scan (the BK4829 does the detection), and shows `PL 100.0` when found; then sets it as receive tone and,
  on confirmation, transmit tone.
- **Quick save** from the same flow: name (arrow editor, as in the APRS build), scan list, and the **band-plan
  offset filled in** from the common US band plan, which you can change: 2 m is 600 kHz, minus for outputs
  below 147.0 MHz and from 147.6, plus from 147.0 to 147.6; 70 cm is 5 MHz, plus for outputs at 442-445 MHz and
  minus at 447-450 MHz. Local coordination differs, so it is a default only, never a limit.
  Writes the next free channel (the callsign typed as its name) and, if given, the city / landmark text.
- The tone readout also shows while receiving, so an unknown repeater tells you its PL.

### 6. Menu, band limits
Stock menu kept; spectrum and FM broadcast stay compiled out (flash, RAM); the US 2 m / 70 cm windows from the
APRS build are reused. APRS code is not compiled in.

## Plan (in order, each with host tests where the code is hardware-free)

0. Branch (done), `FMVoice` preset (from Fusion; splash `KD2DCM-VOICE`; main-only, no VFO B), baseline guard extended to the APRS image so neither preset can drift.
1. Info table: record format, check value, codec between text and record, storage mapping, load/save. Host tests.
2. CHIRP driver patch + CSV converter + the shared codec. Tests; manual check against a real download.
3. The repeater card on the main screen. Bench on a radio.
4. Scan-list naming on screen, the cycle action, per-list last channel.
5. Tone search and quick save (reusing the APRS text editor), band-plan offsets (host-tested table).
6. Radio card and docs; release image with checksum.
7. Bench list (tone-scan reliability on weak signals, scan speed with many lists, save wear, CHIRP round trip).

## Status (2026-10-08)

- Step 0 done: `FMVoice` preset (89 KiB flash, 11 KiB RAM), main-only layout, no RxMode, splash `KD2DCM-VOICE`;
  `claude/baseline/check.sh` now also guards the APRS image against the field-tested release.
- Step 1 done: `app/fmv_info.c` (record, frequency check, text rules, split at the first comma), storage mapping
  (EEPROM 0xD000-0xFFFF -> bank-relative flash 0xB000), `app/fmv_store.c`. 45 C checks.
- Step 2 done: `tools/fmvoice/make_driver.py` builds the CHIRP driver from armel's by anchored replacement (not kept
  in git: GPL); `fmv_codec.py` (twin of the C codec, pinned to it); `check_csv.py`; `test_chirp_driver.py` runs the
  real CHIRP code: CSV import -> image -> the C decoder, download/upload against a fake radio, and the guard that
  keeps the table off the APRS record. Not yet tried on a real radio.
- Step 3 done: the card row on the main screen (marquee), not yet bench tested.
- Step 4 done (host tests + build, not bench tested): `app/fmv_bank.c` (pure: next channel in a bank, landing channel,
  label), `app/fmv_action.c` (action id 22 = BANK: next non-empty scan list, back to the channel it was left on,
  name flashed 1.5 s in the card row; UP/DOWN browse only the active bank, ALL or an empty bank use the stock code).
  Per-bank last channel is kept in RAM (not over a reboot); the active list itself is saved as stock. Action id 23
  is named TONE SEARCH in the menu and the CHIRP driver, with no handler until step 5 (beeps as unavailable).
- Step 5a done (host tests + build, not bench tested): action id 23 = TONE SEARCH (`app/fmv_action.c`): starts the
  stock single-frequency CTCSS/DCS scan from the main screen ("SEARCHING PL" in the card row, up to 16 s); on a hit
  the channel is reloaded, the tone becomes the channel's **transmit** tone (receive tone untouched, so squelch is
  not tightened by accident) and is saved to the channel; label "TX PL 100.0" from the pure `app/fmv_tone.c`.
  Any key or PTT stops it. "NO TONE FOUND" if it times out.
- Step 5b (quick save with band-plan offsets and a name editor) is **not done**: the stock MEM-CH / name menus
  already save a channel, and a third key action would need a new action id. Decision pending with the user.
- Test image: `claude/release/f4hwn.fmvoice.bin` (checksum in `claude/release/SHA256SUMS`).
- Added after the first bench use (2026-10-08): card in the big font (name under the frequency, place text below, step
  size removed, tone value moved beside the tone marks); bank names from a CSV `Scanlist`/`Bank` column; `chirp_cli.py
  upload-csv` (backup, convert, confirm, upload); band policy changed to receive everywhere / transmit on 2 m, 70 cm,
  FRS/GMRS, MURS (`app/fmv_bands.c`). Bench: BANK key works (confirmed by the user); channel list `FMV_Ship.csv` (123
  channels, banks WX FRS GMR MUR REP) uploaded over USB serial with a backup taken first.
- Field test and bench (2026-10-08, later): repeater settings stored and displayed well, several repeaters opened, no voice
  contact confirmed (quiet afternoon). Two problems: the ScList menu showed numbers only, and scanning ALL held on the
  first channel. Cause of both: the radio had **no bank data** - all 24 list names blank and all 123 channels in list 0
  (OFF), which this firmware treats as "in no list, not even ALL" (`RADIO_IsChannelInScanList`); with no valid channel the
  scanner falls back to channel 1 and stays there. Why the earlier bank upload was gone is not known (reflash or reset
  of the config bank is the suspect). Fixed by re-uploading `FMV_Ship.csv` (read back and compared with the image:
  channels, names, attributes, list names match). Banks now FRS GMR MUR REP; the 7 WX channels are in no list.
- Skip (2026-10-08): CHIRP's Skip "S" is now scan list OFF in the driver (`make_driver.py`) and in the CSV bank step.
  The firmware's per-channel exclude bit is **not usable**: `settings.c` clears it for every channel at every boot.
  A skipped channel is not in a bank either (reachable with the arrow keys in ALL only). Tests: 35 pass, including
  `test_skip_is_scan_list_off_and_comes_back` and `test_skipped_csv_channel_is_in_no_bank`.
- AIOC programming (2026-10-08): the radio speaks CHIRP's protocol at 38400 baud through the AIOC's serial port
  (`/dev/cu.usbmodem...`); `chirp_cli.py` now opens ports at 38400 with DTR/RTS low (the AIOC can key PTT from them).
  Plain `chirpc` at 9600 gets `Header short read`. Details in `tools/fmvoice/README.md` ("Connecting").
  `upload-csv` was run live (download, build, upload, read back). Test setup: pyserial in a venv in `build/venv`,
  CHIRP cloned beside the repo (`../chirp`), armel's driver as `build/FMVoice/Drivers/f4hwn.chirp.v6.1.0.py`.
- Long bank names (2026-10-08, **on the radio, field checked**): 16-character names in a new table (EEPROM 0x8900,
  24 x 16 bytes, flash 0x008900 in the attribute sector; `FMV_StoreBankName`), next to the 3-character short names. Card
  flash shows the long name alone; the ScList menu shows the number big and the name wrapped on two small lines (the item
  area is 78 pixels wide); the scan screen title is the long name (too fast to read while scanning, the user says that is
  fine); the status bar and the channel tag keep the short name. CSV: `Scanlist`/`Bank` column = long name, new `Short`
  column = short name (else the first three letters); a clash of two short names is reported; after a CSV is applied the
  names of lists no channel is in are cleared (`--keep-unused-banks` keeps them). Tests: C host tests (42 checks for the
  names) and 42 Python tests; the FMVoice preset builds (flash 91.3 KiB, RAM 11.0 KiB). Flashed `build/FMVoice/f4hwn.fmvoice.bin`
  and uploaded `FMV_Ship.csv` over the AIOC (backup first, read back and compared with the image): banks FRS, REP, GMRS
  (GMR), MURS (MUR), SOUTHMTN (SMT), TUSCARORA (NMT); the old GMR / MUR lists cleared; the 7 WX channels in no bank. The user
  confirmed the menu, card and screens look right.
- Field check after the upload (user, 2026-10-08): scanning works well; the earlier missing bank data is put down to a
  flashing mistake.
- Still to do: re-run the plain `chirp_cli.py -s ... --download-mmap` path on the radio (unit test only so far); the 4th byte
  of the short name is unused (the status bar shows 3 characters, so GMRS / MURS show as GMR / MUR there); the field-tested
  image in `claude/release/` is still the one from before the long names (the new one is in `build/FMVoice/`, not copied);
  voice contact on a repeater is still unconfirmed; tightening where Claude may run commands (sandbox / deny rules) was parked.
- Step 6 done: `claude/fmvoice-card.html` (radio card) and `tools/fmvoice/README.md`.
- Still open: bench list (tone search on a real repeater, scanning inside a bank, weak-signal tone detection, scan speed,
  CHIRP round trip with the FM Voice driver in the GUI); a permanent bank tag on the main screen (offered, not built);
  start-in-ALL or remembered bank (offered); quick save with band-plan offsets and a name editor (deferred by the user).

## Risks and open points

- **Capacity:** 256 repeaters per bank (channels 1-256 only); channels above that show the stock screen. Enough only if a bank is a region, not the
  whole country; the 5 banks give 5 x that. Needs the user's expected counts.
- **CHIRP maintenance:** the patched driver must be re-merged with every upstream driver release.
- **Shared address space:** a future upstream release could map something at the logical addresses I use; the
  check value protects the card but not the data.
- **Reset / config-bank wipe** erases the table with the channels (same as the channels themselves).
- **Tone scan time:** a full CTCSS scan needs a steady carrier for a second or two; short kerchunks will not
  identify a tone. Not fixable in software.
- **RAM/flash:** this build is Fusion-sized (about 105 KiB flash, 12 KiB RAM) with no APRS, so there is headroom.

## Suggested extras (not committed)

- Tone readout while receiving (included above).
- A "reverse" key check that tells whether the input is audible (simplex test): upstream has reverse; add a hint.
- Time-out timer for long conversations; a transmit-time reminder.
- Import a user list of "my" repeaters from the radio card as QR/CSV: low priority.
