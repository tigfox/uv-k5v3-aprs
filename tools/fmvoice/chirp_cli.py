#!/usr/bin/env python3
"""Run CHIRP's command line with the FM Voice driver loaded (the driver is built on the fly from armel's, not kept).

    CHIRP_SRC=~/src/chirp-src FMV_UPSTREAM_DRIVER=~/Downloads/f4hwn.chirp.v6.1.0.py \
        python3 chirp_cli.py csv2img repeaters.csv fmvoice.img      # CSV -> radio image, no radio needed
    python3 chirp_cli.py upload-csv repeaters.csv --port /dev/cu.usbserial-X   # backup, convert, upload
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
RADIO_BAUD = 38400
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
SHORT_HEADERS = ("short", "shortname", "short name", "bank short")


def read_bank_cells(csv_path):
    """({Location: bank cell}, {Location: short-name cell}) from the CSV, or None if it has no bank column."""
    with open(csv_path, newline="", encoding="utf-8-sig") as f:
        reader = csv.DictReader(f)
        fields = reader.fieldnames or []
        column = next((h for h in fields if h.strip().lower() in BANK_HEADERS), None)
        if column is None:
            return None
        short_column = next((h for h in fields if h.strip().lower() in SHORT_HEADERS), None)
        rows = [row for row in reader if (row.get("Location") or "").strip().isdigit()]
        return ({int(r["Location"]): (r[column] or "") for r in rows},
                {int(r["Location"]): ((r[short_column] or "") if short_column else "") for r in rows})


def _raw_name(raw):
    if raw[:1] == b"\xff":
        return ""
    return raw.split(b"\xff")[0].split(b"\x00")[0].decode("ascii", "ignore").strip()


def list_names(radio):
    """The 24 short names (what the status bar shows)."""
    return [_raw_name(bytes(int(c) for c in radio._memobj.listname[i].name)) for i in range(fmv_banks.LISTS)]


def long_list_names(radio):
    """The 24 long names (up to 16 characters; what the card, the ScList menu and the scan screen show)."""
    return [_raw_name(bytes(int(c) for c in radio._memobj.longname[i].name)) for i in range(fmv_banks.LISTS)]


def write_list_names(radio, old_short, old_long, new_short, new_long):
    for i in range(fmv_banks.LISTS):
        if new_short[i] != old_short[i]:
            radio._memobj.listname[i].name = (new_short[i].encode("ascii") + b"    ")[:4]
        if new_long[i] != old_long[i]:
            radio._memobj.longname[i].name = (new_long[i].encode("ascii") + b" " * fmv_banks.LONG_LEN)[:fmv_banks.LONG_LEN]


def new_radio(drv, data, pipe=None):
    from chirp import memmap
    radio = drv.UVK5RadioF4HWNFMVoice(pipe)
    radio._mmap = memmap.MemoryMapBytes(data)
    radio.process_mmap()
    return radio


def image_bytes(radio):
    mm = radio.get_mmap()
    return mm.get_byte_compatible().get_packed() if hasattr(mm, "get_byte_compatible") else bytes(mm)


def apply_csv(radio, csv_path):
    """Put the CSV's memories (by Location), place texts and banks into the radio object; returns the count."""
    from chirp.drivers import generic_csv
    source = generic_csv.CSVRadio(csv_path)
    cells = read_bank_cells(csv_path)
    lo, hi = source.get_features().memory_bounds
    numbers, skipped = [], set()
    for number in range(lo, hi + 1):
        mem = source.get_memory(number)
        if mem.empty:
            continue
        numbers.append(number)
        if mem.skip == "S":
            skipped.add(number)
        for msg in radio.validate_memory(mem):
            print(f"  channel {number} ({mem.name}): {msg}")
        radio.set_memory(mem)
    if cells is not None:
        bank_cells, short_cells = cells
        old_short, old_long = list_names(radio), long_list_names(radio)
        new_short, new_long, values, warnings = fmv_banks.assign_banks(
            old_short, old_long, ["" if n in skipped else bank_cells.get(n, "") for n in numbers],      # a skipped channel is in no bank
            ["" if n in skipped else short_cells.get(n, "") for n in numbers])
        write_list_names(radio, old_short, old_long, new_short, new_long)
        for number, value in zip(numbers, values):
            radio._memobj.ch_attr[number - 1].scanlist = value
        for w in warnings:
            print("  banks: " + w)
        print("  banks: " + ", ".join(f"{i + 1}={new_long[i] or new_short[i]}" + (f" ({new_short[i]})" if new_long[i] and new_short[i] else "")
                                       for i in range(fmv_banks.LISTS) if new_long[i] or new_short[i]))
    return len(numbers)


