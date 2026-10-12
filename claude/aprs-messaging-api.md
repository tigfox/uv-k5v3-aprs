# APRS messaging over USB: host API for the UV-K5 V3 / UV-K1 APRS firmware

Audience: the author of a program that sends batches of short APRS messages through a radio running this firmware
(branch `aprs`, `f4hwn.aprs.bin`), connected to the computer by the USB-C cable or the 2-pin K-plug programming cable.

Revision 2 (firmware **protocol revision 1**, read it with `0x0712`; section 5.6): the radio now keeps a **queue of 4 messages**
and **4 pending acks**, the send reply carries the **line number, free queue places and a reason code**, the monitor prints
**`SENT:` / `DROP:` lines**, and a refused message says why. A full queue refuses and never replaces. Earlier revision (2026-10-11):
message text is 1 to 67 characters (was 30); over 67 is refused. Features of a later revision (status command, set callsign,
digipeater path, gap, signal strength) are listed in section 10 and are **not in the firmware yet**; the capability bits tell you.

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
| Accepts a message (destination + up to 67 characters) per command, **up to 4 waiting**, and transmits them in order on the radio's current frequency, from its own callsign-SSID. | Replace a waiting message: when 4 are waiting, the next command is **refused** (reason `full`). |
| Sends the first within about 0.5 s of the channel being quiet. | Retry on its own when no ack comes. |
| Tells you the **line number** it assigned (in the send reply), and prints **`SENT:`** when the message has gone out, **`DROP:`** with a reason when it is given up. | Check the text for forbidden APRS characters. |
| Waits at least **5 s** between its own transmissions. | Report anything before the transmission ends (it is blocking, about 0.8 s). |
| Gives up on a waiting item after **60 s** of a busy channel (that item only; the next one gets its own minute). | |
| Acknowledges messages addressed to its own callsign-SSID automatically, **up to 4 acks waiting**, sent before queued messages. | |
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

The example pads the text to 30 bytes, as the reference client does. Current firmware does not need the padding (data length
is `14 + text length`, up to `14 + 67 = 81`); firmware from before the 67-character change does (section 5.1).

Frames from the radio carry `FF FF` where a CRC would be, obfuscated with the next key bytes. Examples (decoded payload, then the wire frame):

```
capabilities command      ab cd 04 00 04 6b 14 e6 71 e2 dc ba                       (payload 12 07 00 00: id 0x0712, no data, no ts)
capabilities reply        payload 13 07 0c 00 | 01 01 00 00 | 0f 00 00 00 | 04 04 43 00
                          wire    ab cd 10 00 05 6b 18 e6 2f 90 0d 40 2e 35 d5 40 17 07 aa 80 e9 93 dc ba
send reply, queued        payload 01 07 04 00 | 01 07 03 00       (Ok, line 07, 3 places free, reason 0)
                          wire    ab cd 08 00 17 6b 10 e6 2f 96 0e 40 de ca dc ba
send reply, queue full    payload 01 07 04 00 | 00 00 00 01       (refused, no line number, 0 free, reason 1 = full)
                          wire    ab cd 08 00 17 6b 10 e6 2e 91 0d 41 de ca dc ba
```

## 4. Session handshake (once per port, before any command that changes anything)

Command `0x0514`, data = `ts u32`: a random **non-zero** 32-bit number you choose. The radio latches `ts` for this port.

Reply `0x0515` (as a decoded payload): `id u16 = 0x0515`, `len u16`, then the body; bytes `4..19` are the firmware version as
a NUL-terminated string (16 bytes). Treat anything else as "not this firmware / not answering".

Every command below that changes anything or transmits repeats `ts` as the first 4 data bytes. A command whose `ts` does not
match the latest handshake on that port is **refused** (Ok = 0). A new handshake replaces the old `ts`. After a radio restart
(or USB unplug) do the handshake again. Read-only commands (`setup get`) need no `ts`.

## 5. Commands

Standard reply ("ack"): payload `id+1 u16`, `len u16 = 4`, `Ok u8`, 3 further bytes. `Ok = 1` success, `0` refused. For most commands the
3 bytes are padding (zero); the **send reply** uses them (section 5.1).

