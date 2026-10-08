# Repeater build for the UV-K5 V3 / UV-K1 (planning)

Started 2026-10-08. A second image from the same fork, built from a `Repeater` preset, for analog FM voice
repeaters on 2 m and 70 cm. Branch `repeater` (from `aprs`, so it can reuse the tested modules; the APRS preset
does not change). Nothing is built yet.

## Decisions (user, 2026-10-08)

| Question | Decision |
|---|---|
| Banks | The firmware's **24 named scan lists** (plus ALL and MIX). Quick switching between them. |
| Data entry | **CHIRP**, after a **CSV import**. |
| Info storage | **Per channel**. |
| Screen | No dual watch, no VFO B in this build: the screen is free for a repeater card. |
| Location | **Place names** (the radio has no GPS). No lat/lon, so no distance sorting. |
| Bands / mode | 2 m (144-148) and 70 cm (420-450) only, **FM only** (no digital voice). |
| DCS | CTCSS is the focus; DCS keeps working on channels (the scanner finds both for free). |
| Menu | **Keep most of the stock menu** (voice use needs offset, tones, scan, power). |
| Info layout | **256 entries x 48 bytes**, strings packed (see below). |
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
Fields: **callsign, club, place (nearest town), note**. The channel record has no room, so a table of fixed
records goes in free space of each bank, mapped into the 16-bit EEPROM address space so CHIRP and the serial
tools can reach it (logical 0xD000-0xFFFF, 12 KiB, to bank-relative flash 0xB000; other firmwares ignore it).

**256 entries (channels 1-256, direct index, no lookup table) of 48 bytes**, each holding the four strings packed
back to back and separated by NUL bytes (`call NUL club NUL place NUL note NUL`), plus the frequency check below.
Callsign is capped at 7 characters; the other three share what is left. When a record does not fit, the **note is
cut first, then the club**, and the upload reports it (never silently). Measured on the example export
(81 repeaters from RepeaterBook): with a 14-character club the other three fit whole for 88 % of rows, with a
10-character club for 94 %, and for 99 % with no club. Fixed-width fields would cut far more (a 12-character
note fits only 78 % of rows, a 14-character place 91 %).

Each record also carries a **16-bit check of the channel's receive frequency**. If the channel is changed by
something that does not know about the table (an unpatched CHIRP driver, another firmware, the radio's own
save in a different build), the record no longer matches and is not shown. Without this a repeater's callsign
would end up on the wrong channel.

### 2. CHIRP and the CSV
CHIRP stores a free-text **Comment** per memory and imports it from CSV. The patched driver (armel's 6.1.0 driver
plus this) reads and writes the table through that comment, in a fixed form:

    W2XYZ; Mid-Hudson ARC; Poughkeepsie; linked, 107.2 on Sundays

(callsign; club; place; note; any may be empty; a note may itself contain commas). Upload parses the comment into
the radio's record; download rebuilds it. CSV import, RepeaterBook exports and manual editing then work with no new
CHIRP columns.

**The example export** (`RepeaterBook_CHIRP_SHIP_2m440.csv`): the standard CHIRP columns; `Name` is the callsign
(4-6 characters, 21 of 81 repeat because a repeater has a 2 m and a 70 cm pair); `Comment` is `City, Landmark`
(`Mechanicsburg, Three Square Hollow`; 56 of 81 have no landmark); tones as `Tone` + `rToneFreq`; offsets 0.6 and
5 MHz. The **converter** (`tools/repeater/`) maps `Name` to the callsign, the text before the first comma to the
place, the rest to the note, and leaves the **club empty** (RepeaterBook's CHIRP export does not carry it; you add
it in CHIRP or from a second column of your own). It writes a CHIRP-ready CSV and warns about what it cannot keep:
a `split` row with an empty offset (the transmit frequency is lost: 1 row here), a missing tone (8 rows here, which
may be open repeaters), text that will be cut. **That file is from July 2020**; repeaters change, so re-export
before relying on it.

The codec is one Python module with tests, shared by the driver and the converter. CHIRP is not installed here as a
Python package, so the driver is tested against its harness only if the CHIRP app bundle can be used; otherwise the
codec and the byte layout are tested and the driver is checked by hand against a downloaded `.img`.

### 3. Screen
With the channel selected and the radio idle or receiving, the lower half of the main screen shows the card:

    W2XYZ  Mid-Hudson ARC
    Poughkeepsie
    linked, 107.2 on Sund..

The note scrolls if it is longer than a line. The frequency, name and RSSI bar stay where the stock screen has
them. A channel with no record shows the stock screen. The card hides while transmitting (the TX state needs the
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
  Writes the next free channel and an info record (callsign, club, place, note, each optional).
- The tone readout also shows while receiving, so an unknown repeater tells you its PL.

### 6. Menu, band limits
Stock menu kept; spectrum and FM broadcast stay compiled out (flash, RAM); the US 2 m / 70 cm windows from the
APRS build are reused. APRS code is not compiled in.

## Plan (in order, each with host tests where the code is hardware-free)

0. Branch (done), `Repeater` preset (from Fusion; splash `KD2DCM-VOICE`), baseline guard extended to the APRS image so neither preset can drift.
1. Info table: record format, check value, codec between text and record, storage mapping, load/save. Host tests.
2. CHIRP driver patch + CSV converter + the shared codec. Tests; manual check against a real download.
3. The repeater card on the main screen. Bench on a radio.
4. Scan-list naming on screen, the cycle action, per-list last channel.
5. Tone search and quick save (reusing the APRS text editor), band-plan offsets (host-tested table).
6. Radio card and docs; release image with checksum.
7. Bench list (tone-scan reliability on weak signals, scan speed with many lists, save wear, CHIRP round trip).

## Risks and open points

- **Capacity:** 256 repeaters per bank, with packed variable-length text. Enough only if a bank is a region, not the
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
- Repeater lookup by frequency in VFO mode (needs a lookup table, so only if the record table is keyed that way).
- Time-out timer for long conversations; a transmit-time reminder.
- Import a user list of "my" repeaters from the radio card as QR/CSV: low priority.
