from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
H = (ROOT / "src" / "combat_progression.h").read_text(encoding="utf-8")
C = (ROOT / "src" / "combat_progression.c").read_text(encoding="utf-8")
STRUCTS = (ROOT / "src" / "structs.h").read_text(encoding="utf-8")
SPELLS = (ROOT / "src" / "spells.h").read_text(encoding="utf-8")

def stat_rating(stat):
    stat = max(0, min(30, stat))
    if stat <= 10:
        return 0
    if stat <= 15:
        return (stat - 10) * 70
    if stat <= 20:
        return 350 + (stat - 15) * 60
    if stat <= 25:
        return 650 + (stat - 20) * 45
    return 875 + (stat - 25) * 25

def weighted(primary, secondary, tertiary):
    return (
        stat_rating(primary) * 60
        + stat_rating(secondary) * 25
        + stat_rating(tertiary) * 15
        + 50
    ) // 100

def chance(prof, primary, secondary, tertiary, stage=100):
    if prof <= 0 or stage <= 0:
        return 0
    prof = max(1, min(100, prof))
    stage = max(1, min(100, stage))
    training = 500 + 75 * prof
    rating = weighted(primary, secondary, tertiary)
    multiplier = 750 + (450 * rating + 500) // 1000
    bp = (training * multiplier + 500) // 1000
    bp = (bp * stage + 50) // 100
    return max(500, min(9500, bp))

def test_stat_breakpoints():
    assert stat_rating(10) == 0
    assert stat_rating(15) == 350
    assert stat_rating(20) == 650
    assert stat_rating(25) == 875
    assert stat_rating(30) == 1000
    assert stat_rating(99) == 1000

def test_proficiency_and_stats_both_matter():
    assert chance(1, 30, 30, 30) == 690       # 6.90%
    assert chance(25, 20, 20, 20) == 2477     # 24.77%
    assert chance(50, 20, 20, 20) == 4433     # 44.33%
    assert chance(75, 20, 20, 20) == 6388     # 63.88%
    assert chance(100, 20, 20, 20) == 8344    # 83.44%
    assert chance(100, 30, 30, 30) == 9500    # capped 95.00%

def test_class_priority_weighting_rewards_primary_stat():
    # Primary-stat investment matters more than the same tertiary investment.
    primary_build = chance(75, 30, 20, 20)
    tertiary_build = chance(75, 20, 20, 30)
    assert primary_build == 6964
    assert tertiary_build == 6529
    assert primary_build > tertiary_build

def test_chain_stage_math():
    # Representative optimized class-style profile: primary 30, secondary 25, tertiary 20.
    assert chance(100, 30, 25, 20, 100) == 9296
    assert chance(100, 30, 25, 20, 70) == 6507
    assert chance(100, 30, 25, 20, 50) == 4648

def test_unknown_ability_never_procs():
    assert chance(0, 30, 30, 30) == 0

def test_source_contract():
    for marker in (
        "COMBAT_PROGRESSION_PRIMARY_WEIGHT   60",
        "COMBAT_PROGRESSION_SECONDARY_WEIGHT 25",
        "COMBAT_PROGRESSION_TERTIARY_WEIGHT  15",
        "COMBAT_PROGRESSION_STAGE_FULL    100",
        "COMBAT_PROGRESSION_STAGE_SECOND   70",
        "COMBAT_PROGRESSION_STAGE_THIRD    50",
        "combat_progression_class_chance_basis_points",
        "combat_progression_class_roll",
    ):
        assert marker in H, marker

    for marker in (
        "training_basis_points = 500 + (75 * proficiency);",
        "750 + ((450 * weighted_rating + 500) / 1000);",
        "rand_number(1, COMBAT_PROGRESSION_CHANCE_SCALE)",
    ):
        assert marker in C, marker

def test_no_gameplay_or_ability_capacity_change_in_math_phase():
    assert "#define MAX_SKILLS            269" in STRUCTS
    assert "#define MAX_SPELLS" in SPELLS and "230" in SPELLS
    assert "#define SKILL_PICKPOCKET             269" in SPELLS
    assert "perform_violence" not in C
    assert "hit(ch" not in C
    assert "call_magic" not in C

def run():
    tests = [
        test_stat_breakpoints,
        test_proficiency_and_stats_both_matter,
        test_class_priority_weighting_rewards_primary_stat,
        test_chain_stage_math,
        test_unknown_ability_never_procs,
        test_source_contract,
        test_no_gameplay_or_ability_capacity_change_in_math_phase,
    ]
    for test in tests:
        test()
    print(f"{len(tests)} tests passed: combat progression math V1")

if __name__ == "__main__":
    run()