# Estimate: checkin-board firmware requests (2026-10-12) against the APRS firmware

Answers `checkin-board/docs/specs/2026-10-12-uvk5-firmware-requests.md` (requests 0-11). Written 2026-10-12 from the
source and the build of branch `aprs` at `b1377552`. Sizes are estimates from the code, not measurements; nothing here is built yet.

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
