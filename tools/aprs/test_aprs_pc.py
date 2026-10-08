"""Host tests for aprs_pc.py (no radio, no pyserial):  python3 -m pytest tools/aprs/test_aprs_pc.py"""
import json
import struct

import pytest

import aprs_pc

# The record test_aprs_cmd.c prints for the same settings: the Python and C layouts are pinned to these bytes.
SAMPLE = aprs_pc.Settings(call="W1ABC", ssid=7, aprs_on=True, interval_s=600, digi_mode=2, digi_hops=2,
                          digi_delay=0, beacon_type=1, tone_level=66, tone_twist=-2, msgto="N0CALL-7",
                          loc="130712810599405", comment="Ridge digi")
SAMPLE_HEX = ("4150523101018a07580242fe57314142430000004e3043414c4c2d370000313330373132383130353939343035"
              "005269646765206469676900000000000000000000000000000000000000000000000000000000000000000000a2b100000000")


class FakeRadio:
    """Just enough serial port: records what is written, answers like the firmware."""

    def __init__(self, replies=()):
        self.written = []
        self.rx = bytearray()
        self.timeout = 1.0
        for r in replies:
            self.rx += aprs_pc.frame(r)

    def write(self, b):
        self.written.append(bytes(b))

    def flush(self):
        pass

    def read(self, n=1):
        out = bytes(self.rx[:n])
        del self.rx[:n]
        return out

    def commands(self):
        """The commands written, decoded: (id, data)."""
        out = []
        for w in self.written:
            body = aprs_pc.obf(w[4:-2])
            payload = body[:-2]
            assert struct.unpack("<H", body[-2:])[0] == aprs_pc.crc16(payload)
            cid, size = struct.unpack_from("<HH", payload)
            out.append((cid, payload[4:4 + size]))
        return out


def reply(cid, ok=1, extra=b""):
    return struct.pack("<HHB3x", cid, 4 + len(extra), ok) + extra


# ----------------------------------------------------------------- framing ----
def test_frame_round_trip():
    s = FakeRadio([struct.pack("<HH", 0x0701, 4) + b"\x01\x00\x00\x00"])
    assert aprs_pc.read_reply(s) == struct.pack("<HH", 0x0701, 4) + b"\x01\x00\x00\x00"


def test_read_reply_skips_text_lines_and_noise():
    s = FakeRadio()
    s.rx += b"APRS:W1ABC>hello\r\nDIGI:RPT W1ABC\r\n\xAB\x00junk"
    s.rx += aprs_pc.frame(reply(0x0703))
    assert aprs_pc.read_reply(s)[:2] == struct.pack("<H", 0x0703)


def test_read_reply_gives_up_on_silence_and_truncation():
    assert aprs_pc.read_reply(FakeRadio()) is None
    s = FakeRadio([reply(0x0703)])
    del s.rx[-3:]
    assert aprs_pc.read_reply(s) is None


# ------------------------------------------------------------- the record ----
def test_pack_matches_the_firmware_bytes():
    assert aprs_pc.pack_record(SAMPLE).hex() == SAMPLE_HEX


def test_unpack_round_trip():
    assert aprs_pc.unpack_record(bytes.fromhex(SAMPLE_HEX)) == SAMPLE
    assert aprs_pc.unpack_record(aprs_pc.pack_record(aprs_pc.Settings())) == aprs_pc.Settings()


@pytest.mark.parametrize("mutate", [
    lambda r: r[:-1],                              # short
    lambda r: b"XPR1" + r[4:],                     # magic
    lambda r: r[:4] + b"\x02" + r[5:],             # version
    lambda r: r[:12] + b"X" + r[13:],              # crc
    lambda r: r[:95] + b"\x01",                    # spare bytes must be zero
])
def test_unpack_rejects_what_the_firmware_would(mutate):
    with pytest.raises(ValueError):
        aprs_pc.unpack_record(mutate(bytes.fromhex(SAMPLE_HEX)))


