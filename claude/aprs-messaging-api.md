# APRS messaging over USB: host API for the UV-K5 V3 / UV-K1 APRS firmware

Audience: the author of a program that sends batches of short APRS messages through a radio running this firmware
(branch `aprs`, `f4hwn.aprs.bin`), connected to the computer by the USB-C cable or the 2-pin K-plug programming cable.

Status: written from the firmware source (`App/app/aprs_cmd.c`, `aprs_station.c`, `aprs_serial.c`, `aprs_rxinfo.c`,
`aprs_msg.c`) and the working reference client `tools/aprs/aprs_pc.py` (host-tested in `tools/aprs/test_aprs_pc.py`). The
byte layouts below are pinned by those tests. **Not yet exercised on a radio by a batching client**: the pacing and
timeout numbers are the firmware's limits, the suggested client timings are advice.

Reference client, usable as a library: `tools/aprs/aprs_pc.py` (`frame`, `send_cmd`, `read_reply`, `open_session`,
`get_settings`, `cmd_msg`, `monitor_line`). It needs `pyserial` only to open the port.

---

## 1. What the radio does and does not do

| It does | It does not |
|---|---|
| Accepts one message (destination + up to 30 characters) per command and transmits it on the radio's current frequency, from its own callsign-SSID. | Queue more than **one** message. A second `msg` before the first has gone out **replaces** it. |
| Sends it within about 0.5 s of the channel being quiet. | Tell the host that it has been sent, or that it was dropped. The command reply only means "queued". |
| Waits at least **5 s** between its own transmissions. | Retry on its own when no ack comes. |
| Gives up on any waiting transmission after **60 s** of a busy channel (all pending items are cleared, silently). | Report the line number it used for a message. |
| Acknowledges messages addressed to its own callsign-SSID automatically. | Check the text for forbidden APRS characters. |
| Prints every decoded frame (hex and text) to a host that asked for them (section 6). | |

Consequences for a batch sender are in section 8.

## 2. Connection

- **USB-C**: the radio appears as a virtual serial port (CDC). macOS `/dev/cu.usbmodem*`, Linux `/dev/ttyACM*`, Windows a COM
  port. The baud rate setting has no effect.
- **K-plug cable** (and an AIOC wired to it): macOS `/dev/cu.usbserial*` or `/dev/cu.usbmodem*` for an AIOC, Linux `/dev/ttyUSB*`.
  Use **38400 baud, 8N1, no flow control**. Open the port with **DTR and RTS low**: an AIOC keys the radio's PTT from those
  lines.
- Only **one program** can hold the port at a time (not UV Studio, CHIRP or a second copy of your app). The radio must be on and
  not in the bootloader.
- The radio must have a real callsign set (not the placeholder `N0CALL`), APRS switched on, the main VFO inside
  144-148 MHz or 420-450 MHz, and a battery that is not low. Otherwise a queued message is dropped (section 8.4).

## 3. Framing

Every command and every reply is one frame. All multi-byte numbers are **little-endian**.

```
AB CD | size u16 | obfuscated( payload | crc u16 ) | DC BA
```

- `payload` = `id u16` + `len u16` + `len` bytes of data. `size` = length of `payload` (so `size = 4 + len`).
- `crc` = CRC-16/XMODEM (poly 0x1021, init 0x0000, no reflection) over `payload`, sent little-endian. **Commands from the host
  must carry a correct CRC.** **Replies from the radio carry no checksum**: after de-obfuscation the two bytes in the CRC
  position are always `FF FF` (a marker). Do not verify a CRC on replies; check `AB CD`, the `size`, the trailer `DC BA`, and
  the reply `id`.
- Obfuscation: XOR each byte of `payload | crc` with `KEY[i mod 16]`, `i` counting from 0 at the first payload byte:

```
KEY = 16 6C 14 E6 2E 91 0D 40 21 35 D5 40 13 03 E9 80
```

- A frame on the wire is therefore `size + 6` bytes. To read one: find `AB CD`, read `size` (2 bytes), then read `size + 4`
  bytes (obfuscated payload and the 2-byte CRC slot, then `DC BA`), de-obfuscate the first `size` bytes. This is what the
  reference `read_reply` does.
- The same stream also carries **plain text lines** when monitoring is on (section 6). They never start with byte `AB`, so a
  parser can tell the two apart: `AB CD` starts a binary frame, anything else is text up to `\r\n`.

Worked example, session timestamp `0x11223344`:

