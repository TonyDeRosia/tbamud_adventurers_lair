"""Contracts for one normal prompt after a completed violence round."""
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
FIGHT = (ROOT / "src/fight.c").read_text(encoding="utf-8")
PROMPT = (ROOT / "src/prompt.c").read_text(encoding="utf-8")
COMM = (ROOT / "src/comm.c").read_text(encoding="utf-8")
STRUCTS = (ROOT / "src/structs.h").read_text(encoding="utf-8")


def section(text, start, end):
    pos = text.index(start)
    return text[pos:text.index(end, pos)]


def test_round_marker_is_descriptor_owned_and_runtime_only():
    assert "unsigned long combat_prompt_round" in STRUCTS
    assert "static unsigned long combat_prompt_round" in FIGHT


def test_only_perform_violence_owns_combat_round_dispatch():
    violence = FIGHT[FIGHT.index("void perform_violence(void)"):]
    assert "queue_combat_round_prompts(prompt_round);" in violence
    assert FIGHT.count("queue_combat_round_prompts(") == 3  # declaration, definition, one call
    assert "queue_prompt(" not in section(FIGHT, "void hit(", "void perform_violence(void)")


def test_both_direct_combatants_are_marked_before_actions_can_teardown_combat():
    violence = FIGHT[FIGHT.index("void perform_violence(void)"):]
    valid = violence.index("if (FIGHTING(ch) == NULL")
    marker = violence.index("mark_combat_prompt_participant(ch, prompt_round);")
    attack = violence.index("hit(ch, FIGHTING(ch), TYPE_UNDEFINED);")
    assert valid < marker < attack
    assert "mark_combat_prompt_participant(FIGHTING(ch), prompt_round);" in violence


def test_dispatch_is_deduplicated_and_preserves_prompt_safety_rules():
    dispatch = section(FIGHT, "static void queue_combat_round_prompts", "static void solo_gain")
    assert "d->combat_prompt_round != round" in dispatch
    assert "STATE(d) != CON_PLAYING" in dispatch
    assert "d->showstr_count || d->str" in dispatch
    assert "IS_NPC(d->character)" in dispatch
    assert "queue_prompt(d);" in dispatch


def test_idle_tick_policy_remains_separate_and_skips_fighting_players():
    refresh = section(COMM, "static void refresh_idle_prompts_on_tick", "void heartbeat")
    assert "FIGHTING(d->character)" in refresh
    assert "queue_prompt(d);" in refresh
    assert 'write_to_output(d, "\\r\\n%s", make_prompt(d));' in PROMPT


if __name__ == "__main__":
    tests = [value for name, value in globals().items() if name.startswith("test_")]
    for test in tests:
        test()
    print(f"{len(tests)} combat-round prompt regression tests passed")
