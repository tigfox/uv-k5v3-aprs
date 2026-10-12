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

## Questions for checkin-board

1. **Queue depth 4 or 8?** 4 is what fits without removing anything. Does a node ever need more than 4 messages in flight?
2. **Frequency (9):** should the command put the radio in frequency mode on the given frequency, or change the frequency of the current memory channel? And must it stay inside 144-148 and 420-450 MHz?
3. **Path (5):** one path for messages and acks (as written), or also for the station beacon?
4. **RSSI (10):** is a separate `APRSRSSI:` line acceptable?
5. **SENT (2):** "goes on air" is reported after the transmission ends (it is blocking, about 0.8 s); is that early enough?

## Compatibility

Every change adds a field, a line or a command that older hosts ignore (as the request asks), with one exception: the settings
record gains a byte (items 5, 6), and the current `aprs_pc.py` rejects it until updated. `claude/aprs-messaging-api.md` gets a new
revision when the first of these lands.