@pytest.mark.parametrize("change", [
    dict(call=""), dict(call="w1abc"), dict(call="TOOLONG"), dict(ssid=16), dict(interval_s=30),
    dict(digi_mode=3), dict(digi_hops=0), dict(digi_hops=8), dict(digi_delay=4), dict(beacon_type=2),
    dict(tone_level=9), dict(tone_level=128), dict(tone_twist=9), dict(tone_twist=-5),
    dict(loc="12AB"), dict(loc="130712810599406"), dict(msgto="bad call"), dict(comment="x" * 44),
    dict(comment="tab\there"),
])
def test_validate_rejects(change):
    with pytest.raises(ValueError):
        aprs_pc.validate(aprs_pc.Settings(call="W1ABC", **change) if "call" not in change else aprs_pc.Settings(**change))


def test_validate_accepts_the_edges():
    for s in (aprs_pc.Settings(call="W1ABC", interval_s=0), aprs_pc.Settings(call="W1ABC", interval_s=60),
              aprs_pc.Settings(call="W1ABC", tone_level=127, tone_twist=8, digi_hops=7, ssid=15),
              aprs_pc.Settings(call="W1ABC", tone_level=10, tone_twist=-4, comment="x" * 43, msgto="N0CALL-15")):
        aprs_pc.validate(s)


# ------------------------------------------------------------------- json ----
def test_json_round_trip():
    assert aprs_pc.settings_from_json(aprs_pc.settings_to_json(SAMPLE)) == SAMPLE


def test_json_partial_keeps_the_base():
    s = aprs_pc.settings_from_json({"digi": "fill", "delay": "1S", "hops": 3}, SAMPLE)
    assert (s.digi_mode, s.digi_delay, s.digi_hops, s.call, s.comment) == (1, 3, 3, "W1ABC", "Ridge digi")


def test_json_lat_lon_makes_the_loc_code():
    s = aprs_pc.settings_from_json({"call": "w1abc", "lat": 40.7128, "lon": -74.006})
    assert s.loc == "130712810599405" and s.call == "W1ABC"


@pytest.mark.parametrize("bad", [{"colour": 1}, {"digi": "WIDER"}, {"delay": "2s"}, {"beacon": "CAR"},
                                 {"lat": 40.0}, {"hops": 9}, {"interval_min": 0.5}, {"call": "TOOLONGCALL"}])
def test_json_rejects(bad):
    with pytest.raises(ValueError):
        aprs_pc.settings_from_json(bad, SAMPLE)


def test_json_interval_in_minutes():
    assert aprs_pc.settings_from_json({"interval_min": 10}, SAMPLE).interval_s == 600
    assert aprs_pc.settings_from_json({"interval_min": 0}, SAMPLE).interval_s == 0


# --------------------------------------------------------------------- loc ----
def test_loc_known_code_and_inverse():
    assert aprs_pc.encode_loc(40.7128, -74.006) == "130712810599405"
    lat, lon = aprs_pc.decode_loc("130712810599405")
    assert abs(lat - 40.7128) < 1e-4 and abs(lon + 74.006) < 1e-4


@pytest.mark.parametrize("code", ["130712810599406", "13071281059940", "13071281059940A", "000000000000000"])
def test_loc_decode_rejects(code):
    assert aprs_pc.decode_loc(code) is None


@pytest.mark.parametrize("lat, lon", [(91, 0), (0, 181), (-90, -180)])
def test_loc_encode_rejects(lat, lon):
    with pytest.raises(ValueError):
        aprs_pc.encode_loc(lat, lon)


# -------------------------------------------------------------------- digi ----
def reply_070b(ok=1, mode=2, hops=2, delay=2, bcn=1, uptime=0, stats=(0,) * 6) -> bytes:
    return struct.pack("<HHBBBBB3xI6H", 0x070B, 24, ok, mode, hops, delay, bcn, uptime, *stats)


