#!/usr/bin/env python3
"""PC control for the APRS firmware (radio powered on, USB-C serial port or the K-plug cable).

  aprs_pc.py <port> status                    version, APRS settings, digipeater counters
  aprs_pc.py <port> msg <TO> <text...>        queue an APRS message (30 characters)
  aprs_pc.py <port> beacon                    queue the station beacon
  aprs_pc.py <port> on | off                  switch APRS on or off
  aprs_pc.py <port> raw <hex>                 transmit a raw AX.25 frame (no FCS); the source must be our callsign
  aprs_pc.py <port> digi                      digipeater settings, counters, uptime
  aprs_pc.py <port> digi set MODE HOPS DELAY BCNTY
                                              e.g. digi set WIDE 2 500ms DIGI
      MODE  OFF | FILL | WIDE      HOPS  1-7 (highest n honoured in WIDEn-N)
      DELAY OFF | 250ms | 500ms | 1s        BCNTY MOBILE | DIGI
  aprs_pc.py <port> setup get [file.json]     read every APRS setting (print, or save to a file)
  aprs_pc.py <port> setup set file.json       write every APRS setting from a file, all or nothing
  aprs_pc.py <port> monitor                   print decoded packets and digi decisions (Ctrl-C to stop)
  aprs_pc.py loc LAT LON                      the 15-digit Loc code to type into the radio's Loc menu

The framing is the radio's own (obfuscated, CRC16, as K5TOOL / UV Studio): this tool opens a session with
the 0x0514 handshake, whose timestamp the commands that change anything must repeat. On macOS the USB-C
port is /dev/cu.usbmodem*; the K-plug cable is /dev/cu.usbserial*.

A site file for `setup set` looks like:
  {"call": "W1ABC", "ssid": 7, "aprs_on": true, "interval_min": 10, "digi": "WIDE", "hops": 2,
   "delay": "250ms", "beacon": "DIGI", "comment": "Ridge digi", "msgto": "", "loc": "",
   "tone_level": 66, "tone_twist": 0}
(`loc` is the 15-digit code, or give "lat" and "lon" in degrees instead.)
"""
import json
import os
import struct
import sys
from dataclasses import dataclass

XOR = bytes([0x16, 0x6C, 0x14, 0xE6, 0x2E, 0x91, 0x0D, 0x40,
             0x21, 0x35, 0xD5, 0x40, 0x13, 0x03, 0xE9, 0x80])

# ---------------------------------------------------------------- framing ----


def crc16(data):
    """CRC-16 XMODEM (the radio's command CRC)."""
    crc = 0
    for b in data:
        crc ^= b << 8
        for _ in range(8):
            crc = ((crc << 1) ^ 0x1021) & 0xFFFF if crc & 0x8000 else (crc << 1) & 0xFFFF
    return crc


def obf(data):
    return bytes(c ^ XOR[i % 16] for i, c in enumerate(data))


def frame(payload):
    body = obf(payload + struct.pack("<H", crc16(payload)))
    return b"\xAB\xCD" + struct.pack("<H", len(payload)) + body + b"\xDC\xBA"


def send_cmd(ser, cmd_id, data=b""):
    ser.write(frame(struct.pack("<HH", cmd_id, len(data)) + data))
    ser.flush()


def read_reply(ser, timeout=3.0):
    """One reply, header included (id, size, body), or None. Skips the plain text monitor lines."""
    ser.timeout = timeout
    for _ in range(2000):
        b = ser.read(1)
        if not b:
            return None
        if b != b"\xAB":
            continue
        if ser.read(1) != b"\xCD":
            continue
        hdr = ser.read(2)
        if len(hdr) != 2:
            return None
        size = struct.unpack("<H", hdr)[0]
        rest = ser.read(size + 4)
        if len(rest) < size + 4:
            return None
        return obf(rest[: size + 2])[:size]
    return None


def open_session(ser):
    """The 0x0514 handshake; returns (timestamp, version string) or (None, None)."""
    ts = int.from_bytes(os.urandom(4), "little") or 1
    send_cmd(ser, 0x0514, struct.pack("<I", ts))
    r = read_reply(ser)
    if r is None or len(r) < 20 or struct.unpack_from("<H", r)[0] != 0x0515:
        return None, None
    return ts, r[4:20].split(b"\x00")[0].decode(errors="replace")


def ack_ok(r, reply_id):
    return r is not None and len(r) >= 5 and struct.unpack_from("<H", r)[0] == reply_id and r[4] == 1


