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
