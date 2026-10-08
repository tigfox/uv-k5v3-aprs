import pytest
from fmv_banks import ALL, LISTS, OFF, assign_banks, clean_name

BLANK = [""] * LISTS


def test_empty_and_all_and_numbers():
    names, vals, warn = assign_banks(BLANK, ["", " ", "all", "ALL", "3", " 24 "])
    assert vals == [OFF, OFF, ALL, ALL, 3, 24]
    assert names == BLANK and warn == []


def test_new_names_take_the_first_free_lists_in_order():
    names, vals, _ = assign_banks(BLANK, ["Home", "WORK", "home", "Home"])
    assert vals == [1, 2, 1, 1]
    assert names[:3] == ["HOM", "WOR", ""]


def test_existing_names_are_reused_ignoring_case():
    have = BLANK[:]
    have[4], have[9] = "HOM", "SKI"
    names, vals, _ = assign_banks(have, ["hom", "ski", "NEW"])
    assert vals == [5, 10, 1]
    assert names[0] == "NEW" and names[4] == "HOM"


def test_long_names_are_cut_and_reported():
    _, vals, warn = assign_banks(BLANK, ["Mountain", "Mount"])
    assert vals == [1, 1]                                  # both cut to MOU: same bank
    assert any("'Mountain' is shown as 'MOU'" in w for w in warn)


def test_a_numbered_list_is_not_given_to_a_new_name():
    _, vals, _ = assign_banks(BLANK, ["1", "Home"])
    assert vals == [1, 2]


def test_out_of_range_and_unusable_cells_are_left_out_with_a_warning():
    _, vals, warn = assign_banks(BLANK, ["25", "0", "éè"])
    assert vals == [OFF, OFF, OFF] and len(warn) == 3


def test_running_out_of_lists():
    cells = ["B%02d" % i for i in range(LISTS + 1)]
    names, vals, warn = assign_banks(BLANK, cells)
    assert vals[-1] == OFF and all(n for n in names) and any("no free bank" in w for w in warn)


def test_input_is_not_changed():
    have = BLANK[:]
    assign_banks(have, ["Home"])
    assert have == BLANK


def test_wrong_number_of_names():
    with pytest.raises(ValueError):
        assign_banks([""] * 5, [])


def test_clean_name():
    assert clean_name("  ab c ") == "AB"      # cut to 3 characters then trimmed: "AB "
    assert clean_name("") == ""
