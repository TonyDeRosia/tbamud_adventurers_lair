#!/usr/bin/env python3
"""Regression guards for the creature-sound MEDIT workflow."""
from pathlib import Path

medit = Path("src/medit.c").read_text()
actor = Path("src/ai_actor.c").read_text()
header = Path("src/ai_actor.h").read_text()

# The validator distinguishes each builder-correctable failure and does not
# impose punctuation or sentence grammar on room messages.
for text in (
    "Creature sound cannot be empty.",
    "Creature sound is too long. Maximum length is 200 characters.",
    "Creature sound contains unsupported control characters.",
    "Enter the room message for this creature sound.",
    "Slurrrrpp!",
    "The green gelatinous blob gurgles wetly.",
    "Creature sound added.",
):
    assert text in medit, text
assert "AI_VOCALIZATION_LINE_MAX 201" in header
assert "isspace((unsigned char)end[-1])" in actor
assert "((unsigned char)*p<32)" in actor

# A malformed cooldown stays in its editor; valid input confirms both values.
for text in (
    "Enter both minimum and maximum, for example: 30 90.",
    "Maximum cooldown must be at least the minimum.",
    "Minimum cooldown must be at least 5 seconds.",
    "Maximum cooldown cannot exceed 3600 seconds.",
    "Cooldown set to: %d-%d seconds.",
):
    assert text in medit, text

# Contextual screens and direct selection are visible to builders.
for text in (
    "Creature Sounds (%d/%d)",
    "No creature sounds have been authored.",
    "Enter a sound number to edit it.",
    "Creature Sounds Preview",
    "Creature Sounds is enabled, but no active sounds are authored.",
    "Presence Requirement : %s",
    "Frequency            : %s",
    "Per-room Limit       : %s",
    "MEDIT_AI_VOCALIZATION_INDEX",
):
    assert text in medit, text

print("AI Actor creature-sound usability regression checks passed")
