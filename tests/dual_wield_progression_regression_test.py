from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[1]

FIGHT_C = (ROOT / "src" / "fight.c").read_text(encoding="utf-8")
CLASS_C = (ROOT / "src" / "class.c").read_text(encoding="utf-8")
PROG_H = (ROOT / "src" / "combat_progression.h").read_text(encoding="utf-8")
SPELLS_H = (ROOT / "src" / "spells.h").read_text(encoding="utf-8")

def section(text: str, start: str, end: str) -> str:
    a = text.index(start)
    b = text.index(end, a)
    return text[a:b]

def test_dual_wield_identity_and_access_are_preserved():
    assert re.search(r"^#define\s+SKILL_DUAL_WIELD\s+242\b", SPELLS_H, re.M)

    expected = (
        ("CLASS_WARRIOR", 15),
        ("CLASS_THIEF", 20),
        ("CLASS_MYSTIC", 30),
    )
    for cls, level in expected:
        marker = f"spell_level(SKILL_DUAL_WIELD, {cls}, {level});"
        assert marker in CLASS_C, marker

def test_legacy_flat_proc_formula_is_removed():
    assert "static int offhand_attack_chance(int skill)" not in FIGHT_C
    assert "25 + (MAX(0, skill) * 3 / 4)" not in FIGHT_C

    chance = section(
        FIGHT_C,
        "static int offhand_attack_chance_basis_points",
        "static int offhand_damage_percent",
    )
    assert "combat_progression_physical_multiattack_chance_basis_points(" in chance
    assert "COMBAT_PROGRESSION_STAGE_FULL" in chance

