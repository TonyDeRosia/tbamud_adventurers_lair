#!/usr/bin/env python3
"""Focused static contracts for the read-only XP/reward audit."""
from pathlib import Path
import sys

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tests"))
import xp_reward_audit as audit

FIGHT = (ROOT / "src" / "fight.c").read_text(encoding="utf-8")
LIMITS = (ROOT / "src" / "limits.c").read_text(encoding="utf-8")
MEDIT = (ROOT / "src" / "medit.c").read_text(encoding="utf-8")

expected = {
    -20: 1, -15: 1, -10: 3, -8: 5, -5: 15, -3: 40, -2: 60,
    -1: 90, 0: 120, 1: 150, 2: 180, 3: 220, 4: 260, 5: 300, 6: 350,
}
for delta, value in expected.items():
    assert audit.base_xp(50, 50 + delta) == value

assert "bonus_xp = GET_EXP(victim);" in FIGHT
assert "exp = MAX(0, exp + bonus_xp);" in FIGHT
assert "share = MAX(0, share + bonus_xp);" in FIGHT
assert "return MAX(1, base_xp / 4);" in FIGHT
assert "live_count > RARE_KILL_MAX_COUNT" in FIGHT
assert "gain = MIN(CONFIG_MAX_EXP_GAIN, gain);" in LIMITS
assert "gain = MAX(-CONFIG_MAX_EXP_LOSS, gain);" in LIMITS
assert "Bonus XP is added on top of live kill XP." in MEDIT

records = audit.parse_mobs()
assert len(records) == 3736
assert next(r for r in records if r["vnum"] == 16400)["bonus_xp"] == 0
print("xp reward audit calculation and source-contract checks passed")