### 5.1 Send a message: `0x0700`  ->  `0x0701`

| Offset | Size | Field |
|---|---|---|
| 0 | 4 | `ts u32` |
| 4 | 10 | destination, ASCII, NUL-terminated or NUL-padded; at most **9** characters are used. |
| 14 | 1..67 | message text: **the rest of the data**, 1 to 67 bytes. A NUL byte ends the text early. |

Data length is `14 + n`, where `n` (1 to 67) is the number of text bytes; there is no padding requirement.

**Reply** (`0x0701`, `len = 4`): `Ok u8`, `line u8`, `free u8`, `reason u8`.

| Byte | Meaning |
|---|---|
| `Ok` | 1 = **queued**; 0 = refused |
| `line` | the line number (1-99) the radio assigned to this message; 0 if refused |
| `free` | places left in the message queue after this call (0 when full) |
| `reason` | 0 when queued; otherwise why it was refused (section 5.7): `session` (wrong or missing `ts`), `empty` (no destination or text), `toolong` (more than 67 text bytes: refused, never cut), `full` (4 waiting) |

Firmware before protocol revision 1 sends zero bytes after `Ok`: treat `line = 0` with `Ok = 1` as "queued, number unknown". Only trust
these three bytes when the capability bit `send-fields` is set (section 5.6).

A queued message is **not** checked against the callsign, APRS on, band or battery: those drop it later, and you are told by a
`DROP:` line (section 6) with the reason.

On the air the packet is `:` + destination padded with spaces to 9 characters + `:` + text + `{nn`, where `nn` is a line
number 01-99. The radio picks `nn` **when the message is queued** and returns it in the reply; it counts up by one for each
message (99 wraps to 01) and **every `0x0700` gets a new number, even if the text is identical**. You cannot make the radio reuse
one, so a retry is a new message to the far end.

- Destination: upper case `A-Z 0-9` and `-` (callsign-SSID, e.g. `N0CALL-7`). The firmware does not validate it. The
  reference client upper-cases it.
- Text: the firmware does not validate or filter it either; bytes go onto the air as given. Send **printable ASCII
  `0x20-0x7E`** only, and avoid `{`, `|` and `~` (reserved in APRS message text). Anything over 67 bytes is **refused**:
  split longer text yourself, on character boundaries (UTF-8 multi-byte characters count per byte).

**Older firmware.** Images built before the 67-character change (including the field-test image `f4hwn.aprs.bin` of
2026-10-08) need the text field to be at least 30 bytes (data of at least 44 bytes) and **cut the text to 30 characters
without saying so**. They have one message slot (a second command replaces the first), no capabilities command, and no `SENT:` /
`DROP:` lines. Read `0x0712` first (section 5.6): a firmware that does not answer it is one of these. A client that must also
work with that firmware should pad shorter text with NULs to 30 bytes (the reference client does), send one message at a time,
and use a fixed delay or an ack instead of the `SENT:` line.

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

### 5.6 Capabilities: `0x0712`  ->  `0x0713`  (no data, no `ts`)

Ask once after the handshake. **No answer, or a refusal, means revision 0**: the basic message command only.

Reply data (12 bytes): `Ok u8`, `revision u8`, 2 padding bytes, `features u32`, `queue_depth u8`, `ack_depth u8`, `max_text u8`, 1 padding byte.

| Bit of `features` | Name | Meaning |
|---|---|---|
| 0 | `send-fields` | the send reply carries the line number, free places and reason (5.1) |
| 1 | `events` | `SENT:` / `DROP:` monitor lines (6) |
| 2 | `acks` | several acks can wait (`ack_depth`) |
| 3 | `queue` | several messages can wait (`queue_depth`); a full queue refuses |

Revision 1 is: bits 0-3 set, `queue_depth` 4, `ack_depth` 4, `max_text` 67. Later bits and revisions are added at the end; ignore bits you do not know.

### 5.7 Reason codes (one table: the send reply, `DROP:` lines, and the status command when it is added)

