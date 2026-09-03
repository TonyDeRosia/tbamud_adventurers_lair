#!/usr/bin/env python3
"""Focused static contracts for the read-only XP/reward audit."""
from pathlib import Path
import re
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

assert "npc_kill_raw_xp" in FIGHT
assert "raw += MAX(0, GET_EXP(victim));" in FIGHT
assert "MOB_FLAGGED(victim, MOB_RARE) ? MAX(1, base_xp / 4) : 0" in FIGHT
assert "count_live_mobs_by_vnum" not in FIGHT
assert "final_gain = final_positive_xp_gain" in FIGHT
assert "final_positive_xp_gain" in LIMITS
assert "modified += (modified * HAPPY_EXP) / 100" in LIMITS
assert "gain = final_positive_xp_gain(gain);" in LIMITS
assert "gain = MAX(-CONFIG_MAX_EXP_LOSS, gain);" in LIMITS
assert "Bonus XP is added on top of live kill XP." in MEDIT

records = audit.parse_mobs()
expected_vnums = {
    int(match.group(1))
    for path in (ROOT / "lib" / "world" / "mob").glob("*.mob")
    for match in re.finditer(r"^#(\d+)\s*$", path.read_text(encoding="utf-8", errors="replace"), re.MULTILINE)
}
parsed_vnums = {int(record["vnum"]) for record in records}
assert expected_vnums
assert parsed_vnums == expected_vnums
assert len(records) == len(parsed_vnums)
assert next(r for r in records if r["vnum"] == 16400)["bonus_xp"] == 0
print("xp reward audit calculation and source-contract checks passed")