# ------------------------------------------------------- settings record ----
RECORD_SIZE = 96
DIGI_MODES = ("OFF", "FILL", "WIDE")
DIGI_DELAYS = ("OFF", "250ms", "500ms", "1s")
DIGI_BCNTY = ("MOBILE", "DIGI")
DIGI_STATS = ("heard", "repeated", "dup", "cancelled", "toomany", "dropped")
INTERVAL_MIN_S = 60
TWIST = (-4, 8)
LEVEL = (10, 127)


def crc16_ccitt_false(data):
    """CRC of the settings record: init 0xFFFF, poly 0x1021 (app/aprs_settings.c)."""
    crc = 0xFFFF
    for b in data:
        crc ^= b << 8
        for _ in range(8):
            crc = ((crc << 1) ^ 0x1021) & 0xFFFF if crc & 0x8000 else (crc << 1) & 0xFFFF
    return crc


@dataclass(frozen=True)
class Settings:
    call: str = "N0CALL"
    ssid: int = 0
    aprs_on: bool = False
    interval_s: int = 600
    digi_mode: int = 0
    digi_hops: int = 2
    digi_delay: int = 0
    beacon_type: int = 0
    tone_level: int = 66
    tone_twist: int = 0
    msgto: str = ""
    loc: str = ""
    comment: str = ""


def _text_ok(s, cap, allowed):
    return len(s) <= cap and all(c in allowed for c in s)


UPPER_DIGITS = "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789"
PRINTABLE = "".join(chr(c) for c in range(0x20, 0x7F))


def validate(s: Settings) -> None:
    """Raises ValueError with the reason, mirroring APRS_SettingsValid in the firmware."""
    if not (1 <= len(s.call) <= 6 and _text_ok(s.call, 6, UPPER_DIGITS)):
        raise ValueError("call: 1-6 characters A-Z 0-9")
    if not _text_ok(s.msgto, 9, UPPER_DIGITS + "-"):
        raise ValueError("msgto: up to 9 characters A-Z 0-9 -")
    if not _text_ok(s.loc, 15, "0123456789"):
        raise ValueError("loc: up to 15 digits")
    if s.loc and (len(s.loc) != 15 or decode_loc(s.loc) is None):
        raise ValueError("loc: not a valid 15-digit code (checksum)")
    if not _text_ok(s.comment, 43, PRINTABLE):
        raise ValueError("comment: up to 43 printable characters")
    if not 0 <= s.ssid <= 15:
        raise ValueError("ssid: 0-15")
    if not (s.interval_s == 0 or s.interval_s >= INTERVAL_MIN_S) or s.interval_s > 0xFFFF:
        raise ValueError("interval: 0 (off) or at least 60 s")
    if not 0 <= s.digi_mode <= 2:
        raise ValueError("digi: OFF | FILL | WIDE")
    if not 1 <= s.digi_hops <= 7:
        raise ValueError("hops: 1-7")
    if not 0 <= s.digi_delay <= 3:
        raise ValueError("delay: OFF | 250ms | 500ms | 1s")
    if s.beacon_type not in (0, 1):
        raise ValueError("beacon: MOBILE | DIGI")
    if not LEVEL[0] <= s.tone_level <= LEVEL[1]:
        raise ValueError(f"tone_level: {LEVEL[0]}-{LEVEL[1]}")
    if not TWIST[0] <= s.tone_twist <= TWIST[1]:
        raise ValueError(f"tone_twist: {TWIST[0]} to {TWIST[1]}")


def pack_record(s: Settings) -> bytes:
    """The 96-byte record of app/aprs_settings.c (layout and CRC pinned by test_aprs_pc.py)."""
    validate(s)
    digi = (s.digi_mode & 3) | ((s.digi_hops & 7) << 2) | ((s.digi_delay & 3) << 5) | ((s.beacon_type & 1) << 7)
    r = bytearray(RECORD_SIZE)
    r[0:4] = b"APR1"
    r[4] = 1
    r[5] = 1 if s.aprs_on else 0
    r[6] = digi
    r[7] = s.ssid
    struct.pack_into("<H", r, 8, s.interval_s)
    r[10] = s.tone_level
    r[11] = s.tone_twist & 0xFF
    r[12:12 + len(s.call)] = s.call.encode()
    r[20:20 + len(s.msgto)] = s.msgto.encode()
    r[30:30 + len(s.loc)] = s.loc.encode()
    r[46:46 + len(s.comment)] = s.comment.encode()
    struct.pack_into("<H", r, 90, crc16_ccitt_false(bytes(r[:90])))
    return bytes(r)