| Code | Text | Meaning |
|---|---|---|
| 0 | | ok |
| 1 | `full` | the message queue (or the ack list) is full |
| 2 | `session` | wrong or missing `ts` (a refusal only) |
| 3 | `empty` | empty destination or text (a refusal only) |
| 4 | `toolong` | more than 67 text bytes (a refusal only) |
| 5 | `busy` | the channel, or the radio (scanning, being configured), stayed busy for 60 s |
| 6 | `nocall` | the callsign is the placeholder |
| 7 | `aprsoff` | APRS is switched off |
| 8 | `band` | the TX frequency is outside the allowed bands (144-148, 420-450 MHz) |
| 9 | `battery` | the battery is too low |
| 10 | `mode` | the TX VFO is not FM |
| 11 | `replaced` | reserved: this firmware never replaces |
| 12 | `off` | APRS was switched off while this was waiting |

## 6. Receiving: monitor lines

With monitoring on, each decoded frame produces two text lines ending `\r\n`:

```
APRSRAW:<hex>        the AX.25 frame without its FCS, upper-case hex, no spaces
APRS:<text>          what the radio shows for it (see below)
DIGI:<what> <src>    a digipeater decision; ignore for messaging
```

Own transmissions are not printed as frames (the radio ignores frames from its own call-SSID). What the radio itself did is reported by
two more kinds of line, one per event:

```
SENT:<to>{nn}                    a queued message went on the air (after the transmission ended, about 0.8 s later)
DROP:<to>{nn} <reason>           a queued message was given up; <reason> is the text of section 5.7
DROP:ack <to>{<seq>} <reason>    an ack was given up; <seq> is the sender's line number as received (up to 5 characters)
```

`<to>` is the destination exactly as it went on the air (a callsign-SSID, up to 9 characters), `nn` the two-digit line number from the
send reply. The closing brace is part of the line; the packet itself has none. A message produces **exactly one** `SENT:` or one `DROP:`.
Messages given up because APRS was switched off are reported (`off`), one line each. Acks are reported only when dropped, not when sent.

Use `APRSRAW:` for matching; the `APRS:` text is for display only (at most 43 characters, non-printables replaced).
Frame layout: destination (7 bytes), source (7 bytes), 0-8 digipeaters (7 bytes each), control `03`, PID `F0`, information
field. Each address is 6 ASCII characters shifted left one bit, plus an SSID byte: SSID = `(byte >> 1) & 0x0F`; the **last**
address has bit 0 of its SSID byte set.

A message in the information field is `:` + 9-character addressee padded with spaces + `:` + text, optionally `{nn`.
An **ack** for your message is such a frame **addressed to the radio's own callsign-SSID** whose text is `ack` + `nn` (or `rej`
+ `nn` for a refusal), sent by the station you messaged.

## 7. Recommended client loop

1. Open the port (DTR/RTS low), handshake (4). No answer in 3 s: stop and report "radio not answering / wrong port".
2. `0x0712`: note the revision, the bits and the limits (5.6). Revision 0: use the one-message mode described in 5.1.
3. `0x070C`: check `"APR1"`, APRS on, callsign not `N0CALL`. Report problems to the user before sending anything.
4. Turn monitoring on (5.4) and keep reading continuously, in its own thread or task, so lines are not missed while sleeping.
5. Send up to `queue_depth` messages without waiting, as long as the `free` byte of the last reply is above 0. For each:
   1. Normalise (printable ASCII, upper-case destination) and split into pieces of at most 67 bytes.
   2. Send `0x0700`. `Ok = 1`: remember the `line` number. `Ok = 0` with `full`: wait for a `SENT:` or `DROP:` line and send again.
      `session`: re-do the handshake once and retry. `empty` / `toolong`: a bug in the caller.
   3. Wait for the matching **`SENT:<to>{nn}`** (the transmission happened) or **`DROP:<to>{nn} <reason>`** (it will not happen). Mark
      the message failed with that reason on a `DROP:`, and show the reason to the operator (`battery`, `band` and `nocall` are
      things they can fix at once).
   4. After `SENT:`, wait for the ack: an `APRSRAW:` frame addressed to the radio's call-SSID, from the destination, whose text is
      `ack` + `nn` (or `rej` + `nn`). Match on the **source**, the **line number** and your message, so a late ack for an earlier
      copy never confirms a later message. Give it **30-60 s**, depending on the path.
   5. No ack in time: resend (it is a new message with a new line number; the far end may get it twice). After 3-5 tries, mark it failed.
