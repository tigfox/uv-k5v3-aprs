# APRS build for the UV-K5 V3 / UV-K1

A fork of [armel/uv-k1-k5v3-firmware-custom](https://github.com/armel/uv-k1-k5v3-firmware-custom) (F4HWN Fusion,
v6.1.0) with APRS running inside the normal firmware: receive and decode, auto-beacon, messages with acks, and a
digipeater. It is built from the `APRS` preset. Every other preset is unchanged (byte for byte apart from the build
stamp). Licensed amateurs only.

What it does

- **Receive and decode** Bell 202 (1200 baud) packets in software from the radio's audio, continuously, with the
  speaker as usual. 16 µs average per sample on a 104 µs budget.
- **Transmit** real 1200/2200 Hz AFSK, so hardware-TNC radios (Yaesu, Kenwood) decode it.
- **Beacon** on a timer or on demand: position (`!` with `/>` mobile or `/#` digipeater) or, with no location, a
  status packet.
- **Messages**: send, receive, line numbers, automatic acks, a read-back menu item.
- **Digipeater**: OFF / FILL (WIDE1-1) / WIDE (WIDEn-N, New-N), a hop limit, duplicate and cancel rules, a random
  delay, busy-channel and rate limits.
- **Screen**: an APRS panel in place of VFO B (last heard, heard / repeated / duplicate / dropped), a packet box.
- **Menu** cut down to APRS, RF, battery and key lock; the rest is set with a programming cable.
- **Serial** (USB-C and the K-plug cable): configure from a file, send, monitor. See `claude/serial-quickstart.md`.
- 2 m (144-148 MHz) and 70 cm (420-450 MHz) only, for the VFO and for transmitting.

Build

```
./compile-firmware.sh APRS        # needs Docker; result: build/APRS/f4hwn.aprs.bin
make -C tools/aprs test           # host tests (C under AddressSanitizer, the demodulator against the Python model, pytest)
claude/baseline/check.sh          # the other presets still match the saved images
```

The release image of the version in use is kept in `claude/release/` with its checksum.

Flash with [UV Studio](https://armel.github.io/uvstudio/) (the radio in DFU mode). Back up the calibration first, and
keep stock Fusion in multiboot slot 1: hold MENU at power-on to reach the selector.

Documents

| File | What |
|---|---|
| `claude/radio-card.html` | the printable operating card: menus, set-up, digipeater settings, flashing and recovery |
| `claude/serial-quickstart.md` | configuring and monitoring from a computer |
| `claude/aprs-port-plan.md` | the design, decisions, and what was built at each step |
| `tools/aprs/README.md` | the tools and the tests |

Not in this build: the spectrum analyser and FM radio (compiled out), voice prompts (the audio pin is the decoder's
input), dual / full watch while APRS is on (it listens on the main VFO only).

The ported APRS logic comes from the ta1js UV-K5 APRS digipeater; the software modem (receive and transmit method) and
the Python model are armel's. All Apache 2.0.
