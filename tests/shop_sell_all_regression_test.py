#!/usr/bin/env python3
from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[1]
SHOP = (ROOT / "src" / "shop.c").read_text(encoding="utf-8")

required = [
    "static void shopping_sell_all(struct char_data *ch, struct char_data *keeper, int shop_nr);",
    "for (obj = ch->carrying; obj; obj = next_obj)",
    "next_obj = obj->next_content;",
    "OBJ_FLAGGED(obj, ITEM_KEPT)",
    "result = trade_with(obj, shop_nr);",
    "if (result != OBJECT_OK)",
    "price = sell_price(obj, shop_nr, keeper, ch);",
    "available = (long long)GET_GOLD(keeper) + (long long)SHOP_BANK(shop_nr);",
    "obj_from_char(obj);",
    "slide_obj(obj, keeper, shop_nr);",
    "GET_GOLD(ch) += goldamt;",
    'if (!str_cmp(all_ptr, "all"))',
    "shopping_sell_all(ch, keeper, shop_nr);",
    "You have nothing this shopkeeper will buy.",
]

for needle in required:
    assert needle in SHOP, f"missing sell-all contract: {needle}"

helper_defs = list(re.finditer(
    r"(?m)^static void shopping_sell_all\(struct char_data \*ch, struct char_data \*keeper, int shop_nr\)\s*\{",
    SHOP,
))
assert len(helper_defs) == 1, f"expected 1 shopping_sell_all definition, found {len(helper_defs)}"

sell_defs = list(re.finditer(
    r"(?m)^static void shopping_sell\(char \*arg, struct char_data \*ch, struct char_data \*keeper, int shop_nr\)\s*\{",
    SHOP,
))
assert len(sell_defs) == 1, f"expected 1 shopping_sell definition, found {len(sell_defs)}"

sell_start = sell_defs[0].start()
sell_end = SHOP.find("static void shopping_value(", sell_start)
assert sell_end > sell_start, "could not isolate shopping_sell()"
sell = SHOP[sell_start:sell_end]

dispatch = sell.find('if (!str_cmp(all_ptr, "all"))')
call = sell.find("shopping_sell_all(ch, keeper, shop_nr);")
transaction = sell.find("transaction_amt(arg)")
lookup = sell.find("one_argument(arg, name)")

assert min(dispatch, call, transaction, lookup) >= 0
assert dispatch < transaction, "sell all dispatch must precede transaction_amt"
assert call < lookup, "sell all dispatch must precede named-item lookup"

# Normal item/quantity sale remains intact.
assert "get_selling_obj(ch, name, keeper, shop_nr, TRUE)" in sell
assert "while (obj &&" in sell

# Inventory-wide path deliberately walks only ch->carrying, not equipment.
helper_start = helper_defs[0].start()
helper_end = sell_start
helper = SHOP[helper_start:helper_end]
assert "ch->carrying" in helper
assert "GET_EQ(" not in helper

# Rejected or unaffordable items must continue rather than abort the scan.
assert "if (result != OBJECT_OK)" in helper
assert "continue;" in helper
assert "if (available < price)" in helper

print("shop sell-all regression passed")