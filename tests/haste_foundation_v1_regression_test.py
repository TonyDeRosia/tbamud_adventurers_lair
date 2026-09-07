from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[1]

STRUCTS = (ROOT / "src" / "structs.h").read_text(encoding="utf-8")
CONSTANTS = (ROOT / "src" / "constants.c").read_text(encoding="utf-8")
SPELLS = (ROOT / "src" / "spells.h").read_text(encoding="utf-8")
PARSER = (ROOT / "src" / "spell_parser.c").read_text(encoding="utf-8")
MAGIC = (ROOT / "src" / "magic.c").read_text(encoding="utf-8")
CLASS = (ROOT / "src" / "class.c").read_text(encoding="utf-8")
FIGHT = (ROOT / "src" / "fight.c").read_text(encoding="utf-8")
APPRAISE = (ROOT / "src" / "act.offensive.c").read_text(encoding="utf-8")

def define_value(text: str, name: str) -> int:
    m = re.search(rf"^#define\s+{re.escape(name)}\s+(\d+)\b", text, re.M)
    assert m, name
    return int(m.group(1))

def affected_bit_names():
    m = re.search(
        r"const\s+char\s+\*affected_bits\[\]\s*=\s*\{(?P<body>.*?)\n\};",
        CONSTANTS,
        re.S,
    )
    assert m
    return re.findall(r'"((?:\\.|[^"\\])*)"', m.group("body"))

def test_haste_ability_id_is_stable():
    assert define_value(SPELLS, "SPELL_HASTE") == 276
    assert define_value(SPELLS, "SPELL_DG_AFFECT") == 298
    assert define_value(STRUCTS, "MAX_SKILLS") == 297

def test_haste_uses_live_next_affect_bit():
    assert define_value(STRUCTS, "AFF_NERVE_DISRUPTION") == 64
    assert define_value(STRUCTS, "AFF_HASTE") == 65
    assert define_value(STRUCTS, "NUM_AFF_FLAGS") == 66
    assert define_value(STRUCTS, "AF_ARRAY_MAX") == 4

def test_affected_bits_table_is_index_aligned():
    names = affected_bit_names()
    assert names[64] == "NERVE-DISRUPTION"
    assert names[65] == "HASTE"
    assert names[66] == r"\n"
    assert len(names) == 67

def test_haste_registers_as_affect_spell():
    start = PARSER.index('spello(SPELL_HASTE, "haste"')
    reg = PARSER[start:start + 280]

    assert "50, 25, 2, POS_FIGHTING" in reg
    assert "TAR_CHAR_ROOM, FALSE, MAG_AFFECTS" in reg
    assert "Your magically quickened pace returns to normal." in reg
    assert "MAG_DAMAGE" not in reg
    assert "MAG_AREAS" not in reg

def test_haste_affect_is_medium_and_refreshable():
    assert re.search(
        r"case SPELL_HASTE:\s*"
        r"af\[0\]\.duration = spell_dur_medium\(level\);\s*"
        r"af\[0\]\.location = APPLY_NONE;\s*"
        r"SET_BIT_AR\(af\[0\]\.bitvector, AFF_HASTE\);\s*"
        r"refresh_on_recast = TRUE;\s*"
        r"break;",
        MAGIC,
        re.S,
    )

def test_normal_haste_access_is_mage_and_bard_only():
    assert "spell_level(SPELL_HASTE, CLASS_MAGIC_USER, 25);" in CLASS
    assert "spell_level(SPELL_HASTE, CLASS_BARD, 30);" in CLASS

    for cls in (
        "CLASS_CLERIC",
        "CLASS_THIEF",
        "CLASS_WARRIOR",
        "CLASS_PALADIN",
        "CLASS_WARLOCK",
        "CLASS_DRUID",
        "CLASS_MYSTIC",
    ):
        assert f"spell_level(SPELL_HASTE, {cls}," not in CLASS

def test_existing_appraise_haste_detection_is_now_real():
    assert "#ifdef AFF_HASTE" in APPRAISE
    assert "AFF_FLAGGED(vict, AFF_HASTE)" in APPRAISE
    assert "#ifdef SPELL_HASTE" in APPRAISE
    assert "affected_by_spell(vict, SPELL_HASTE)" in APPRAISE

def test_haste_foundation_does_not_add_combat_packets():
    assert "perform_haste" not in FIGHT
    assert "haste_bonus" not in FIGHT
    assert "AFF_FLAGGED(ch, AFF_HASTE)" not in FIGHT

def test_combat_scheduler_is_unchanged():
    assert "perform_combat_pulse(bool effects_due)" in FIGHT
    assert "combat_effects_due = effects_due;" in FIGHT

def run():
    tests = [
        test_haste_ability_id_is_stable,
        test_haste_uses_live_next_affect_bit,
        test_affected_bits_table_is_index_aligned,
        test_haste_registers_as_affect_spell,
        test_haste_affect_is_medium_and_refreshable,
        test_normal_haste_access_is_mage_and_bard_only,
        test_existing_appraise_haste_detection_is_now_real,
        test_haste_foundation_does_not_add_combat_packets,
        test_combat_scheduler_is_unchanged,
    ]

    for test in tests:
        test()

    print(f"{len(tests)} tests passed: Haste Foundation V1 regression")

if __name__ == "__main__":
    run()