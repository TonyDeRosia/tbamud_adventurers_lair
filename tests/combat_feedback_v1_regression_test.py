from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
FIGHT = (ROOT / "src" / "fight.c").read_text(encoding="utf-8")
STRUCTS = (ROOT / "src" / "structs.h").read_text(encoding="utf-8")

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

def test_existing_damage_severity_system_is_preserved():
    assert "static int damage_severity_tier(int dam, struct char_data *victim)" in FIGHT

    base = c_function_block(FIGHT, "static const char *severity_verb_base")
    third = c_function_block(FIGHT, "static const char *severity_verb_third")

    for tier in range(1, 10):
        assert f"case {tier}:" in base
        assert f"case {tier}:" in third

def test_telemetry_is_post_damage_hp_percent_without_exact_hp_totals():
    block = c_function_block(
        FIGHT,
        "static void append_attacker_combat_telemetry",
    )

    assert "dam <= 0" in block
    assert "GET_MAX_HIT(victim) <= 0" in block
    assert "GET_HIT(victim) <= 0" in block
    assert "(GET_HIT(victim) * 100) / MAX(1, GET_MAX_HIT(victim))" in block
    assert "MAX(0, MIN(100, hp_pct))" in block
    assert "%d dmg" in block
    assert "%d%% HP" in block
    assert "%d/%d" not in block

def test_hp_percent_has_simple_condition_color_bands():
    block = c_function_block(FIGHT, "static const char *combat_feedback_hp_color")

    assert "hp_pct >= 70" in block
    assert "hp_pct >= 35" in block
    assert 'return "\\tG";' in block
    assert 'return "\\tY";' in block
    assert 'return "\\tR";' in block

def test_weapon_success_gets_one_attacker_only_suffix():
    block = c_function_block(
        FIGHT,
        "static void dam_message(int dam, struct char_data *ch, struct char_data *victim,",
    )

    assert block.count("append_attacker_combat_telemetry(") == 1

    miss_return = block.index("return;")
    telemetry = block.index("append_attacker_combat_telemetry(")

    # dam_message() has one attacker act() in the miss branch and another in
    # the success branch. Search from telemetry forward so this assertion
    # deliberately selects the successful-hit call site.
    attacker_act = block.index(
        "act(to_char, FALSE, ch, NULL, victim, TO_CHAR);",
        telemetry,
    )

    assert miss_return < telemetry < attacker_act
    assert 'send_to_char(ch, "(%d) ", dam);' not in block
    assert 'send_to_char(victim, "\\tR(%d)", dam);' in block

def test_nonweapon_direct_success_gets_one_suffix():
    block = c_function_block(
        FIGHT,
        "static void nonweapon_damage_message(int dam, struct char_data *ch,",
    )

    assert block.count("append_attacker_combat_telemetry(") == 1
    telemetry = block.index("append_attacker_combat_telemetry(")
    attacker_act = block.index("act(to_char, FALSE, ch, NULL, victim, TO_CHAR);")
    assert telemetry < attacker_act

def test_misses_and_death_flavor_do_not_get_telemetry():
    miss = c_function_block(
        FIGHT,
        "static void nonweapon_miss_message(struct char_data *ch,",
    )
    skill = c_function_block(
        FIGHT,
        "int skill_message(int dam, struct char_data *ch, struct char_data *vict,",
    )

    assert "append_attacker_combat_telemetry" not in miss
    assert "append_attacker_combat_telemetry" not in skill

def test_room_and_victim_messages_are_not_augmented():
    dam = c_function_block(
        FIGHT,
        "static void dam_message(int dam, struct char_data *ch, struct char_data *victim,",
    )
    nonweapon = c_function_block(
        FIGHT,
        "static void nonweapon_damage_message(int dam, struct char_data *ch,",
    )

    assert "append_attacker_combat_telemetry(to_room" not in dam
    assert "append_attacker_combat_telemetry(to_victim" not in dam
    assert "append_attacker_combat_telemetry(to_vict" not in nonweapon
    assert "append_attacker_combat_telemetry(to_room" not in nonweapon

def test_existing_condition_band_alerts_remain():
    assert "static int victim_condition_band(const struct char_data *victim)" in FIGHT
    assert "static const char *victim_condition_text(int band)" in FIGHT
    assert (
        'send_to_char(ch, "\\tC%s %s\\tn\\r\\n", '
        "PERS(victim, ch), victim_condition_text(new_band));"
        in FIGHT
    )

def test_v1_does_not_add_a_preference_flag():
    assert "PRF_COMBATFEEDBACK" not in STRUCTS
    assert "PRF_DAMAGEINFO" not in STRUCTS
    assert "PRF_ENEMYHP" not in STRUCTS

def run():
    tests = [
        test_existing_damage_severity_system_is_preserved,
        test_telemetry_is_post_damage_hp_percent_without_exact_hp_totals,
        test_hp_percent_has_simple_condition_color_bands,
        test_weapon_success_gets_one_attacker_only_suffix,
        test_nonweapon_direct_success_gets_one_suffix,
        test_misses_and_death_flavor_do_not_get_telemetry,
        test_room_and_victim_messages_are_not_augmented,
        test_existing_condition_band_alerts_remain,
        test_v1_does_not_add_a_preference_flag,
    ]

    for test in tests:
        test()

    print(f"{len(tests)} tests passed: Combat Feedback V1 regression")

if __name__ == "__main__":
    run()