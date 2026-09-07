from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[1]

FIGHT = (ROOT / "src" / "fight.c").read_text(encoding="utf-8")
FIGHT_H = (ROOT / "src" / "fight.h").read_text(encoding="utf-8")
STRUCTS = (ROOT / "src" / "structs.h").read_text(encoding="utf-8")

def define_value(text: str, name: str) -> int:
    m = re.search(rf"^#define\s+{re.escape(name)}\s+(\d+)\b", text, re.M)
    assert m, name
    return int(m.group(1))

def c_function_block(text: str, signature: str) -> str:
    start = text.index(signature)
    open_brace = text.index("{", start)
    depth = 0

    for i in range(open_brace, len(text)):
        if text[i] == "{":
            depth += 1
        elif text[i] == "}":
            depth -= 1
            if depth == 0:
                return text[start:i + 1]

    raise AssertionError(f"unterminated C function: {signature}")

def test_haste_packet_contract_is_explicit():
    assert define_value(FIGHT_H, "HASTE_BONUS_ATTACKS_PER_PULSE") == 2
    assert define_value(FIGHT_H, "HASTE_BONUS_ATTACK_DAMAGE_PERCENT") == 50
    assert define_value(STRUCTS, "AFF_HASTE") == 65

def test_haste_uses_one_shot_damage_scale():
    assert "static int next_haste_damage_percent = 100;" in FIGHT

    block = c_function_block(FIGHT, "int damage(struct char_data *ch")
    capture = block.index("int haste_damage_percent = next_haste_damage_percent;")
    consume = block.index("next_haste_damage_percent = 100;")
    scale = block.index("dam = MAX(1, (dam * haste_damage_percent) / 100);")
    offhand = block.index("/* OFFHAND DAMAGE SCALE */")

    assert capture < consume < scale < offhand

def test_haste_helper_reuses_normal_hit_accuracy():
    block = c_function_block(
        FIGHT,
        "static void perform_haste_bonus_mainhand_attack",
    )

    assert "victim = FIGHTING(ch);" in block
    assert "IN_ROOM(ch) == NOWHERE" in block
    assert "IN_ROOM(victim) == NOWHERE" in block
    assert "IN_ROOM(ch) != IN_ROOM(victim)" in block
    assert "hit(ch, victim, TYPE_UNDEFINED);" in block
    assert "HASTE_BONUS_ATTACK_DAMAGE_PERCENT" in block

def test_haste_helper_suppresses_duplicate_periodic_and_dg_work():
    block = c_function_block(
        FIGHT,
        "static void perform_haste_bonus_mainhand_attack",
    )

    before_hit = block.index("combat_effects_due = FALSE;")
    hit = block.index("hit(ch, victim, TYPE_UNDEFINED);")
    restore = block.index("combat_effects_due = previous_effects_due;")
    assert before_hit < hit < restore

def test_haste_damage_scale_is_restored_after_each_hit():
    block = c_function_block(
        FIGHT,
        "static void perform_haste_bonus_mainhand_attack",
    )

    assert "previous_damage_percent = next_haste_damage_percent;" in block
    assert (
        "next_haste_damage_percent = HASTE_BONUS_ATTACK_DAMAGE_PERCENT;"
        in block
    )
    assert "next_haste_damage_percent = previous_damage_percent;" in block

def test_haste_runs_exactly_two_packets_and_revalidates_each_time():
    block = c_function_block(FIGHT, "static void do_haste_attacks")

    assert "AFF_FLAGGED(ch, AFF_HASTE)" in block
    assert "attack < HASTE_BONUS_ATTACKS_PER_PULSE" in block
    assert block.count("perform_haste_bonus_mainhand_attack(ch);") == 1

    # The helper re-reads FIGHTING(ch) on every loop iteration, so a kill,
    # extraction, flee, or combat switch after packet one stops packet two.
    helper = c_function_block(
        FIGHT,
        "static void perform_haste_bonus_mainhand_attack",
    )
    assert "victim = FIGHTING(ch);" in helper

def test_haste_is_not_player_only():
    block = c_function_block(FIGHT, "static void do_haste_attacks")
    helper = c_function_block(
        FIGHT,
        "static void perform_haste_bonus_mainhand_attack",
    )

    assert "IS_NPC(ch)" not in block
    assert "IS_NPC(ch)" not in helper

def test_haste_does_not_recursively_invoke_progression_paths():
    helper = c_function_block(
        FIGHT,
        "static void perform_haste_bonus_mainhand_attack",
    )
    haste = c_function_block(FIGHT, "static void do_haste_attacks")
    combined = helper + "\n" + haste

    for forbidden in (
        "do_double_attack(",
        "do_offhand_attack(",
        "do_spirit_procs(",
        "GET_MOB_SPEC",
        "dual_skill_attack(",
        "improve_ability_from_use(",
    ):
        assert forbidden not in combined

def test_haste_order_is_after_physical_progression_before_spirit_spec():
    block = c_function_block(FIGHT, "void perform_violence(void)")

    base = block.index("hit(ch, FIGHTING(ch), TYPE_UNDEFINED);")
    double = block.index("do_double_attack(ch);")
    offhand = block.index("do_offhand_attack(ch, FIGHTING(ch));")
    haste = block.index("do_haste_attacks(ch);")
    spirit = block.index("do_spirit_procs(ch, FIGHTING(ch));")
    mob_spec = block.index("GET_MOB_SPEC(ch)")

    assert base < double < offhand < haste < spirit < mob_spec

def test_haste_packets_run_on_every_one_second_combat_pulse():
    block = c_function_block(FIGHT, "void perform_violence(void)")
    haste_line = "do_haste_attacks(ch);"
    pos = block.index(haste_line)

    nearby = block[max(0, pos - 120):pos + len(haste_line) + 120]
    assert "if (combat_effects_due" not in nearby.split(haste_line)[0].splitlines()[-1]

def test_normal_multiattack_helper_remains_player_only():
    block = c_function_block(
        FIGHT,
        "static void perform_bonus_mainhand_attack",
    )
    assert "IS_NPC(ch)" in block

def run():
    tests = [
        test_haste_packet_contract_is_explicit,
        test_haste_uses_one_shot_damage_scale,
        test_haste_helper_reuses_normal_hit_accuracy,
        test_haste_helper_suppresses_duplicate_periodic_and_dg_work,
        test_haste_damage_scale_is_restored_after_each_hit,
        test_haste_runs_exactly_two_packets_and_revalidates_each_time,
        test_haste_is_not_player_only,
        test_haste_does_not_recursively_invoke_progression_paths,
        test_haste_order_is_after_physical_progression_before_spirit_spec,
        test_haste_packets_run_on_every_one_second_combat_pulse,
        test_normal_multiattack_helper_remains_player_only,
    ]

    for test in tests:
        test()

    print(f"{len(tests)} tests passed: Haste Combat Packets V1 regression")

if __name__ == "__main__":
    run()