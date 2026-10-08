#!/usr/bin/env python3
"""Make the FM Voice build's CHIRP driver from armel's driver.

    make_driver.py f4hwn.chirp.v6.1.0.py  f4hwn.fmvoice.chirp.v6.1.0.py

The result is armel's driver (GPL, not kept in this repository) plus the repeater info table: each memory's CHIRP
**Comment** is the radio's city / landmark text for that channel (RepeaterBook's own "City, Landmark" works as it is).
Changes, all by anchored text replacement so a new upstream driver that moved something fails loudly here instead of
producing a wrong driver:
  - the radio is listed as "UV-K1 & UV-K5 V3 (F4HWN FM Voice)" so it does not clash with the stock driver;
  - the 24 long bank names (16 characters each, EEPROM 0x8900) are in the memory map, so CHIRP downloads and uploads them;
  - download also reads the info table (EEPROM 0xD000-0xFFFF); upload writes it only when it looks like that table
    (never over the APRS build's settings record, which lives at the same address);
  - get_memory / set_memory carry the comment, validate_memory warns about text that will be cut;
  - CHIRP's Skip "S" is scan list OFF (0): the firmware leaves a channel in no list out of every scan, ALL included.
"""
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))


def replace_once(text, old, new, what):
    n = text.count(old)
    if n != 1:
        raise SystemExit(f"make_driver: cannot patch ({what}): anchor found {n} times, expected once. "
                         "Is this the v6.1.0 driver? A new driver release needs this script updated.")
    return text.replace(old, new)


CLASS_METHODS = '''
    # ---- repeater info table (FM Voice build): the memory's Comment is the radio's city / landmark text ----
    upload_repinfo = True

    def _mm_get(self, off, n):
        d = self._mmap.get(off, n)
        return d if isinstance(d, bytes) else d.encode("latin-1")     # the older MemoryMap hands out str

    def _repinfo_slot(self, number):
        """Byte offset in the image of the record for CHIRP memory `number`, or None (no slot)."""
        if isinstance(number, str) or not 1 <= number <= FMV_SLOTS:
            return None
        return FMV_ADDR + (number - 1) * FMV_RECORD

    def repinfo_writable(self):
        """True if the region at 0xD000 in the image looks like this table (see fmv_table_ok)."""
        return fmv_table_ok(self._mm_get(FMV_ADDR, FMV_END - FMV_ADDR))

    def _load_comment(self, mem):
        mem.comment = ""
        off = self._repinfo_slot(mem.number)
        if off is None or mem.empty or isinstance(mem.freq, str):
            return
        text = fmv_decode(self._mm_get(off, FMV_RECORD), int(self._memobj.channel[mem.number - 1].freq))
        if text:
            mem.comment = text

    def _store_comment(self, mem):
        off = self._repinfo_slot(mem.number)
        if off is None:
            return
        text = "" if mem.empty else (mem.comment or "")
        rec, _cut = fmv_encode(int(self._memobj.channel[mem.number - 1].freq), text)
        self._mmap.set(off, rec)

    def _skip_attr(self, number):
        """The channel attribute of a memory channel, or None (a VFO or special memory)."""
        if isinstance(number, str) or not 1 <= number <= MR_CHANNELS_MAX:
            return None
        return self._memobj.ch_attr[number - 1]

    def _load_skip(self, mem):
        """In this firmware a channel in no scan list (OFF) is left out of every scan, ALL included: that is CHIRP's Skip."""
        attr = self._skip_attr(mem.number)
        mem.skip = "S" if attr is not None and not mem.empty and int(attr.scanlist) == 0 else ""

    def _store_skip(self, mem):
        attr = self._skip_attr(mem.number)
        if attr is not None and not mem.empty and mem.skip == "S":
            attr.scanlist = 0

    def get_memory(self, number):
        mem = self._f4hwn_get_memory(number)
        self._load_comment(mem)
        self._load_skip(mem)
        return mem

    def set_memory(self, memory):
        result = self._f4hwn_set_memory(memory)
        self._store_comment(memory)
        self._store_skip(memory)
        return result

'''

DOWNLOAD_TABLE = '''    # the repeater info table (EEPROM 0xD000-0xFFFF); the gap between is not read. A radio that does not answer for
    # it (another firmware) is taken to have no table.
    eeprom += b"\\xff" * (FMV_ADDR - len(eeprom))
    addr = FMV_ADDR
    while addr < FMV_END:
        data = _readmem(serport, addr, MEM_BLOCK)
        status.cur = MEM_SIZE + (addr - FMV_ADDR)
        radio.status_fn(status)
        if data and len(data) == MEM_BLOCK:
            eeprom += data
            addr += MEM_BLOCK
        else:
            eeprom += b"\\xff" * (FMV_END - addr)
            break

    return memmap.MemoryMapBytes(eeprom)
'''

