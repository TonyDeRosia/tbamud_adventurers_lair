from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[1]

CLASS_C = (ROOT / "src" / "class.c").read_text(encoding="utf-8")
FIGHT_C = (ROOT / "src" / "fight.c").read_text(encoding="utf-8")
PROG_H = (ROOT / "src" / "combat_progression.h").read_text(encoding="utf-8")
SPELLS_H = (ROOT / "src" / "spells.h").read_text(encoding="utf-8")

ACCESS = {
    "SKILL_TRIPLE_ATTACK": {
        "CLASS_WARRIOR": 40,
        "CLASS_THIEF": 45,
        "CLASS_PALADIN": 50,
        "CLASS_BARD": 55,
        "CLASS_MYSTIC": 60,
    },
    "SKILL_FOURTH_ATTACK": {
        "CLASS_WARRIOR": 70,
        "CLASS_THIEF": 75,
        "CLASS_PALADIN": 80,
        "CLASS_BARD": 85,
        "CLASS_MYSTIC": 90,
    },
}

NO_NORMAL_ACCESS = (
    "CLASS_MAGIC_USER",
    "CLASS_CLERIC",
    "CLASS_WARLOCK",
    "CLASS_DRUID",
)

def section(text: str, start: str, end: str) -> str:
    a = text.index(start)
    b = text.index(end, a)
    return text[a:b]

def test_ids_remain_stable():
    assert re.search(r"^#define\s+SKILL_DOUBLE_ATTACK\s+270\b", SPELLS_H, re.M)
    assert re.search(r"^#define\s+SKILL_TRIPLE_ATTACK\s+271\b", SPELLS_H, re.M)
    assert re.search(r"^#define\s+SKILL_FOURTH_ATTACK\s+272\b", SPELLS_H, re.M)

def test_class_access_ladder():
    for ability, rows in ACCESS.items():
        for cls, level in rows.items():
            marker = f"spell_level({ability}, {cls}, {level});"
            assert CLASS_C.count(marker) == 1, marker

        for cls in NO_NORMAL_ACCESS:
            assert f"spell_level({ability}, {cls}," not in CLASS_C

def test_stage_constants_are_used():
    chain = section(
        FIGHT_C,
        "static void do_double_attack",
        "/* dual wield offhand system */",
    )

    assert "SKILL_DOUBLE_ATTACK" in chain
    assert "COMBAT_PROGRESSION_STAGE_FULL" in chain
    assert "SKILL_TRIPLE_ATTACK" in chain
    assert "COMBAT_PROGRESSION_STAGE_SECOND" in chain
    assert "SKILL_FOURTH_ATTACK" in chain
    assert "COMBAT_PROGRESSION_STAGE_THIRD" in chain

    assert "#define COMBAT_PROGRESSION_STAGE_FULL    100" in PROG_H
    assert "#define COMBAT_PROGRESSION_STAGE_SECOND   70" in PROG_H
    assert "#define COMBAT_PROGRESSION_STAGE_THIRD    50" in PROG_H

def test_chain_is_strictly_ordered():
    chain = section(
        FIGHT_C,
        "static void do_double_attack",
        "/* dual wield offhand system */",
    )

    double_skill = chain.index("SKILL_DOUBLE_ATTACK")
    double_stage = chain.index("COMBAT_PROGRESSION_STAGE_FULL")
    first_swing = chain.index("perform_bonus_mainhand_attack(ch);")

    triple_skill = chain.index("SKILL_TRIPLE_ATTACK")
    triple_stage = chain.index("COMBAT_PROGRESSION_STAGE_SECOND")
    second_swing = chain.index("perform_bonus_mainhand_attack(ch);", first_swing + 1)

    fourth_skill = chain.index("SKILL_FOURTH_ATTACK")
    fourth_stage = chain.index("COMBAT_PROGRESSION_STAGE_THIRD")
    third_swing = chain.index("perform_bonus_mainhand_attack(ch);", second_swing + 1)

    assert double_skill < double_stage < first_swing
    assert first_swing < triple_skill < triple_stage < second_swing
    assert second_swing < fourth_skill < fourth_stage < third_swing

