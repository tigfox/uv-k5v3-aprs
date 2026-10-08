#!/usr/bin/env python3
"""Run CHIRP's command line with the FM Voice driver loaded (the driver is built on the fly from armel's, not kept).

    CHIRP_SRC=~/src/chirp-src FMV_UPSTREAM_DRIVER=~/Downloads/f4hwn.chirp.v6.1.0.py \
        python3 chirp_cli.py csv2img repeaters.csv fmvoice.img      # CSV -> radio image, no radio needed
    python3 chirp_cli.py <any chirpc arguments>                      # e.g. -s /dev/cu.usbserial-X -r "F4HWN FM Voice" --download-mmap a.img

csv2img takes the memories of the CSV in order (channel 1, 2, ...) with their Comment as the place text, and
prints what the driver warned about. Needs CHIRP's dependencies (pyserial, pyyaml, requests, lark).
"""
import importlib.util
import os
import subprocess
import sys
import tempfile
from unittest import mock

HERE = os.path.dirname(os.path.abspath(__file__))
CHIRP_SRC = os.path.expanduser(os.environ.get("CHIRP_SRC", ""))
UPSTREAM = os.path.expanduser(os.environ.get("FMV_UPSTREAM_DRIVER", "~/Downloads/f4hwn.chirp.v6.1.0.py"))
IMAGE_SIZE = 0x10000
os.environ.setdefault("CHIRP_TESTENV", "1")      # without a terminal CHIRP sends all output to a log file


def load_driver():
    if not (CHIRP_SRC and os.path.isdir(CHIRP_SRC)) or not os.path.exists(UPSTREAM):
        sys.exit("set CHIRP_SRC (a clone of kk7ds/chirp) and FMV_UPSTREAM_DRIVER (armel's f4hwn.chirp.v6.1.0.py)")
    sys.path.insert(0, CHIRP_SRC)
    for name in ("wx", "wx.lib", "wx.lib.newevent"):       # the driver's settings dialogs need the GUI toolkit
        sys.modules.setdefault(name, mock.MagicMock())
    out = os.path.join(tempfile.mkdtemp(prefix="fmv_driver_"), "f4hwn_fmvoice_chirp.py")
    subprocess.run([sys.executable, os.path.join(HERE, "make_driver.py"), UPSTREAM, out], check=True,
                   capture_output=True)
    spec = importlib.util.spec_from_file_location("f4hwn_fmvoice_chirp", out)
    mod = importlib.util.module_from_spec(spec)
    sys.modules["f4hwn_fmvoice_chirp"] = mod
    spec.loader.exec_module(mod)
    return mod


def csv2img(drv, csv_path, img_path):
    from chirp import memmap
    from chirp.drivers import generic_csv
    radio = drv.UVK5RadioF4HWNFMVoice(None)
    radio._mmap = memmap.MemoryMapBytes(b"\xff" * IMAGE_SIZE)
    radio.process_mmap()
    source = generic_csv.CSVRadio(csv_path)
    lo, hi = source.get_features().memory_bounds
    count = 0
    for number in range(lo, hi + 1):
        mem = source.get_memory(number)
        if mem.empty:
            continue
        count += 1
        for msg in radio.validate_memory(mem):
            print(f"  channel {number} ({mem.name}): {msg}")
        radio.set_memory(mem)
    with open(img_path, "wb") as f:
        f.write(radio.get_mmap().get_byte_compatible().get_packed()
                if hasattr(radio.get_mmap(), "get_byte_compatible") else bytes(radio.get_mmap()))
    print(f"wrote {img_path}: {count} memories")


def main(argv):
    drv = load_driver()
    if len(argv) == 4 and argv[1] == "csv2img":
        csv2img(drv, argv[2], argv[3])
        return 0
    from chirp.cli import main as chirpc
    sys.argv = ["chirpc"] + argv[1:]
    return chirpc.main()


if __name__ == "__main__":
    sys.exit(main(sys.argv))
