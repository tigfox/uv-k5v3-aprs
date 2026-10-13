# Estimate: checkin-board firmware requests (2026-10-12) against the APRS firmware

Answers `checkin-board/docs/specs/2026-10-12-uvk5-firmware-requests.md` (requests 0-11). Written 2026-10-12 from the
source and the build of branch `aprs` at `b1377552`. The estimate below is what was written *before* the work; the status above is current.

## STATUS FOR THE APP TEAM (latest; read this first)

**Built: requests 0-8, 10 and 11 (nine was dropped).** Host-tested and compiled into the APRS firmware; **not yet tried on a radio**. Branch
`aprs`, commits `74da2b0c` (0-4), `2ff27cb9` (7, 11), `02af7c6e` (5, 6, 8, 10). Not pushed yet. The spec, with worked hex examples to pin
golden tests on, is `claude/aprs-messaging-api.md` (revision 2, protocol revision 1). The reference client is `tools/aprs/aprs_pc.py`.

| # | Request | State | Where in the spec |
|---|---|---|---|
| 0 | Capability flags | **built**: `0x0712`, revision 1, features `0x3ff` (bits 0-9), queue 4, acks 4, text 67 | 5.6 |
| 1 | Line number in the send reply | **built**: reply bytes `Ok, line, free places, reason` | 5.1 |
| 2 | SENT / DROP lines with reasons | **built**: `SENT:<to>{nn}`, `DROP:<to>{nn} <reason>`, `DROP:ack <to>{seq} <reason>` | 5.7, 6 |
| 3 | Several pending acks | **built**: 4, oldest first, deduplicated by sender + number | 8 |
| 4 | Deeper outgoing queue | **built**: 4; a fifth is refused (`full`), nothing is replaced | 5.1, 8 |
| 5 | Digipeater path (messages and acks) | **built**: settings byte 92 + menu item **Path** + `"path"` in the settings file | 5.2 |
| 6 | Gap between transmissions | **built**: settings byte 93 + menu item **Gap** + `"gap_s"`; 2-10 s, default 5 s | 5.2 |
| 7 | Status command | **built**: `0x0714` (battery, frequency, in band, would transmit and why not, channel busy, monitoring, queue, acks) | 5.8 |
| 8 | Set callsign and SSID | **built**: `0x0716` | 5.9 |
| 9 | Set the frequency | dropped (your answer to Q5) | |
| 10 | Signal strength | **built**: `APRSRSSI:<dBm>` right after its `APRSRAW:` line | 6 |
| 11 | Frame counters | **built**, in the status reply: decoded, damaged, sent | 5.8 |

**Format decisions you asked about**

- SENT / DROP carry the destination and the line number as on the air. The closing brace is part of the line (`SENT:KD2DCM-1{07}`), as in your request; the packet itself has none.
  Acks are reported only when **dropped** (`DROP:ack ...`), not when sent. Tell us if you want `SENT:ack` as well.
- One reason table, numbers pinned (`claude/aprs-messaging-api.md` 5.7): the send reply's byte 7, the DROP lines and the status reply use it. Two codes
  were added beyond your list: `invalid` (13, a callsign or SSID the radio does not accept) and `store` (14, the radio could not save). `replaced` (11) is
  defined but never used: nothing is replaced any more.
- The status reply layout (20 bytes) is in 5.8 with a worked example. "Damaged" is an **approximation**: a packet-shaped burst that failed its checksum
  and was not decoded by any of the radio's three demodulators; noise can also count.
- `APRSRSSI:` is the highest reading taken every 10 ms while a packet was arriving, taken only while a host is monitoring; no line when no reading was taken.
- Settings record bytes 92-93: **your reading of the record is unaffected** (you ignore reserved bytes). Records saved by earlier firmware stay valid.
  A radio with this firmware keeps working if it is later given a record with those bytes zero; an *older* firmware rejects a record with them set.

**Behaviour that changed, so check your assumptions**

1. A host `msg` no longer touches the radio's on-screen message fields (the menu's MsgTo / Msg): it goes straight into the queue.
2. A radio with **APRS switched off** now refuses to transmit (reason `aprsoff`); before, a queued message went out anyway.
3. A packet or ack given up on a busy channel is given up **alone** after 60 s (the next one gets its own minute); before, everything pending was cleared.
4. A beacon given up on a busy channel no longer stops the automatic beacon timer (it did before).
5. The replies to `msg` keep the older layout in byte 4; bytes 5-7 are meaningful only when the capability bit `send-fields` is set.

