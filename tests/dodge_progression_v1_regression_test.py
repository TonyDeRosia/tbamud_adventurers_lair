from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[1]
SPELLS = (ROOT / "src" / "spells.h").read_text(encoding="utf-8")
STRUCTS = (ROOT / "src" / "structs.h").read_text(encoding="utf-8")
PARSER = (ROOT / "src" / "spell_parser.c").read_text(encoding="utf-8")
CLASS = (ROOT / "src" / "class.c").read_text(encoding="utf-8")
FIGHT = (ROOT / "src" / "fight.c").read_text(encoding="utf-8")
FIGHT_H = (ROOT / "src" / "fight.h").read_text(encoding="utf-8")

def define_value(text: str, name: str) -> int:
    match = re.search(rf"^#define\s+{re.escape(name)}\s+(\d+)\b", text, re.M)
    assert match, name
    return int(match.group(1))

def c_function_block(text: str, signature: str) -> str:
    search_from = 0

    while True:
        start = text.find(signature, search_from)
        if start < 0:
            raise AssertionError(f"function definition not found: {signature}")

        open_brace = text.find("{", start)
        semicolon = text.find(";", start)

        if open_brace >= 0 and (semicolon < 0 or open_brace < semicolon):
            depth = 0
            for i in range(open_brace, len(text)):
                if text[i] == "{":
                    depth += 1
                elif text[i] == "}":
                    depth -= 1
                    if depth == 0:
                        return text[start:i + 1]
            raise AssertionError(f"unterminated C function: {signature}")

        search_from = start + len(signature)

def test_dodge_uses_first_free_extended_player_id():
    assert define_value(SPELLS, "SKILL_DODGE") == 277
    assert define_value(STRUCTS, "MAX_SKILLS") == 297
    assert define_value(SPELLS, "SPELL_DG_AFFECT") == 298
    assert define_value(SPELLS, "TOP_SPELL_DEFINE") == 299

def test_dodge_is_registered_as_skill_kind():
    assert 'skillo_cost(SKILL_DODGE, "dodge", 0);' in PARSER
    assert "spello(SKILL_DODGE" not in PARSER

def test_normal_class_access_favors_agile_martials():
    expected = {
        "CLASS_THIEF": 10,
        "CLASS_BARD": 15,
        "CLASS_MYSTIC": 20,
        "CLASS_WARRIOR": 25,
        "CLASS_PALADIN": 30,
    }

    for cls, level in expected.items():
        assert f"spell_level(SKILL_DODGE, {cls}, {level});" in CLASS

    for cls in (
        "CLASS_MAGIC_USER",
        "CLASS_CLERIC",
        "CLASS_WARLOCK",
        "CLASS_DRUID",
    ):
        assert not re.search(
            rf"spell_level\(SKILL_DODGE,\s*{cls},",
            CLASS,
        )

def test_dodge_uses_reduced_shared_progression_curve():
    assert define_value(FIGHT_H, "DODGE_PROGRESSION_STAGE_PERCENT") == 25

    chance = c_function_block(FIGHT, "static int dodge_chance_basis_points")
    assert "GET_SKILL(victim, SKILL_DODGE)" in chance
    assert "combat_progression_chance_basis_points(" in chance
    assert "DODGE_PROGRESSION_STAGE_PERCENT" in chance

def test_dodge_is_dex_primary_but_class_sensitive():
    stats = c_function_block(FIGHT, "static void dodge_progression_stats")

    assert "*primary_stat = CLASS_STAT_DEX;" in stats
    assert "case CLASS_THIEF:" in stats
    assert "case CLASS_BARD:" in stats
    assert "case CLASS_MYSTIC:" in stats
    assert "case CLASS_WARRIOR:" in stats
    assert "case CLASS_PALADIN:" in stats
    assert "CLASS_STAT_CHA" in stats
    assert "CLASS_STAT_WIS" in stats
    assert "CLASS_STAT_STR" in stats
    assert "CLASS_STAT_INT" in stats
    assert "CLASS_STAT_CON" in stats

def test_runtime_is_pc_physical_and_mobility_gated():
    block = c_function_block(FIGHT, "static bool try_dodge_attack")

    assert "IS_NPC(victim)" in block
    assert "!IS_WEAPON(attacktype)" in block
    assert "!AWAKE(victim)" in block
    assert "GET_POS(victim) < POS_FIGHTING" in block
    assert "AFF_ROOTED" in block
    assert "AFF_STUNNED" in block

