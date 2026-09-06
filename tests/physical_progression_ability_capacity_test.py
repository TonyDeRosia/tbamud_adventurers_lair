from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[1]
STRUCTS = (ROOT / "src" / "structs.h").read_text(encoding="utf-8")
SPELLS = (ROOT / "src" / "spells.h").read_text(encoding="utf-8")
PARSER = (ROOT / "src" / "spell_parser.c").read_text(encoding="utf-8")
CLASS_C = (ROOT / "src" / "class.c").read_text(encoding="utf-8")
PLAYERS = (ROOT / "src" / "players.c").read_text(encoding="utf-8")
UTIL_PLR = (ROOT / "src" / "util" / "plrtoascii.c").read_text(encoding="utf-8")

EXPECTED = {
    "SKILL_PICKPOCKET": 269,
    "SKILL_DOUBLE_ATTACK": 270,
    "SKILL_TRIPLE_ATTACK": 271,
    "SKILL_FOURTH_ATTACK": 272,
}

def define_value(text: str, name: str) -> int:
    match = re.search(rf"^#define\s+{re.escape(name)}\s+(\d+)\b", text, re.M)
    assert match, name
    return int(match.group(1))

def test_minimal_capacity_expansion():
    assert define_value(STRUCTS, "MAX_SKILLS") == 297
    for name, value in EXPECTED.items():
        assert define_value(SPELLS, name) == value
    assert define_value(SPELLS, "MAX_SPELLS") == 230
    assert define_value(SPELLS, "SPELL_DG_AFFECT") == 298
    assert define_value(SPELLS, "TOP_SPELL_DEFINE") == 299

def test_player_arrays_follow_max_skills():
    assert "byte skills[MAX_SKILLS+1]" in STRUCTS
    assert "byte tome_abilities[MAX_SKILLS+1]" in STRUCTS
    assert "byte study_learned_level[MAX_SKILLS+1]" in STRUCTS
    assert "int spell_cooldowns[MAX_SKILLS + 1]" in STRUCTS

def test_runtime_save_loops_follow_max_skills():
    # Current player save/load format uses indexed text entries and follows
    # MAX_SKILLS, so the three new indexes are covered without a binary layout
    # migration.
    assert "for (i = 1; i <= MAX_SKILLS; i++)" in PLAYERS
    assert "fprintf(fl, \"%d %d\\n\", i, GET_SKILL(ch, i));" in PLAYERS

def test_legacy_conversion_stays_versioned_separately():
    # The standalone legacy plrtoascii conversion utility has its own historic
    # limit and must not be silently rewritten by this gameplay capacity phase.
    assert "PLRTOASCII_LEGACY_MAX_SKILLS" in UTIL_PLR
    assert "skills[PLRTOASCII_LEGACY_MAX_SKILLS + 1]" in UTIL_PLR

def test_skill_registrations_exist():
    expected = (
        'skillo_cost(SKILL_DOUBLE_ATTACK, "double attack", 0);',
        'skillo_cost(SKILL_TRIPLE_ATTACK, "triple attack", 0);',
        'skillo_cost(SKILL_FOURTH_ATTACK, "fourth attack", 0);',
    )
    for marker in expected:
        assert marker in PARSER, marker

def test_progression_rollout_state():
    expected_access = {
        "SKILL_DOUBLE_ATTACK": (
            ("CLASS_WARRIOR", 10),
            ("CLASS_THIEF", 15),
            ("CLASS_PALADIN", 20),
            ("CLASS_BARD", 25),
            ("CLASS_MYSTIC", 30),
        ),
        "SKILL_TRIPLE_ATTACK": (
            ("CLASS_WARRIOR", 40),
            ("CLASS_THIEF", 45),
            ("CLASS_PALADIN", 50),
            ("CLASS_BARD", 55),
            ("CLASS_MYSTIC", 60),
        ),
        "SKILL_FOURTH_ATTACK": (
            ("CLASS_WARRIOR", 70),
            ("CLASS_THIEF", 75),
            ("CLASS_PALADIN", 80),
            ("CLASS_BARD", 85),
            ("CLASS_MYSTIC", 90),
        ),
    }

    for ability, rows in expected_access.items():
        for cls, level in rows:
            marker = f"spell_level({ability}, {cls}, {level});"
            assert marker in CLASS_C, marker

def test_no_spell_range_reclassification():
    # These remain ordinary skills above MAX_SPELLS and below DG/object IDs.
    assert define_value(SPELLS, "SKILL_DOUBLE_ATTACK") > define_value(SPELLS, "MAX_SPELLS")
    assert define_value(SPELLS, "SKILL_FOURTH_ATTACK") < define_value(SPELLS, "SPELL_DG_AFFECT")

def run():
    tests = [
        test_minimal_capacity_expansion,
        test_player_arrays_follow_max_skills,
        test_runtime_save_loops_follow_max_skills,
        test_legacy_conversion_stays_versioned_separately,
        test_skill_registrations_exist,
        test_progression_rollout_state,
        test_no_spell_range_reclassification,
    ]
    for test in tests:
        test()
    print(f"{len(tests)} tests passed: physical progression ability capacity")

if __name__ == "__main__":
    run()