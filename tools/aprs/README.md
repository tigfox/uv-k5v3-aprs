# APRS tools

Host tests (no radio): `make -C tools/aprs test` (C unit tests under AddressSanitizer, the demodulator against
armel's Python model, and `python3 -m pytest tools/aprs/test_aprs_pc.py`). `make -C tools/aprs coverage` reports
line coverage of the hardware-free firmware files.

## aprs_pc.py: control a radio from a computer

Needs `pip install pyserial`. Radio on, USB-C cable (macOS: `/dev/cu.usbmodem*`) or the K-plug cable
(`/dev/cu.usbserial*`). Run `aprs_pc.py` with no arguments for the list of commands.

    aprs_pc.py /dev/cu.usbmodem1101 status
    aprs_pc.py /dev/cu.usbmodem1101 setup get site.json     # read every APRS setting
    aprs_pc.py /dev/cu.usbmodem1101 setup set site.json     # configure a unit from a saved file
    aprs_pc.py /dev/cu.usbmodem1101 msg N0CALL-7 "hello"
    aprs_pc.py /dev/cu.usbmodem1101 monitor                 # decoded packets and digipeater decisions
    aprs_pc.py loc 40.7128 -74.0060                         # the code for the radio's Loc menu item

One saved `site.json` configures every digipeater identically (change `call`, `ssid` and `comment` per site).
`setup set` validates the whole file on the PC and again in the radio and writes all of it or none of it.
Everything that changes a setting or transmits needs the session timestamp from the 0x0514 handshake, which the
tool does for you; `status`, `digi`, `setup get` and `monitor` are read-only. A raw frame (`raw <hex>`) is accepted
only with our own callsign as its source.

The `monitor` lines (`APRSRAW:`, `APRS:`, `DIGI:`) are plain text and sent only to a port that asked for them, so
UV Studio and K5Viewer, which share the USB port, never see them.

Command numbers (0x0700-0x0710), their layouts and the settings record are documented in `App/app/aprs_cmd.h`
and `App/app/aprs_settings.c`.
