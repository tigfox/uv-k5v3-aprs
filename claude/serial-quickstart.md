# Serial quick start: configure and monitor the APRS radio from a computer

Works over the **USB-C cable** (the radio shows up as a serial port) or the **2-pin K-plug programming cable**.
Everything below is `tools/aprs/aprs_pc.py`. The radio must be on and not busy transmitting.

## 1. One-time setup on the computer

```
pip3 install pyserial
cd uv-k1-k5v3-firmware-custom/tools/aprs
```

Find the port (radio connected, on):

| Cable | macOS | Linux | Windows |
|---|---|---|---|
| USB-C | `ls /dev/cu.usbmodem*` | `ls /dev/ttyACM*` | Device Manager, COM port |
| K-plug | `ls /dev/cu.usbserial*` | `ls /dev/ttyUSB*` | Device Manager, COM port |

Below, `PORT` stands for that path, for example `/dev/cu.usbmodem1101`.

## 2. Check it talks

```
python3 aprs_pc.py PORT status
```

You should see the firmware version, your callsign and settings, and the digipeater counters. If it says
*no answer from the radio*: wrong port, or the radio is off. Close UV Studio first (only one program can have
the port).

## 3. Configure a unit from a file (the quick way to set up several digis)

```
cp site.example.json site.json        # edit call, ssid, comment ...
python3 aprs_pc.py PORT setup set site.json
python3 aprs_pc.py PORT setup get check.json    # read it back
```

`site.json` holds every APRS setting. Leave a line out and that setting keeps its current value. Names are
spelled out: `"digi": "OFF" | "FILL" | "WIDE"`, `"delay": "OFF" | "250ms" | "500ms" | "1s"`,
`"beacon": "MOBILE" | "DIGI"`, `"interval_min": 0` (no automatic beacon) or 1 and up.
The file is checked on the computer **and** in the radio; it is applied entirely or not at all.

Give each radio its own **ssid** (for example `-1` for the digi, `-7` for a handheld). Two stations on the same
callsign **and** SSID are treated as one: the digi will not repeat its own call-SSID.

A position without GPS: `python3 aprs_pc.py loc 40.7128 -74.0060` prints a 15-digit code. Put it in the file
(`"loc": "130712810599405"`) or type it into the radio's **Loc** menu item. Or write `"lat": 40.7128,
"lon": -74.0060` in the file instead. With **no** location the beacon is a status packet (`>comment`): the unit
still identifies itself on the air.

## 4. Watch traffic

```
python3 aprs_pc.py PORT monitor
```

Prints each decoded packet and what the digipeater decided: `QUE` (queued), `RPT` (repeated), `DUP` (already
repeated), `CNCL` (a neighbour got there first), `HOPS` (too many hops asked), `DROP`/`BUSY` (channel busy),
`NOTX` (the radio refused to transmit). Add `--raw` to also print the raw hex. Ctrl-C stops it. UV Studio is not
affected by this: the radio sends these lines only to a program that asks for them.

## 5. Send

```
python3 aprs_pc.py PORT msg N0CALL-7 "hello from the ridge"   # an APRS message (30 characters)
python3 aprs_pc.py PORT beacon                                  # beacon now
python3 aprs_pc.py PORT on        # or: off
python3 aprs_pc.py PORT digi      # digipeater settings and counters
python3 aprs_pc.py PORT digi set WIDE 2 500ms DIGI
```

A message or beacon is **queued**: the radio sends it within about half a second once the channel is quiet,
and not more often than every 5 seconds. It needs a real callsign set, and the main VFO inside 144-148 or
420-450 MHz. A message is acknowledged by the other station; `monitor` shows the ack arriving.

## 6. If something does not work

| You see | Likely cause |
|---|---|
| `no answer from the radio` | wrong port; radio off; another program has the port |
| `the radio refused it` | no real callsign set; wrong session (just retry); APRS command while the radio is busy |
| `not queued` (msg) | empty text or destination, or no callsign |
| a settings file error | the message names the item; fix the file |
| `monitor` shows `NOTX` | transmit refused: frequency outside the bands, or low battery |
| `monitor` shows `DROP` or `BUSY` | the channel stayed busy for 5 s, or the unattended cap was reached (20 repeats a minute) |

All the commands are listed with `python3 aprs_pc.py` (no arguments). Technical detail: `tools/aprs/README.md`.
