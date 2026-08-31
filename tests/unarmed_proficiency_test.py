"""Focused contracts and balance checks for universal Unarmed proficiency."""
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
SPELLS_H = (ROOT / "src/spells.h").read_text(encoding="utf-8")
STRUCTS_H = (ROOT / "src/structs.h").read_text(encoding="utf-8")
PARSER = (ROOT / "src/spell_parser.c").read_text(encoding="utf-8")
CLASS = (ROOT / "src/class.c").read_text(encoding="utf-8")
FIGHT = (ROOT / "src/fight.c").read_text(encoding="utf-8")
SCORE = (ROOT / "src/act.informative.c").read_text(encoding="utf-8")
PLAYERS = (ROOT / "src/players.c").read_text(encoding="utf-8")
PRACTICE = (ROOT / "src/spec_procs.c").read_text(encoding="utf-8")
UTILS = (ROOT / "src/utils.c").read_text(encoding="utf-8")

LEVELS = (1, 5, 10, 20, 30, 50, 75, 100)
SKILLS = (0, 25, 50, 75, 100)


def profile(level: int) -> tuple[int, int, int]:
    return min(4, 1 + level // 30), min(7, 2 + level // 20), max(0, level // 30)


def expected_unarmed(level: int, skill: int) -> tuple[float, float]:
    num, size, level_bonus = profile(level)
    base = num * (size + 1) / 2 + level_bonus
    return base, base * (1 + skill / 800)


def test_persistent_id_is_appended_without_renumbering_existing_skills():
    assert "#define SKILL_STUDY                 260" in SPELLS_H
    assert "#define SKILL_UNARMED               261" in SPELLS_H
    assert "#define MAX_SKILLS            261" in STRUCTS_H
    assert 'skillo_cost(SKILL_UNARMED, "unarmed", 5);' in PARSER


def test_every_registered_player_class_gets_level_one_access():
    assignment = (
        "for (i = 0; i < num_pc_classes(); i++)\n"
        "    spell_level(SKILL_UNARMED, i, 1);"
    )
    assert assignment in CLASS
    assert "const int start_prof = 1;" in CLASS
    assert "ensure_class_abilities(ch);" in PLAYERS


def test_unarmed_uses_normal_practice_then_improve_by_use_progression():
    assert "can_character_practice_ability(ch, skill_num)" in PRACTICE
    assert "spell_info[ability_id].min_level[(int) GET_CLASS(ch)]" in PRACTICE
    assert "SET_SKILL(ch, skill_num, MIN(LEARNED(ch), percent));" in PRACTICE
    assert "if (cur < 75 || cur >= 100)" in UTILS
    assert "SET_SKILL(ch, ability, cur + 1);" in UTILS
    assert "improve_ability_from_use(ch, SKILL_UNARMED, TRUE);" in FIGHT


def test_damage_bonus_is_player_only_and_weapon_gated():
    assert "if (wielded && GET_OBJ_TYPE(wielded) == ITEM_WEAPON)" in FIGHT
    assert "if (IS_NPC(ch))" in FIGHT
    player_branch = FIGHT[FIGHT.index("get_player_unarmed_profile(GET_LEVEL(ch)"):]
    assert "GET_SKILL(ch, SKILL_UNARMED)" in player_branch[:1000]
    assert "unarmed_proficiency_bonus(unarmed_component, unarmed_skill)" in player_branch[:1000]
    assert "improve_ability_from_use(ch, SKILL_UNARMED, TRUE);" in player_branch[:1000]
    assert "dice(ch->mob_specials.damnodice, ch->mob_specials.damsizedice)" in FIGHT


def test_score_and_debug_use_shared_unarmed_calculations():
    assert "unarmed_expected_average_x100(GET_LEVEL(ch), unarmed_skill)" in SCORE
    assert "get_player_unarmed_profile(GET_LEVEL(ch)" in SCORE
    assert "Unarmed Proficiency:" in FIGHT
    assert "Unarmed Skill Bonus:" in FIGHT


def test_balance_matrix_is_monotonic_and_capped_at_twelve_point_five_percent():
    for level in LEVELS:
        values = [expected_unarmed(level, skill)[1] for skill in SKILLS]
        assert values == sorted(values)
        base = values[0]
        assert abs(values[-1] / base - 1.125) < 1e-9


if __name__ == "__main__":
    tests = [value for name, value in globals().items() if name.startswith("test_")]
    for test in tests:
        test()
    print(f"{len(tests)} Unarmed proficiency regression tests passed")
    print("level dice level+ avg@0 avg@25 avg@50 avg@75 avg@100")
    for level in LEVELS:
        num, size, level_bonus = profile(level)
        values = [expected_unarmed(level, skill)[1] for skill in SKILLS]
        print(
            f"{level:>5} {num}d{size:<2} {level_bonus:>6} "
            + " ".join(f"{value:>7.2f}" for value in values)
        )