def test_digi_reply_size_and_fields():
    assert len(reply_070b()) == aprs_pc.DIGI_REPLY_SIZE == 28
    st = aprs_pc.parse_digi_reply(reply_070b(mode=2, hops=3, delay=1, bcn=1, uptime=366_000, stats=(10, 4, 3, 2, 1, 5)))
    assert st == aprs_pc.DigiStatus(ok=True, mode="WIDE", hops=3, delay="250ms", bcnty="DIGI", uptime_s=3660.0,
                                    stats={"heard": 10, "repeated": 4, "dup": 3, "cancelled": 2, "toomany": 1, "dropped": 5})
    assert aprs_pc.parse_digi_reply(reply_070b(ok=0)).ok is False
    assert aprs_pc.parse_digi_reply(None) is None
    assert aprs_pc.parse_digi_reply(reply_070b()[:-1]) is None
    st = aprs_pc.parse_digi_reply(reply_070b(mode=9, delay=7, bcn=4))
    assert (st.mode, st.delay, st.bcnty) == ("?9", "?7", "?4")


@pytest.mark.parametrize("args, body", [
    (("WIDE", 2, "500ms", "DIGI"), bytes([1, 2, 2, 2, 1])),
    (("fill", 1, "off", "mobile"), bytes([1, 1, 1, 0, 0])),
    (("OFF", 7, "1S", "MOBILE"), bytes([1, 0, 7, 3, 0])),
])
def test_pack_digi_set_carries_the_session(args, body):
    assert aprs_pc.pack_digi_set(*args, ts=0xAABBCCDD) == body + struct.pack("<I", 0xAABBCCDD)


@pytest.mark.parametrize("args", [("WIDER", 2, "OFF", "DIGI"), ("WIDE", 0, "OFF", "DIGI"), ("WIDE", 8, "OFF", "DIGI"),
                                  ("WIDE", 2, "2s", "DIGI"), ("WIDE", 2, "OFF", "CAR")])
def test_pack_digi_set_rejects(args):
    with pytest.raises(ValueError):
        aprs_pc.pack_digi_set(*args, ts=1)


# ---------------------------------------------------------------- session ----
def version_reply(text=b"EGZUMER+F4HWN v6.1.0"):
    return struct.pack("<HH", 0x0515, 28) + text.ljust(16, b"\x00")[:16] + b"\x00" * 12


def test_open_session_sends_a_nonzero_timestamp():
    radio = FakeRadio([version_reply()])
    ts, version = aprs_pc.open_session(radio)
    cid, data = radio.commands()[0]
    assert cid == 0x0514 and struct.unpack("<I", data)[0] == ts != 0
    assert version.startswith("EGZUMER")
    assert aprs_pc.open_session(FakeRadio()) == (None, None)


def test_msg_beacon_on_carry_the_timestamp():
    radio = FakeRadio([reply(0x0701), reply(0x0703), reply(0x0707), reply(0x0701, ok=0)])
    assert aprs_pc.cmd_msg(radio, 0x11223344, "n0call-7", "hello") == 0
    assert aprs_pc.cmd_simple(radio, 0x11223344, 0x0702, "beacon queued") == 0
    assert aprs_pc.cmd_simple(radio, 0x11223344, 0x0706, "APRS ON", bytes([1])) == 0
    assert aprs_pc.cmd_msg(radio, 0x11223344, "N0CALL", "x") == 1          # refused
    cmds = radio.commands()
    ts = struct.pack("<I", 0x11223344)
    assert cmds[0] == (0x0700, ts + b"N0CALL-7\x00\x00" + b"hello".ljust(30, b"\x00"))
    assert cmds[1] == (0x0702, ts)
    assert cmds[2] == (0x0706, ts + b"\x01")


def test_msg_text_is_cut_to_30_and_the_target_to_9():
    radio = FakeRadio([reply(0x0701)])
    aprs_pc.cmd_msg(radio, 1, "ABCDEFGHIJKL", "z" * 50)
    _, data = radio.commands()[0]
    assert data[4:14] == b"ABCDEFGHI\x00" and data[14:] == b"z" * 30


def test_raw_rejects_bad_hex_and_reports_refusal():
    assert aprs_pc.cmd_raw(FakeRadio(), 1, "xyz") == 1
    radio = FakeRadio([reply(0x0709, ok=0)])
    assert aprs_pc.cmd_raw(radio, 1, "aabb") == 1
    assert radio.commands() == [(0x0708, struct.pack("<I", 1) + b"\xaa\xbb")]


