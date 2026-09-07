from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[1]
SRC = ROOT / "src"

H = (SRC / "combat_progression.h").read_text(encoding="utf-8")
C = (SRC / "combat_progression.c").read_text(encoding="utf-8")
M = (SRC / "medit.c").read_text(encoding="utf-8")

def define(name: str) -> int:
    m = re.search(rf"^#define\s+{re.escape(name)}\s+(\d+)\s*$", H, re.M)
    assert m, name
    return int(m.group(1))

# Multicast stage weights now mirror physical multiattack philosophy.
assert define("COMBAT_PROGRESSION_MULTICAST_STAGE_DOUBLE") == 75
assert define("COMBAT_PROGRESSION_MULTICAST_STAGE_TRIPLE") == 50
assert define("COMBAT_PROGRESSION_MULTICAST_STAGE_FOURTH") == 25

# Bonus spell-damage packets are deliberately unchanged.
assert define("COMBAT_PROGRESSION_MULTICAST_DAMAGE_SECOND") == 80
assert define("COMBAT_PROGRESSION_MULTICAST_DAMAGE_THIRD") == 65
assert define("COMBAT_PROGRESSION_MULTICAST_DAMAGE_FOURTH") == 50

# Physical tuning remains separate and unchanged.
assert define("COMBAT_PROGRESSION_PHYSICAL_STAGE_DOUBLE") == 75
assert define("COMBAT_PROGRESSION_PHYSICAL_STAGE_TRIPLE") == 50
assert define("COMBAT_PROGRESSION_PHYSICAL_STAGE_FOURTH") == 25

# Effective multicast proficiency still combines passive skill and spell skill.
assert "passive_proficiency = MAX(0, MIN(100, passive_proficiency));" in C
assert "spell_proficiency = MAX(0, MIN(100, spell_proficiency));" in C
assert "return (passive_proficiency * spell_proficiency + 50) / 100;" in C

# NPC multicast still uses mental stats.
for marker in (
    "primary = GET_INT(ch);",
    "secondary = GET_WIS(ch);",
    "tertiary = GET_CHA(ch);",
):
    assert marker in C, marker

# Discover actual local runtime consumers instead of assuming filenames.
runtime = {}
for path in SRC.glob("*.c"):
    text = path.read_text(encoding="utf-8", errors="replace")
    stage_count = sum(
        text.count(symbol)
        for symbol in (
            "COMBAT_PROGRESSION_MULTICAST_STAGE_DOUBLE",
            "COMBAT_PROGRESSION_MULTICAST_STAGE_TRIPLE",
            "COMBAT_PROGRESSION_MULTICAST_STAGE_FOURTH",
        )
    )
    if stage_count:
        runtime[path.name] = text

assert runtime, "No runtime multicast stage consumers found"

assert any(
    all(marker in text for marker in (
        "GET_MOB_DOUBLE_CAST",
        "GET_MOB_TRIPLE_CAST",
        "GET_MOB_FOURTH_CAST",
    ))
    for name, text in runtime.items()
) or any(
    all(marker in path.read_text(encoding="utf-8", errors="replace") for marker in (
        "GET_MOB_DOUBLE_CAST",
        "GET_MOB_TRIPLE_CAST",
        "GET_MOB_FOURTH_CAST",
    ))
    for path in SRC.glob("*.c")
    if path.name != "medit.c"
), "NPC multicast runtime proficiencies not found"

assert any(
    all(marker in path.read_text(encoding="utf-8", errors="replace") for marker in (
        "SKILL_DOUBLE_CAST",
        "SKILL_TRIPLE_CAST",
        "SKILL_FOURTH_CAST",
    ))
    for path in SRC.glob("*.c")
), "Player multicast runtime proficiencies not found"

# All three stage symbols remain live somewhere in runtime source.
all_runtime_text = "\n".join(
    p.read_text(encoding="utf-8", errors="replace") for p in SRC.glob("*.c")
)
for marker in (
    "COMBAT_PROGRESSION_MULTICAST_STAGE_DOUBLE",
    "COMBAT_PROGRESSION_MULTICAST_STAGE_TRIPLE",
    "COMBAT_PROGRESSION_MULTICAST_STAGE_FOURTH",
):
    assert marker in all_runtime_text, marker

# Builder-facing description documents both roll weights and damage weights.
assert "Roll weights: 75%% / 50%% / 25%%." in M
assert "80%% / 65%% / 50%% bonus damage packets." in M

print("Multicast Stage Tuning V1 regression: PASS")