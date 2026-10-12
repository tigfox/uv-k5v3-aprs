# Implementation plan: checkin-board firmware requests (APRS preset)

Plan only, 2026-10-12. Nothing here is built. Source of requests and answers: `claude/aprs-firmware-request-estimate.md`
(requests 0-11 and the reply from checkin-board). Starting point: branch `aprs` at `186844b2`, flash 106.31 KiB, RAM 14.13 KiB.

## Decisions taken from checkin-board's answers

| Topic | Decision |
|---|---|
| Queue depth | **4** outgoing messages, **4** pending acks |
| Request 9 (set frequency) | **dropped**; the status command (7) reports the frequency |
| Order | 0, 1, 2 + reason codes; then 3, 4; then 7 and 11; then 8; then 5, 10, 6 |
| Path (5) | messages and acks only; beacons unchanged |
| RSSI (10) | separate `APRSRSSI:<dBm>` line, **immediately after** its `APRSRAW:` line |
| SENT (2) | reported after the transmission ends |
| Full queue | `Ok = 0` with reason "full" in the reply only (no monitor line) |
| Capabilities (0) | revision number + feature bits (+ limits); no version string |
| Status (7) | includes the frame counters (11); no separate command |
| Keep | RX/TX log, K5Viewer. Removable if RAM needs it, in this order: scan progress bar (256 B), scan-range skip (140 B), then the K-plug UART buffers (~512 B, **ask first**) |
| Retries | stay in the host; every `0x0700` gets a new line number |
| HQ | radios are checkpoints only for now; acks (3) and queue (4) still matter there |

## Shared design (decided once, used by several requests)

### Reason codes (one table: DROP lines, byte 7 of the `0x0701` reply, the status command)

| Code | Text | Meaning |
|---|---|---|
| 0 | (none) | ok |
| 1 | `full` | outgoing queue (or ack list) is full: refused / given up |
| 2 | `session` | wrong or missing `ts` (refusal only) |
| 3 | `empty` | empty destination or text (refusal only) |
| 4 | `toolong` | more than 67 text bytes (refusal only) |
| 5 | `busy` | the channel, or the radio (scanning, being configured), stayed busy for 60 s |
| 6 | `nocall` | callsign is the placeholder |
| 7 | `aprsoff` | APRS is switched off |
| 8 | `band` | the TX frequency is outside the allowed bands |
| 9 | `battery` | battery too low |
| 10 | `mode` | the TX VFO is not FM |
| 11 | `replaced` | a newer message took the slot (only while the queue is one deep, i.e. between phases 1 and 2) |
| 12 | `off` | APRS was switched off with this still waiting |

Implementation: a new `APRS_TxWhy()` in `driver/aprs_tx.c` returns this code for the same checks `APRS_TxCheck()` makes today (and the
APRS-on check), so the refusal logic exists once. `APRS_TxCheck()` keeps its enum for the existing caller and is derived from it.

### New commands and bytes (all older hosts ignore them)

| Item | Layout |
|---|---|
| `0x0712` capabilities -> `0x0713` | body: `Ok`, `revision u8` (starts at 1), 2 pad, `features u32` (bits below), `queue_depth u8`, `ack_depth u8`, `max_text u8`, 1 pad. No `ts`. |
| Feature bits | 0 send-reply fields (line number, free slots, reason), 1 SENT/DROP lines, 2 pending acks, 3 queue, 4 status command, 5 set callsign, 6 path setting, 7 gap setting, 8 RSSI line, 9 counters in status |
| `0x0701` reply (bytes 5-7 of the existing 8-byte ack) | byte 5 = line number assigned (0 if refused), byte 6 = free queue slots after this call, byte 7 = reason code (0 if queued) |
| `0x0714` status -> `0x0715` (no `ts`) | body: `Ok`, `flags u8` (bit 0 monitoring on this port, 1 would transmit now, 2 channel busy, 3 TX frequency in band), `reason u8` (why not, 0 if it would), 1 pad, `battery_mv u16`, `freq u32` (10 Hz units), `decoded u16`, `bad_fcs u16`, `sent u16`, `queue_free u8`, `acks_pending u8`; layout is fixed in the spec revision with a worked example |
| `0x0716` set callsign -> `0x0717` | data: `ts u32`, `call 8` (NUL-padded, 1-6 chars `A-Z 0-9`), `ssid u8`. Validated like the settings record; flash write only if changed; same path as `0x070E` (`apply_settings`) |
| `APRS_CMD_LAST` | 0x0710 -> 0x0716 |
| SENT line | `SENT:<to>{nn}` (a closing brace as in the request; the on-air text has none: confirm with checkin-board) |
| DROP lines | `DROP:<to>{nn} <reason>` and, for an ack, `DROP:ack <to>{<seq>} <reason>`; `<seq>` as received (up to 5 characters) |
| RSSI line | `APRSRSSI:<dBm>` written by `APRS_SerialFrame` right after `APRSRAW:` in the same call, so nothing can come between them |

