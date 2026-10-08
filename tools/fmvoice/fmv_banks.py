"""Bank (scan list) assignment from a CSV column, without CHIRP: which list each channel goes in, and which lists
get a new name. The firmware's lists are numbered 1-24 and have a 3-character name; a channel is in one list, in
ALL (every list), or in none (OFF).

A cell is: empty (OFF), ALL, a list number 1-24, or a bank name. A name is matched to a list that already has that
name (ignoring case); otherwise it takes the first list that has no name and names it. Names are upper-cased and cut
to 3 characters (the radio shows 3).
"""
LISTS = 24
OFF = 0
ALL = LISTS + 1
NAME_LEN = 3


def clean_name(text):
    return "".join(c for c in text.strip().upper() if " " <= c <= "~")[:NAME_LEN].strip()


def assign_banks(names, cells):
    """names: the 24 current list names ('' for none). cells: one cell (str) per channel, in any order.
    Returns (new_names, values, warnings): values[i] is the list value for cells[i] (0 OFF, 1-24, 25 ALL)."""
    if len(names) != LISTS:
        raise ValueError("expected %d list names" % LISTS)
    new_names = [n.strip() for n in names]
    values, warnings = [], []
    numbered = {int(c.strip()) for c in cells if c.strip().isdigit() and 1 <= int(c.strip()) <= LISTS}
    for cell in cells:
        text = cell.strip()
        if not text:
            values.append(OFF)
        elif text.upper() == "ALL":
            values.append(ALL)
        elif text.isdigit():
            n = int(text)
            if 1 <= n <= LISTS:
                values.append(n)
            else:
                warnings.append("bank number %s is not 1-%d: left out of every bank" % (text, LISTS))
                values.append(OFF)
        else:
            name = clean_name(text)
            if not name:
                warnings.append("bank '%s' has no usable characters: left out of every bank" % text)
                values.append(OFF)
                continue
            if clean_name(text) != text.strip().upper():
                warnings.append("bank '%s' is shown as '%s' (3 characters)" % (text.strip(), name))
            lowered = [n.upper() for n in new_names]
            if name in lowered:
                values.append(lowered.index(name) + 1)
                continue
            free = [i for i, n in enumerate(new_names) if not n and (i + 1) not in numbered]
            if not free:
                warnings.append("no free bank for '%s' (all %d lists are named): left out of every bank" % (name, LISTS))
                values.append(OFF)
                continue
            new_names[free[0]] = name
            values.append(free[0] + 1)
    return new_names, values, sorted(set(warnings))