```
handshake command    ab cd 08 00 02 69 10 e6 6a a2 2f 51 f2 22 dc ba
msg  N0CALL-7 "hello from the ridge"   (payload 48 bytes, 44 bytes of data)
  ab cd 30 00 16 6b 38 e6 6a a2 2f 51 6f 05 96 01 5f 4f c4 b7 16 6c 7c 83 42 fd 62 60 47 47 ba 2d 33 77 81 e5 36 1e 7d 82
  49 f4 0d 40 21 35 d5 40 13 03 e9 80 66 0b dc ba
unobfuscated payload 00 07 2c 00 | 44 33 22 11 | "N0CALL-7" 00 00 | "hello from the ridge" 00 x10
```

## 4. Session handshake (once per port, before any command that changes anything)

Command `0x0514`, data = `ts u32`: a random **non-zero** 32-bit number you choose. The radio latches `ts` for this port.

Reply `0x0515` (as a decoded payload): `id u16 = 0x0515`, `len u16`, then the body; bytes `4..19` are the firmware version as
a NUL-terminated string (16 bytes). Treat anything else as "not this firmware / not answering".

Every command below that changes anything or transmits repeats `ts` as the first 4 data bytes. A command whose `ts` does not
match the latest handshake on that port is **refused** (Ok = 0). A new handshake replaces the old `ts`. After a radio restart
(or USB unplug) do the handshake again. Read-only commands (`setup get`) need no `ts`.

## 5. Commands

Standard reply ("ack"): payload `id+1 u16`, `len u16 = 4`, `Ok u8`, 3 padding bytes. `Ok = 1` success, `0` refused.

### 5.1 Send a message: `0x0700`  ->  `0x0701`

| Offset | Size | Field |
|---|---|---|
| 0 | 4 | `ts u32` |
| 4 | 10 | destination, ASCII, NUL-terminated or NUL-padded; at most **9** characters are used. |
| 14 | 30 | message text, NUL-terminated or NUL-padded; at most **30** characters. |

Data length is exactly 44. Reply `Ok = 1` means **queued**. `Ok = 0` means any of: wrong or missing `ts`; empty destination;
empty text. (It does *not* check callsign, APRS on, or band: those drop the message later, silently.)

On the air the packet is `:` + destination padded with spaces to 9 characters + `:` + text + `{nn`, where `nn` is a line
number 01-99. The radio picks `nn`; it counts up by one for each message (99 wraps to 01) and **every `0x0700` gets a new
number, even if the text is identical**. There is no way to read the number back or to make the radio reuse one.

- Destination: upper case `A-Z 0-9` and `-` (callsign-SSID, e.g. `N0CALL-7`). The firmware does not validate it. The
  reference client upper-cases it.
- Text: the firmware does not validate or filter it either; bytes go onto the air as given. Send **printable ASCII
  `0x20-0x7E`** only, and avoid `{`, `|` and `~` (reserved in APRS message text). Anything over 30 bytes is **cut silently**:
  split longer text yourself, on character boundaries (UTF-8 multi-byte characters count per byte).

### 5.2 Read the settings: `0x070C`  ->  `0x070D`  (no data, no `ts`)

Reply data: `Ok u8`, 3 padding bytes, then the **96-byte settings record**. Use it as a pre-flight check. Fields (offsets
inside the record):

| Offset | Size | Field |
|---|---|---|
| 0 | 4 | `"APR1"` (anything else: not this firmware) |
| 5 | 1 | APRS on (1) or off (0) |
| 7 | 1 | SSID 0-15 |
| 12 | 8 | callsign, NUL-terminated, 1-6 characters `A-Z 0-9`. `N0CALL` is the placeholder: not configured. |
| 20 | 10 | the radio's current message target (not needed) |

The record's CRC (offset 90, CRC-16/CCITT-FALSE over bytes 0-89) is checked by the reference `unpack_record`.

### 5.3 APRS on/off: `0x0706`  ->  `0x0707`

Data: `ts u32`, `on u8` (1 on, 0 off). Switching off also clears anything queued. Normally leave it on.

### 5.4 Monitor on/off: `0x0710`  ->  `0x0711`

Data: `on u8` (no `ts` is needed; the reply is a standard ack). While on, the radio writes the text lines of section 6
to **this port only**. It stays on until you send `0` or the radio restarts. Binary replies keep coming as normal frames
among the text lines.

### 5.5 Other commands (not needed for messaging)

