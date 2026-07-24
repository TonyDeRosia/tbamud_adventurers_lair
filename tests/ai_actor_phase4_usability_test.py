"""Source-level regression coverage for AI Actor builder polish."""
from pathlib import Path

medit = Path("src/medit.c").read_text()
runtime = Path("src/ai_actor.c").read_text()

traits = (
    "How likely the NPC is to respond with force instead of diplomacy.",
    "How willing the NPC is to continue fighting despite danger.",
    "How readily the NPC starts or joins conversations.",
    "How interested the NPC is in investigating unusual events.",
    "How closely the NPC follows routines and responsibilities.",
    "How truthful the NPC tends to be during dialogue.",
    "How strongly rewards influence decisions.",
    "How much concern the NPC shows toward others.",
    "How resistant the NPC is to betrayal.",
    "How long the NPC tolerates delays or annoyance.",
    "How quickly strangers become potential threats.",
    "How strongly insults and status affect reactions.",
)
assert len(set(traits)) == 12
assert all(text in medit for text in traits)
assert "medit_disp_ai_help" in medit and "Press ENTER to return." in medit
assert "MEDIT_AI_HELP" in medit and "medit_return_from_ai_help" in medit
assert "Balanced profile" in medit and "Aggressive personality" in medit
assert 'static const char *names[] = { "Generic", "Guard"' in runtime
assert "Remains in its room unless moved by combat, scripts, or schedules." in runtime
assert "Validation Information" in medit
assert "Personality Presets" in medit and "Friendly Merchant" in medit
assert "Current value: %d" in medit
print("AI Actor Phase 4 usability regression checks passed")