def test_offhand_damage_uses_proficiency_and_stat_weighted_chance():
    damage = section(
        FIGHT_C,
        "static int offhand_damage_percent",
        "static int can_offhand_attack",
    )

    assert "skill = MAX(1, MIN(100, skill));" in damage
    assert "chance_basis_points = offhand_attack_chance_basis_points(ch, skill);" in damage
    assert "percent = 40 + (skill / 3) + (chance_basis_points / 400);" in damage
    assert "return MAX(40, MIN(100, percent));" in damage

    # Established shared-math examples.
    assert 40 + (25 // 3) + (2477 // 400) == 54
    assert 40 + (50 // 3) + (4433 // 400) == 67
    assert 40 + (100 // 3) + (9500 // 400) == 96

def test_existing_equipment_restrictions_remain():
    gate = section(
        FIGHT_C,
        "static int can_offhand_attack",
        "static int find_affect_modifier_for_flag",
    )

    required = (
        "if (IS_NPC(ch)) return 0;",
        "if (!prim || !off) return 0;",
        "if (GET_OBJ_TYPE(prim) != ITEM_WEAPON) return 0;",
        "if (GET_OBJ_TYPE(off)  != ITEM_WEAPON) return 0;",
        "if (!GET_SKILL(ch, SKILL_DUAL_WIELD)) return 0;",
        "if (OBJ_FLAGGED(prim, ITEM_TWO_HANDER)) return 0;",
        "if (!OBJ_FLAGGED(off, ITEM_OFFHAND)) return 0;",
        "if (GET_EQ(ch, WEAR_SHIELD)) return 0;",
        "if (GET_OBJ_WEIGHT(off) > GET_OBJ_WEIGHT(prim)) return 0;",
    )
    for marker in required:
        assert marker in gate, marker

def test_auto_offhand_uses_stat_proficiency_roll():
    offhand = section(
        FIGHT_C,
        "static void do_offhand_attack",
        "/* Deliberate paired attacks",
    )

    assert "skill = GET_SKILL(ch, SKILL_DUAL_WIELD);" in offhand
    assert "chance_basis_points = offhand_attack_chance_basis_points(ch, skill);" in offhand
    assert "COMBAT_PROGRESSION_CHANCE_SCALE" in offhand
    assert "rand_number(1, COMBAT_PROGRESSION_CHANCE_SCALE)" in offhand

def test_auto_offhand_revalidates_live_target():
    offhand = section(
        FIGHT_C,
        "static void do_offhand_attack",
        "/* Deliberate paired attacks",
    )
    assert "physical_multiattack_target_valid(ch, victim)" in offhand

def test_success_only_learning_is_throttled():
    offhand = section(
        FIGHT_C,
        "static void do_offhand_attack",
        "/* Deliberate paired attacks",
    )

    assert "if (combat_effects_due)" in offhand
    assert "improve_ability_from_use(ch, SKILL_DUAL_WIELD, TRUE);" in offhand
    assert "improve_ability_from_use(ch, SKILL_DUAL_WIELD, FALSE);" not in FIGHT_C

def test_auto_offhand_suppresses_duplicate_hit_owned_round_triggers():
    offhand = section(
        FIGHT_C,
        "static void do_offhand_attack",
        "/* Deliberate paired attacks",
    )

    assert "previous_effects_due = combat_effects_due;" in offhand
    assert "combat_effects_due = FALSE;" in offhand
    assert "hit(ch, victim, TYPE_UNDEFINED);" in offhand
    assert "combat_effects_due = previous_effects_due;" in offhand

    hit_block = section(FIGHT_C, "void hit(", "static void process_round_effects")
    assert re.search(r"if\s*\(combat_effects_due\)\s*\n\s*fight_mtrigger\(ch\);", hit_block)
    assert re.search(r"if\s*\(combat_effects_due\)\s*\n\s*hitprcnt_mtrigger\(victim\);", hit_block)

def test_offhand_damage_path_uses_character_aware_scaler():
    damage = section(
        FIGHT_C,
        "int damage(",
        "void hit(",
    )
    assert (
        "offhand_damage_percent(ch, GET_SKILL(ch, SKILL_DUAL_WIELD))"
        in damage
    )

def test_mainhand_chain_and_offhand_remain_separate():
    violence = FIGHT_C[FIGHT_C.index("void perform_violence(void)"):]

    base = violence.index("hit(ch, FIGHTING(ch), TYPE_UNDEFINED);")
    main_chain = violence.index("do_double_attack(ch);")
    offhand = violence.index("do_offhand_attack(ch, FIGHTING(ch));")
    spirit = violence.index("do_spirit_procs(ch, FIGHTING(ch));")
    spec = violence.index("GET_MOB_SPEC(ch)")

    assert base < main_chain < offhand < spirit < spec

    offhand_block = section(
        FIGHT_C,
        "static void do_offhand_attack",
        "/* Deliberate paired attacks",
    )
    assert "do_double_attack(" not in offhand_block
    assert "SKILL_TRIPLE_ATTACK" not in offhand_block
    assert "SKILL_FOURTH_ATTACK" not in offhand_block

def test_explicit_dual_skill_attack_remains_deliberate_separate_path():
    explicit = section(
        FIGHT_C,
        "void dual_skill_attack",
        "int damage(",
    )

    assert "hit(ch, victim, type);" in explicit
    assert "g_offhand_attack = 1;" in explicit
    assert "hit(ch, victim, type);" in explicit
    # The automatic chance roll must not be injected into explicit paired skills.
    assert "offhand_attack_chance_basis_points" not in explicit

def run():
    tests = [
        test_dual_wield_identity_and_access_are_preserved,
        test_legacy_flat_proc_formula_is_removed,
        test_offhand_damage_uses_proficiency_and_stat_weighted_chance,
        test_existing_equipment_restrictions_remain,
        test_auto_offhand_uses_stat_proficiency_roll,
        test_auto_offhand_revalidates_live_target,
        test_success_only_learning_is_throttled,
        test_auto_offhand_suppresses_duplicate_hit_owned_round_triggers,
        test_offhand_damage_path_uses_character_aware_scaler,
        test_mainhand_chain_and_offhand_remain_separate,
        test_explicit_dual_skill_attack_remains_deliberate_separate_path,
    ]

    for test in tests:
        test()

    print(f"{len(tests)} tests passed: Dual Wield progression V1 regression")

if __name__ == "__main__":
    run()