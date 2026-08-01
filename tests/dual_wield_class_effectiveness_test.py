#!/usr/bin/env python3
"""Class progression and effectiveness contracts for Dual Wield."""
from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[1]
CLASS = (ROOT / "src" / "class.c").read_text(encoding="utf-8")
ITEM = (ROOT / "src" / "act.item.c").read_text(encoding="utf-8")
FIGHT = (ROOT / "src" / "fight.c").read_text(encoding="utf-8")
HELP = (ROOT / "lib" / "text" / "help" / "help.hlp").read_text(encoding="utf-8")


assignments = re.findall(r"spell_level\(SKILL_DUAL_WIELD,\s*(CLASS_[A-Z_]+),\s*(\d+)\)", CLASS)
assert assignments == [("CLASS_THIEF", "20"), ("CLASS_WARRIOR", "15"), ("CLASS_MYSTIC", "30")]

for message in (
    "Your class cannot learn Dual Wield.",
    "You have not reached the level required to learn Dual Wield.",
    "Dual Wield is available to you, but you have not learned it",
    "your current skill is insufficient",
):
    assert message in ITEM

chance = lambda skill: min(95, 25 + skill * 3 // 4)
damage = lambda skill: 40 + skill // 2
expected = {
    0: (25, 40), 1: (25, 40), 40: (55, 60), 80: (85, 80),
    85: (88, 82), 95: (95, 87), 100: (95, 90),
}
for skill, pair in expected.items():
    assert (chance(skill), damage(skill)) == pair

assert "MIN(95, 25 + (MAX(0, skill) * 3 / 4))" in FIGHT
assert "40 + (MAX(0, skill) / 2)" in FIGHT
assert "rand_number(1, 100) > offhand_attack_chance" in FIGHT
assert "dam * offhand_damage_percent" in FIGHT

npc_guard = FIGHT[FIGHT.index("static int can_offhand_attack"):FIGHT.index("static int find_affect_modifier")]
assert npc_guard.index("if (IS_NPC(ch)) return 0") < npc_guard.index("GET_SKILL(ch, SKILL_DUAL_WIELD)")
assert "NPC offhand equipment is cosmetic" in npc_guard

assert "Warrior: level 15" in HELP
assert "Thief:   level 20" in HELP
assert "Mystic:  level 30" in HELP
assert "25 + skill * 3 / 4" in HELP
assert "40 + skill / 2" in HELP

print("dual wield class/effectiveness regression checks passed")
