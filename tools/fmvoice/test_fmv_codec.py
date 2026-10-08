"""Python codec vs the C codec (App/app/fmv_info.c): the same checks, and a table written by Python is read by C.
Run via `make -C tools/fmvoice test` (needs ./fmv_vectors built first)."""
import os
import random
import subprocess

import pytest

import fmv_codec as rc

HERE = os.path.dirname(os.path.abspath(__file__))
C = os.path.join(HERE, "fmv_vectors")


def c_run(*args, stdin=""):
    return subprocess.run([C, *args], input=stdin, capture_output=True, text=True, check=True).stdout


def test_freq_check_matches_c():
    for line in c_run("checks").splitlines():
        f, chk = (int(x) for x in line.split())
        assert rc.fmv_freq_check(f) == chk, f
    assert all(rc.fmv_freq_check(f) != 0xFFFF for f in range(0, 2_000_000, 7))


def test_roundtrip_and_cleaning():
    rec, cut = rc.fmv_encode(14682000, "Mechanicsburg, Three Square Hollow")
    assert not cut and len(rec) == 48
    assert rc.fmv_decode(rec, 14682000) == "Mechanicsburg, Three Square Hollow"
    assert rc.fmv_decode(rec, 14682500) is None                    # the channel's frequency changed
    assert rc.fmv_clean("  Big  Flat \x01 Mté  ") == ("Big Flat ? Mt?", False)
    assert rc.fmv_clean("Biglerville,  Big Flat So Mt")[0] == "Biglerville, Big Flat So Mt"
    text, cut = rc.fmv_clean("x" * 60)
    assert len(text) == 45 and cut
    assert rc.fmv_clean("y" * 45) == ("y" * 45, False)
    assert rc.fmv_encode(14682000, "") == (b"\xff" * 48, False)
    assert rc.fmv_encode(14682000, "   ") == (b"\xff" * 48, False)
    assert rc.fmv_encode(14682000, None) == (b"\xff" * 48, False)


def test_decode_rejects_bad_records():
    rec, _ = rc.fmv_encode(14682000, "Poughkeepsie")
    assert rc.fmv_decode(b"\xff" * 48, 14682000) is None
    assert rc.fmv_decode(b"\x00" * 48, 14682000) is None
    assert rc.fmv_decode(rec[:-1], 14682000) is None
    assert rc.fmv_decode(rec[:2] + b"z" * 46, 14682000) is None    # no NUL inside the record
    assert rc.fmv_decode(rec[:2] + b"\x07" + rec[3:], 14682000) is None


def test_table_ok_guards_other_firmware_data():
    empty = b"\xff" * (256 * 48)
    assert rc.fmv_table_ok(empty)
    rec, _ = rc.fmv_encode(14682000, "Enola")
    assert rc.fmv_table_ok(rec + empty[48:])
    assert not rc.fmv_table_ok(b"APR1" + empty[4:])                 # the APRS build's settings record
    assert not rc.fmv_table_ok(b"\x01\x02\x03\x04" * (256 * 12))    # anything else
    assert not rc.fmv_table_ok(empty[:-1])


@pytest.mark.skipif(not os.path.exists(C), reason="build fmv_vectors first (make -C tools/fmvoice)")
def test_c_decodes_what_python_wrote():
    rnd = random.Random(7)
    table = bytearray(b"\xff" * (256 * 48))
    want = []
    words = ["Biglerville", "Gettysburg", "Mt Holly Springs", "Blue Mountain at Lamb's Gap", "Three Square Hollow", "x" * 70]
    for ch in range(256):
        freq = 14400000 + rnd.randrange(0, 40000) * 10
        if ch % 3 == 0:
            continue                                                 # leave some slots erased
        text = ", ".join(rnd.choice(words) for _ in range(rnd.randrange(1, 3)))
        rec, _ = rc.fmv_encode(freq, text)
        table[ch * 48:(ch + 1) * 48] = rec
        want.append((ch, freq, rc.fmv_decode(rec, freq)))
    path = os.path.join(HERE, "table_test.bin")
    with open(path, "wb") as f:
        f.write(table)
    try:
        queries = "\n".join(f"{ch} {freq}" for ch, freq, _ in want) + "\n300 14400000\n"
        out = c_run("decode", path, stdin=queries).splitlines()
    finally:
        os.remove(path)
    for (ch, freq, text), line in zip(want, out):
        assert line == f"{ch}|{text}", (ch, line, text)
    assert out[-1] == "300|"                                         # no slot above 255


def test_check_csv_reports_the_known_problems(tmp_path):
    import check_csv
    rows = [
        {"Location": "1", "Name": "W3AAA", "Frequency": "146.82", "Duplex": "-", "Offset": "0.6", "Tone": "Tone", "Comment": "Enola"},
        {"Location": "2", "Name": "N3BBB", "Frequency": "146.46", "Duplex": "split", "Offset": "", "Tone": "Tone", "Comment": ""},
        {"Location": "3", "Name": "K3CCC", "Frequency": "147.06", "Duplex": "+", "Offset": "0.6", "Tone": "", "Comment": ""},
        {"Location": "4", "Name": "K3DDD", "Frequency": "146.94", "Duplex": "-", "Offset": "0.6", "Tone": "Tone", "Comment": "x" * 60},
        {"Location": "300", "Name": "K3EEE", "Frequency": "146.70", "Duplex": "-", "Offset": "0.6", "Tone": "Tone", "Comment": "Town"},
        {"Location": "5", "Name": "W3AAA", "Frequency": "146.82", "Duplex": "-", "Offset": "0.6", "Tone": "Tone", "Comment": ""},
        {"Location": "6", "Name": "K3FFF", "Frequency": "162.55", "Duplex": "", "Offset": "", "Tone": "", "Comment": ""},
    ]
    text = "\n".join(f"{loc}: {msg}" for loc, msg in check_csv.check(rows))
    assert "2: N3BBB" in text and "lost" in text
    assert "3: K3CCC" in text and "no tone" in text and "6: K3FFF" in text
    assert "will be cut" in text
    assert "300: K3EEE" in text and "above 256" in text
    assert "5: W3AAA" in text and "twice" in text
    assert "6: K3FFF" in text and "outside" in text
    assert "1: W3AAA" not in text