### Settings record (path, gap): bytes 92 and 93

Today `APRS_SettingsDecode` requires bytes 92-95 to be zero. Plan: byte 92 = message path (0 = `WIDE1-1,WIDE2-1`, **today's behaviour, so every
saved record stays valid**; 1 = `WIDE1-1`; 2 = direct), byte 93 = minimum gap in half-seconds (0 = default 5 s; allowed 4-20, i.e. 2-10 s).
Version stays 1 and the CRC (bytes 0-89) is unchanged; an out-of-range byte falls back to its default instead of discarding the whole record.
Bytes 94-95 stay zero. Consequences to state in the spec: a firmware older than this change rejects a record with those bytes
non-zero (settings reset to defaults if you downgrade after changing them); `aprs_pc.py unpack_record` must accept them.

## Phases

Each phase ends with: host tests green, the APRS preset builds, RAM and flash recorded in the plan, spec revision with worked examples
for any new byte layout, one commit (not pushed), then a **bench pass** by you.

### Phase 1: requests 0, 1, 2 and the reason codes (about 1 day)

1. `APRS_TxWhy()` and the reason table (`App/app/aprs_reason.h/.c`, hardware-free text and codes; host test for the strings).
2. **Line number at queue time.** Replace the `dirty`/`APRS_MsgNextSeq` send-time logic with a counter: `0x0700` and the menu Send each take
   the next number (1-99, wrapping) when queued. The menu keeps its old "same text again is a retry" rule (reuse the number); the serial path
   never reuses. The reply carries it (byte 5), plus free slots (byte 6, 0 while the slot is full) and the reason (byte 7).
3. **Event hooks** in `aprs_station.c` / `aprs_task.c`: after `APRS_TxSend` returns OK for a message -> `SENT:`; when a pending item is
   dropped (refused by `APRS_TxWhy`, 60 s expiry, replaced by a newer `0x0700`, APRS switched off) -> `DROP:` with the code.
   Acks get `DROP:ack` only (no SENT line for acks unless you want one: say so). New functions `APRS_SerialSent()` / `APRS_SerialDrop()`
   in `aprs_serial.c`, same chunked writer, still silent unless a port turned monitoring on.
4. `0x0712` capabilities (revision 1: bits 0, 1; `queue_depth` 1, `ack_depth` 1, `max_text` 67).
5. Tests: `test_aprs_cmd` (reply bytes, capability reply), `test_aprs_station` (numbering, drop reasons), a new `test_aprs_serial` with a
   capturing output sink (SENT/DROP/RSSI-order lines), Python: `aprs_pc.py` shows the number, the reason and the capabilities.
6. Spec revision: sections for the reply bytes, lines and reason table, with worked hex examples (checkin-board pins its codec on them).

RAM: about +20 B. Bench: send with the channel busy, with the callsign unset, with the battery low (or a stand-in), APRS switched off mid-queue.

### Phase 2: requests 3 and 4, then measure (about 1 day)