def test_target_is_revalidated_between_stages():
    chain = section(
        FIGHT_C,
        "static void do_double_attack",
        "/* dual wield offhand system */",
    )

    assert chain.count("victim = FIGHTING(ch);") == 3
    assert chain.count("physical_multiattack_target_valid(ch, victim)") == 3
    assert "FIGHTING(ch) == victim" in FIGHT_C

def test_each_stage_uses_its_own_proficiency():
    chain = section(
        FIGHT_C,
        "static void do_double_attack",
        "/* dual wield offhand system */",
    )

    for ability in (
        "SKILL_DOUBLE_ATTACK",
        "SKILL_TRIPLE_ATTACK",
        "SKILL_FOURTH_ATTACK",
    ):
        assert f"proficiency = GET_SKILL(ch, {ability});" in chain

    assert chain.count("combat_progression_physical_multiattack_roll(") == 3

def test_success_learning_is_stage_specific_and_throttled():
    chain = section(
        FIGHT_C,
        "static void do_double_attack",
        "/* dual wield offhand system */",
    )

    for ability in (
        "SKILL_DOUBLE_ATTACK",
        "SKILL_TRIPLE_ATTACK",
        "SKILL_FOURTH_ATTACK",
    ):
        assert (
            f"improve_ability_from_use(ch, {ability}, TRUE);" in chain
        )
        assert (
            f"improve_ability_from_use(ch, {ability}, FALSE);" not in chain
        )

    assert chain.count("if (combat_effects_due)") == 3

def test_safe_bonus_hit_helper_remains_shared():
    helper = section(
        FIGHT_C,
        "static void perform_bonus_mainhand_attack",
        "static int physical_multiattack_target_valid",
    )

    assert "combat_effects_due = FALSE;" in helper
    assert "hit(ch, victim, TYPE_UNDEFINED);" in helper
    assert "combat_effects_due = previous_effects_due;" in helper

    chain = section(
        FIGHT_C,
        "static void do_double_attack",
        "/* dual wield offhand system */",
    )
    assert chain.count("perform_bonus_mainhand_attack(ch);") == 3

def test_npcs_remain_excluded():
    chain = section(
        FIGHT_C,
        "static void do_double_attack",
        "/* dual wield offhand system */",
    )
    assert "if (!ch || IS_NPC(ch))" in chain

def test_round_ownership_and_order_are_preserved():
    violence = FIGHT_C[FIGHT_C.index("void perform_violence(void)"):]

    base = violence.index("hit(ch, FIGHTING(ch), TYPE_UNDEFINED);")
    chain = violence.index("do_double_attack(ch);")
    offhand = violence.index("do_offhand_attack(ch, FIGHTING(ch));")
    spirit = violence.index("do_spirit_procs(ch, FIGHTING(ch));")
    spec = violence.index("GET_MOB_SPEC(ch)")

    assert base < chain < offhand < spirit < spec
    assert violence.count("do_double_attack(ch);") == 1

def test_hit_does_not_recursively_own_progression():
    hit_block = section(
        FIGHT_C,
        "void hit(",
        "static void process_round_effects",
    )
    assert "SKILL_DOUBLE_ATTACK" not in hit_block
    assert "SKILL_TRIPLE_ATTACK" not in hit_block
    assert "SKILL_FOURTH_ATTACK" not in hit_block
    assert "do_double_attack(" not in hit_block

def run():
    tests = [
        test_ids_remain_stable,
        test_class_access_ladder,
        test_stage_constants_are_used,
        test_chain_is_strictly_ordered,
        test_target_is_revalidated_between_stages,
        test_each_stage_uses_its_own_proficiency,
        test_success_learning_is_stage_specific_and_throttled,
        test_safe_bonus_hit_helper_remains_shared,
        test_npcs_remain_excluded,
        test_round_ownership_and_order_are_preserved,
        test_hit_does_not_recursively_own_progression,
    ]

    for test in tests:
        test()

    print(f"{len(tests)} tests passed: Triple/Fourth Attack V1 regression")

if __name__ == "__main__":
    run()