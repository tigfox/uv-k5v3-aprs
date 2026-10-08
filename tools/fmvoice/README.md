# FM Voice build tools

Host tests (C under AddressSanitizer, the Python codec pinned to the C one): `make -C tools/fmvoice test`.

## Programming the info text from CHIRP

The FM Voice build keeps, for memories 1-256, a city / landmark text (up to 45 characters) beside the channel. The
callsign is the channel's name and the frequency its own. In CHIRP the text is the memory's **Comment**: a RepeaterBook
export already has it ("Mechanicsburg, Three Square Hollow"), so importing the CSV is enough.

CHIRP's stock driver does not know the text, so a patched one is made from armel's driver (GPL; it is not kept in this
repository):

    python3 tools/fmvoice/make_driver.py ~/Downloads/f4hwn.chirp.v6.1.0.py ~/Downloads/f4hwn.fmvoice.chirp.v6.1.0.py

It fails with a clear message if armel's driver changed in a way the script does not expect. In CHIRP: enable the
Developer menu (Help), then *File > Load Module* and pick the new file. The radio then appears as
**Quansheng UV-K1 & UV-K5 V3 (F4HWN FM Voice)**; download from the radio, import your CSV (*File > Import*), upload.

- Texts over 45 characters are cut, and CHIRP warns about it per memory. Memories above 256 keep no text.
- Upload writes the table only when the radio's table region looks like this table. It will not write over an APRS
  build's settings record, which uses the same addresses.
- Use this driver only with the FM Voice build; use armel's stock driver for the others. An edit made with the stock
  driver does not break anything: the text carries a check of the channel's frequency, and a channel that changed
  behind its back simply shows no text.

`check_csv.py file.csv` lists what the radio cannot keep before you import: a split row with no offset, rows with no
tone, over-long text, frequencies outside 144-148 / 420-450 MHz, duplicates.

## Tests that need CHIRP

`test_chirp_driver.py` imports a CSV through the real CHIRP code into the patched driver, reads the image back with the
radio's own C decoder, and runs the download and upload against a fake radio. It needs CHIRP's source and armel's driver:

    git clone --depth 1 https://github.com/kk7ds/chirp /path/to/chirp-src
    python3 -m venv venv && venv/bin/pip install pyserial pyyaml requests lark pytest
    CHIRP_SRC=/path/to/chirp-src FMV_UPSTREAM_DRIVER=~/Downloads/f4hwn.chirp.v6.1.0.py \
        venv/bin/python -m pytest tools/fmvoice/test_chirp_driver.py

Without them those tests are skipped.

## One command: CSV to the radio

    python3 tools/fmvoice/chirp_cli.py upload-csv repeaters.csv --port /dev/cu.usbserial-XXXX

It downloads the radio first and keeps that as `fmvoice-backup-<time>.img` (change with `--backup`), puts the CSV on
top of the download (channels by `Location`, place text from Comment, banks from a `Scanlist` / `Bank` column; channels
the CSV does not mention stay as they are), keeps the result as `fmvoice-upload-<time>.img`, and asks you to type
`yes` before uploading (`--yes` skips the question). `--dry-run --base some.img` builds the image without a radio.
Close CHIRP first (one program can hold the serial port), and restart the radio after the upload. Restore a backup with
`chirp_cli.py -s PORT -r Quansheng_UV-K1_\&_UV-K5_V3_F4HWN_FM_Voice --upload-mmap` and `--mmap fmvoice-backup-<time>.img`.

## CHIRP from the command line

`chirp_cli.py` builds the driver on the fly and runs CHIRP's `chirpc` with it loaded (needs a clone of
kk7ds/chirp as `CHIRP_SRC`, its Python dependencies, and armel's driver as `FMV_UPSTREAM_DRIVER`):

    python3 chirp_cli.py csv2img repeaters.csv fmvoice.img            # RepeaterBook CSV -> radio image, no radio
    python3 chirp_cli.py --mmap fmvoice.img -r Quansheng_UV-K1_\&_UV-K5_V3_F4HWN_FM_Voice --list-mem
    python3 chirp_cli.py -s /dev/cu.usbserial-XXXX -r Quansheng_UV-K1_\&_UV-K5_V3_F4HWN_FM_Voice --download-mmap radio.img
    python3 chirp_cli.py -s /dev/cu.usbserial-XXXX -r Quansheng_UV-K1_\&_UV-K5_V3_F4HWN_FM_Voice --upload-mmap fmvoice.img

Banks from the CSV: add a column named `Scanlist` (or `Bank`) to the CSV. A cell is empty (no bank), `ALL`, a list
number 1-24, or a bank **name**. Names are upper-cased and cut to 3 characters (what the radio shows); a name
that a list already has is reused, a new name takes the first list without a name and names it. CHIRP's own CSV
import ignores that column, so go through `csv2img` with the image you just downloaded from the radio:

    python3 chirp_cli.py -s /dev/cu.usbserial-XXXX -r Quansheng_UV-K1_\&_UV-K5_V3_F4HWN_FM_Voice --download-mmap radio.img
    python3 chirp_cli.py csv2img repeaters.csv new.img --base radio.img   # keeps the radio's settings, sets channels + banks
    # then upload new.img with --upload-mmap, or open it in CHIRP (with the FM Voice module loaded) and upload there

Without `--base` the image has no radio settings: fine for looking at, not for uploading.

`csv2img` prints what the driver objects to (text over 45 characters, power levels the radio lacks). Upload
overwrites the radio's channels (and settings stored in the image): download first and keep a copy.
