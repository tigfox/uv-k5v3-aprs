"""End to end: RepeaterBook-style CSV -> CHIRP -> the patched driver -> a radio image -> the C decoder; and the
download / upload paths against a fake radio. Needs CHIRP's source and armel's v6.1.0 driver (not kept in this
repository; both are skipped-for if missing):

    git clone --depth 1 https://github.com/kk7ds/chirp /some/where/chirp-src
    CHIRP_SRC=/some/where/chirp-src RPT_UPSTREAM_DRIVER=~/Downloads/f4hwn.chirp.v6.1.0.py \
        python3 -m pytest tools/repeater/test_chirp_driver.py
"""
import importlib.util
import os
import subprocess
import sys

import pytest

import rpt_codec as rc

HERE = os.path.dirname(os.path.abspath(__file__))
CHIRP_SRC = os.path.expanduser(os.environ.get("CHIRP_SRC", ""))
UPSTREAM = os.path.expanduser(os.environ.get("RPT_UPSTREAM_DRIVER", "~/Downloads/f4hwn.chirp.v6.1.0.py"))
VECTORS = os.path.join(HERE, "rpt_vectors")

pytestmark = pytest.mark.skipif(not (CHIRP_SRC and os.path.isdir(CHIRP_SRC) and os.path.exists(UPSTREAM)),
                                reason="needs CHIRP_SRC and the upstream driver (see the module docstring)")


@pytest.fixture(scope="module")
def drv(tmp_path_factory):
    sys.path.insert(0, CHIRP_SRC)
    from unittest import mock
    for name in ("wx", "wx.lib", "wx.lib.newevent"):       # the driver's settings dialogs need the GUI toolkit; the tests do not
        sys.modules.setdefault(name, mock.MagicMock())
    out = tmp_path_factory.mktemp("drv") / "f4hwn_repeater_chirp.py"
    subprocess.run([sys.executable, os.path.join(HERE, "make_driver.py"), UPSTREAM, str(out)], check=True,
                   capture_output=True)
    spec = importlib.util.spec_from_file_location("f4hwn_repeater_chirp", out)
    mod = importlib.util.module_from_spec(spec)
    sys.modules["f4hwn_repeater_chirp"] = mod
    spec.loader.exec_module(mod)
    return mod


def new_radio(drv, image=None):
    from chirp import memmap
    r = drv.UVK5RadioF4HWNRepeater(None)
    r._mmap = memmap.MemoryMapBytes(image if image is not None else b"\xff" * 0x10000)
    r.process_mmap()
    return r


def csv_memories():
    from chirp.drivers import generic_csv
    csv_radio = generic_csv.CSVRadio(os.path.join(HERE, "sample_repeaterbook.csv"))
    return [csv_radio.get_memory(i) for i in range(1, 8)]


def load_csv(radio):
    for mem in csv_memories():
        assert not [m for m in radio.validate_memory(mem) if "not supported" not in str(m) and "will be cut" not in str(m)]
        radio.set_memory(mem)


def test_driver_is_a_separate_radio(drv):
    assert drv.UVK5RadioF4HWNRepeater.MODEL == "UV-K1 & UV-K5 V3 (F4HWN Repeater)"
    assert drv.UVK5RadioF4HWNRepeater.get_features(new_radio(drv)).has_comment is True


def test_csv_comments_reach_the_image_and_come_back(drv):
    r = new_radio(drv)
    load_csv(r)
    got = {i: r.get_memory(i).comment for i in range(1, 8)}
    assert got[1] == "Biglerville"
    assert got[2] == "Biglerville, Big Flat So Mt"                   # double space collapsed
    assert got[4] == "Mechanicsburg, Three Square Hollow"
    assert got[5] == "Enola, Blue Mountain at Lamb's Gap"
    assert got[6] == ""
    assert len(got[7]) == 45 and got[7].startswith("A very long town name, with a landmark")   # cut and reported
    assert r.get_memory(4).name == "N3DDD"                            # the callsign is the channel name, as before


def test_cut_text_is_warned_about(drv):
    r = new_radio(drv)
    mem = csv_memories()[6]
    assert any("cut" in str(m) for m in r.validate_memory(mem))
    mem = csv_memories()[3]
    assert not any("cut" in str(m) for m in r.validate_memory(mem))
    mem.number = 300
    assert any("1-256" in str(m) for m in r.validate_memory(mem))