def unpack_record(r: bytes) -> Settings:
    """Raises ValueError for a record the firmware itself would refuse."""
    if len(r) != RECORD_SIZE or r[0:4] != b"APR1" or r[4] != 1 or any(r[92:]):
        raise ValueError("not an APRS settings record")
    if struct.unpack_from("<H", r, 90)[0] != crc16_ccitt_false(bytes(r[:90])):
        raise ValueError("settings record checksum mismatch")

    def text(a, b):
        return r[a:b].split(b"\x00")[0].decode(errors="replace")

    d = r[6]
    s = Settings(call=text(12, 20), ssid=r[7], aprs_on=bool(r[5]), interval_s=struct.unpack_from("<H", r, 8)[0],
                 digi_mode=d & 3, digi_hops=(d >> 2) & 7, digi_delay=(d >> 5) & 3, beacon_type=d >> 7,
                 tone_level=r[10], tone_twist=struct.unpack_from("<b", r, 11)[0], msgto=text(20, 30),
                 loc=text(30, 46), comment=text(46, 90))
    validate(s)
    return s


def settings_to_json(s: Settings) -> dict:
    return {"call": s.call, "ssid": s.ssid, "aprs_on": s.aprs_on, "interval_min": s.interval_s / 60,
            "digi": DIGI_MODES[s.digi_mode], "hops": s.digi_hops, "delay": DIGI_DELAYS[s.digi_delay],
            "beacon": DIGI_BCNTY[s.beacon_type], "comment": s.comment, "msgto": s.msgto, "loc": s.loc,
            "tone_level": s.tone_level, "tone_twist": s.tone_twist}


def settings_from_json(d: dict, base: Settings = Settings()) -> Settings:
    """Missing keys keep the value in base; ValueError for unknown keys or bad values."""
    known = {"call", "ssid", "aprs_on", "interval_min", "digi", "hops", "delay", "beacon", "comment", "msgto",
             "loc", "lat", "lon", "tone_level", "tone_twist"}
    extra = set(d) - known
    if extra:
        raise ValueError("unknown setting(s): " + ", ".join(sorted(extra)))

    def pick(table, key, name, cur):
        if key not in d:
            return cur
        names = [x.lower() for x in table]
        if str(d[key]).lower() not in names:
            raise ValueError(f"{name}: " + " | ".join(table))
        return names.index(str(d[key]).lower())

    loc = d.get("loc", base.loc)
    if "lat" in d or "lon" in d:
        if "lat" not in d or "lon" not in d:
            raise ValueError("give both lat and lon")
        loc = encode_loc(float(d["lat"]), float(d["lon"]))
    s = Settings(
        call=str(d.get("call", base.call)).upper(), ssid=int(d.get("ssid", base.ssid)),
        aprs_on=bool(d.get("aprs_on", base.aprs_on)),
        interval_s=int(round(float(d["interval_min"]) * 60)) if "interval_min" in d else base.interval_s,
        digi_mode=pick(DIGI_MODES, "digi", "digi", base.digi_mode), digi_hops=int(d.get("hops", base.digi_hops)),
        digi_delay=pick(DIGI_DELAYS, "delay", "delay", base.digi_delay),
        beacon_type=pick(DIGI_BCNTY, "beacon", "beacon", base.beacon_type),
        tone_level=int(d.get("tone_level", base.tone_level)), tone_twist=int(d.get("tone_twist", base.tone_twist)),
        msgto=str(d.get("msgto", base.msgto)).upper(), loc=str(loc), comment=str(d.get("comment", base.comment)))
    validate(s)
    return s


# ------------------------------------------------------------ Loc code ----


def _loc_check(digits14: str) -> int:
    return sum((i + 1) * int(c) for i, c in enumerate(digits14)) % 10


def encode_loc(lat: float, lon: float) -> str:
    """The 15-digit code for the radio's Loc item: (lat+90)*1e4 [7], (lon+180)*1e4 [7], checksum [1]."""
    if not (-90 <= lat <= 90 and -180 <= lon <= 180) or (lat == -90 and lon == -180):
        raise ValueError("lat/lon out of range")
    d = f"{round((lat + 90) * 1e4):07d}{round((lon + 180) * 1e4):07d}"
    return d + str(_loc_check(d))


def decode_loc(code: str):
    """(lat, lon) in degrees, or None if the code is malformed or its checksum is wrong."""
    if len(code) != 15 or not code.isdigit() or _loc_check(code[:14]) != int(code[14]):
        return None
    la, lo = int(code[:7]), int(code[7:14])
    if la > 1800000 or lo > 3600000 or (la == 0 and lo == 0):
        return None
    return la / 1e4 - 90, lo / 1e4 - 180


