from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[1]

CLASS_C = (ROOT / "src" / "class.c").read_text(encoding="utf-8")
FIGHT_C = (ROOT / "src" / "fight.c").read_text(encoding="utf-8")
PROG_C = (ROOT / "src" / "combat_progression.c").read_text(encoding="utf-8")
PROG_H = (ROOT / "src" / "combat_progression.h").read_text(encoding="utf-8")
SPELLS_H = (ROOT / "src" / "spells.h").read_text(encoding="utf-8")
STRUCTS_H = (ROOT / "src" / "structs.h").read_text(encoding="utf-8")

EXPECTED_ACCESS = {
    "CLASS_WARRIOR": 10,
    "CLASS_THIEF": 15,
    "CLASS_PALADIN": 20,
    "CLASS_BARD": 25,
    "CLASS_MYSTIC": 30,
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

def test_reserved_ids_unchanged():
    assert re.search(r"^#define\s+MAX_SKILLS\s+272\b", STRUCTS_H, re.M)
    assert re.search(r"^#define\s+SKILL_DOUBLE_ATTACK\s+270\b", SPELLS_H, re.M)
    assert re.search(r"^#define\s+SKILL_TRIPLE_ATTACK\s+271\b", SPELLS_H, re.M)
    assert re.search(r"^#define\s+SKILL_FOURTH_ATTACK\s+272\b", SPELLS_H, re.M)

def test_double_attack_class_access():
    for cls, level in EXPECTED_ACCESS.items():
        marker = f"spell_level(SKILL_DOUBLE_ATTACK, {cls}, {level});"
        assert CLASS_C.count(marker) == 1, marker

    for cls in NO_NORMAL_ACCESS:
        assert f"spell_level(SKILL_DOUBLE_ATTACK, {cls}," not in CLASS_C

    # Later progression stages are tested in their own rollout regression.

def test_physical_profiles_are_class_specific():
    block = section(
        PROG_C,
        "static void combat_progression_physical_multiattack_stats",
        "int combat_progression_physical_multiattack_chance_basis_points",
    )

    required = (
        "case CLASS_WARRIOR:",
        "*primary_stat = CLASS_STAT_STR;",
        "*secondary_stat = CLASS_STAT_DEX;",
        "*tertiary_stat = CLASS_STAT_CON;",
        "case CLASS_THIEF:",
        "*primary_stat = CLASS_STAT_DEX;",
        "*secondary_stat = CLASS_STAT_STR;",
        "*tertiary_stat = CLASS_STAT_INT;",
        "case CLASS_PALADIN:",
        "*secondary_stat = CLASS_STAT_CON;",
        "*tertiary_stat = CLASS_STAT_WIS;",
        "case CLASS_BARD:",
        "*secondary_stat = CLASS_STAT_CHA;",
        "case CLASS_MYSTIC:",
        "*secondary_stat = CLASS_STAT_WIS;",
    )
    for marker in required:
        assert marker in block, marker

    # Rare cross-class/tome access has an explicit physical fallback.
    assert block.count("*primary_stat = CLASS_STAT_STR;") >= 2
    assert "combat_progression_physical_multiattack_roll" in PROG_H

def test_npcs_are_excluded_from_player_passive():
    chance_block = section(
        PROG_C,
        "int combat_progression_physical_multiattack_chance_basis_points",
        "bool combat_progression_physical_multiattack_roll",
    )
    assert "if (!ch || IS_NPC(ch))" in chance_block

    double_block = section(
        FIGHT_C,
        "static void do_double_attack",
        "/* dual wield offhand system */",
    )
    assert "if (!ch || IS_NPC(ch))" in double_block

def test_bonus_attack_suppresses_only_hit_owned_round_triggers():
    helper = section(
        FIGHT_C,
        "static void perform_bonus_mainhand_attack",
        "static void do_double_attack",
    )

    assert "previous_effects_due = combat_effects_due;" in helper
    assert "combat_effects_due = FALSE;" in helper
    assert "hit(ch, victim, TYPE_UNDEFINED);" in helper
    assert "combat_effects_due = previous_effects_due;" in helper

    hit_block = section(FIGHT_C, "void hit(", "static void process_round_effects")
    assert re.search(r"if\s*\(combat_effects_due\)\s*\n\s*fight_mtrigger\(ch\);", hit_block)
    assert re.search(r"if\s*\(combat_effects_due\)\s*\n\s*hitprcnt_mtrigger\(victim\);", hit_block)

def test_double_attack_rereads_target_and_uses_normal_hit_path():
    double_block = section(
        FIGHT_C,
        "static void do_double_attack",
        "/* dual wield offhand system */",
    )

    assert "victim = FIGHTING(ch);" in double_block
    assert "physical_multiattack_target_valid(ch, victim)" in double_block

    target_helper = section(
        FIGHT_C,
        "static int physical_multiattack_target_valid",
        "static void do_double_attack",
    )
    assert "IN_ROOM(ch) != NOWHERE" in target_helper
    assert "IN_ROOM(victim) != NOWHERE" in target_helper
    assert "IN_ROOM(ch) == IN_ROOM(victim)" in target_helper
    assert "FIGHTING(ch) == victim" in target_helper

    assert "combat_progression_physical_multiattack_roll(" in double_block
    assert "COMBAT_PROGRESSION_STAGE_FULL" in double_block
    assert "perform_bonus_mainhand_attack(ch);" in double_block

    # No recursive progression from the hit() function itself. Later chain
    # stages are owned by do_double_attack(), never by hit().
    hit_block = section(FIGHT_C, "void hit(", "static void process_round_effects")
    assert "do_double_attack(" not in hit_block

def test_learning_is_success_only_and_throttled_to_effect_pulse():
    double_block = section(
        FIGHT_C,
        "static void do_double_attack",
        "/* dual wield offhand system */",
    )

    assert "if (combat_effects_due)" in double_block
    assert "improve_ability_from_use(ch, SKILL_DOUBLE_ATTACK, TRUE);" in double_block
    assert "improve_ability_from_use(ch, SKILL_DOUBLE_ATTACK, FALSE);" not in FIGHT_C

def test_round_order_preserves_offhand_spirit_and_mob_special_ownership():
    violence = FIGHT_C[FIGHT_C.index("void perform_violence(void)"):]

    base = violence.index("hit(ch, FIGHTING(ch), TYPE_UNDEFINED);")
    double = violence.index("do_double_attack(ch);")
    offhand = violence.index("do_offhand_attack(ch, FIGHTING(ch));")
    spirit = violence.index("do_spirit_procs(ch, FIGHTING(ch));")
    mob_spec = violence.index("GET_MOB_SPEC(ch)")

    assert base < double < offhand < spirit < mob_spec
    assert violence.count("do_double_attack(ch);") == 1

def test_existing_offhand_path_remains_separate():
    assert FIGHT_C.count("static void do_offhand_attack(") == 1
    assert FIGHT_C.count("g_offhand_attack = 1;") >= 2

    offhand = section(
        FIGHT_C,
        "static void do_offhand_attack",
        "/* Deliberate paired attacks",
    )
    assert "hit(ch, victim, TYPE_UNDEFINED);" in offhand
    assert "do_double_attack(" not in offhand

def run():
    tests = [
        test_reserved_ids_unchanged,
        test_double_attack_class_access,
        test_physical_profiles_are_class_specific,
        test_npcs_are_excluded_from_player_passive,
        test_bonus_attack_suppresses_only_hit_owned_round_triggers,
        test_double_attack_rereads_target_and_uses_normal_hit_path,
        test_learning_is_success_only_and_throttled_to_effect_pulse,
        test_round_order_preserves_offhand_spirit_and_mob_special_ownership,
        test_existing_offhand_path_remains_separate,
    ]

    for test in tests:
        test()

    print(f"{len(tests)} tests passed: Double Attack V1 regression")

if __name__ == "__main__":
    run()