#!/usr/bin/env python3
"""Source-level UX regression checks for Builder Diagnostics."""
from pathlib import Path

medit = Path("src/medit.c").read_text()
actor = Path("src/ai_actor.c").read_text()

assert "Builder Diagnostics" in medit
assert "NPC Behavior Diagnostics" in actor
assert medit.index("Builder Diagnostics") < medit.index("Preview Compiled Profile")

# The concise page begins with the quick health summary and groups warnings.
diagnostics = actor.split("NPC Behavior Diagnostics", 1)[1]
assert diagnostics.index("Overall Status") < diagnostics.index("Movement")
assert diagnostics.rindex("Builder Warnings") > diagnostics.index("Restrictions")
for heading in ("Movement", "Combat", "Memory", "Helping Allies", "Speech & Social", "Restrictions", "Help"):
    assert heading in diagnostics, heading

# Builder-facing ordering and conditional restrictions remain intentional.
assert diagnostics.index("AI Memory") < diagnostics.index("Legacy MEMORY")
assert diagnostics.index("AI Assistance") < diagnostics.index("Legacy HELPER")
assert '"None\\r\\n"' in diagnostics
assert "MOB_SENTINEL" in diagnostics and "MOB_STAY_ZONE" in diagnostics

# Engine terminology is available from Help, not the concise diagnostics output.
technical = actor.split("if (detailed)", 1)[1].split("NPC Behavior Diagnostics", 1)[0]
for phrase in ("pulse ownership", "legacy mobile tail", "override mask"):
    assert phrase in technical.lower(), phrase
for phrase in ("pulse ownership", "legacy tail", "runtime consumer", "override mask"):
    assert phrase not in diagnostics.lower(), phrase

assert "MEDIT_AI_COMPATIBILITY" in medit
assert "ai_actor_compatibility_report(OLC_MOB(d), report, sizeof(report), TRUE)" in medit
print("AI Actor Builder Diagnostics usability checks passed")
