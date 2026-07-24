#!/usr/bin/env python3
"""Source-level coverage for the self-explanatory AI Actor OLC screens."""
from pathlib import Path

medit = Path("src/medit.c").read_text()
runtime = Path("src/ai_actor.c").read_text()

# The main screen labels every reachable configuration area and explains it.
main = medit.rsplit("static void medit_disp_ai_menu", 1)[1].split("static int medit_is_ai_mode", 1)[0]
for text in ("Profile Mode", "Role:", "Movement:", "Personality", "Social Behavior",
             "Dialogue Lines", "Perception", "Memory", "Threat Response",
             "Combat Reactions", "Schedules and Patrol Routines", "Preview Compiled Profile",
             "Reset to Inferred Defaults", "Configures", "H) Help  Q) Return"):
    assert text in main, text

# Canonical names and summaries are shared with the builder rather than copied.
for text in ("ai_actor_config_role_summary", "ai_actor_config_movement_summary",
             "Unknown or corrupted role value", "Unknown or corrupted movement value"):
    assert text in runtime, text
for text in ("medit_disp_ai_role", "medit_disp_ai_movement", "medit_disp_ai_mode"):
    assert text in medit, text

# Every canonical role/movement is printed with its name and an explanation.
assert "for (i = ROLE_UNKNOWN; i <= ROLE_BOSS; i++)" in medit
assert "for (i = AI_MOVE_STATIONARY; i <= AI_MOVE_RETURN_HOME; i++)" in medit
assert "SENTINEL prevents normal random movement" in medit

# Dialogue spells out the X/8 capacity and every category carries a summary.
assert "authored entries out of the maximum" in medit
assert "ai_dialogue_summaries[AI_DIALOGUE_CATEGORIES]" in medit
assert "H) Help  Q) Return" in medit

# Personality and social values include their meaning and range/cooldown units.
assert "AI Actor Personality (0-100)" in medit
assert "Minimum seconds between speech" in medit
assert "requires authored ambient speech lines" in medit

print("AI Actor menu usability regression checks passed")
