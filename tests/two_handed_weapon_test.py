"""Focused contracts for two-handed equipment behavior and persistence."""
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
ITEM = (ROOT / "src/act.item.c").read_text()
INFO = (ROOT / "src/act.informative.c").read_text()
HANDLER = (ROOT / "src/handler.c").read_text()
OBJSAVE = (ROOT / "src/objsave.c").read_text()
CONSTANTS = (ROOT / "src/constants.c").read_text()


def body(source: str, start: str, end: str) -> str:
    return source[source.index(start):source.index(end, source.index(start))]


def test_flag_and_builder_display_are_stable():
    structs = (ROOT / "src/structs.h").read_text()
    assert "#define ITEM_TWO_HANDER 18" in structs
    assert '"TWO_HANDER"' in CONSTANTS


def test_shared_helper_reserves_secondary_hand():
    helper = body(HANDLER, "int character_is_using_two_hander", "void equip_char")
    equip = body(HANDLER, "void equip_char", "struct obj_data *unequip_char")
    assert "ITEM_TWO_HANDER" in helper
    assert "pos == WEAR_HOLD || pos == WEAR_SHIELD" in equip
    assert "obj_to_char(obj, ch)" in equip


def test_commands_and_wear_all_use_shared_state():
    perform = body(ITEM, "static int can_equip_weapon", "int find_eq_pos")
    wear = body(ITEM, "ACMD(do_wear)", "ACMD(do_wield)")
    assert "character_is_using_two_hander(ch)" in perform
    assert "!character_is_using_two_hander(ch)" in wear
    assert "Your weapon requires both hands." in perform


def test_equipment_display_is_dynamic_without_duplicate_object():
    equipment = body(INFO, "ACMD(do_equipment)", "ACMD(do_time)")
    assert 'label = "Two-Handed"' in equipment
    assert "[UNAVAILABLE - TWO-HANDED]" in equipment
    assert "GET_EQ(ch, WEAR_HOLD)" not in equipment


def test_load_repairs_conflicts_without_deleting_objects():
    load = body(OBJSAVE, "static int Crash_load_objs(struct char_data *ch) {", "static int handle_obj")
    assert "character_is_using_two_hander(ch)" in load
    assert "obj_to_char(unequip_char(ch, i), ch)" in load
    assert "object preserved in inventory" in load


if __name__ == "__main__":
    tests = [value for name, value in globals().items() if name.startswith("test_")]
    for test in tests:
        test()
    print(f"{len(tests)} two-handed weapon regression tests passed")