def csv2img(drv, csv_path, img_path, base_path=None):
    if base_path:
        with open(base_path, "rb") as f:
            data = f.read()
    else:
        data = b"\xff" * IMAGE_SIZE
        print("  no --base image: the result has no radio settings and must not be uploaded to a radio")
    radio = new_radio(drv, data)
    count = apply_csv(radio, csv_path)
    with open(img_path, "wb") as f:
        f.write(image_bytes(radio))
    print(f"wrote {img_path}: {count} memories")


def upload_csv(drv, args):
    """Download the radio (kept as a backup), put the CSV on top of it, upload. --base replaces the download (dry run)."""
    import argparse
    import datetime
    p = argparse.ArgumentParser(prog="chirp_cli.py upload-csv")
    p.add_argument("csv")
    p.add_argument("--port", help="serial port of the radio, e.g. /dev/cu.usbserial-1410")
    p.add_argument("--backup", help="where to keep the downloaded image (default: fmvoice-backup-<time>.img)")
    p.add_argument("--yes", action="store_true", help="do not ask before uploading")
    p.add_argument("--dry-run", action="store_true", help="build the new image but do not upload; with --base, no radio is needed")
    p.add_argument("--base", help="start from this image instead of downloading (for --dry-run)")
    o = p.parse_args(args)
    if not o.base and not o.port:
        p.error("give --port, or --base with --dry-run")
    if o.base and not o.dry_run:
        p.error("--base is only for --dry-run; an upload always starts from a fresh download")
    stamp = datetime.datetime.now().strftime("%Y%m%d-%H%M%S")
    backup = o.backup or f"fmvoice-backup-{stamp}.img"
    new_path = f"fmvoice-upload-{stamp}.img"
    pipe = None
    if o.base:
        with open(o.base, "rb") as f:
            data = f.read()
    else:
        import serial
        pipe = serial.Serial(port=o.port, baudrate=drv.UVK5RadioF4HWNFMVoice.BAUD_RATE, timeout=0.5)
        radio = drv.UVK5RadioF4HWNFMVoice(pipe)
        print(f"downloading from {o.port} ...")
        radio.sync_in()
        data = image_bytes(radio)
        with open(backup, "wb") as f:
            f.write(data)
        print(f"backup of the radio: {backup}")
    radio = new_radio(drv, data, pipe)
    count = apply_csv(radio, o.csv)
    with open(new_path, "wb") as f:
        f.write(image_bytes(radio))
    print(f"{count} memories from {o.csv}; the image to upload is kept as {new_path}")
    if o.dry_run:
        print("dry run: nothing uploaded")
        return 0
    if not o.yes and input("Upload to the radio now? It replaces the channels and the settings in the image. Type yes: ").strip() != "yes":
        print("not uploaded")
        return 1
    print("uploading ... (do not touch the radio)")
    radio.sync_out()
    print("Upload successful. Restart the radio.")
    return 0


def hold_modem_lines_low():
    """Open every serial port at the radio's 38400 baud (chirpc opens at 9600 and the radio does not answer) with DTR and
    RTS low (an AIOC keys the radio's PTT from those lines; CHIRP's own open asserts both)."""
    import serial
    original = serial.Serial.__init__

    def init(self, port=None, *args, **kwargs):
        kwargs.setdefault("baudrate", RADIO_BAUD)
        original(self, None, *args, **kwargs)          # no port yet: nothing is opened (or asserted) here
        if port is not None:
            self.dtr = False
            self.rts = False
            self.port = port
            self.open()

    serial.Serial.__init__ = init


def main(argv):
    hold_modem_lines_low()
    drv = load_driver()
    if len(argv) > 2 and argv[1] == "upload-csv":
        return upload_csv(drv, argv[2:])
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
