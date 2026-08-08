"""Focused source/data contracts for equipment and Sanctuary boot fixes."""
from pathlib import Path
import re


ROOT = Path(__file__).resolve().parents[1]
ITEM = (ROOT / "src/act.item.c").read_text()
HANDLER = (ROOT / "src/handler.c").read_text()
DB = (ROOT / "src/db.c").read_text()
ULTIMA_MOBS = (ROOT / "lib/world/mob/555.mob").read_text()


def function_body(source: str, start: str, end: str) -> str:
    return source[source.index(start):source.index(end, source.index(start))]


def test_wear_all_uses_two_pass_hand_equipping_and_shared_rules():
    wear = function_body(ITEM, "ACMD(do_wear)", "ACMD(do_wield)")
    assert "ITEM_TWO_HANDED" not in ITEM
    assert "is_offhand_weapon(obj)" in wear
    assert "perform_wear(ch, obj, WEAR_WIELD)" in wear
    assert "perform_wear(ch, obj, WEAR_HOLD)" in wear
    assert wear.index("perform_wear(ch, obj, WEAR_WIELD)") < wear.index(
        "perform_wear(ch, obj, WEAR_HOLD)"
    )
    assert "if (perform_wear(ch, obj, WEAR_HOLD))" in wear
    assert "items_worn++;\n          \n" not in wear


def test_explicit_and_automatic_wield_share_strength_validation():
    wear = function_body(ITEM, "ACMD(do_wear)", "ACMD(do_wield)")
    validator = function_body(ITEM, "static int can_equip_weapon", "static int perform_wear")
    assert "can_wield_by_weight(ch, obj, show_message)" in validator
    assert "perform_wear(ch, obj, WEAR_WIELD)" in wear
    wield = function_body(ITEM, "ACMD(do_wield)", "ACMD(do_offhand)")
    assert "perform_weapon_swap(ch, obj, WEAR_WIELD)" in wield


def test_perform_wear_remains_the_single_compatibility_gate():
    perform = function_body(ITEM, "static int can_equip_weapon", "int find_eq_pos")
    for contract in (
        "GET_SKILL(ch, SKILL_DUAL_WIELD)",
        "character_is_using_two_hander(ch)",
        "is_offhand_weapon(obj)",
        "GET_OBJ_WEIGHT(obj) > GET_OBJ_WEIGHT(prim)",
        "if (GET_EQ(ch, where))",
        "wear_otrigger(obj, ch, where)",
    ):
        assert contract in perform


def test_affect_recalculation_preserves_baseline_sanctuary():
    affect_total = function_body(HANDLER, "void affect_total", "void affect_to_char")
    assert "[SANCT-SCRUB]" not in HANDLER
    assert '"[DBG]' not in HANDLER
    assert "REMOVE_BIT_AR(ch->char_specials.saved.affected_by, AFF_SANCTUARY)" not in affect_total
    assert "AFF_FLAGS(ch)[i] = ch->char_specials.saved.affected_by[i]" in affect_total


def test_world_contains_legitimate_sanctuary_baselines():
    # 256 is AFF_SANCTUARY; 320 combines it with another baseline affect.
    assert re.search(r"^72 0 0 0 256 0 0 0 1000 E$", ULTIMA_MOBS, re.MULTILINE)
    assert re.search(r"^72 0 0 0 320 0 0 0 1000 E$", ULTIMA_MOBS, re.MULTILINE)


def test_obsolete_mob_19500_boot_probe_is_absent():
    assert "real_mobile(19500)" not in DB
    assert "AI test mob 19500" not in DB


def test_immortal_cleanse_still_excludes_permanent_affects():
    spells = (ROOT / "src/spells.c").read_text()
    cleanse = function_body(
        spells, "static int immortal_cleanse_is_timed_debuff", "static int remove_spell_affect_if_present"
    )
    assert "af->duration < 0" in cleanse
    assert "spell_info[af->spell].violent" in cleanse


if __name__ == "__main__":
    tests = [value for name, value in globals().items() if name.startswith("test_")]
    for test in tests:
        test()
    print(f"{len(tests)} focused regression tests passed")