**Resources** (APRS preset): flash 109.09 KiB of 118 (was 106.31 before this work), RAM 14.50 KiB of 16 (was 14.13). Nothing had to be removed. The stack
gauge (`stk`) has not been read on a radio: the gate in the plan (about 600 bytes free after a long session) is still to be checked. If it fails, the
agreed order of removal is the scan progress bar, scan-range skip, then (after asking) the K-plug buffers.

**Not verified on a radio**: the queue and the SENT / DROP timing, the menu items Path and Gap, the signal strength line (the reading is taken in the main loop
every 10 ms while the demodulator says a packet is arriving), the status values (battery, frequency), and the 67-character message on the air. A bench pass
list is in `claude/aprs-requests-implementation-plan.md`.

**Open questions for you**: none blocking. Two small ones: (1) `SENT:ack` wanted? (2) The `APRSRSSI` reading is a peak, not the level at the end: is that what the link check needs?

---

## Verdict

**All twelve are possible. None needs another feature removed**, if the outgoing queue is **4 deep**. Flash is not a constraint
(about 11.7 KiB free). **RAM is the constraint**: about 1.9 KiB of the 16 KiB is free, and the linker keeps 1 KiB of that for the
stack (the heap is not reserved in this preset, `ENABLE_NO_HEAP`), so the usable spare is **roughly 0.9 KiB, to be confirmed by
the stack gauge (`stk`) on a build**. The requests need about 0.3 KiB at depth 4 and about 0.65 KiB at depth 8. Depth 8 is where
removing something starts to make sense; candidates are listed below.

## Budget today (APRS preset, `f4hwn.aprs.bin`)

| | Used | Free |
|---|---|---|
| Flash | 106.31 KiB | 11.69 KiB (of the 118 KiB limit) |
| RAM (static) | 14.13 KiB | 1.88 KiB, of which the stack needs at least 1 KiB |

Largest RAM users: the 4 KiB flash sector cache (needed for every settings write), the APRS demodulator and queue (1.5 KiB), the
display buffer (0.9 KiB), USB (0.4 KiB), the station state (0.3 KiB).

## Request by request