UPLOAD_STEPS = '''        elif step == 1 and not radio.upload_calibration:
            step += 1                       # calibration not asked for: on to the info table
            continue

        elif step == 2 and radio.upload_repinfo and radio.repinfo_writable():
            # the repeater info table: only when the image's 0xD000 region looks like it (never over the APRS record)
            start_addr = FMV_ADDR
            stop_addr  = FMV_END
            status.max = stop_addr - start_addr
            status.cur = 0
            status.msg = "Uploading repeater info"
            radio.status_fn(status)

        else:
            break  # done
'''

VALIDATE = '''        msgs = super().validate_memory(mem)

        comment = getattr(mem, "comment", "") or ""
        if comment.strip():
            if isinstance(mem.number, int) and mem.number > FMV_SLOTS:
                msgs.append(chirp_common.ValidationWarning(
                    "The radio keeps the city / landmark text for memories 1-%d only; this one is not stored" % FMV_SLOTS))
            elif fmv_clean(comment)[1]:
                msgs.append(chirp_common.ValidationWarning(
                    "The city / landmark text is longer than %d characters and will be cut" % FMV_TEXT_MAX))
'''


def make(src_text, codec_text):
    t = src_text
    t = replace_once(t, 'MODEL = "UV-K1 & UV-K5 V3 (F4HWN)"', 'MODEL = "UV-K1 & UV-K5 V3 (F4HWN FM Voice)"', "model name")
    t = replace_once(t, "class UVK5RadioEgzumer(chirp_common.CloneModeRadio):",
                     "class UVK5RadioF4HWNFMVoice(chirp_common.CloneModeRadio):", "class name")
    if "UVK5RadioEgzumer" in t:
        raise SystemExit("make_driver: the class name is used elsewhere in the driver; update this script")
    t = replace_once(t, '@directory.register\nclass UVK5RadioF4HWNFMVoice', codec_text + '\n\n@directory.register\nclass UVK5RadioF4HWNFMVoice', "codec")
    t = replace_once(t, '                   "FOX HUNT",\n                   "BEACON"\n', '                   "BANK",\n                   "TONE SEARCH"\n', "key actions 22 / 23")
    t = replace_once(t, "        rf.has_comment = False", "        rf.has_comment = True", "has_comment")
    t = replace_once(t, '        rf.valid_skips = [""]', '        rf.valid_skips = ["", "S"]', "valid_skips")
    t = replace_once(t, "    upload_advanced = False\n", "    upload_advanced = False\n" + CLASS_METHODS, "class attributes")
    t = replace_once(t, "    def get_memory(self, number):\n\n        mem = chirp_common.Memory()",
                     "    def _f4hwn_get_memory(self, number):\n\n        mem = chirp_common.Memory()", "get_memory")
    t = replace_once(t, '    def set_memory(self, memory):\n        """\n        Store details about a high-level memory',
                     '    def _f4hwn_set_memory(self, memory):\n        """\n        Store details about a high-level memory', "set_memory")
    t = replace_once(t, "        msgs = super().validate_memory(mem)\n", VALIDATE, "validate_memory")
    t = replace_once(t, 'MEM_FORMAT = """\n', 'MEM_FORMAT = """\n#seekto 0x00D000;\nstruct {\n  ul16 check;\n  char text[46];\n} repinfo[256];\n\n'
                     '#seekto 0x008900;\nstruct {\n  char name[16];\n} longname[24];\n\n', "memory format")
    t = replace_once(t, "    status.max = MEM_SIZE\n    status.msg = \"Downloading from radio\"",
                     "    status.max = MEM_SIZE + (FMV_END - FMV_ADDR)\n    status.msg = \"Downloading from radio\"", "download status")
    t = replace_once(t, "            raise errors.RadioError(\"Memory download incomplete\")\n\n    return memmap.MemoryMapBytes(eeprom)\n",
                     "            raise errors.RadioError(\"Memory download incomplete\")\n\n" + DOWNLOAD_TABLE, "download")
    t = replace_once(t, "        else:\n            break  # done\n", UPLOAD_STEPS, "upload steps")
    return t


def main():
    if len(sys.argv) != 3:
        print(__doc__)
        return 1
    with open(sys.argv[1]) as f:
        src = f.read()
    with open(os.path.join(HERE, "fmv_codec.py")) as f:
        codec = f.read()
    out = make(src, "# ---- from tools/fmvoice/fmv_codec.py (FM Voice build) ----\n" + codec)
    out = out.replace("# Adapted For UV-K5 EGZUMER custom software By EGZUMER, JOC2",
                      "# FM Voice info table added by make_driver.py (tools/fmvoice) for the FM Voice build\n"
                      "# Adapted For UV-K5 EGZUMER custom software By EGZUMER, JOC2", 1)
    with open(sys.argv[2], "w") as f:
        f.write(out)
    print(f"wrote {sys.argv[2]} ({len(out.splitlines())} lines)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