# ---------------------------------------------------------------- digi ----
DIGI_REPLY_SIZE = 28  # the 0x070B reply, header included (app/aprs_cmd.c)


@dataclass(frozen=True)
class DigiStatus:
    ok: bool
    mode: str
    hops: int
    delay: str
    bcnty: str
    uptime_s: float
    stats: dict


def pack_digi_set(mode: str, hops: int, delay: str, bcnty: str, ts: int) -> bytes:
    """0x070A body that sets and saves the digi settings; ValueError if invalid."""
    try:
        m = DIGI_MODES.index(mode.upper())
        d = [x.lower() for x in DIGI_DELAYS].index(delay.lower())
        b = DIGI_BCNTY.index(bcnty.upper())
    except ValueError as e:
        raise ValueError(f"bad digi setting: {e}") from None
    if not 1 <= hops <= 7:
        raise ValueError(f"hops must be 1-7, got {hops}")
    return bytes([1, m, hops, d, b]) + struct.pack("<I", ts)


def parse_digi_reply(r: bytes) -> "DigiStatus | None":
    if r is None or len(r) < DIGI_REPLY_SIZE or struct.unpack_from("<H", r)[0] != 0x070B:
        return None
    ok, mode, hops, delay, bcn = r[4:9]
    uptime = struct.unpack_from("<I", r, 12)[0]
    stats = struct.unpack_from("<6H", r, 16)

    def name(table, i):
        return table[i] if i < len(table) else f"?{i}"

    return DigiStatus(ok=bool(ok), mode=name(DIGI_MODES, mode), hops=hops, delay=name(DIGI_DELAYS, delay),
                      bcnty=name(DIGI_BCNTY, bcn), uptime_s=uptime / 100.0, stats=dict(zip(DIGI_STATS, stats)))


def format_digi(st: DigiStatus) -> str:
    h, rem = divmod(int(st.uptime_s), 3600)
    stats = "  ".join(f"{k} {v}" for k, v in st.stats.items())
    return (f"digi {st.mode}  hops {st.hops}  delay {st.delay}  beacon {st.bcnty}\n"
            f"uptime {h}h{rem // 60:02d}m  {stats}")


# ------------------------------------------------------------- commands ----


def get_settings(ser) -> Settings:
    send_cmd(ser, 0x070C)
    r = read_reply(ser)
    if r is None or len(r) < 8 + RECORD_SIZE or struct.unpack_from("<H", r)[0] != 0x070D or r[4] != 1:
        raise RuntimeError("no settings reply (is this the APRS firmware?)")
    return unpack_record(bytes(r[8:8 + RECORD_SIZE]))


def cmd_msg(ser, ts, dest, text):
    dest_b = dest.upper().encode()[:9].ljust(10, b"\x00")
    text_b = text.encode()[:30].ljust(30, b"\x00")
    send_cmd(ser, 0x0700, struct.pack("<I", ts) + dest_b + text_b)
    if ack_ok(read_reply(ser), 0x0701):
        print(f"queued: {dest.upper()} <- {text[:30]}")
        return 0
    print("not queued (callsign set? APRS on? destination and text given?)")
    return 1


def cmd_simple(ser, ts, cmd_id, label, extra=b""):
    send_cmd(ser, cmd_id, struct.pack("<I", ts) + extra)
    if ack_ok(read_reply(ser), cmd_id + 1):
        print(label)
        return 0
    print("the radio refused it")
    return 1


def cmd_raw(ser, ts, hexstr):
    try:
        frame_bytes = bytes.fromhex(hexstr)
    except ValueError:
        print("raw: not hexadecimal")
        return 1
    send_cmd(ser, 0x0708, struct.pack("<I", ts) + frame_bytes)
    if ack_ok(read_reply(ser), 0x0709):
        print(f"queued {len(frame_bytes)} bytes")
        return 0
    print("refused (source must be our callsign; 17-150 bytes, UI frame; one frame at a time)")
    return 1


def cmd_digi(ser, ts, args):
    if not args:
        body = bytes([0, 0, 0, 0, 0])
    elif args[0] == "set" and len(args) == 5:
        try:
            body = pack_digi_set(args[1], int(args[2]), args[3], args[4], ts)
        except ValueError as e:
            print(e)
            return 1
    else:
        print(__doc__)
        return 1
    send_cmd(ser, 0x070A, body)
    st = parse_digi_reply(read_reply(ser))
    if st is None:
        print("no reply")
        return 1
    if not st.ok:
        print("the radio refused the settings; unchanged:")
    print(format_digi(st))
    return 0 if st.ok else 1


