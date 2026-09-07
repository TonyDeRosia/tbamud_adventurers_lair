from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
FIGHT = (ROOT / "src" / "fight.c").read_text(encoding="utf-8")

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

def test_one_authoritative_npc_gold_roll_helper():
    block = c_function_block(FIGHT, "static long npc_kill_gold_roll")

    assert "victim->mob_specials.gold_min" in block
    assert "victim->mob_specials.gold_max" in block
    assert "GET_GOLD(victim)" in block
    assert "rand_number(imin, imax)" in block
    assert "IS_HAPPYHOUR && IS_HAPPYGOLD" in block
    assert "HAPPY_GOLD" in block
    assert "INT_MAX" in block

    range_branch = block.index("if (gmin > 0 || gmax > 0)")
    legacy_branch = block.index("else if (GET_GOLD(victim) > 0)")
    happy = block.index("if (gold > 0 && IS_HAPPYHOUR && IS_HAPPYGOLD)")
    assert range_branch < legacy_branch < happy

def test_die_no_longer_directly_pays_npc_gold():
    block = c_function_block(
        FIGHT,
        "void die(struct char_data * ch, struct char_data * killer)",
    )

    assert "NPC gold payout to the killer" not in block
    assert "gold_gain" not in block
    assert "increase_money_gold(killer, gold_gain)" not in block
    assert "You claim %s for the bounty on %s." in block

def test_damage_prepares_exact_gold_before_die():
    block = c_function_block(
        FIGHT,
        "int damage(struct char_data *ch, struct char_data *victim, int dam, int attacktype)",
    )

    roll = block.index("local_gold = npc_kill_gold_roll(victim);")
    store = block.index("SET_GOLD(victim, (int)local_gold);")
    clear_min = block.index("victim->mob_specials.gold_min = 0;")
    clear_max = block.index("victim->mob_specials.gold_max = 0;")
    die = block.index("die(victim, reward_killer);")

    assert roll < store < clear_min < clear_max < die
    assert 'snprintf(local_buf, sizeof(local_buf), "%ld", local_gold);' in block
    assert "happy_gold" not in block

def test_make_corpse_consumes_prepared_gold_once():
    block = c_function_block(FIGHT, "static void make_corpse(struct char_data *ch)")

    assert "if (gmin > 0 || gmax > 0)" in block
    assert "else if (GET_GOLD(ch) > 0)" in block
    assert "dropped_gold = (long long)GET_GOLD(ch);" in block
    assert "create_money((int)dropped_gold, 0)" in block
    assert "SET_GOLD(ch, 0);" in block
    assert "ch->mob_specials.gold_min = 0;" in block
    assert "ch->mob_specials.gold_max = 0;" in block

def test_autogold_autosplit_autoloot_autosac_remain_corpse_based():
    block = c_function_block(
        FIGHT,
        "int damage(struct char_data *ch, struct char_data *victim, int dam, int attacktype)",
    )

    die = block.index("die(victim, reward_killer);")
    autosplit = block.index("PRF_FLAGGED(reward_killer, PRF_AUTOSPLIT)")
    autogold = block.index("PRF_FLAGGED(reward_killer, PRF_AUTOGOLD)")
    autoloot = block.index("PRF_FLAGGED(reward_killer, PRF_AUTOLOOT)")
    autosac = block.index("PRF_FLAGGED(reward_killer, PRF_AUTOSAC)")

    assert die < autosplit < autogold < autoloot < autosac
    assert block.count('do_get(reward_killer, "all.coin corpse", 0, 0);') == 2
    assert 'do_get(reward_killer, "all corpse", 0, 0);' in block
    assert 'do_sac(reward_killer,"corpse",0,0);' in block

def test_autosplit_uses_exact_rolled_gold_amount():
    block = c_function_block(
        FIGHT,
        "int damage(struct char_data *ch, struct char_data *victim, int dam, int attacktype)",
    )

    assert "(local_gold > 0)" in block
    assert 'snprintf(local_buf, sizeof(local_buf), "%ld", local_gold);' in block
    assert "do_split(reward_killer, local_buf, 0, 0);" in block

def test_rare_kill_xp_bonus_is_unchanged():
    block = c_function_block(
        FIGHT,
        "static int rare_kill_bonus_for_victim",
    )

    assert "MOB_FLAGGED(victim, MOB_RARE)" in block
    assert "MAX(1, base_xp / 4)" in block

def test_npc_raw_xp_still_adds_builder_and_rare_bonus():
    block = c_function_block(
        FIGHT,
        "static int npc_kill_raw_xp",
    )

    assert "raw += MAX(0, GET_EXP(victim));" in block
    assert "raw += rare_kill_bonus_for_victim(victim, base_xp);" in block

def run():
    tests = [
        test_one_authoritative_npc_gold_roll_helper,
        test_die_no_longer_directly_pays_npc_gold,
        test_damage_prepares_exact_gold_before_die,
        test_make_corpse_consumes_prepared_gold_once,
        test_autogold_autosplit_autoloot_autosac_remain_corpse_based,
        test_autosplit_uses_exact_rolled_gold_amount,
        test_rare_kill_xp_bonus_is_unchanged,
        test_npc_raw_xp_still_adds_builder_and_rare_bonus,
    ]

    for test in tests:
        test()

    print(f"{len(tests)} tests passed: Corpse Gold / Autogold Pipeline V1 regression")

if __name__ == "__main__":
    run()