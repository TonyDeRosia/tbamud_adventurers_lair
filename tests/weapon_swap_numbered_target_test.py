#!/usr/bin/env python3
"""Regression contracts for N.object targeting and atomic weapon swaps."""
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
ITEM = (ROOT / "src" / "act.item.c").read_text(encoding="utf-8")
HANDLER = (ROOT / "src" / "handler.c").read_text(encoding="utf-8")


def section(text: str, start: str, end: str) -> str:
    pos = text.index(start)
    return text[pos:text.index(end, pos)]


get_number = section(HANDLER, "int get_number(char **name)", "struct obj_data *get_obj_in_list_num")
lookup = section(HANDLER, "struct obj_data *get_obj_in_list_vis", "struct obj_data *get_obj_vis")
swap = section(ITEM, "/* Explicit weapon commands may replace", "int find_eq_pos")
wield = section(ITEM, "ACMD(do_wield)", "ACMD(do_offhand)")
offhand = section(ITEM, "ACMD(do_offhand)", "ACMD(do_grab)")
grab = section(ITEM, "ACMD(do_grab)", "static void perform_remove")
wear = section(ITEM, "ACMD(do_wear)", "ACMD(do_wield)")

# Existing Circle/TBA syntax is numeric prefix + dot and abbreviations continue
# through isname().  The command lookup list is carried inventory only.
assert "strchr(*name, '.')" in get_number
assert "isdigit" in get_number and "atoi(number)" in get_number
assert "isname(name, i->name)" in lookup
for body in (wield, offhand, grab, wear):
    assert "get_obj_in_list_vis" in body
    assert "ch->carrying" in body

# Candidate validation and both triggers precede the first equipment mutation.
for token in ("can_equip_weapon(ch, obj, where, TRUE, TRUE)", "ITEM_NODROP",
              "wear_otrigger", "remove_otrigger"):
    assert token in swap
first_move = min(swap.index("unequip_char"), swap.index("obj_from_char"))
assert swap.index("can_equip_weapon") < first_move
assert swap.index("wear_otrigger") < first_move
assert swap.index("remove_otrigger") < first_move
assert "equip_char(ch, old, where)" in swap  # defensive rollback

assert "perform_weapon_swap(ch, obj, WEAR_WIELD)" in wield
assert "perform_weapon_swap(ch, obj, WEAR_HOLD)" in offhand
assert "perform_weapon_swap(ch, obj, weapon_wear_position(ch, obj))" in grab
assert "perform_weapon_swap(ch, obj, weapon_wear_position(ch, obj))" in wear

# Two-handed and offhand-weight conflicts remain candidate-validation failures.
validator = section(ITEM, "static int can_equip_weapon", "static int perform_wear")
assert "is_two_hander(obj) && (GET_EQ(ch, WEAR_SHIELD) || GET_EQ(ch, WEAR_HOLD))" in validator
assert "GET_OBJ_WEIGHT(GET_EQ(ch, WEAR_HOLD)) > GET_OBJ_WEIGHT(obj)" in validator

print("weapon swap and numbered-target regression checks passed")
