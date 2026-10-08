"""Repeater info table codec (Python twin of App/app/rpt_info.c; test_rpt_codec.py pins them together).

Table: 256 records of 48 bytes at EEPROM address 0xD000, record i for memory channel i (0-based). A record is a
little-endian 16-bit check of the channel's receive frequency (10 Hz units), then the text, NUL-terminated, at most
45 characters; an erased slot is all 0xFF. This file is also pasted into the patched CHIRP driver by make_driver.py,
so it must stay self-contained (no imports other than the standard library).
"""

RPT_SLOTS = 256
RPT_RECORD = 48
RPT_TEXT_MAX = 45
RPT_ADDR = 0xD000
RPT_END = RPT_ADDR + RPT_SLOTS * RPT_RECORD      # 0x10000


def rpt_freq_check(freq10):
    h = (freq10 * 2654435761) & 0xFFFFFFFF
    c = ((h >> 16) ^ (h & 0xFFFF)) & 0xFFFF
    return 0xFFFE if c == 0xFFFF else c


def rpt_clean(text):
    """(cleaned text, was it cut): trimmed, runs of spaces collapsed, anything but printable ASCII becomes '?'."""
    out = []
    more = False
    for ch in (text or ""):
        c = ch if 0x20 <= ord(ch) <= 0x7E else "?"
        if c == " " and (not out or out[-1] == " "):
            continue
        if len(out) == RPT_TEXT_MAX:
            more = True
            break
        out.append(c)
    while out and out[-1] == " ":
        out.pop()
    return "".join(out), more


def rpt_encode(freq10, text):
    """(48-byte record, cut). An empty text gives an erased record."""
    clean, cut = rpt_clean(text)
    if not clean:
        return b"\xff" * RPT_RECORD, False
    chk = rpt_freq_check(freq10)
    rec = bytearray(b"\xff" * RPT_RECORD)
    rec[0] = chk & 0xFF
    rec[1] = chk >> 8
    rec[2:2 + len(clean)] = clean.encode("ascii")
    rec[2 + len(clean)] = 0
    return bytes(rec), cut


def rpt_decode(rec, freq10):
    """The text of a valid record that belongs to a channel on freq10, else None."""
    if len(rec) != RPT_RECORD or (rec[0] | (rec[1] << 8)) != rpt_freq_check(freq10):
        return None
    body = bytes(rec[2:])
    n = body.find(b"\x00")
    if n <= 0 or n > RPT_TEXT_MAX or any(b < 0x20 or b > 0x7E for b in body[:n]):
        return None
    return body[:n].decode("ascii")


def rpt_plausible(rec):
    """A record this table could have written: erased, or check + printable NUL-terminated text."""
    if rec == b"\xff" * RPT_RECORD:
        return True
    body = bytes(rec[2:])
    n = body.find(b"\x00")
    return 0 < n <= RPT_TEXT_MAX and all(0x20 <= b <= 0x7E for b in body[:n])


def rpt_table_ok(table):
    """True if the whole 12 KiB region looks like this table (so it is safe to write): every record erased or plausible,
    and not the start of another firmware's data (the APRS build keeps its settings record, magic 'APR1', at 0xD000)."""
    if len(table) != RPT_SLOTS * RPT_RECORD or bytes(table[:4]) == b"APR1":
        return False
    return all(rpt_plausible(bytes(table[i:i + RPT_RECORD])) for i in range(0, len(table), RPT_RECORD))
