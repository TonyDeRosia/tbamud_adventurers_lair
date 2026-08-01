#!/usr/bin/env python3
"""Regression checks for ITEM_OFFHAND meaning offhand-capable, not offhand-only."""
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
ITEM = (ROOT / "src" / "act.item.c").read_text(encoding="utf-8")
OEDIT = (ROOT / "src" / "oedit.c").read_text(encoding="utf-8")
HANDLER = (ROOT / "src" / "handler.c").read_text(encoding="utf-8")
OBJSAVE = (ROOT / "src" / "objsave.c").read_text(encoding="utf-8")


def between(text: str, start: str, end: str) -> str:
    first = text.index(start)
    return text[first:text.index(end, first)]


slot = between(ITEM, "static int weapon_wear_position(struct char_data *ch, const struct obj_data *obj)\n{", "static int can_equip_weapon")
validator = between(ITEM, "static int can_equip_weapon", "static int perform_wear")
wear = between(ITEM, "ACMD(do_wear)", "ACMD(do_wield)")
wield = between(ITEM, "ACMD(do_wield)", "ACMD(do_offhand)")
offhand = between(ITEM, "ACMD(do_offhand)", "ACMD(do_grab)")
oedit_wear = between(OEDIT, "case OEDIT_WEAR:", "case OEDIT_WEIGHT:")

# General commands select Wield while empty and Hold only for an offhand-capable
# one-hander when a primary already exists.
assert "GET_EQ(ch, WEAR_WIELD)" in slot
assert "is_offhand_weapon(obj)" in slot
assert "!is_two_hander(obj)" in slot
assert "return WEAR_WIELD" in slot
assert "designed for offhand use" not in validator

# Wield remains primary-only; explicit aliases remain offhand-only.
assert "perform_wear(ch, obj, WEAR_WIELD)" in wield
assert "perform_wear(ch, obj, WEAR_HOLD)" in offhand

# Wear-all defers offhand-capable weapons, then uses the first as primary when
# needed and a distinct carried object for offhand.
assert "!is_offhand_weapon(obj)" in wear
assert "if (!GET_EQ(ch, WEAR_WIELD))" in wear
assert "perform_wear(ch, obj, WEAR_WIELD)" in wear
assert "perform_wear(ch, obj, WEAR_HOLD)" in wear

# Malformed legacy objects fail safely, and OLC cannot create the combination.
assert "is_two_hander(obj) && is_offhand_weapon(obj)" in validator
assert "ITEM_TWO_HANDER" in oedit_wear and "ITEM_OFFHAND" in oedit_wear
assert oedit_wear.count("REMOVE_BIT_AR") >= 2
assert "incompatible TWO_HANDER and OFFHAND flags" in HANDLER
assert "OBJ_FLAGGED(obj, ITEM_TWO_HANDER) && OBJ_FLAGGED(obj, ITEM_OFFHAND)" in OBJSAVE

print("ambidextrous weapon regression checks passed")
