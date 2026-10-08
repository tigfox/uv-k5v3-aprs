#!/usr/bin/env python3
"""Check a CHIRP / RepeaterBook CSV before it goes to the Repeater build.

    check_csv.py file.csv

Reports what the radio cannot keep or what is probably a mistake:
  - a "split" row with no offset (the transmit frequency is lost);
  - a row with no tone (fine for an open repeater, but check);
  - a Comment (the city / landmark text) longer than 45 characters: it will be cut;
  - memories above 256: they keep no text;
  - a name longer than 10 characters (CHIRP's limit for this radio);
  - frequencies outside 144-148 and 420-450 MHz (the Repeater build neither receives nor transmits there);
  - the same name on the same frequency twice.
Exit status 0 if nothing was reported, 1 otherwise.
"""
import csv
import sys

from rpt_codec import RPT_SLOTS, RPT_TEXT_MAX, rpt_clean

BANDS = ((144.0, 148.0), (420.0, 450.0))


def check(rows):
    """Problems as (row number, message), row number being the CSV's Location column or the line."""
    out = []
    seen = set()
    for n, r in enumerate(rows, start=1):
        loc = r.get("Location") or str(n)
        try:
            number = int(loc)
        except ValueError:
            number = n
        try:
            freq = float(r["Frequency"])
        except (KeyError, ValueError):
            out.append((loc, "no usable Frequency"))
            continue
        name = (r.get("Name") or "").strip()
        if not any(lo <= freq < hi for lo, hi in BANDS):
            out.append((loc, f"{name} {freq:.4f} MHz is outside 144-148 / 420-450 MHz"))
        if r.get("Duplex") == "split" and not (r.get("Offset") or "").strip():
            out.append((loc, f"{name} {freq:.4f} MHz is split with no offset: the transmit frequency is lost"))
        if not (r.get("Tone") or "").strip():
            out.append((loc, f"{name} {freq:.4f} MHz has no tone (open repeater?)"))
        comment = r.get("Comment") or ""
        if rpt_clean(comment)[1]:
            out.append((loc, f"{name}: the text is longer than {RPT_TEXT_MAX} characters and will be cut"))
        if comment.strip() and number > RPT_SLOTS:
            out.append((loc, f"{name}: memory {number} is above {RPT_SLOTS}: it keeps no city / landmark text"))
        if len(name) > 10:
            out.append((loc, f"name '{name}' is longer than 10 characters"))
        key = (name, round(freq, 5))
        if key in seen:
            out.append((loc, f"{name} {freq:.4f} MHz appears twice"))
        seen.add(key)
    return out


def main(argv):
    if len(argv) != 2:
        print(__doc__)
        return 1
    with open(argv[1], newline="") as f:
        rows = list(csv.DictReader(f))
    problems = check(rows)
    for loc, msg in problems:
        print(f"  memory {loc}: {msg}")
    print(f"{len(rows)} memories, {len(problems)} thing(s) to look at")
    return 1 if problems else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
