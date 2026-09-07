from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[1]

CLASS = (ROOT / "src" / "class.c").read_text(encoding="utf-8")
MAGIC = (ROOT / "src" / "magic.c").read_text(encoding="utf-8")
FIGHT = (ROOT / "src" / "fight.c").read_text(encoding="utf-8")
PARSER = (ROOT / "src" / "spell_parser.c").read_text(encoding="utf-8")
SPELLS = (ROOT / "src" / "spells.h").read_text(encoding="utf-8")
PROG_H = (ROOT / "src" / "combat_progression.h").read_text(encoding="utf-8")

def function_block(text: str, name: str, next_marker: str) -> str:
    start = text.index(name)
    end = text.index(next_marker, start)
    return text[start:end]

def c_function_block(text: str, signature: str) -> str:
    """Return exactly one C function definition using brace matching."""
    start = text.index(signature)
    open_brace = text.index("{", start)
    depth = 0
    for i in range(open_brace, len(text)):
        ch = text[i]
        if ch == "{":
            depth += 1
        elif ch == "}":
            depth -= 1
            if depth == 0:
                return text[start:i + 1]
    raise AssertionError(f"unterminated C function: {signature}")

def test_normal_class_access_ladder():
    expected = {
        "CLASS_MAGIC_USER": (10, 40, 70),
        "CLASS_WARLOCK": (15, 45, 75),
        "CLASS_CLERIC": (20, 50, 80),
        "CLASS_DRUID": (25, 55, 85),
    }
    abilities = (
        "SKILL_DOUBLE_CAST",
        "SKILL_TRIPLE_CAST",
        "SKILL_FOURTH_CAST",
    )

    for cls, levels in expected.items():
        for ability, level in zip(abilities, levels):
            assert f"spell_level({ability}, {cls}, {level});" in CLASS

def test_non_v1_classes_do_not_get_normal_multicast_unlocks():
    for cls in (
        "CLASS_THIEF",
        "CLASS_WARRIOR",
        "CLASS_PALADIN",
        "CLASS_BARD",
        "CLASS_MYSTIC",
    ):
        for ability in (
            "SKILL_DOUBLE_CAST",
            "SKILL_TRIPLE_CAST",
            "SKILL_FOURTH_CAST",
        ):
            assert f"spell_level({ability}, {cls}," not in CLASS

def test_scaled_damage_api_is_public_and_one_shot():
    assert "int mag_damage_scaled(" in SPELLS
    assert "static int next_mag_damage_scale_percent = 100;" in MAGIC

    block = function_block(MAGIC, "int mag_damage_scaled(", "/* Every spell that does damage")
    assert "next_mag_damage_scale_percent = MIN(100, damage_percent);" in block
    assert "result = mag_damage(level, ch, victim, spellnum, savetype);" in block
    assert "next_mag_damage_scale_percent = previous_scale;" in block

def test_mag_damage_consumes_scale_before_nested_damage():
    block = function_block(MAGIC, "int mag_damage(int level", "/* Every spell that does an affect")
    capture = block.index("int damage_scale_percent = next_mag_damage_scale_percent;")
    reset = block.index("next_mag_damage_scale_percent = 100;")
    damage_call = block.rindex("return (damage(ch, victim, dam, spellnum));")
    assert capture < reset < damage_call

def test_scaled_packet_enters_shared_damage_before_spell_crit():
    mag_block = function_block(
        MAGIC,
        "int mag_damage(int level",
        "/* Every spell that does an affect",
    )
    scale = mag_block.index("dam = MAX(1, (dam * damage_scale_percent) / 100);")
    damage_call = mag_block.index("return (damage(ch, victim, dam, spellnum));")
    assert scale < damage_call

    # Spell criticals are intentionally centralized in fight.c::damage(), not
    # mag_damage(). Therefore each Multicast bonus packet is weighted first,
    # then enters the same shared damage path and gets an independent normal
    # spell-critical roll.
    damage_start = FIGHT.index(
        "int damage(struct char_data *ch, struct char_data *victim, int dam, int attacktype)"
    )
    damage_tail = FIGHT[damage_start:]
    trigger = damage_tail.index("damage_mtrigger(ch, victim, dam, attacktype)")
    spell_guard = damage_tail.index("ability_is_spell(attacktype)")
    crit = damage_tail.index("crit_apply_spell(ch, victim, &dam);")
    assert trigger < spell_guard < crit

def test_eligibility_is_pc_direct_single_target_pure_damage_only():
    block = function_block(
        PARSER,
        "static bool multicast_spell_is_eligible",
        "static void perform_player_multicast_damage_chain",
    )
    assert "IS_NPC(ch)" not in block  # target-valid helper owns this check
    assert "multicast_target_valid(ch, victim)" in block
    assert "obj != NULL" in block
    assert "!ability_is_spell(spellnum)" in block
    assert "spell_info[spellnum].routines != MAG_DAMAGE" in block
    assert "TAR_CHAR_ROOM | TAR_FIGHT_VICT" in block

    valid = function_block(
        PARSER,
        "static bool multicast_target_valid",
        "static bool multicast_spell_is_eligible",
    )
    assert "IS_NPC(ch)" in valid
    assert "IN_ROOM(ch) == NOWHERE" in valid
    assert "IN_ROOM(ch) == IN_ROOM(victim)" in valid