def cmd_setup(ser, ts, args):
    if args and args[0] == "get":
        s = get_settings(ser)
        text = json.dumps(settings_to_json(s), indent=2)
        if len(args) > 1:
            with open(args[1], "w") as f:
                f.write(text + "\n")
            print(f"saved to {args[1]}")
        else:
            print(text)
        return 0
    if args and args[0] == "set" and len(args) == 2:
        with open(args[1]) as f:
            wanted = json.load(f)
        try:
            s = settings_from_json(wanted, get_settings(ser))
        except ValueError as e:
            print(f"{args[1]}: {e}")
            return 1
        send_cmd(ser, 0x070E, struct.pack("<I", ts) + pack_record(s))
        if not ack_ok(read_reply(ser), 0x070F):
            print("the radio refused the settings; nothing was changed")
            return 1
        back = get_settings(ser)
        print("settings written" + ("" if back == s else " (read-back differs: check them with `setup get`)"))
        return 0 if back == s else 1
    print(__doc__)
    return 1


def format_status(version, s: Settings, st: "DigiStatus | None") -> str:
    lines = [f"firmware {version}",
             f"{s.call}-{s.ssid}  APRS {'ON' if s.aprs_on else 'OFF'}  beacon every "
             f"{'off' if not s.interval_s else f'{s.interval_s / 60:g} min'}  {DIGI_BCNTY[s.beacon_type]}",
             f"loc {s.loc or '-'}  comment {s.comment or '-'}",
             f"tones level {s.tone_level} twist {s.tone_twist}"]
    if st:
        lines.append(format_digi(st))
    return "\n".join(lines)


def cmd_status(ser, version):
    s = get_settings(ser)
    send_cmd(ser, 0x070A, bytes([0, 0, 0, 0, 0]))
    print(format_status(version, s, parse_digi_reply(read_reply(ser))))
    return 0


def monitor_line(line: bytes):
    """What to print for one monitor line, or None."""
    if line.startswith(b"APRS:"):
        return line[5:].decode(errors="replace")
    if line.startswith(b"APRSRAW:"):
        return "    raw " + line[8:].decode(errors="replace")
    if line.startswith(b"DIGI:"):
        return "    digi " + line[5:].decode(errors="replace")
    return None


def cmd_monitor(ser, raw=False):
    send_cmd(ser, 0x0710, b"\x01")
    print("monitoring (Ctrl-C to stop)...")
    ser.timeout = 1.0
    buf = b""
    try:
        while True:
            chunk = ser.read(256)
            if not chunk:
                continue
            buf += chunk
            while b"\n" in buf:
                line, buf = buf.split(b"\n", 1)
                out = monitor_line(line.strip())
                if out is not None and (raw or not out.startswith("    raw")):
                    print(out)
    except KeyboardInterrupt:
        send_cmd(ser, 0x0710, b"\x00")
        print()
    return 0


def main(argv=None):
    argv = sys.argv[1:] if argv is None else argv
    if len(argv) == 3 and argv[0] == "loc":
        print(encode_loc(float(argv[1]), float(argv[2])))
        return 0
    if len(argv) < 2:
        print(__doc__)
        return 1
    import serial  # here, so the protocol helpers import without pyserial (tests)

    port, cmd, rest = argv[0], argv[1], argv[2:]
    ser = serial.Serial(port, 38400, timeout=1.0)
    ts, version = open_session(ser)
    if ts is None:
        print("no answer from the radio (right port? radio on, not in a menu that blocks?)")
        return 1
    if cmd == "status":
        return cmd_status(ser, version)
    if cmd == "msg" and len(rest) >= 2:
        return cmd_msg(ser, ts, rest[0], " ".join(rest[1:]))
    if cmd == "beacon":
        return cmd_simple(ser, ts, 0x0702, "beacon queued")
    if cmd in ("on", "off"):
        return cmd_simple(ser, ts, 0x0706, f"APRS {cmd.upper()}", bytes([cmd == "on"]))
    if cmd == "raw" and len(rest) == 1:
        return cmd_raw(ser, ts, rest[0])
    if cmd == "digi":
        return cmd_digi(ser, ts, rest)
    if cmd == "setup":
        return cmd_setup(ser, ts, rest)
    if cmd == "monitor":
        return cmd_monitor(ser, raw="--raw" in rest)
    print(__doc__)
    return 1


if __name__ == "__main__":
    sys.exit(main())
