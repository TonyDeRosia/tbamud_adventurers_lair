#!/usr/bin/env python3
from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[1]
LIMITS = (ROOT / "src" / "limits.c").read_text(encoding="utf-8")
CLASSC = (ROOT / "src" / "class.c").read_text(encoding="utf-8")
PLAYERS = (ROOT / "src" / "players.c").read_text(encoding="utf-8")

assert "const int FIXED_TNL_XP = 1000;" in CLASSC
assert "static void normalize_mortal_exp_floor(struct char_data *ch)" in LIMITS
assert "floor_exp = level_exp(GET_CLASS(ch), GET_LEVEL(ch));" in LIMITS
assert "GET_EXP(ch) = floor_exp;" in LIMITS
assert "normalize_mortal_exp_floor(ch);" in LIMITS

start = LIMITS.find("void gain_exp(struct char_data *ch, int gain)")
end = LIMITS.find("void gain_exp_regardless(struct char_data *ch, int gain, int max_level)")
assert start >= 0, "gain_exp() not found"
assert end > start, "gain_exp_regardless() boundary not found"
body = LIMITS[start:end]

# Repair must occur before the sign branch so a corrupted live character is
# fixed on the next XP event.
repair_pos = body.find("normalize_mortal_exp_floor(ch);")
positive_pos = body.find("if (gain > 0)")
assert repair_pos >= 0 and positive_pos >= 0 and repair_pos < positive_pos

# Negative XP must also be normalized after subtraction.
neg_start = body.find("} else if (gain < 0) {")
assert neg_start >= 0, "negative gain branch not found"
neg_body = body[neg_start:]
assert "GET_EXP(ch) += gain;" in neg_body
assert "normalize_mortal_exp_floor(ch);" in neg_body

# Login/load remains a second line of defense for persisted legacy/corrupt XP.
assert "clamp_player_exp_to_level(ch);" in PLAYERS

print("tnl/xp floor invariant regression passed")