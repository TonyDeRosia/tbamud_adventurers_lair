from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[1]

SPELLS = (ROOT / "src" / "spells.h").read_text(encoding="utf-8")
STRUCTS = (ROOT / "src" / "structs.h").read_text(encoding="utf-8")
PLAYERS = (ROOT / "src" / "players.c").read_text(encoding="utf-8")
PLRTOASCII = (ROOT / "src" / "util" / "plrtoascii.c").read_text(encoding="utf-8")
PARSER = (ROOT / "src" / "spell_parser.c").read_text(encoding="utf-8")

def define_value(text: str, name: str) -> int:
    m = re.search(rf"^#define\s+{re.escape(name)}\s+(\d+)\b", text, re.M)
    assert m, name
    return int(m.group(1))

def test_extended_player_capacity_reaches_last_safe_id():
    assert define_value(STRUCTS, "MAX_SKILLS") == 297
    assert define_value(SPELLS, "SPELL_DG_AFFECT") == 298
    assert define_value(SPELLS, "TOP_SPELL_DEFINE") == 299

def test_reserved_progression_ids_are_stable():
    expected = {
        "SKILL_DOUBLE_CAST": 273,
        "SKILL_TRIPLE_CAST": 274,
        "SKILL_FOURTH_CAST": 275,
        "SPELL_HASTE": 276,
    }
    for name, value in expected.items():
        assert define_value(SPELLS, name) == value

    ids = [define_value(SPELLS, name) for name in expected]
    assert len(ids) == len(set(ids))
    assert min(ids) > define_value(SPELLS, "SKILL_FOURTH_ATTACK")
    assert max(ids) < define_value(SPELLS, "SPELL_DG_AFFECT")

def test_legacy_spell_boundary_is_not_moved():
    assert define_value(SPELLS, "NUM_SPELLS") == 230
    assert define_value(SPELLS, "MAX_SPELLS") == 230

def test_legacy_physical_progression_ids_are_unchanged():
    assert define_value(SPELLS, "SKILL_DOUBLE_ATTACK") == 270
    assert define_value(SPELLS, "SKILL_TRIPLE_ATTACK") == 271
    assert define_value(SPELLS, "SKILL_FOURTH_ATTACK") == 272

def test_player_arrays_expand_automatically_with_max_skills():
    for marker in (
        "byte skills[MAX_SKILLS+1]",
        "byte tome_abilities[MAX_SKILLS+1]",
        "byte study_learned_level[MAX_SKILLS+1]",
        "int spell_cooldowns[MAX_SKILLS + 1]",
    ):
        assert marker in STRUCTS, marker

def test_normal_ascii_player_persistence_tracks_max_skills():
    assert PLAYERS.count("for (i = 1; i <= MAX_SKILLS; i++)") >= 4
    assert "if (num < 1 || num > MAX_SKILLS)" in PLAYERS
    assert 'fprintf(fl, "Skil:\\n");' in PLAYERS

def test_legacy_binary_converter_keeps_independent_bound():
    # plrtoascii converts the old binary character format. Its historical
    # skill-array width must remain independent from modern player capacity.
    assert "PLRTOASCII_LEGACY_MAX_SKILLS" in PLRTOASCII
    assert "byte skills[PLRTOASCII_LEGACY_MAX_SKILLS + 1]" in PLRTOASCII

def test_reserved_multicast_passives_are_registered_but_haste_is_not():
    assert 'skillo_cost(SKILL_DOUBLE_CAST, "double cast", 0);' in PARSER
    assert 'skillo_cost(SKILL_TRIPLE_CAST, "triple cast", 0);' in PARSER
    assert 'skillo_cost(SKILL_FOURTH_CAST, "fourth cast", 0);' in PARSER
    assert "spello(SPELL_HASTE" not in PARSER

def test_extended_band_documentation_matches_kind_model():
    assert "EXTENDED PLAYER ABILITY IDS" in SPELLS
    assert "Explicit ability-kind metadata is authoritative" in SPELLS
    assert "IDs 277-297 remain available for future player abilities." in SPELLS

def run():
    tests = [
        test_extended_player_capacity_reaches_last_safe_id,
        test_reserved_progression_ids_are_stable,
        test_legacy_spell_boundary_is_not_moved,
        test_legacy_physical_progression_ids_are_unchanged,
        test_player_arrays_expand_automatically_with_max_skills,
        test_normal_ascii_player_persistence_tracks_max_skills,
        test_legacy_binary_converter_keeps_independent_bound,
        test_reserved_multicast_passives_are_registered_but_haste_is_not,
        test_extended_band_documentation_matches_kind_model,
    ]

    for test in tests:
        test()

    print(f"{len(tests)} tests passed: extended player ability capacity V1")

if __name__ == "__main__":
    run()