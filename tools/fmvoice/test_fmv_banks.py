import pytest
from fmv_banks import ALL, LISTS, OFF, assign_banks, clean_long, clean_short

BLANK = [""] * LISTS


def run(cells, shorts=None, have_short=None, have_long=None):
    s0 = list(have_short or BLANK)
    l0 = list(have_long or BLANK)
    return assign_banks(s0, l0, cells, shorts if shorts is not None else [""] * len(cells))


def test_empty_and_all_and_numbers():
    s, l, vals, warn = run(["", " ", "all", "ALL", "3", " 24 "])
    assert vals == [OFF, OFF, ALL, ALL, 3, 24]
    assert s == BLANK and l == BLANK and warn == []


def test_a_new_name_takes_the_first_free_list_with_a_short_name_from_its_first_letters():
    s, l, vals, _ = run(["GMRS Repeaters", "Weather", "gmrs repeaters"])
    assert vals == [1, 2, 1]
    assert l[:3] == ["GMRS Repeaters", "Weather", ""] and s[:3] == ["GMR", "WEA", ""]


def test_a_short_name_column_sets_the_short_name_of_its_bank():
    s, l, vals, warn = run(["GMRS Repeaters", "GMRS Repeaters", "Weather"], ["GR", "", "WX"])
    assert vals == [1, 1, 2] and s[:2] == ["GR", "WX"] and l[:2] == ["GMRS Repeaters", "Weather"] and warn == []


def test_two_different_short_names_for_one_bank_keep_the_first_and_warn():
    s, _, _, warn = run(["Local", "Local"], ["LOC", "LC"])
    assert s[0] == "LOC" and any("two short names" in w for w in warn)


def test_names_are_matched_ignoring_case_and_the_stored_case_is_kept():
    have_l = BLANK[:]; have_s = BLANK[:]
    have_l[4], have_s[4] = "Ski Trips", "SKI"
    s, l, vals, _ = run(["ski trips", "NEW"], have_short=have_s, have_long=have_l)
    assert vals == [5, 1] and l[4] == "Ski Trips" and l[0] == "NEW"


def test_a_list_that_only_has_a_short_name_is_matched_by_it_and_gets_the_long_name():
    have_s = BLANK[:]
    have_s[2] = "WX"
    s, l, vals, _ = run(["wx"], have_short=have_s)
    assert vals == [3] and l[2] == "wx" and s[2] == "WX"


def test_long_names_are_cut_to_16_and_reported():
    s, l, vals, warn = run(["A name that is far too long"])
    assert l[0] == "A name that is f" and len(l[0]) == 16
    assert any("cut to 16" in w for w in warn)


def test_short_names_are_upper_case_and_cut_to_3():
    s, _, _, warn = run(["Home", "Work"], ["hometown", "w"])
    assert s[:2] == ["HOM", "W"] and any("short name 'hometown' is shown as 'HOM'" in w for w in warn)


def test_two_banks_with_the_same_derived_short_name_are_reported():
    s, _, vals, warn = run(["GMRS Local", "GMRS Repeaters"])
    assert vals == [1, 2] and s[:2] == ["GMR", "GMR"] and any("both shown as 'GMR'" in w for w in warn)


def test_a_numbered_list_is_not_given_to_a_new_name():
    _, _, vals, _ = run(["1", "Home"])
    assert vals == [1, 2]


def test_out_of_range_and_unusable_cells_are_left_out_with_a_warning():
    _, _, vals, warn = run(["25", "0", "éè"])
    assert vals == [OFF, OFF, OFF] and len(warn) == 3


def test_running_out_of_lists():
    cells = ["B%02d" % i for i in range(LISTS + 1)]
    s, l, vals, warn = run(cells)
    assert vals[-1] == OFF and all(n for n in l) and any("no free bank" in w for w in warn)


def test_input_is_not_changed():
    have_s, have_l = BLANK[:], BLANK[:]
    assign_banks(have_s, have_l, ["Home"], [""])
    assert have_s == BLANK and have_l == BLANK


def test_wrong_number_of_names_or_cells():
    with pytest.raises(ValueError):
        assign_banks([""] * 5, BLANK, [], [])
    with pytest.raises(ValueError):
        assign_banks(BLANK, [""] * 5, [], [])
    with pytest.raises(ValueError):
        assign_banks(BLANK, BLANK, ["a"], [])


def test_clean_names():
    assert clean_long("  Hello, World!  ") == "Hello, World!"
    assert clean_long("é") == "" and clean_long("a\tb") == "ab"
    assert clean_short("  gmrs ") == "GMR" and clean_short("é") == ""
