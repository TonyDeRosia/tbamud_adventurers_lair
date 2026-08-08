#!/usr/bin/env python3
"""Source-level regression checks for unified weapon-equipping commands."""

from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[1]
ACT_ITEM = (ROOT / "src" / "act.item.c").read_text(encoding="utf-8")
INTERPRETER = (ROOT / "src" / "interpreter.c").read_text(encoding="utf-8")


def function_body(name: str) -> str:
    match = re.search(rf"(?:ACMD\({name}\)|static int {name}\([^)]*\))\s*\{{", ACT_ITEM)
    assert match, f"missing {name}"
    start = match.end()
    depth = 1
    pos = start
    while depth and pos < len(ACT_ITEM):
        if ACT_ITEM[pos] == "{":
            depth += 1
        elif ACT_ITEM[pos] == "}":
            depth -= 1
        pos += 1
    assert depth == 0, f"unterminated {name}"
    return ACT_ITEM[start : pos - 1]


validator = function_body("can_equip_weapon")
perform = function_body("perform_wear")
wear = function_body("do_wear")
wield = function_body("do_wield")
offhand = function_body("do_offhand")
grab = function_body("do_grab")

# One shared validator owns all weapon restrictions.
for required in (
    "ITEM_WEAR_WIELD",
    "GET_OBJ_LEVEL",
    "invalid_align(ch, obj)",
    "invalid_class(ch, obj)",
    "can_wield_by_weight",
    "is_offhand_weapon(obj)",
    "SKILL_DUAL_WIELD",
    "is_two_hander(obj)",
    "character_is_using_two_hander(ch)",
    "WEAR_SHIELD",
    "GET_OBJ_WEIGHT(obj) > GET_OBJ_WEIGHT(prim)",
):
    assert required in validator, f"shared validator lacks {required}"
assert "can_equip_weapon(ch, obj, where, TRUE, FALSE)" in perform

# Wear and hold choose primary first and offhand second.  Wield is always an
# explicit primary-slot command and never removes an existing weapon.
for body, command in ((wear, "wear"), (grab, "hold")):
    assert "weapon_wear_position(ch, obj)" in body, f"{command} does not use shared slot selection"
assert "perform_weapon_swap(ch, obj, WEAR_WIELD)" in wield
assert "perform_remove" not in wield

# Both offhand spellings reach the same implementation, and the two-word form
# discards its command word before looking up the object.
assert re.search(r'\{\s*"offhand"[^\n]*do_offhand', INTERPRETER)
assert re.search(r'\{\s*"dual"[^\n]*do_offhand', INTERPRETER)
assert 'strn_cmp(argument, "wield", 5)' in offhand
assert "perform_weapon_swap(ch, obj, WEAR_HOLD)" in offhand

# Nonweapon hold/light behavior and two-pass automatic wear remain present.
for item_type in ("ITEM_LIGHT", "ITEM_WAND", "ITEM_STAFF", "ITEM_SCROLL", "ITEM_POTION"):
    assert item_type in grab
assert "Offhand-capable weapons are deferred" in wear
assert "wear_attempted" in wear
assert "if (!items_worn && !wear_attempted)" in wear

print("weapon command unification regression checks passed")
