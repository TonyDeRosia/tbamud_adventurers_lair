#!/usr/bin/env python3
"""Source-level regression coverage for Phase 1D AI Actor MEDIT usability."""
from pathlib import Path

medit = Path("src/medit.c").read_text()
actor = Path("src/ai_actor.c").read_text()

# Capability choices are named, described, and never expose raw enum entry prompts.
for token in ("Choose %s", "Humanoid — ordinary speaking person.",
              "Mindless — instinct only; no social relationships.",
              "Vocalize — uses authored creature sounds instead of speech.",
              "Basic Hostile Memory", "Same Kind — helps matching creatures.",
              "Q) Cancel"):
    assert token in medit, token
assert "Archetype (-1 inferred" not in medit
assert "Memory style (-1 inferred" not in medit

# Cancelling or blank input is clean and returns/redisplays the appropriate screen.
assert "if (LOWER(*arg) == 'q') { medit_disp_ai_capabilities(d); return; }" in medit
assert "if (!*arg)" in medit
assert "Choose a delay from 1 to 60 seconds, or Q to cancel" in medit

# The screen groups capabilities and makes movement delay applicability explicit.
for token in ("Identity", "Communication", "Relationships", "Status: %s",
              "Inactive — current movement mode is not Random.", "Effective: %s", "Source: %s"):
    assert token in medit, token

# Normal preview is builder-facing; technical authored/inferred controls remain advanced.
for token in ("Effective NPC Behavior", "Communication", "Intelligence", "Restrictions", "Warnings"):
    assert token in medit, token

# Diagnostics answer builder questions, including the next configuration direction.
for token in ("NPC Behavior Diagnostics", "Speech & Social", "Helping Allies", "What to configure next"):
    assert token in actor, token

print("AI Actor Phase 1D usability regression checks passed")