def test_runtime_uses_actual_proficiency_not_class_gate():
    block = c_function_block(FIGHT, "static bool try_dodge_attack")
    chance = c_function_block(FIGHT, "static int dodge_chance_basis_points")

    assert "GET_SKILL(victim, SKILL_DODGE)" in chance
    assert "GET_CLASS(victim)" in chance

    # No runtime class whitelist: rare tome/cross-class knowledge can function.
    assert "CLASS_THIEF" not in block
    assert "CLASS_WARRIOR" not in block
    assert "CLASS_PALADIN" not in block

def test_successful_proc_negates_damage_and_trains_only_on_effects_pulse():
    helper = c_function_block(FIGHT, "static bool try_dodge_attack")

    assert 'You dodge $n\'s attack!' in helper
    assert '$N dodges your attack!' in helper
    assert "if (combat_effects_due)" in helper
    assert "improve_ability_from_use(victim, SKILL_DODGE, TRUE);" in helper

    hook = "if (dam > 0 && try_dodge_attack(ch, victim, attacktype))\n    dam = 0;"
    assert hook in FIGHT

    # Failed automatic rolls do not train.
    fail_region = helper[
        helper.index("chance_basis_points = dodge_chance_basis_points"):
        helper.index('act("\\tCYou dodge')
    ]
    assert "improve_ability_from_use" not in fail_region

def test_existing_defense_order_is_preserved_with_dodge_inserted():
    phase = FIGHT.index("AFF_FLAGGED(victim, AFF_PHASE)")
    mirror = FIGHT.index("AFF_FLAGGED(victim, AFF_MIRROR_IMAGE)")
    dodge = FIGHT.index("if (dam > 0 && try_dodge_attack(ch, victim, attacktype))")
    monarch = FIGHT.index("GET_SKILL(victim, SKILL_MONARCH_REFLEXES)")

    assert phase < mirror < dodge < monarch

def test_dodge_does_not_replace_base_evasion_model():
    chance = c_function_block(FIGHT, "static int dodge_chance_basis_points")
    helper = c_function_block(FIGHT, "static bool try_dodge_attack")

    assert "GET_EVASION" not in chance
    assert "GET_EVASION" not in helper
    assert "compute_defensive_evasion_value" in FIGHT
    assert "compute_hit_chance_from_values" in FIGHT

def test_existing_magic_and_signature_defenses_remain_unchanged():
    assert "AFF_FLAGGED(victim, AFF_PHASE) && rand_number(1, 100) <= 30" in FIGHT
    assert "AFF_FLAGGED(victim, AFF_MIRROR_IMAGE) && rand_number(1, 100) <= 20" in FIGHT
    assert "GET_SKILL(victim, SKILL_MONARCH_REFLEXES) > 0" in FIGHT
    assert "rand_number(1, 100) <= 10" in FIGHT

def test_dodge_does_not_apply_to_spells_or_direct_skill_damage():
    helper = c_function_block(FIGHT, "static bool try_dodge_attack")

    # IS_WEAPON is the explicit V1 scope. Direct SKILL_* damage and spells
    # therefore remain outside Dodge and available for later defense systems.
    assert "!IS_WEAPON(attacktype)" in helper
    assert "ability_is_spell" not in helper
    assert "ability_is_skill" not in helper

def run():
    tests = [
        test_dodge_uses_first_free_extended_player_id,
        test_dodge_is_registered_as_skill_kind,
        test_normal_class_access_favors_agile_martials,
        test_dodge_uses_reduced_shared_progression_curve,
        test_dodge_is_dex_primary_but_class_sensitive,
        test_runtime_is_pc_physical_and_mobility_gated,
        test_runtime_uses_actual_proficiency_not_class_gate,
        test_successful_proc_negates_damage_and_trains_only_on_effects_pulse,
        test_existing_defense_order_is_preserved_with_dodge_inserted,
        test_dodge_does_not_replace_base_evasion_model,
        test_existing_magic_and_signature_defenses_remain_unchanged,
        test_dodge_does_not_apply_to_spells_or_direct_skill_damage,
    ]

    for test in tests:
        test()

    print(f"{len(tests)} tests passed: Dodge Progression V1 regression")

if __name__ == "__main__":
    run()