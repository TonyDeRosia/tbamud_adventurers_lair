from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[1]

SPELLS = (ROOT / "src" / "spells.h").read_text(encoding="utf-8")
PARSER = (ROOT / "src" / "spell_parser.c").read_text(encoding="utf-8")
PROG_H = (ROOT / "src" / "combat_progression.h").read_text(encoding="utf-8")
PROG_C = (ROOT / "src" / "combat_progression.c").read_text(encoding="utf-8")
CLASS_C = (ROOT / "src" / "class.c").read_text(encoding="utf-8")
MAGIC_C = (ROOT / "src" / "magic.c").read_text(encoding="utf-8")

def define_value(text: str, name: str) -> int:
    m = re.search(rf"^#define\s+{re.escape(name)}\s+(\d+)\b", text, re.M)
    assert m, name
    return int(m.group(1))

def test_reserved_ids_are_still_stable():
    assert define_value(SPELLS, "SKILL_DOUBLE_CAST") == 273
    assert define_value(SPELLS, "SKILL_TRIPLE_CAST") == 274
    assert define_value(SPELLS, "SKILL_FOURTH_CAST") == 275
    assert define_value(SPELLS, "SPELL_HASTE") == 276

def test_multicast_passives_register_as_skill_kind():
    for marker in (
        'skillo_cost(SKILL_DOUBLE_CAST, "double cast", 0);',
        'skillo_cost(SKILL_TRIPLE_CAST, "triple cast", 0);',
        'skillo_cost(SKILL_FOURTH_CAST, "fourth cast", 0);',
    ):
        assert marker in PARSER

    # skillo_cost is explicitly backed by ABILITY_KIND_SKILL.
    assert "#define skillo_cost(skill, name, cost) abilityo(skill, ABILITY_KIND_SKILL" in PARSER

def test_haste_remains_reserved_only():
    assert "spello(SPELL_HASTE" not in PARSER
    assert "case SPELL_HASTE" not in MAGIC_C

def test_multicast_chain_stage_contract():
    assert define_value(PROG_H, "COMBAT_PROGRESSION_MULTICAST_STAGE_DOUBLE") == 100
    assert define_value(PROG_H, "COMBAT_PROGRESSION_MULTICAST_STAGE_TRIPLE") == 60
    assert define_value(PROG_H, "COMBAT_PROGRESSION_MULTICAST_STAGE_FOURTH") == 35

def test_multicast_damage_weight_contract():
    assert define_value(PROG_H, "COMBAT_PROGRESSION_MULTICAST_DAMAGE_SECOND") == 80
    assert define_value(PROG_H, "COMBAT_PROGRESSION_MULTICAST_DAMAGE_THIRD") == 65
    assert define_value(PROG_H, "COMBAT_PROGRESSION_MULTICAST_DAMAGE_FOURTH") == 50

def test_effective_proficiency_combines_passive_and_spell_mastery():
    assert "combat_progression_effective_multicast_proficiency(" in PROG_H
    assert "passive_proficiency * spell_proficiency + 50" in PROG_C
    assert "/ 100;" in PROG_C
    assert "passive_proficiency <= 0 || spell_proficiency <= 0" in PROG_C

def test_multicast_uses_existing_class_stat_priority_profile():
    assert "combat_progression_class_chance_basis_points(" in PROG_C
    assert "effective_proficiency, stage_percent" in PROG_C
    assert "combat_progression_multicast_chance_basis_points(" in PROG_H

def test_multicast_excludes_npcs_in_shared_helper():
    m = re.search(
        r"int combat_progression_multicast_chance_basis_points\(.*?\n\}",
        PROG_C,
        re.S,
    )
    assert m
    body = m.group(0)
    assert "IS_NPC(ch)" in body
    assert "return 0;" in body

def test_multicast_roll_uses_standard_basis_point_scale():
    m = re.search(
        r"bool combat_progression_multicast_roll\(.*?\n\}",
        PROG_C,
        re.S,
    )
    assert m
    body = m.group(0)
    assert "COMBAT_PROGRESSION_CHANCE_SCALE" in body
    assert "rand_number(1, COMBAT_PROGRESSION_CHANCE_SCALE)" in body

def test_foundation_does_not_assign_class_access_or_damage_packets_yet():
    for ability in (
        "SKILL_DOUBLE_CAST",
        "SKILL_TRIPLE_CAST",
        "SKILL_FOURTH_CAST",
    ):
        assert f"spell_level({ability}," not in CLASS_C

    # This phase must not touch actual spell-damage execution.
    assert "combat_progression_multicast_roll(" not in MAGIC_C
    assert "COMBAT_PROGRESSION_MULTICAST_DAMAGE_SECOND" not in MAGIC_C

def run():
    tests = [
        test_reserved_ids_are_still_stable,
        test_multicast_passives_register_as_skill_kind,
        test_haste_remains_reserved_only,
        test_multicast_chain_stage_contract,
        test_multicast_damage_weight_contract,
        test_effective_proficiency_combines_passive_and_spell_mastery,
        test_multicast_uses_existing_class_stat_priority_profile,
        test_multicast_excludes_npcs_in_shared_helper,
        test_multicast_roll_uses_standard_basis_point_scale,
        test_foundation_does_not_assign_class_access_or_damage_packets_yet,
    ]

    for test in tests:
        test()

    print(f"{len(tests)} tests passed: Multicast progression foundation V1")

if __name__ == "__main__":
    run()