@pytest.mark.skipif(not os.path.exists(VECTORS), reason="build rpt_vectors first (make -C tools/repeater)")
def test_the_c_decoder_reads_what_the_driver_wrote(drv, tmp_path):
    r = new_radio(drv)
    load_csv(r)
    image = r._mmap.get(0, 0x10000)
    table = tmp_path / "table.bin"
    table.write_bytes(image[0xD000:0x10000])
    queries = "".join(f"{i - 1} {int(r._memobj.channel[i - 1].freq)}\n" for i in range(1, 8))
    out = subprocess.run([VECTORS, "decode", str(table)], input=queries, capture_output=True, text=True, check=True)
    lines = dict(l.split("|", 1) for l in out.stdout.splitlines())
    assert lines["3"] == "Mechanicsburg, Three Square Hollow"
    assert lines["5"] == ""                                           # channel 6 had no text
    assert lines["0"] == "Biglerville"


def test_a_channel_changed_elsewhere_loses_its_text(drv):
    r = new_radio(drv)
    load_csv(r)
    r._memobj.channel[3].freq = r._memobj.channel[3].freq + 50       # retuned by something that knows nothing of the table
    assert r.get_memory(4).comment == ""
    mem = r.get_memory(4)
    r.set_memory(mem)                                                 # CHIRP saves it again: the stale record is cleared
    assert r._mmap.get(0xD000 + 3 * 48, 48) == b"\xff" * 48


def test_editing_the_frequency_in_chirp_keeps_the_text(drv):
    r = new_radio(drv)
    load_csv(r)
    mem = r.get_memory(4)
    mem.freq = mem.freq + 5000                                        # +5 kHz
    r.set_memory(mem)
    assert r.get_memory(4).comment == "Mechanicsburg, Three Square Hollow"


def test_clearing_a_memory_clears_its_text(drv):
    r = new_radio(drv)
    load_csv(r)
    mem = r.get_memory(4)
    mem.empty = True
    r.set_memory(mem)
    assert r._mmap.get(0xD000 + 3 * 48, 48) == b"\xff" * 48
    assert r.get_memory(4).empty and r.get_memory(4).comment == ""


class FakeRadio:
    """Replaces the serial protocol: an EEPROM in a bytearray behind the driver's _sayhello/_readmem/_writemem."""

    def __init__(self, drv, eeprom):
        self.eeprom = bytearray(eeprom)
        self.writes = []
        drv._sayhello = lambda s: "FAKE"
        drv._readmem = lambda s, off, n: bytes(self.eeprom[off:off + n])
        drv._writemem = self._write

    def _write(self, serport, data, off):
        self.eeprom[off:off + len(data)] = data
        self.writes.append((off, len(data)))
        return True


class Pipe:
    timeout = 1.0

    def write(self, data):          # the reset command at the end of an upload
        return len(data)

    def flush(self):
        pass


def attach(radio):
    radio.pipe = Pipe()
    radio.status_fn = lambda status: None
    return radio


def test_download_reads_the_table_and_upload_writes_it_back(drv):
    base = new_radio(drv)
    load_csv(base)
    eeprom = bytearray(base._mmap.get(0, 0x10000))
    eeprom[0xB190:0xD000] = b"\xff" * (0xD000 - 0xB190)
    fake = FakeRadio(drv, eeprom)

    r = attach(new_radio(drv, b"\xff" * 0x10000))
    r._mmap = drv.do_download(r)
    r.process_mmap()
    assert len(r._mmap) == 0x10000
    assert r.get_memory(4).comment == "Mechanicsburg, Three Square Hollow"

    mem = r.get_memory(1)
    mem.comment = "Gettysburg, Culp's Hill"
    r.set_memory(mem)
    fake.writes.clear()
    drv.do_upload(r)
    assert any(off >= 0xD000 for off, _ in fake.writes)               # the table went to the radio
    assert max(off for off, _ in fake.writes if off < 0xD000) < 0xA178 + 0x80   # and the config area as before, not calibration
    assert rc.rpt_decode(bytes(fake.eeprom[0xD000:0xD000 + 48]), int(r._memobj.channel[0].freq)) == "Gettysburg, Culp's Hill"


def test_upload_never_writes_over_the_aprs_record(drv):
    base = new_radio(drv)
    load_csv(base)
    eeprom = bytearray(base._mmap.get(0, 0x10000))
    aprs = b"APR1" + bytes(range(92))                                  # what the APRS build keeps at 0xD000
    eeprom[0xD000:0xD000 + 96] = aprs
    eeprom[0xD000 + 96:0x10000] = b"\xff" * (0x10000 - 0xD000 - 96)
    fake = FakeRadio(drv, eeprom)
    r = attach(new_radio(drv, b"\xff" * 0x10000))
    r._mmap = drv.do_download(r)
    r.process_mmap()
    assert not r.repinfo_writable()
    fake.writes.clear()
    drv.do_upload(r)
    assert not any(off >= 0xD000 for off, _ in fake.writes)
    assert bytes(fake.eeprom[0xD000:0xD000 + 96]) == aprs