6. At the end, turn monitoring off.

The radio sends one transmission at a time, 5 s apart, so a full queue of 4 takes about 20 s to clear; that is the throughput limit,
not the number of messages you may have waiting.

## 8. Limits and failure behaviour

1. **Queue of 4.** The fifth `0x0700` is refused (`full`). Nothing is ever replaced.
2. **Minimum 5 s between transmissions** from this station (messages, acks, beacons). Incoming messages addressed to the
   radio are acknowledged automatically and go **before** queued messages, so a busy conversation delays the batch. Up to 4 acks
   wait; a fifth is dropped and reported (`DROP:ack ... full`), and the sender will retry.
3. **Busy channel**: the radio waits for a clear channel. If it stays busy for 60 s for the item at the front, that item is given up
   (`DROP:... busy`) and the next one gets its own minute. A scanning or configuring radio counts as busy.
4. **Refused transmissions.** When the radio will not transmit (placeholder callsign, APRS off, frequency outside the bands, TX VFO not
   FM, battery low) it gives a double beep and reports `DROP:` with the reason, one per waiting item.
   A callsign check before you start (5.2) catches `nocall` and `aprsoff`; a status command for the others is listed in section 10.
5. **Delivery information** is the far station's ack. `SENT:` only says the radio transmitted. A message to a station that does not
   ack looks the same as one lost on the air.
6. **Text** over 67 bytes is refused; the host splits. Characters are not filtered by the radio (section 5.1).
7. **Line numbers** run 01-99 and wrap; every serial message gets a new one. Retries are new messages. With 4 waiting and the
   radio's own acks in between, numbers do not repeat for 99 messages: more than enough to tell them apart.
8. **Regulatory**: the radio transmits under the callsign stored in it. Whoever runs the batch is the control operator; keep
   the rate sensible (the radio cannot send more than one packet every 5 s) and do not message stations that have not agreed to it.

## 9. Error summary

| Symptom | Cause |
|---|---|
| No handshake reply | wrong port; radio off or in a menu that blocks serial; another program holds the port; cable wiring |
| Handshake ok, `Ok = 0` on `msg` | read the `reason` byte: `session` (radio restarted or a second client re-handshook), `empty`, `toolong`, `full` |
| `Ok = 1`, then `DROP:` | the reason in the line: `nocall`, `aprsoff`, `band`, `mode`, `battery`, `busy`, `off` |
| `Ok = 1`, `SENT:`, no ack | far station did not receive, is not acking, or its ack was lost |
| `Ok = 1` but no `SENT:` or `DROP:` ever | monitoring is off, or the line was lost on the port: fall back to a timeout |
| Text lines in the stream | monitoring is on; parse as in section 3 |

## 10. Not in the firmware yet (planned, in this order)

See `claude/aprs-firmware-request-estimate.md` for the status. Each adds a feature bit to `0x0712`; do not rely on one until the bit is set.

| Feature | Command / line | Notes |
|---|---|---|
| Status | `0x0714` -> `0x0715` | battery, frequency, in band, would it transmit now and why, channel busy, monitoring on, frame counters |
| Set callsign / SSID | `0x0716` -> `0x0717` | with `ts`; lets a program assign callsigns from a list |
| Digipeater path for messages and acks | settings byte 92, menu item | none / `WIDE1-1` / `WIDE1-1,WIDE2-1` (default, as today) |
| Minimum gap between transmissions | settings byte 93, menu item | 2-10 s, default 5 s |
| Signal strength per frame | `APRSRSSI:<dBm>` line right after its `APRSRAW:` line | |