`0x0702` beacon now; `0x0708` raw AX.25 frame (source must be the radio's own callsign); `0x070A` digipeater settings and
counters; `0x070E` write all settings. See `claude/serial-quickstart.md` and `App/app/aprs_cmd.h`.

## 6. Receiving: monitor lines

With monitoring on, each decoded frame produces two text lines ending `\r\n`:

```
APRSRAW:<hex>        the AX.25 frame without its FCS, upper-case hex, no spaces
APRS:<text>          what the radio shows for it (see below)
DIGI:<what> <src>    a digipeater decision; ignore for messaging
```

Own transmissions are not printed (the radio ignores frames from its own call-SSID, so there is **no "sent" line**).

Use `APRSRAW:` for matching; the `APRS:` text is for display only (at most 43 characters, non-printables replaced).
Frame layout: destination (7 bytes), source (7 bytes), 0-8 digipeaters (7 bytes each), control `03`, PID `F0`, information
field. Each address is 6 ASCII characters shifted left one bit, plus an SSID byte: SSID = `(byte >> 1) & 0x0F`; the **last**
address has bit 0 of its SSID byte set.

A message in the information field is `:` + 9-character addressee padded with spaces + `:` + text, optionally `{nn`.
An **ack** for your message is such a frame **addressed to the radio's own callsign-SSID** whose text is `ack` + `nn` (or `rej`
+ `nn` for a refusal), sent by the station you messaged.

## 7. Recommended client loop

1. Open the port (DTR/RTS low), handshake (4). No answer in 3 s: stop and report "radio not answering / wrong port".
2. `0x070C`: check `"APR1"`, APRS on, callsign not `N0CALL`. Report problems to the user before sending anything.
3. Turn monitoring on (5.4) and keep reading continuously, in its own thread or task, so acks are not missed while sleeping.
4. For each message in the batch, after normalising it (printable ASCII, upper-case destination) and splitting it into pieces of
   at most 30 bytes:
   1. Wait until at least **6 s** have passed since the previous `0x0700` (the radio's own gap is 5 s).
   2. Send `0x0700`. `Ok = 0`: re-do the handshake once and retry; if it still fails, report it and stop.
   3. Wait for an ack from the destination's callsign-SSID (match on the source address of an `APRSRAW` frame addressed to
      the radio's call-SSID with text `ack...`). Give it **30-60 s**, depending on the path.
   4. No ack in time: resend (it is a new message to the far end; it may arrive twice). After 3-5 tries, mark it failed.
   5. Only then send the next message. **Never** send the next one while this one is waiting: it would overwrite it.
5. At the end, turn monitoring off.

Matching acks to messages: with one message in flight at a time, an ack from the destination during the wait belongs to it.
The line number is not reported by the radio, but the first ack tells you the number in use and each new message adds
one (99 wraps to 1); a client may use that to detect a stale or duplicate ack, but should not depend on it.

## 8. Limits and failure behaviour

1. **One message slot.** A second `0x0700` replaces a message that has not been transmitted yet. Pace as in section 7.
2. **Minimum 5 s between transmissions** from this station (messages, acks, beacons). Incoming messages addressed to the
   radio are acknowledged automatically and go **before** a queued message, so a busy conversation delays the batch.
3. **Busy channel**: the radio waits for a clear channel. If it stays busy for 60 s, the radio gives up and clears *every*
   pending transmission without telling the host. If the radio is scanning, or being configured, a request stays queued
   up to that same minute.
4. **Silent drops.** The transmission is refused, with no reply to the host (the radio gives a double beep), when: the callsign is the placeholder, APRS is off,
   the main VFO is outside the allowed bands, or the battery is low. In monitor output you may see `DIGI:NOTX` for a
   digipeater repeat refused for those reasons; a refused message gives no line. Hence the pre-flight check and the ack timeout.
5. **No delivery information** except the far station's ack. A message to a station that does not ack looks the same as one
   lost on the air.
6. **Text** over 30 bytes is cut; the host splits. Characters are not filtered by the radio (section 5.1).
7. **Line numbers** run 01-99 and wrap; every serial message gets a new one. Retries are new messages.
8. **Regulatory**: the radio transmits under the callsign stored in it. Whoever runs the batch is the control operator; keep
   the rate sensible (30 characters every 6 s is the fastest this path can go) and do not message stations that have not
   agreed to it.

## 9. Error summary

| Symptom | Cause |
|---|---|
| No handshake reply | wrong port; radio off or in a menu that blocks serial; another program holds the port; cable wiring |
| Handshake ok, `Ok = 0` on `msg` | stale `ts` (radio restarted or a second client re-handshook); empty destination or text |
| `Ok = 1` but nothing transmitted | placeholder callsign; APRS off; frequency out of band; low battery; channel busy for 60 s; or replaced by a later `msg` |
| `Ok = 1`, transmitted, no ack | far station did not receive, is not acking, or its ack was lost |
| Text lines in the stream | monitoring is on; parse as in section 3 |

## 10. Possible firmware additions (not built)

If your app needs them I can add: a `0x0700` reply that includes the line number used; a text line such as
`SENT:<to>{nn}` when a queued message actually goes out, and `DROP:` when it is given up; a message queue deeper than one.
Each is a small change in `aprs_cmd.c` / `aprs_station.c` with host tests, and a new field or line that older clients
simply ignore.
