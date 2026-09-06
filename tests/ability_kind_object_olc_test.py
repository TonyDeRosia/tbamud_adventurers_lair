from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[1]

DB = (ROOT / "src" / "db.c").read_text(encoding="utf-8")
OEDIT = (ROOT / "src" / "oedit.c").read_text(encoding="utf-8")
OTHER = (ROOT / "src" / "act.other.c").read_text(encoding="utf-8")
PARSER = (ROOT / "src" / "spell_parser.c").read_text(encoding="utf-8")
SPELLS_H = (ROOT / "src" / "spells.h").read_text(encoding="utf-8")
STRUCTS_H = (ROOT / "src" / "structs.h").read_text(encoding="utf-8")

def define_value(text: str, name: str) -> int:
    m = re.search(rf"^#define\s+{re.escape(name)}\s+(\d+)\b", text, re.M)
    assert m, name
    return int(m.group(1))

def test_numeric_layout_is_still_unchanged():
    assert define_value(SPELLS_H, "MAX_SPELLS") == 230
    assert define_value(SPELLS_H, "NUM_SPELLS") == 230
    assert define_value(STRUCTS_H, "MAX_SKILLS") == 297
    assert define_value(SPELLS_H, "SPELL_DG_AFFECT") == 298
    assert define_value(SPELLS_H, "TOP_SPELL_DEFINE") == 299

def test_boot_order_registers_ability_metadata_before_world_objects():
    spell_pos = DB.index("mag_assign_spells();")
    world_pos = DB.index("boot_world();")
    assert spell_pos < world_pos

def test_db_object_validation_uses_explicit_kind_in_normal_boot():
    assert "!ability_is_spell(GET_OBJ_VAL(obj, val))" in DB
    assert "out of range or non-spell ability" in DB

def test_db_syntax_check_keeps_legacy_numeric_fallback():
    assert "if (scheck) {" in DB
    assert (
        "GET_OBJ_VAL(obj, val) > MAX_SPELLS && "
        "GET_OBJ_VAL(obj, val) <= MAX_SKILLS"
        in DB
    )

def test_oedit_menu_scans_full_table_and_filters_spell_kind():
    assert (
        "for (counter = 1; counter <= TOP_SPELL_DEFINE; counter++)"
        in OEDIT
    )
    assert "if (!ability_is_spell(counter))" in OEDIT
    assert "NUM_SPELLS" not in OEDIT

def test_oedit_magic_item_slots_validate_kind_instead_of_clamping():
    assert OEDIT.count("if (!ability_is_spell(number))") >= 3
    assert "LIMIT(number, 1, NUM_SPELLS)" not in OEDIT
    assert "max_val = NUM_SPELLS" not in OEDIT

def test_scroll_and_potion_still_allow_empty_secondary_slots():
    assert "number == 0 || number == -1" in OEDIT
    assert "GET_OBJ_VAL(OLC_OBJ(d), 2) = -1;" in OEDIT
    assert "GET_OBJ_VAL(OLC_OBJ(d), 3) = -1;" in OEDIT

def test_direct_wand_activation_rejects_non_spells():
    assert "if (!ability_is_spell(GET_OBJ_VAL(wand, 3)))" in OTHER
    assert "GET_OBJ_VAL(wand, 3) > TOP_SPELL_DEFINE" not in OTHER

def test_object_magic_wand_path_rejects_non_spells():
    assert "if (!ability_is_spell(GET_OBJ_VAL(obj, 3)))" in PARSER

def test_explicit_kind_foundation_remains_authoritative():
    assert "ABILITY_KIND_SPELL" in SPELLS_H
    assert "int ability_is_spell(int ability);" in SPELLS_H

def run():
    tests = [
        test_numeric_layout_is_still_unchanged,
        test_boot_order_registers_ability_metadata_before_world_objects,
        test_db_object_validation_uses_explicit_kind_in_normal_boot,
        test_db_syntax_check_keeps_legacy_numeric_fallback,
        test_oedit_menu_scans_full_table_and_filters_spell_kind,
        test_oedit_magic_item_slots_validate_kind_instead_of_clamping,
        test_scroll_and_potion_still_allow_empty_secondary_slots,
        test_direct_wand_activation_rejects_non_spells,
        test_object_magic_wand_path_rejects_non_spells,
        test_explicit_kind_foundation_remains_authoritative,
    ]

    for test in tests:
        test()

    print(f"{len(tests)} tests passed: object spell / OLC ability-kind V1")

if __name__ == "__main__":
    run()