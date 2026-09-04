from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]


def source(name):
    return (ROOT / "src" / name).read_text(encoding="utf-8")


def test_active_resource_storage_is_not_16_bit():
    structs = source("structs.h")
    block = structs[structs.index("struct char_point_data"):structs.index("struct char_point_data", structs.index("struct char_point_data") + 1)]
    for field in ("mana", "max_mana", "hit", "max_hit", "move", "max_move"):
        assert f"int {field};" in block
        assert f"sh_int {field};" not in block
    # The old binary converter must keep its on-disk layout.
    assert "sh_int mana;" in source("util/plrtoascii.c")


def test_ascii_player_persistence_uses_decimal_ints():
    players = source("players.c")
    assert '"Hit : %d/%d' in players
    assert '"Mana: %d/%d' in players
    assert '"Move: %d/%d' in players
    assert 'sscanf(line, "%d/%d", &num, &num2)' in players


def test_set_convenience_accepts_full_supported_range():
    wizard = source("act.wizard.c")
    start = wizard.index('if (!str_cmp(field, "health")', wizard.index("ACMD(do_set)"))
    block = wizard[start:start + 4000]
    assert 'value_long > 999999' in block
    assert '!str_cmp(field, "mana")' in block
    assert '!str_cmp(field, "move")' in block
    assert 'GET_BASE_MAX_MANA(vict) = (int)value_long' in block
    assert 'GET_BASE_MAX_MOVE(vict) = (int)value_long' in block
    assert 'GET_BASE_MAX_HIT(vict) = (int)value_long' in block


def test_base_maxima_survive_effect_rebuilds():
    handler = source("handler.c")
    total = handler[handler.index("void affect_total"):handler.index("void affect_to_char")]
    assert "GET_MAX_HIT(ch) = GET_BASE_MAX_HIT(ch)" in total
    assert "GET_MAX_MANA(ch) = GET_BASE_MAX_MANA(ch)" in total
    assert "GET_MAX_MOVE(ch) = GET_BASE_MAX_MOVE(ch)" in total
    assert "loc == APPLY_MANA || loc == APPLY_HIT || loc == APPLY_MOVE" in handler


def test_progression_is_deterministic_and_uses_real_abilities():
    classes = source("class.c")
    start = classes.index("void advance_level")
    block = classes[start:classes.index("int backstab_mult", start)]
    for cls in ("CLASS_MAGIC_USER", "CLASS_CLERIC", "CLASS_THIEF", "CLASS_WARRIOR",
                "CLASS_PALADIN", "CLASS_BARD", "CLASS_WARLOCK", "CLASS_DRUID",
                "CLASS_MYSTIC", "CLASS_ADVENTURER"):
        assert f"[{cls}]" in block
    assert "ch->real_abils.con" in block
    assert "ch->real_abils.intel" in block
    assert "ch->real_abils.wis" in block
    assert "ch->real_abils.dex" in block
    assert "rand_number" not in block


def test_level_one_has_practical_starting_move_reserve():
    classes = source("class.c")
    assert "#define STARTING_MOVE_BASE 100" in classes
    start = classes.index("void do_start")
    block = classes[start:classes.index("void advance_level", start)]
    assert "GET_BASE_MAX_MOVE(ch) = GET_MAX_MOVE(ch) = STARTING_MOVE_BASE;" in block


def test_regen_scales_with_pool_and_preserves_condition_hooks():
    limits = source("limits.c")
    assert "GET_MAX_MANA(ch) / 40" in limits
    assert "GET_MAX_HIT(ch) / 50" in limits
    assert "GET_MAX_MOVE(ch) / 40" in limits
    assert "is_starving(ch) || is_dehydrated(ch)" in limits
    assert "AFF_POISON" in limits and "AFF_CLARITY" in limits
    assert "best_regen_multiplier(ch)" in limits


if __name__ == "__main__":
    tests = [value for name, value in globals().items() if name.startswith("test_")]
    for test in tests:
        test()
    print(f"{len(tests)} resource-pool regression tests passed")