def test_strict_double_triple_fourth_chain():
    block = c_function_block(
        PARSER,
        "static void perform_player_multicast_damage_chain",
    )
    double = block.index("SKILL_DOUBLE_CAST")
    triple = block.index("SKILL_TRIPLE_CAST")
    fourth = block.index("SKILL_FOURTH_CAST")
    assert double < triple < fourth

    assert "COMBAT_PROGRESSION_MULTICAST_STAGE_DOUBLE" in block
    assert "COMBAT_PROGRESSION_MULTICAST_STAGE_TRIPLE" in block
    assert "COMBAT_PROGRESSION_MULTICAST_STAGE_FOURTH" in block

def test_bonus_packets_use_reserved_damage_weights():
    block = c_function_block(
        PARSER,
        "static void perform_player_multicast_damage_chain",
    )
    assert "COMBAT_PROGRESSION_MULTICAST_DAMAGE_SECOND" in block
    assert "COMBAT_PROGRESSION_MULTICAST_DAMAGE_THIRD" in block
    assert "COMBAT_PROGRESSION_MULTICAST_DAMAGE_FOURTH" in block
    assert block.count("mag_damage_scaled(") == 3

def test_bonus_packets_do_not_recast_full_spell_pipeline():
    block = c_function_block(
        PARSER,
        "static void perform_player_multicast_damage_chain",
    )
    assert not re.search(r"\bcast_spell\s*\(", block)
    assert not re.search(r"\bcall_magic\s*\(", block)
    assert not re.search(r"\bmag_affects\s*\(", block)
    assert not re.search(r"\bWAIT_STATE\s*\(", block)
    assert "GET_MANA" not in block
    assert "set_spell_cooldown" not in block
    assert len(re.findall(r"\bmag_damage_scaled\s*\(", block)) == 3

def test_only_normal_do_cast_invokes_multicast_chain():
    assert PARSER.count("perform_player_multicast_damage_chain(") == 2
    # One occurrence is the definition, exactly one is the do_cast call.
    do_cast = PARSER[PARSER.index("ACMD(do_cast)"):]
    assert do_cast.count("perform_player_multicast_damage_chain(") == 1

    # buff-all and generic cast_spell paths must not invoke it.
    pre_do_cast = PARSER[:PARSER.index("ACMD(do_cast)")]
    assert pre_do_cast.count("perform_player_multicast_damage_chain(") == 1

def test_primary_target_death_is_guarded_before_bonus_packets():
    do_cast = PARSER[PARSER.index("ACMD(do_cast)"):]
    assert "int cast_result = cast_spell(ch, tch, tobj, spellnum);" in do_cast
    assert "if (cast_result > 0)" in do_cast
    assert "perform_player_multicast_damage_chain(ch, tch, tobj, spellnum);" in do_cast

def test_passives_train_only_after_successful_proc():
    block = c_function_block(
        PARSER,
        "static void perform_player_multicast_damage_chain",
    )
    for ability in (
        "SKILL_DOUBLE_CAST",
        "SKILL_TRIPLE_CAST",
        "SKILL_FOURTH_CAST",
    ):
        assert f"improve_ability_from_use(ch, {ability}, TRUE);" in block
        assert f"improve_ability_from_use(ch, {ability}, FALSE);" not in block

def test_foundation_contract_remains_unchanged():
    assert "#define COMBAT_PROGRESSION_MULTICAST_STAGE_DOUBLE 100" in PROG_H
    assert "#define COMBAT_PROGRESSION_MULTICAST_STAGE_TRIPLE  60" in PROG_H
    assert "#define COMBAT_PROGRESSION_MULTICAST_STAGE_FOURTH  35" in PROG_H
    assert "#define COMBAT_PROGRESSION_MULTICAST_DAMAGE_SECOND 80" in PROG_H
    assert "#define COMBAT_PROGRESSION_MULTICAST_DAMAGE_THIRD  65" in PROG_H
    assert "#define COMBAT_PROGRESSION_MULTICAST_DAMAGE_FOURTH 50" in PROG_H

def run():
    tests = [
        test_normal_class_access_ladder,
        test_non_v1_classes_do_not_get_normal_multicast_unlocks,
        test_scaled_damage_api_is_public_and_one_shot,
        test_mag_damage_consumes_scale_before_nested_damage,
        test_scaled_packet_enters_shared_damage_before_spell_crit,
        test_eligibility_is_pc_direct_single_target_pure_damage_only,
        test_strict_double_triple_fourth_chain,
        test_bonus_packets_use_reserved_damage_weights,
        test_bonus_packets_do_not_recast_full_spell_pipeline,
        test_only_normal_do_cast_invokes_multicast_chain,
        test_primary_target_death_is_guarded_before_bonus_packets,
        test_passives_train_only_after_successful_proc,
        test_foundation_contract_remains_unchanged,
    ]

    for test in tests:
        test()

    print(f"{len(tests)} tests passed: Multicast Damage V1 regression")

if __name__ == "__main__":
    run()