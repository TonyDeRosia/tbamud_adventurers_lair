"""Deterministic source audit for the immortal push/social collision."""
from pathlib import Path
import re


ROOT = Path(__file__).resolve().parents[1]
INTERPRETER = (ROOT / "src/interpreter.c").read_text()
WIZARD = (ROOT / "src/act.wizard.c").read_text()
SOCIAL = (ROOT / "src/act.social.c").read_text()


def test_push_is_a_grgod_command_and_socials_are_still_loaded():
    assert re.search(
        r'\{ "push"\s*, "push"\s*, POS_DEAD\s*, do_push\s*, LVL_GRGOD, 0 \}',
        INTERPRETER,
    )
    assert "complete_cmd_info[k].command_pointer\t= do_action;" in SOCIAL


def test_accessible_real_commands_are_resolved_before_social_fallback():
    real = INTERPRETER.index("command_pointer != do_action", INTERPRETER.index("void command_interpreter"))
    level = INTERPRETER.index("GET_LEVEL(ch) >= complete_cmd_info[cmd].minimum_level", real)
    fallback = INTERPRETER.index("command_pointer == do_action", level)
    assert real < level < fallback


def test_push_uses_descriptor_safe_persistence_and_canonical_extraction():
    push = WIZARD[WIZARD.index("static void perform_push"):WIZARD.index("/* AFFREMOVE COMMAND BEGIN */")]
    assert "if (vict == ch)" in push
    assert "if (vict->desc)" in push
    assert "if (!save_char(vict))" in push
    assert push.index("save_char(vict)") < push.index("Crash_rentsave(vict, 0)") < push.index("extract_char(vict)")
    assert "command_interpreter" not in push
    assert "do_quit" not in push
    assert "matches > 1" in push


def test_pull_still_loads_a_linkless_player_and_restores_objects():
    pull = WIZARD[WIZARD.index("ACMD(do_pull)"):WIZARD.index("static void perform_push")]
    assert "vict->desc = NULL;" in pull
    assert "load_char(arg, vict)" in pull
    assert "char_to_room(vict, IN_ROOM(ch));" in pull
    assert "Crash_load(vict);" in pull