1. **Ack list of 4** in `aprs_station_t`: `(sender 10, seq 6)` entries, dedupe by sender + seq, oldest first; full -> `DROP:ack ... full`.
2. **Outgoing queue of 4**: entries `(to 10, text 68, seq)`; `APRS_StationEnqueue()` returns the number or a reason; the on-radio
   composer (`msgto`, `msg_text`) stays and its Send copies into the queue. A full queue refuses with reason `full`; nothing is replaced
   (the `replaced` code goes unused). Dropping applies to the **head item only**, with its own 60 s clock, then the next item is tried.
   Order: acks, then raw frame, then queue head, then beacon (as now).
3. Capability `queue_depth` 4, `ack_depth` 4, bits 2 and 3.
4. Tests: queue full/refusal, order, per-item expiry, ack dedupe and overflow, the menu path unaffected.

RAM: about **+370 B** (queue 4 x ~78 B = ~312, acks +48, indices). **Gate:** build, then read the stack gauge (`stk`) on a radio after a long
send/receive session. Accept if the gauge shows at least ~600 B free. If not, remove, in this order and one at a time: the scan progress bar,
scan-range skip (together ~400 B), and only then ask you about the K-plug UART buffers (~512 B). RX/TX log and K5Viewer stay.

### Phase 3: requests 7 and 11 (about 0.5 day)

1. Counters: decoded (exists), **bad FCS** (a counter where `aprs_demod.c` rejects a frame at the closing flag), **sent** (count of
   successful `APRS_TxSend`, all kinds).
2. `0x0714` status as laid out above; battery from the battery driver, frequency from the main VFO, `APRS_TxWhy()`, `APRS_RxBusy()`.
3. Tests with a stubbed radio (like `test_aprs_cmd`), Python `aprs_pc.py status` shows them; capability bits 4 and 9.

RAM: about +6 B static. Bench: compare the counters with `monitor`; low-battery and out-of-band reasons.

### Phase 4: request 8, set callsign and SSID (about 0.25 day)

`0x0716` as above; bit 5; tests for validation, session, no flash write when unchanged. Bench: set from the PC, read back with `setup get`.

### Phase 5: requests 5, 10 and 6 (about 0.75 day)

- **5 path:** record byte 92, decoder/encoder, `APRS_BuildHeaderPath()` used for messages and acks only (the beacon keeps `APRS_BuildHeader`),
  a menu item `Path` (placed last so the existing menu numbers do not shift; update `radio-card.html`), `aprs_pc.py` settings, bit 6.
  A direct frame is 14 bytes shorter; the static size asserts stay valid.
- **10 RSSI:** read the RSSI when a frame is delivered, convert to dBm with the firmware's existing conversion, write `APRSRSSI:` immediately after
  `APRSRAW:`; bit 8. The value reflects the end of the packet.
- **6 gap:** record byte 93, replaces the `APRS_TX_MIN_GAP_SLOTS` constant, a menu item `Gap`, bit 7.
- Tests for header lengths in each mode, the record round trip with the new bytes (and that old records still decode), the RSSI line order, and
  the gap bounds. Spec section on the settings record.

## Totals

About 4 days with tests and docs. Flash about +3 KiB (to ~109.5 of 118). RAM about +400 B (to ~14.5 KiB), tighter than today: that is why the stack
gauge is a gate after phase 2. Request 9 is not built.

## Risks

- **RAM and stack** (above). Mitigation: measure after phase 2, remove the unused features in the agreed order.
- **Behaviour change for the serial path:** a second `msg` no longer replaces (after phase 2), and replies carry bytes that older hosts ignore.
- **Settings downgrade:** an older firmware discards a record with non-zero bytes 92-93. Documented in the spec.
- **SENT timing:** reported after the ~0.8 s blocking transmission, as agreed.
- **Not bench-tested.** Each phase needs a pass on a radio; the APRS firmware has not run the 67-character change on air either.

## To confirm with checkin-board (small)

1. SENT/DROP with a closing brace (`{07}`) as in their request, though the on-air text has none.
2. No `SENT:` line for acks (only `DROP:ack`), unless they want one.
3. The status layout above, once the spec revision with a worked example is ready.
