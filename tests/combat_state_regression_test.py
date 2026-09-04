"""Regression contracts for opening initiative and normal combat teardown."""
from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[1]
FIGHT = (ROOT / "src/fight.c").read_text(encoding="utf-8")
OFFENSIVE = (ROOT / "src/act.offensive.c").read_text(encoding="utf-8")
HANDLER = (ROOT / "src/handler.c").read_text(encoding="utf-8")
INTERPRETER = (ROOT / "src/interpreter.c").read_text(encoding="utf-8")


def section(text, start, end):
    pos = text.index(start)
    return text[pos:text.index(end, pos)]


def command_entries():
    pattern = re.compile(
        r'\{\s*"([^"]+)"\s*,\s*"([^"]+)"\s*,[^,]+,\s*([^,\s]+)'
    )
    return pattern.findall(INTERPRETER)


def resolve(command):
    for name, _minimum_abbreviation, handler in command_entries():
        if name.startswith(command):
            return name, handler
    raise AssertionError(f"command did not resolve: {command}")


def test_cast_and_circle_prefix_order():
    assert resolve("c") == ("cast", "do_cast")
    assert resolve("ci") == ("circle", "do_circle")
    assert resolve("circle") == ("circle", "do_circle")


def test_both_opening_initiative_paths_call_hit():
    do_hit = section(OFFENSIVE, "ACMD(do_hit)", "ACMD(do_kill)")
    assert "hit(ch, vict, TYPE_UNDEFINED);" in do_hit
    assert "hit(vict, ch, TYPE_UNDEFINED);" in do_hit


def test_damage_unconditionally_establishes_both_combat_links_even_on_a_miss():
    damage = section(FIGHT, "int damage(", "static int compute_thaco")
    establish = section(damage, "if (victim != ch) {", "/* If you attack a pet")
    assert "if (!IS_NPC(ch) && ch != victim)\n\n  if (victim != ch) {" not in damage
    assert "if (GET_POS(ch) > POS_STUNNED && (FIGHTING(ch) == NULL))" in establish
    assert "set_fighting(ch, victim);" in establish
    assert "if (GET_POS(victim) > POS_STUNNED && (FIGHTING(victim) == NULL))" in establish
    assert "set_fighting(victim, ch);" in establish
    # Establishment occurs before hit-point subtraction, so dam == 0 still fights.
    assert damage.index("if (victim != ch) {") < damage.index("GET_HIT(victim) -= dam;")


def test_combat_list_membership_and_multiple_round_dispatch_are_retained():
    set_fighting = section(FIGHT, "void set_fighting", "void stop_fighting")
    violence = FIGHT[FIGHT.index("void perform_violence(void)"):]
    assert "ch->next_fighting = combat_list;" in set_fighting
    assert "combat_list = ch;" in set_fighting
    assert "FIGHTING(ch) = vict;" in set_fighting
    assert "for (ch = combat_list; ch; ch = next_combat_list)" in violence
    assert "next_combat_list = ch->next_fighting;" in violence
    assert "hit(ch, FIGHTING(ch), TYPE_UNDEFINED);" in violence
    assert "combat_list = NULL" not in violence


def test_legitimate_teardown_paths_remain_intact():
    raw_kill = section(FIGHT, "void raw_kill", "void die(")
    stop = section(FIGHT, "void stop_fighting", "static void make_corpse")
    flee = section(OFFENSIVE, "ACMD(do_flee)", "ACMD(do_bash)")
    char_from_room = section(HANDLER, "void char_from_room", "void char_to_room")
    char_to_room = section(HANDLER, "void char_to_room", "void obj_to_char")
    violence = FIGHT[FIGHT.index("void perform_violence(void)"):]
    assert "if (FIGHTING(ch))\n    stop_fighting(ch);" in raw_kill
    assert "REMOVE_FROM_LIST(ch, combat_list, next_fighting);" in stop
    assert "FIGHTING(ch) = NULL;" in stop
    assert "stop_fighting(ch);" in flee
    assert "stop_fighting(was_fighting);" in flee
    assert "stop_fighting(ch);" in char_from_room
    assert "stop_fighting(FIGHTING(ch));" in char_to_room
    assert "IN_ROOM(ch) != IN_ROOM(FIGHTING(ch))" in violence
    assert "stop_fighting(ch);" in violence


if __name__ == "__main__":
    tests = [value for name, value in globals().items() if name.startswith("test_")]
    for test in tests:
        test()
    print(f"{len(tests)} combat-state regression tests passed")
