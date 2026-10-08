"""Bank (scan list) assignment from CSV columns, without CHIRP: which list each channel goes in, and which lists get a
name. The firmware's lists are numbered 1-24. Each has a **long name** of up to 16 characters (shown on the card when the
BANK key is pressed, in the ScList menu and on the scan screen) and a **short name** of up to 3 (status bar, channel tag).
A channel is in one list, in ALL (every list), or in none (OFF, which is also what CHIRP calls Skip).

A bank cell (CSV column `Scanlist` or `Bank`) is: empty (OFF), ALL, a list number 1-24, or a bank name. A name is matched
to a list that already has that long name (ignoring case), or whose only name is that short name; otherwise it takes the
first list that has no name and names it. An optional short-name cell (column `Short`) sets the short name of its bank;
without one the short name is the first three letters of the long name, upper-cased.
"""
LISTS = 24
OFF = 0
ALL = LISTS + 1
LONG_LEN = 16
SHORT_LEN = 3


def _printable(text):
    return "".join(c for c in text if " " <= c <= "~")


def clean_long(text):
    return _printable(text).strip()


def clean_short(text):
    return _printable(text.upper()).strip()[:SHORT_LEN].strip()


def assign_banks(short_names, long_names, cells, shorts):
    """short_names / long_names: the 24 current names ('' for none). cells, shorts: one bank cell and one short-name
    cell (str) per channel, in the same order. Returns (new_short_names, new_long_names, values, warnings): values[i] is
    the list value for cells[i] (0 OFF, 1-24, 25 ALL)."""
    if len(short_names) != LISTS or len(long_names) != LISTS:
        raise ValueError("expected %d list names" % LISTS)
    if len(cells) != len(shorts):
        raise ValueError("one short-name cell per bank cell")
    new_s = [n.strip() for n in short_names]
    new_l = [n.strip() for n in long_names]
    values, warnings = [], []
    explicit, used = {}, set()
    numbered = {int(c.strip()) for c in cells if c.strip().isdigit() and 1 <= int(c.strip()) <= LISTS}
    for cell, short_cell in zip(cells, shorts):
        text = cell.strip()
        if not text:
            values.append(OFF)
            continue
        if text.upper() == "ALL":
            values.append(ALL)
            continue
        if text.isdigit():
            n = int(text)
            if 1 <= n <= LISTS:
                values.append(n)
            else:
                warnings.append("bank number %s is not 1-%d: left out of every bank" % (text, LISTS))
                values.append(OFF)
            continue
        name = clean_long(text)
        if not name:
            warnings.append("bank '%s' has no usable characters: left out of every bank" % text)
            values.append(OFF)
            continue
        if len(name) > LONG_LEN:
            warnings.append("bank '%s' is cut to %d characters" % (name, LONG_LEN))
            name = name[:LONG_LEN].rstrip()
        index = _find_list(new_s, new_l, name)
        if index is None:
            free = [i for i in range(LISTS) if not new_l[i] and not new_s[i] and (i + 1) not in numbered]
            if not free:
                warnings.append("no free bank for '%s' (all %d lists are named): left out of every bank" % (name, LISTS))
                values.append(OFF)
                continue
            index = free[0]
            new_l[index] = name
            new_s[index] = clean_short(name)
        elif not new_l[index]:
            new_l[index] = name                     # a list that only had a short name
        used.add(index)
        values.append(index + 1)
        _apply_short(index, short_cell, new_s, explicit, warnings)
    _report_clashes(used, new_s, new_l, warnings)
    return new_s, new_l, values, sorted(set(warnings))


def _find_list(new_s, new_l, name):
    lowered = [n.lower() for n in new_l]
    if name.lower() in lowered:
        return lowered.index(name.lower())
    for i in range(LISTS):
        if not new_l[i] and new_s[i].upper() == name.upper():
            return i
    return None


def _apply_short(index, short_cell, new_s, explicit, warnings):
    text = short_cell.strip()
    if not text:
        return
    short = clean_short(text)
    if not short:
        warnings.append("short name '%s' has no usable characters: ignored" % text)
        return
    if short != text.upper():
        warnings.append("short name '%s' is shown as '%s' (%d characters)" % (text, short, SHORT_LEN))
    if index in explicit and explicit[index] != short:
        warnings.append("bank %d has two short names ('%s', '%s'): kept '%s'" % (index + 1, explicit[index], short, explicit[index]))
        return
    explicit[index] = short
    new_s[index] = short


def _report_clashes(used, new_s, new_l, warnings):
    seen = {}
    for i in sorted(used):
        if new_s[i]:
            seen.setdefault(new_s[i], []).append(i)
    for short, lists in seen.items():
        if len(lists) > 1:
            names = " and ".join("'%s'" % (new_l[i] or new_s[i]) for i in lists)
            warnings.append("banks %s are both shown as '%s' on the status bar: give one a Short name" % (names, short))
