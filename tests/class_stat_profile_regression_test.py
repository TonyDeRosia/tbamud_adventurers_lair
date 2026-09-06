from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[1]
CLASS_H = (ROOT / "src" / "class.h").read_text(encoding="utf-8")
CLASS_C = (ROOT / "src" / "class.c").read_text(encoding="utf-8")
STRUCTS_H = (ROOT / "src" / "structs.h").read_text(encoding="utf-8")
SPELLS_H = (ROOT / "src" / "spells.h").read_text(encoding="utf-8")

EXPECTED = {
    "CLASS_MAGIC_USER": ("CLASS_STAT_INT", "CLASS_STAT_WIS", "CLASS_STAT_DEX"),
    "CLASS_CLERIC": ("CLASS_STAT_WIS", "CLASS_STAT_CON", "CLASS_STAT_STR"),
    "CLASS_THIEF": ("CLASS_STAT_DEX", "CLASS_STAT_INT", "CLASS_STAT_STR"),
    "CLASS_WARRIOR": ("CLASS_STAT_STR", "CLASS_STAT_CON", "CLASS_STAT_DEX"),
    "CLASS_PALADIN": ("CLASS_STAT_STR", "CLASS_STAT_WIS", "CLASS_STAT_CON"),
    "CLASS_BARD": ("CLASS_STAT_CHA", "CLASS_STAT_DEX", "CLASS_STAT_INT"),
    "CLASS_WARLOCK": ("CLASS_STAT_INT", "CLASS_STAT_CHA", "CLASS_STAT_WIS"),
    "CLASS_DRUID": ("CLASS_STAT_WIS", "CLASS_STAT_CON", "CLASS_STAT_INT"),
    "CLASS_MYSTIC": ("CLASS_STAT_WIS", "CLASS_STAT_DEX", "CLASS_STAT_CON"),
}

def class_block(symbol: str) -> str:
    start = CLASS_C.index(f"[{symbol}] = {{")
    end = CLASS_C.index("\n  },", start)
    return CLASS_C[start:end]

def test_profile_fields_exist():
    for text in (
        "int primary_stat;",
        "int secondary_stat;",
        "int tertiary_stat;",
        "int get_class_primary_stat(int class_num);",
        "int get_class_secondary_stat(int class_num);",
        "int get_class_tertiary_stat(int class_num);",
        "int get_class_stat_value(struct char_data *ch, int stat);",
    ):
        assert text in CLASS_H, text

def test_all_nine_class_profiles():
    for symbol, (primary, secondary, tertiary) in EXPECTED.items():
        block = class_block(symbol)
        assert f".primary_stat = {primary}," in block, symbol
        assert f".secondary_stat = {secondary}," in block, symbol
        assert f".tertiary_stat = {tertiary}," in block, symbol

def test_stat_value_helper_covers_all_stats():
    for macro in ("GET_STR", "GET_DEX", "GET_CON", "GET_INT", "GET_WIS", "GET_CHA"):
        assert macro in CLASS_C, macro

def test_profile_phase_preserves_existing_ability_boundaries():
    # Later progression phases may increase MAX_SKILLS. This regression should
    # protect the established spell boundary and legacy skill IDs rather than
    # permanently freezing the whole project at the old capacity.
    max_skills_match = re.search(r"^#define\s+MAX_SKILLS\s+(\d+)\b", STRUCTS_H, re.M)
    assert max_skills_match
    assert int(max_skills_match.group(1)) >= 269
    assert "#define MAX_SPELLS" in SPELLS_H and "230" in SPELLS_H
    assert re.search(r"^#define\s+SKILL_PICKPOCKET\s+269\b", SPELLS_H, re.M)

def run():
    tests = [
        test_profile_fields_exist,
        test_all_nine_class_profiles,
        test_stat_value_helper_covers_all_stats,
        test_profile_phase_preserves_existing_ability_boundaries,
    ]
    for test in tests:
        test()
    print(f"{len(tests)} tests passed: class stat profile regression")

if __name__ == "__main__":
    run()