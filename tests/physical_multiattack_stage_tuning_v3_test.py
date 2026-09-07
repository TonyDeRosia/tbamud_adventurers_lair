from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[1]
H = (ROOT / "src" / "combat_progression.h").read_text(encoding="utf-8")
C = (ROOT / "src" / "combat_progression.c").read_text(encoding="utf-8")
F = (ROOT / "src" / "fight.c").read_text(encoding="utf-8")
M = (ROOT / "src" / "medit.c").read_text(encoding="utf-8")

def section(text, start, end):
    a = text.index(start)
    b = text.index(end, a)
    return text[a:b]

def define(name):
    m = re.search(rf"^#define\s+{re.escape(name)}\s+(\d+)\s*$", H, re.M)
    assert m, name
    return int(m.group(1))

assert define("COMBAT_PROGRESSION_PHYSICAL_STAGE_DOUBLE") == 75
assert define("COMBAT_PROGRESSION_PHYSICAL_STAGE_TRIPLE") == 50
assert define("COMBAT_PROGRESSION_PHYSICAL_STAGE_FOURTH") == 25

assert define("COMBAT_PROGRESSION_STAGE_FULL") == 100
assert define("COMBAT_PROGRESSION_STAGE_SECOND") == 70
assert define("COMBAT_PROGRESSION_STAGE_THIRD") == 50

assert define("COMBAT_PROGRESSION_MULTICAST_STAGE_DOUBLE") == 75
assert define("COMBAT_PROGRESSION_MULTICAST_STAGE_TRIPLE") == 50
assert define("COMBAT_PROGRESSION_MULTICAST_STAGE_FOURTH") == 25
assert define("COMBAT_PROGRESSION_MULTICAST_DAMAGE_SECOND") == 80
assert define("COMBAT_PROGRESSION_MULTICAST_DAMAGE_THIRD") == 65
assert define("COMBAT_PROGRESSION_MULTICAST_DAMAGE_FOURTH") == 50

helper = section(
    F,
    "static void perform_bonus_mainhand_attack",
    "static int next_haste_damage_percent",
)
assert "if (!ch || IS_NPC(ch))" not in helper
assert "if (!ch)" in helper
assert "hit(ch, victim, TYPE_UNDEFINED);" in helper

chain = section(F, "static void do_double_attack", "/* dual wield offhand system */")
assert "GET_MOB_DOUBLE_ATTACK(ch)" in chain
assert "GET_MOB_TRIPLE_ATTACK(ch)" in chain
assert "GET_MOB_FOURTH_ATTACK(ch)" in chain
assert "COMBAT_PROGRESSION_PHYSICAL_STAGE_DOUBLE" in chain
assert "COMBAT_PROGRESSION_PHYSICAL_STAGE_TRIPLE" in chain
assert "COMBAT_PROGRESSION_PHYSICAL_STAGE_FOURTH" in chain
assert chain.count("perform_bonus_mainhand_attack(ch);") == 3
assert chain.count("if (combat_effects_due && !IS_NPC(ch))") == 3

double_stage = chain.index("COMBAT_PROGRESSION_PHYSICAL_STAGE_DOUBLE")
first_swing = chain.index("perform_bonus_mainhand_attack(ch);")
triple_stage = chain.index("COMBAT_PROGRESSION_PHYSICAL_STAGE_TRIPLE")
second_swing = chain.index("perform_bonus_mainhand_attack(ch);", first_swing + 1)
fourth_stage = chain.index("COMBAT_PROGRESSION_PHYSICAL_STAGE_FOURTH")
third_swing = chain.index("perform_bonus_mainhand_attack(ch);", second_swing + 1)
assert double_stage < first_swing < triple_stage < second_swing < fourth_stage < third_swing

physical = section(
    C,
    "int combat_progression_physical_multiattack_chance_basis_points",
    "bool combat_progression_physical_multiattack_roll",
)
assert "if (IS_NPC(ch))" in physical
assert "MAX(GET_STR(ch), GET_DEX(ch))" in physical
assert "MIN(GET_STR(ch), GET_DEX(ch))" in physical
assert "GET_CON(ch)" in physical

offhand = section(
    F,
    "static int offhand_attack_chance_basis_points",
    "static int offhand_damage_percent",
)
assert "COMBAT_PROGRESSION_STAGE_FULL" in offhand
assert "COMBAT_PROGRESSION_PHYSICAL_STAGE_DOUBLE" not in offhand
assert "COMBAT_PROGRESSION_PHYSICAL_STAGE_TRIPLE" not in offhand
assert "COMBAT_PROGRESSION_PHYSICAL_STAGE_FOURTH" not in offhand

assert "Roll weights: 75%% / 50%% / 25%%." in M
assert "Later stages only roll after the previous succeeds." in M

print("Physical Multiattack Tuning V3 regression: PASS")