#!/usr/bin/env python3
"""Run CHIRP's command line with the FM Voice driver loaded (the driver is built on the fly from armel's, not kept).

    CHIRP_SRC=~/src/chirp-src FMV_UPSTREAM_DRIVER=~/Downloads/f4hwn.chirp.v6.1.0.py \
        python3 chirp_cli.py csv2img repeaters.csv fmvoice.img      # CSV -> radio image, no radio needed
    python3 chirp_cli.py <any chirpc arguments>                      # e.g. -s /dev/cu.usbserial-X -r "F4HWN FM Voice" --download-mmap a.img

csv2img [--base downloaded.img] takes the memories of the CSV by Location with their Comment as the place text and
the optional Scanlist / Bank column as the bank (see README), and prints what the driver warned about. Needs CHIRP's dependencies (pyserial, pyyaml, requests, lark).
"""
import csv
import importlib.util
import os
import subprocess
import sys
import tempfile
from unittest import mock

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import fmv_banks  # noqa: E402
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


BANK_HEADERS = ("scanlist", "bank", "banks", "scan list")


def read_bank_cells(csv_path):
    """{Location: cell} from the CSV's bank column, or None if it has no such column."""
    with open(csv_path, newline="", encoding="utf-8-sig") as f:
        reader = csv.DictReader(f)
        column = next((h for h in (reader.fieldnames or []) if h.strip().lower() in BANK_HEADERS), None)
        if column is None:
            return None
        return {int(row["Location"]): (row[column] or "") for row in reader if (row.get("Location") or "").strip().isdigit()}


def list_names(radio):
    out = []
    for i in range(fmv_banks.LISTS):
        raw = bytes(int(c) for c in radio._memobj.listname[i].name)
        out.append(raw.split(b"\xff")[0].decode("ascii", "ignore").strip() if raw[:1] != b"\xff" else "")
    return out


def csv2img(drv, csv_path, img_path, base_path=None):
    from chirp import memmap
    from chirp.drivers import generic_csv
    radio = drv.UVK5RadioF4HWNFMVoice(None)
    if base_path:
        with open(base_path, "rb") as f:
            data = f.read()
    else:
        data = b"\xff" * IMAGE_SIZE
        print("  no --base image: the result has no radio settings and must not be uploaded to a radio")
    radio._mmap = memmap.MemoryMapBytes(data)
    radio.process_mmap()
    source = generic_csv.CSVRadio(csv_path)
    cells = read_bank_cells(csv_path)
    lo, hi = source.get_features().memory_bounds
    numbers = []
    for number in range(lo, hi + 1):
        mem = source.get_memory(number)
        if mem.empty:
            continue
        numbers.append(number)
        for msg in radio.validate_memory(mem):
            print(f"  channel {number} ({mem.name}): {msg}")
        radio.set_memory(mem)
    if cells is not None:
        names, values, warnings = fmv_banks.assign_banks(list_names(radio), [cells.get(n, "") for n in numbers])
        for i, name in enumerate(names):
            if name != list_names(radio)[i]:
                radio._memobj.listname[i].name = (name.encode("ascii") + b"    ")[:4]
        for number, value in zip(numbers, values):
            radio._memobj.ch_attr[number - 1].scanlist = value
        for w in warnings:
            print("  banks: " + w)
        print("  banks: " + ", ".join(f"{i + 1}={n}" for i, n in enumerate(names) if n))
    with open(img_path, "wb") as f:
        f.write(radio.get_mmap().get_byte_compatible().get_packed()
                if hasattr(radio.get_mmap(), "get_byte_compatible") else bytes(radio.get_mmap()))
    print(f"wrote {img_path}: {len(numbers)} memories")


def main(argv):
    drv = load_driver()
    if len(argv) in (4, 6) and argv[1] == "csv2img":
        base = argv[5] if len(argv) == 6 and argv[4] == "--base" else None
        if len(argv) == 6 and base is None:
            sys.exit("usage: csv2img in.csv out.img [--base downloaded.img]")
        csv2img(drv, argv[2], argv[3], base)
        return 0
    from chirp.cli import main as chirpc
    sys.argv = ["chirpc"] + argv[1:]
    return chirpc.main()


if __name__ == "__main__":
    sys.exit(main(sys.argv))
