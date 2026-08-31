from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def source(name):
    return (ROOT / "src" / name).read_text(encoding="utf-8")


def test_environment_policy_and_immortal_eyes_are_centralized():
    utils = source("utils.h")
    parser = source("spell_parser.c")
    assert "CAN_BYPASS_ENVIRONMENT" in utils
    assert "CAN_BYPASS_ENVIRONMENT(sub)" in utils
    assert "!CAN_BYPASS_ENVIRONMENT(caster)" in parser


def test_admin_aliases_do_not_replace_dg_mload_namespace():
    interp = source("interpreter.c")
    dg = source("dg_mobcmd.c")
    assert '{ "oload"' in interp and "do_load" in interp
    assert '{ "mload"' in interp and "do_load" in interp
    assert "ACMD(do_mload)" in dg


def test_cleanse_removes_timed_affect_entries_and_rebuilds():
    spells = source("spells.c")
    wizard = source("act.wizard.c")
    assert "af && af->duration >= 0" in spells
    assert "affect_total(victim);" in spells
    assert "ACMD(do_cleanse_admin)" in wizard
    assert "GET_LEVEL(vict) >= GET_LEVEL(ch)" in wizard


def test_zap_prefers_held_then_wielded_and_validates_spell():
    other = source("act.other.c")
    parser = source("spell_parser.c")
    item = source("act.item.c")
    assert "GET_EQ(ch, WEAR_HOLD)" in other
    assert "wand = GET_EQ(ch, WEAR_WIELD)" in other
    assert "TOP_SPELL_DEFINE" in other
    assert "mag_objectmagic(ch, wand, argument)" in other
    assert "if (result > 0)" in parser
    assert "GET_OBJ_VAL(obj, 2)--" in parser
    assert "where == WEAR_WIELD && GET_OBJ_TYPE(obj) == ITEM_WAND" in item


def test_set_dual_order_aliases_and_strict_numeric_input():
    wizard = source("act.wizard.c")
    assert "canonical_set_field" in wizard
    assert "field_first" in wizard
    assert '"health", "hp"' not in wizard  # handled as explicit convenience names
    assert "value_long > 999999" in wizard
    assert "errno == ERANGE" in wizard
    assert "parsed < INT_MIN || parsed > INT_MAX" in wizard
    do_set = wizard.index("ACMD(do_set)")
    target = wizard.index("/* find the target */", do_set)
    health = wizard.index('if (!str_cmp(field, "health")', do_set)
    assert target < health


def test_rreset_list_is_read_only_and_compare_has_no_score():
    wizard = source("act.wizard.c")
    informative = source("act.informative.c")
    list_block = wizard[wizard.index('if (!str_cmp(subcmd_arg, "list"))'):]
    list_block = list_block[:list_block.index('if (is_abbrev(subcmd_arg, "del"))')]
    assert "show_room_resets" in list_block
    assert "save_zone" not in list_block
    assert "add_to_save_list" not in list_block
    assert "INVALID TRIGGER" in wizard
    assert "This is a factual comparison, not an overall item score" in informative


if __name__ == "__main__":
    tests = [value for name, value in globals().items() if name.startswith("test_")]
    for test in tests:
        test()
    print(f"{len(tests)} admin/world-system regression tests passed")