| # | Request | Possible | Flash | RAM | Effort | Notes |
|---|---|---|---|---|---|---|
| 0 | Capability flags | yes | ~0.1 KiB | 0 | small | A **new command** `0x0712` returning a protocol revision, a feature bit field, queue depth, ack depth and max text length. The handshake reply is shared with UV Studio and K5Viewer, so I would not change its layout. |
| 1 | Line number in the send reply | yes | ~0 | 0 | small | The 8-byte `0x0701` reply has three padding bytes: byte 5 = line number, byte 6 = free queue slots, byte 7 = refusal reason. Older hosts ignore them. The number must be assigned **when queued** (today it is assigned when sent), which the queue (request 4) needs anyway. |
| 2 | SENT / DROP lines with reasons | yes | ~0.5 KiB | ~0 | medium | The transmit check already knows `NO_CALL`, `DENIED`, `BUSY`; it needs to split `DENIED` into band / battery / modulation, and the 60 s expiry, "APRS off" and "replaced" need to report. The lines go through the existing chunked monitor writer (no buffer). Lines are sent only to a port that turned monitoring on. |
| 3 | Several pending acks | yes | ~0.2 KiB | ~50 B | small | Four entries of (sender, line number), deduplicated. Acks still go before messages. |
| 4 | Deeper outgoing queue | yes, **4 deep** (8 costs more) | ~0.4 KiB | ~240 B at 4, ~560 B at 8 | medium | A full queue answers `Ok = 0` with reason "full" (request 1's byte 7); nothing is silently replaced. The on-radio composer stays separate. |
| 5 | Settable digipeater path | yes | ~0.3 KiB | 1 B | medium | Direct / WIDE1-1 / WIDE1-1,WIDE2-1 for messages and acks, as a setting plus a menu item. It needs a byte in the settings record (the reserved bytes 92-95). **The current `aprs_pc.py` refuses a record with those bytes non-zero**, so the tool and the spec change with it. Beacons keep their own path rules. |
| 6 | Configurable gap | yes | ~0.1 KiB | 1 B | small | 2-10 s, default 5 s; same record and menu work as request 5. Collisions are the risk of going low, not a firmware limit. |
| 7 | Status command | yes | ~0.5 KiB | 0 (stack only) | medium | Battery, VFO frequency and band check, "would refuse and why" (shares the reason code with request 2), squelch open, monitoring flag. Read-only, no `ts`. |
| 8 | Set callsign / SSID | yes | ~0.2 KiB | 0 | small | New command with `ts`; validates like the settings record; one flash write only when the value changes. |
| 9 | Set the frequency | yes, **the riskiest** | ~0.5 KiB | 0 | medium-large | Touches the radio core (VFO retune, band check, saving). Needs a decision: frequency mode or a memory channel? See questions. |
| 10 | Signal strength per frame | yes | ~0.2 KiB | 2 B | small | Read the BK4819 RSSI when a frame is delivered (end of frame, so it reflects the tail of the packet). **Suggest a separate line** (`APRSRSSI:-87`) rather than appending to `APRSRAW:`: hosts that parse that line as pure hex would break. |
| 11 | Frame counters | partly | ~0.2 KiB | 6 B | small | Decoded (exists), transmitted (exists), bad FCS: the demodulator drops them at the closing flag, so a counter is one line there. A "best guess" frame does not exist: there is no error correction. |

Total: about **3.2 KiB flash**, about **0.3 KiB RAM** at queue depth 4. Host tests for every item, spec revision, `aprs_pc.py` changes.
Rough effort including tests and docs: **3-4 days of work**, in this order:
(1) 0, 1, 2 + the reason code, (2) 3, 4, (3) 7, 11, 10, (4) 5, 6, 8, (5) 9. Items 1-3 and 4 give "exact delivery"; the rest can ship later.
None of it has been on a radio: each step needs a bench pass, and the stack gauge after the queue lands.

## If RAM gets tight (queue depth 8, or the stack gauge says so): what could go

In the order I would remove them, by value lost per byte saved:

| Candidate | RAM saved | Cost to you |
|---|---|---|
| K5Viewer remote-screen buffer (`previousHash`) | 256 B | the PC "K5Viewer" screen mirror stops working |
| Scan progress maps (`ENABLE_FEAT_F4HWN_SCAN_PROGRESS`) | 256 B | the progress bar on the scan screen |
| Scan range skip (`scanRangeSkip`) | 140 B | scan-range skipping |
| RX/TX log view cache | 112 B | the on-radio RX/TX log |
| UART buffers (the K-plug cable) | ~512 B | **K-plug programming / AIOC / checkin-board over the cable stops**; USB-C unaffected. Last resort. |

Each also saves flash. That is about 0.5 KiB for the first four, more than enough for depth 8.

## Questions for checkin-board (please answer in your reply)

The estimate above holds for any answer. These answers change *what gets built*, or let me skip work. Where I list a default, I
will build that if I hear nothing. Please reply with the question number and your choice; one line each is enough.

| # | Question | Why it matters | Options | If no answer (default) | Blocks |
|---|---|---|---|---|---|
| Q1 | **How many messages must a node be able to have waiting in the radio at once?** | Each queued message costs about 80 bytes of the roughly 0.9 KiB of RAM that is left. | 4 / 8 / "more than 8" | **4** | request 4 (and the room left for the rest) |
| Q2 | **What is the busiest burst a radio must clear, and how fast?** For example "20 runners arrive within 2 minutes at one checkpoint". | With the 5 s gap and acks sent first, one radio sends at most about 12 transmissions a minute. This tells me whether the gap setting (6) is a nicety or a requirement, and whether a depth of 4 is enough. | a number of messages and a time | keep 5 s as the default and allow 2-10 s | requests 4, 6 |
| Q3 | **Will every radio be connected by USB-C only, never by the 2-pin K-plug cable?** | The K-plug cable's buffers are the last RAM I would give up (about 0.5 KiB), and the first choice if the queue needs to be deeper. | USB-C only / K-plug also used | **assume K-plug is still used**; nothing is removed | the "what could go" list |
| Q4 | **Do you use the K5Viewer screen mirror, the scan progress bar, scan-range skipping or the RX/TX log on these radios?** | They are the cheapest things to remove for RAM and none is needed for messaging. | list which you use | none are removed unless RAM requires it, and then I ask again | the "what could go" list |
| Q5 | **Frequency (request 9): when `stationsetup` sets a frequency, what should happen to the radio?** | The radio can be in frequency mode or on a memory channel; retuning in each case touches different code. | (a) switch to frequency mode and set it; (b) change the frequency of the current memory channel; (c) not needed: operators set it on the radio | **(c)**, and 9 is built last | request 9 |
| Q6 | **Which frequencies must be allowed?** | The firmware transmits only inside 144-148 and 420-450 MHz. Should the command refuse anything outside, as the radio does? | refuse outside (as now) / other | refuse outside | request 9 |
| Q7 | **Digipeater path (request 5): should the setting apply to messages and acks only, or to the station beacon too?** | Beacons today use "WIDE1-1,WIDE2-1" for a mobile station and go direct for a digipeater. | messages + acks only / also beacons | **messages + acks only** | request 5 |
| Q8 | **Is a separate `APRSRSSI:<dBm>` line (after the matching `APRSRAW:` line) acceptable, instead of appending to the `APRSRAW:` line?** | A hex-only parser reading the whole `APRSRAW:` line would break on an appended field. | separate line / appended (host must tolerate) | **separate line** | request 10 |
| Q9 | **Is "SENT" reported when the transmission has finished (about 0.8 s after it starts) early enough?** | The radio blocks while it transmits; the line cannot be written before it ends. | yes / need it at the start | **after it ends** | request 2 |
| Q10 | **When a send is refused because the queue is full, is `Ok = 0` with reason "full" in the reply enough, or do you also want a monitor line?** | Both cost little; I would rather not add lines nobody reads. | reply only / also a `DROP:` line | **reply only** | requests 1, 4 |
| Q11 | **Is a protocol revision number plus feature bits enough for request 0, or do you need version strings of the firmware too?** | A bit field is smaller and easier to test against; the firmware version string is already in the handshake. | revision + bits / also a string | **revision + bits** | request 0 |
| Q12 | **Which requests should ship first if they cannot all land together?** | I plan: 0, 1, 2 and the reason codes; then 3 and 4; then 7, 10, 11; then 5, 6, 8; then 9. Say if your race schedule wants a different order. | reorder | the order in the effort list | all |

**Please also confirm two assumptions I made from your document:**

- A node that serves as HQ will also **send** (acks and replies), so the pending-ack list (request 3) and the queue (request 4) both apply to it.
- **Retries stay in the host**: the radio will not resend an unacknowledged message, and a retry is a new message with a new line
  number (as the current firmware behaves).

## Compatibility

Every change adds a field, a line or a command that older hosts ignore (as the request asks), with one exception: the settings
record gains a byte (items 5, 6), and the current `aprs_pc.py` rejects it until updated. `claude/aprs-messaging-api.md` gets a new
revision when the first of these lands.

---

## Reply from checkin-board (2026-10-12)

Thank you: the estimate answers everything we needed. Build at **queue depth 4**; nothing needs removing for it. Answers by
number, then the two assumptions, then a few notes on formats. checkin-board's plan is in
`checkin-board/docs/specs/2026-10-12-uvk5-backend-plan.md`.

| # | Answer |
|---|---|
| Q1 | **4.** RC1 keeps at most a few batches in flight per checkpoint (default 4), and a 67-character report carries about 5 bibs, so 4 queued messages is enough. |
| Q2 | Our design burst is **20 bibs a minute for 10 minutes** at one checkpoint (soak and load tests). A mass start at the first checkpoint can reach about **100 runners in 3 minutes (about 33 a minute)**. At about 5 bibs per message that's **about 4-7 messages a minute**, plus a heartbeat every 5 minutes and a few acks: well inside your ~12 transmissions a minute at the 5 s gap. So depth 4 is enough, and the gap setting (6) is a **nicety, not a requirement**: keep 5 s as the default, 2-10 s allowed. |
| Q3 | **USB-C only.** Race radios connect to the node's Pi by USB-C. Keep the K-plug buffers unless RAM is needed; if it is, they can go **before** the items in Q4. |
| Q4 | We **use** the **RX/TX log view** and the **K5Viewer screen mirror**: please keep both. The **scan progress bar** and **scan-range skip** aren't used and can go if RAM requires it. |
| Q5 | **(c) Not needed.** Operators set the frequency on the radio; checkin-board only reads it (request 7) to show it and check the band. Request 9 can be dropped. |
| Q6 | Moot with Q5. (If 9 is ever built: refuse outside 144-148 and 420-450 MHz, as now.) |
| Q7 | **Messages and acks only.** Beacons keep their rules. |
| Q8 | **A separate `APRSRSSI:<dBm>` line is fine**, as long as it comes **immediately after** the `APRSRAW:` line it belongs to, with nothing between them (we pair them by order). If that can't be guaranteed, add a short tag shared by both lines. |
| Q9 | **After it ends is fine.** We use SENT to know the slot is free and to mark "sent"; 0.8 s doesn't matter. |
| Q10 | **Reply only** (`Ok = 0`, reason "full"). We never send into a full queue; we track free slots from byte 6. |
| Q11 | **Revision + feature bits** are enough. We log the version string from the handshake for support. |
| Q12 | Your order, with one change: **0, 1, 2 + reason codes; then 3, 4; then 7 and 11; then 8; then 5, 10, 6.** We move 8 (set callsign) ahead because `stationsetup` assigns callsigns from the race CSV, and a duplicate callsign-SSID was our first field-test mistake. 9 is dropped (Q5). |

**Assumptions:**

- **HQ sends**: confirmed. An HQ node sends acks, gap requests (`RC1 G ...`) and link-check probes. For now, though, **radios
  will be used on checkpoints only**, with HQ staying on graywolf until requests 3 and 4 are in and bench-tested. A checkpoint
  also sends acks (for HQ's gap requests and probes), so 3 and 4 matter there too.
- **Retries stay in the host**: confirmed. The radio never resends; each retry is a new `0x0700` with a new line number.
  checkin-board already works that way with graywolf.

**Notes on formats** (please say if any differs from what you'll build):

1. **Capability command `0x0712`**: fine. We'll read it after the handshake. A firmware without it (no reply, or a refusal)
   counts as revision 0, with only today's behaviour.
2. **`0x0701` bytes 5-7** (line number, free slots, refusal reason): fine. We'll ignore them unless the capability bits say
   they're valid, since older firmware sends padding there.
3. **SENT / DROP lines**: please include the destination and the line number exactly as on the air, e.g.
   `SENT:KD2DCM-1{07}` and `DROP:KD2DCM-1{07} busy`, so we can match them without guessing. Please also report a DROP for an
   **ack** given up (e.g. `DROP:ack KD2DCM-1{050} busy`, for an ack of graywolf's message 050), or tell us acks are never dropped silently. A lost ack makes the far
   station retry, which is the cost we're trying to avoid.
4. **Reason codes**: one table shared by DROP, the `0x0701` refusal byte and the status command (request 7) would be ideal.
   Please list the codes in the spec revision.
5. **Settings record bytes 92-95** (path, gap): checkin-board reads only the fields it needs and ignores the reserved bytes,
   so the record change doesn't affect us.
6. **Status command (7)**: please include the received/damaged/sent counters (11) in it rather than a separate command, if
   that's cheaper.
7. When the first of these lands, a spec revision with the new byte layouts and a worked example (like section 3's) lets us
   pin our codec with golden tests, as we did for today's frames.

### Answers to the two open questions (checkin-board, 2026-10-12)

1. **`SENT:ack` is not needed.** checkin-board tracks only its own messages; `DROP:ack` for a given-up ack is enough.
2. **A peak `APRSRSSI` reading is fine.** The link check compares stations and runs, so a consistent measure matters more than which one.
   checkin-board keeps the latest reading per sender, and doesn't use it in the link check yet.

checkin-board now speaks protocol revision 1 as published in revision 2 of the spec: the golden tests reproduce your worked examples for the handshake,
message, status, set callsign and settings record. It refuses a radio without `send-fields`, `events`, `acks` and `queue`. Next is the bench with
the two radios on the HQ Pi 3 (USB-C).

### First bench with two radios (checkin-board, 2026-10-13)

Two radios on F4HWN v6.1.0, protocol revision 1, on two Pi Zero W nodes over USB-C. These work:
- the handshake;
- capabilities (features `0x3ff`, queue 4, acks 4, max text 67);
- settings;
- status;
- set callsign;
- send (line numbers, free places);
- the radio's own acks;
- `SENT:`;
- `APRSRAW:` and `APRSRSSI:`.

**One mismatch, which cost us every monitor line until we found it:** the spec (sections 2 and 7.1) says to open the port with
**DTR and RTS low**. Over USB-C, though, `APRS_SerialLine` and `APRS_SerialFrame` write through `VCP_Send` →
`cdc_acm_data_send_with_dtr`, which sends **nothing while DTR is low**.
- Command replies go through `cdc_acm_data_send_with_dtr_async` with no DTR check, so the handshake and every command worked.
- The radio's status reported monitoring on, yet no `SENT:`, `DROP:` or `APRSRAW:` line ever arrived.

checkin-board now opens with DTR and RTS low, as the spec says, because an AIOC is also a ttyACM and keys PTT from them. It
raises DTR only once the port has answered the handshake, before turning monitoring on. With that, the lines arrive:
`SENT:KD2DCM-4{07}`, then the far radio's `ack07` heard at -10 dBm. Please either:
- say in the spec that over USB-C, DTR must be up for monitor lines (and that it's safe to raise once the radio has answered); or
- send monitor lines whenever monitoring is on for the VCP port, whatever DTR is.

Also worth knowing: when `cdc_acm_data_send_with_dtr` times out, it clears `dtr_enable` ("consider USB disconnected"). After that,
monitor lines stop until the host toggles DTR. A host that was only slow would silently lose every later line. Could monitoring
recover by itself (for example by re-enabling on the next command from that port)?

Not firmware: one of our Pi Zero nodes drops off USB ("disabled by hub (EMI?)") each time its radio transmits. The radio carries on,
and the node reconnects about 2 s later. The other node doesn't.

### Reply to the first bench (firmware, 2026-10-13)

- **DTR over USB-C: fixed in firmware, not yet on a radio.** Sending monitor-on from the USB port now marks the host as
  connected, so lines arrive with DTR low. It also answers the recovery question: after a send time-out, resend monitor-on and
  the lines come back. Keep DTR low as the spec says, and resend monitor-on if lines stop. Spec section 2 now says this.
  The status reply still reports monitoring as on after a time-out, so it cannot tell you the lines stopped.
- Raising DTR after the handshake keeps working as a fallback on older firmware.
- The Pi Zero dropping off the hub when the radio transmits is RF or power noise, not firmware.
- Build: flash 109.11 KiB of 118, RAM 14.50 KiB of 16. The new image needs flashing before the fix applies.

**A bug that stops long messages being acked** (found in the bench race, 2026-10-13):
- Where: `APRS_RxInfo` (`App/app/aprs_rxinfo.c`) looks for the `{nn` line number inside the loop that copies the display text:
  `for (i = 11; i < ilen && o < APRS_RXTEXT_MAX; i++)`.
- What goes wrong: `out->text` starts with "sender>", and `APRS_RXTEXT_MAX` is 43. With a 9-character sender, a message over
  about 34 characters fills the display text before the loop reaches `{`. `ack_seq` stays empty, and the radio never acks it.
  It also reports no `DROP:ack`.
- What we saw: checkin-board's race reports are up to 67 characters, so a UV-K5 HQ acked every short heartbeat
  (`RC1 H PIPI 5 195332`) but none of the 20 batches. The checkpoint resent them until its window filled, and the last 4 bibs
  never went out.
- The fix: find `{` (and the line number) over the whole info field, separately from the display copy. Only the display
  text should be truncated. Please add a host test with a 67-character message.

Bench setup for both findings: two Pi Zero W nodes over USB-C, F4HWN v6.1.0, protocol revision 1, same room, low power.
Our bench also has a hardware problem of its own: one node's USB drops ("disabled by hub (EMI?)") each time its radio
transmits, and the other node decodes few of that radio's packets. We'll look at its antenna and cabling.

### Reply: long messages not acked (firmware, 2026-10-13)

- **Confirmed and fixed**, as you described it: `APRS_RxInfo` looked for `{nn` inside the loop that fills the 43-character display text,
  so a message that filled the display first was never acked. A sender of 9 characters made it about 34 characters. The search for
  `{` now covers the whole message; only the display copy is cut short.
- Host test added: a 67-character message from `KA2AA-15` gets `ack_seq` `07` (and a 3-digit number is kept too). It failed before the fix.
- No change to the wire protocol or the spec. The new image needs flashing on the HQ radio; any radio that receives messages needs it.
- Not yet run on a radio.
