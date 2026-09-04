"""Deterministic contracts for natural proficiency advancement."""
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
UTILS = (ROOT / "src/utils.c").read_text(encoding="utf-8")
FIGHT = (ROOT / "src/fight.c").read_text(encoding="utf-8")
SPELLS = (ROOT / "src/spell_parser.c").read_text(encoding="utf-8")
PRACTICE = (ROOT / "src/spec_procs.c").read_text(encoding="utf-8")


def section(text, start, end):
    pos = text.index(start)
    return text[pos:text.index(end, pos)]


IMPROVE = section(UTILS, "void improve_ability_from_use", "int MIN(")
HIT = section(FIGHT, "void hit(", "void perform_violence")
CAST = section(SPELLS, "ACMD(do_cast)", "void spell_level")


def base_chance(proficiency):
    if proficiency <= 24:
        return 20
    if proficiency <= 49:
        return 12
    if proficiency <= 74:
        return 7
    if proficiency <= 89:
        return 4
    if proficiency <= 94:
        return 2
    return 1


def test_all_curve_boundaries():
    expected = {1: 20, 24: 20, 25: 12, 49: 12, 50: 7, 74: 7,
                75: 4, 89: 4, 90: 2, 94: 2, 95: 1, 99: 1}
    assert {value: base_chance(value) for value in expected} == expected
    for boundary in (24, 49, 74, 89, 94):
        assert f"cur <= {boundary}" in IMPROVE
    assert "cur < 75" not in IMPROVE


def test_known_gate_cap_stat_modifier_and_failure_multiplier():
    assert "if (cur <= 0)" in IMPROVE
    assert "if (cur >= 100)" in IMPROVE
    assert "MAX(-3, MIN(3, (stat - 13) / 4))" in IMPROVE
    assert "(chance * 3 + 1) / 2" in IMPROVE
    assert "chance *= 4" not in IMPROVE
    assert "MIN(100, cur + 1)" in IMPROVE


def test_class_tracking_precedes_cap_and_random_roll():
    tracking = IMPROVE.index("classtrack_record_ability_use")
    cap = IMPROVE.index("if (cur >= 100)")
    roll = IMPROVE.index("rand_number(1, 100)")
    assert tracking < cap < roll
    assert IMPROVE.index("if (cur <= 0)") < tracking


def test_practice_cap_is_unchanged():
    practice = section(PRACTICE, "SPECIAL(guild)", "SPECIAL(dump)")
    assert "if (GET_SKILL(ch, skill_num) > PRACTICE_CAP)" in practice
    assert "SET_SKILL(ch, skill_num, PRACTICE_CAP);" in practice


def test_unarmed_hit_and_miss_share_one_genuine_attempt_hook():
    roll = HIT.index("dam = (!AWAKE(victim) || final_hit_roll <= hit_chance);")
    hook = HIT.index("improve_ability_from_use(ch, SKILL_UNARMED, dam);")
    miss = HIT.index("if (!dam)")
    assert roll < hook < miss
    assert "!IS_NPC(ch)" in HIT[roll:hook]
    assert "GET_OBJ_TYPE(wielded) != ITEM_WEAPON" in HIT[roll:hook]
    assert HIT.count("improve_ability_from_use(ch, SKILL_UNARMED") == 1


def test_cast_learning_occurs_only_after_validation_and_mana_check():
    mana_rejection = CAST.index("You haven't the energy to cast that spell!")
    first_learning = CAST.index("improve_ability_from_use(ch, spellnum")
    invalid_target = CAST.index("Cannot find the target of your spell!")
    assert invalid_target < mana_rejection < first_learning
    assert "improve_ability_from_use" not in CAST[:mana_rejection]
    assert "improve_ability_from_use(ch, spellnum, FALSE);" in CAST
    assert "improve_ability_from_use(ch, spellnum, 0);" in CAST
    assert "improve_ability_from_use(ch, spellnum, 1);" in CAST


if __name__ == "__main__":
    tests = [value for name, value in globals().items() if name.startswith("test_")]
    for test in tests:
        test()
    print(f"{len(tests)} proficiency advancement regression tests passed")