def test_setup_get_and_set(tmp_path):
    record = aprs_pc.pack_record(SAMPLE)
    get_reply = struct.pack("<HHB3x", 0x070D, 100, 1) + record
    radio = FakeRadio([get_reply])
    out = tmp_path / "site.json"
    assert aprs_pc.cmd_setup(radio, 5, ["get", str(out)]) == 0
    assert aprs_pc.settings_from_json(json.loads(out.read_text())) == SAMPLE

    # set: read the current settings, write the merged ones, read back
    site = tmp_path / "new.json"
    site.write_text(json.dumps({"call": "K1ABC", "digi": "FILL"}))
    merged = aprs_pc.settings_from_json({"call": "K1ABC", "digi": "FILL"}, SAMPLE)
    radio = FakeRadio([get_reply, reply(0x070F),
                       struct.pack("<HHB3x", 0x070D, 100, 1) + aprs_pc.pack_record(merged)])
    assert aprs_pc.cmd_setup(radio, 0xCAFE, ["set", str(site)]) == 0
    cmds = radio.commands()
    assert [c[0] for c in cmds] == [0x070C, 0x070E, 0x070C]
    assert cmds[1][1] == struct.pack("<I", 0xCAFE) + aprs_pc.pack_record(merged)


def test_setup_set_refuses_bad_files_before_touching_the_radio(tmp_path):
    record = aprs_pc.pack_record(SAMPLE)
    radio = FakeRadio([struct.pack("<HHB3x", 0x070D, 100, 1) + record])
    bad = tmp_path / "bad.json"
    bad.write_text(json.dumps({"digi": "WIDER"}))
    assert aprs_pc.cmd_setup(radio, 1, ["set", str(bad)]) == 1
    assert [c[0] for c in radio.commands()] == [0x070C]                    # nothing was written


def test_setup_set_reports_a_refusal(tmp_path):
    record = aprs_pc.pack_record(SAMPLE)
    radio = FakeRadio([struct.pack("<HHB3x", 0x070D, 100, 1) + record, reply(0x070F, ok=0)])
    site = tmp_path / "s.json"
    site.write_text("{}")
    assert aprs_pc.cmd_setup(radio, 1, ["set", str(site)]) == 1


def test_digi_get_and_set():
    radio = FakeRadio([reply_070b(), reply_070b(mode=1, hops=3)])
    assert aprs_pc.cmd_digi(radio, 9, []) == 0
    assert aprs_pc.cmd_digi(radio, 9, ["set", "FILL", "3", "OFF", "MOBILE"]) == 0
    cmds = radio.commands()
    assert cmds[0] == (0x070A, bytes([0, 0, 0, 0, 0]))
    assert cmds[1] == (0x070A, bytes([1, 1, 3, 0, 0]) + struct.pack("<I", 9))
    assert aprs_pc.cmd_digi(FakeRadio([reply_070b(ok=0)]), 9, ["set", "WIDE", "2", "OFF", "DIGI"]) == 1
    assert aprs_pc.cmd_digi(FakeRadio(), 9, ["set", "WIDER", "2", "OFF", "DIGI"]) == 1


def test_status_text():
    st = aprs_pc.parse_digi_reply(reply_070b(uptime=366_000, stats=(10, 4, 0, 0, 0, 0)))
    text = aprs_pc.format_status("EGZUMER+F4HWN v6.1.0", SAMPLE, st)
    assert "W1ABC-7  APRS ON  beacon every 10 min  DIGI" in text
    assert "digi WIDE  hops 2  delay 500ms  beacon DIGI" in text and "uptime 1h01m  heard 10  repeated 4" in text


def test_monitor_lines():
    assert aprs_pc.monitor_line(b"APRS:W1ABC>hello") == "W1ABC>hello"
    assert aprs_pc.monitor_line(b"DIGI:RPT W1ABC") == "    digi RPT W1ABC"
    assert aprs_pc.monitor_line(b"APRSRAW:AABB") == "    raw AABB"
    assert aprs_pc.monitor_line(b"garbage") is None
