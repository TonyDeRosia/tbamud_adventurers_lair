"""OLC-only aliases for OFFHAND and TWO_HANDER remain extra flags."""
from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[1]
OEDIT = (ROOT / "src/oedit.c").read_text()
STRUCTS = (ROOT / "src/structs.h").read_text()
CONSTANTS = (ROOT / "src/constants.c").read_text()


def section(start: str, end: str) -> str:
    return OEDIT[OEDIT.index(start):OEDIT.index(end, OEDIT.index(start))]


def test_internal_bits_and_tables_are_unchanged():
    assert re.search(r"#define\s+ITEM_TWO_HANDER\s+18\b", STRUCTS)
    assert re.search(r"#define\s+ITEM_OFFHAND\s+19\b", STRUCTS)
    assert '"TWO_HANDER",\n  "OFFHAND",' in CONSTANTS
    assert "ITEM_WEAR_OFFHAND" not in STRUCTS
    assert "ITEM_WEAR_TWOHAND" not in STRUCTS


def test_extra_menu_hides_weapon_handling_aliases():
    menu = section("/* Object extra flags. */", "/* Object perm flags. */")
    assert "counter == ITEM_OFFHAND || counter == ITEM_TWO_HANDER" in menu
    assert "oedit_format_extra_flags" in menu


def test_wear_menu_exposes_both_aliases():
    menu = section("/* Object wear flags. */", "/* Display main menu. */")
    assert '"OFFHAND"' in menu
    assert '"TWO-HANDED"' in menu
    assert "oedit_format_wear_flags" in menu


def test_wear_parser_toggles_existing_extra_bits():
    parser = section("case OEDIT_WEAR:", "case OEDIT_WEIGHT:")
    assert "GET_OBJ_EXTRA" in parser
    assert "ITEM_OFFHAND" in parser
    assert "ITEM_TWO_HANDER" in parser
    assert "GET_OBJ_WEAR" in parser


def test_wear_parser_prevents_incompatible_hand_flags():
    parser = section("case OEDIT_WEAR:", "case OEDIT_WEIGHT:")
    assert "REMOVE_BIT_AR(GET_OBJ_EXTRA(OLC_OBJ(d)), ITEM_TWO_HANDER)" in parser
    assert "REMOVE_BIT_AR(GET_OBJ_EXTRA(OLC_OBJ(d)), ITEM_OFFHAND)" in parser
    assert "OFFHAND weapons must be one-handed" in parser


def test_main_menu_formats_aliases_as_wear_flags_only():
    menu = section("/* Display main menu. */", "/* main loop (of sorts)..")
    assert "oedit_format_extra_flags(obj" in menu
    assert "oedit_format_wear_flags(OLC_OBJ(d)" in menu


if __name__ == "__main__":
    tests = [value for name, value in globals().items() if name.startswith("test_")]
    for test in tests:
        test()
    print(f"{len(tests)} OEDIT weapon-hand flag tests passed")
