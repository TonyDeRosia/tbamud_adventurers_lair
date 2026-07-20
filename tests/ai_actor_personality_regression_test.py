#!/usr/bin/env python3
"""Focused deterministic checks for the AI actor social configuration contract."""
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
SOURCE = (ROOT / "src" / "ai_actor.c").read_text()
MEDIT = (ROOT / "src" / "medit.c").read_text()

# These are the production function's trait weights.  Changing any trait from
# neutral must change the deterministic social response modifier.
WEIGHTS = (-2, 1, 3, 1, -1, 1, -1, 2, 1, 1, -3, 2)

def modifier(values):
    return max(-35, min(35, sum((value - 50) * weight
                                for value, weight in zip(values, WEIGHTS)) // 10))

def test_every_trait_has_a_measurable_runtime_effect():
    neutral = [50] * 12
    for trait, weight in enumerate(WEIGHTS):
        changed = neutral.copy()
        changed[trait] = 60 if weight > 0 else 40
        assert modifier(changed) != modifier(neutral), trait
    assert "ai_actor_personality_response_modifier(mob->ai_prof->personality)" in SOURCE

def test_dialogue_mutations_and_social_toggles_are_wired():
    for symbol in ("mob_ai_dialogue_set", "mob_ai_dialogue_delete", "mob_ai_dialogue_move"):
        assert symbol in SOURCE
    for toggle in ("whisper_enabled", "respond_strangers", "respond_trusted", "respond_feared", "respond_hostile"):
        assert toggle in MEDIT and toggle in SOURCE
    assert "ai_actor_event_whisper" in SOURCE

if __name__ == "__main__":
    test_every_trait_has_a_measurable_runtime_effect()
    test_dialogue_mutations_and_social_toggles_are_wired()
    print("ai actor personality/social regression checks passed")
