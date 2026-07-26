"""Regression checks for BuilderBot's two-step graceful logout contract."""

from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
ACT_OTHER = (ROOT / "src/act.other.c").read_text()
HANDLER = (ROOT / "src/handler.c").read_text()
INTERPRETER = (ROOT / "src/interpreter.c").read_text()


def function_body(source, signature, next_signature):
    """Return one top-level function region delimited by stable signatures."""
    return source[source.index(signature):source.index(next_signature)]


def test_in_game_quit_saves_extracts_and_does_not_close_descriptor():
    do_quit = function_body(ACT_OTHER, "ACMD(do_quit)", "ACMD(do_save)")

    assert 'send_to_char(ch, "Goodbye, friend.. Come back soon!\\r\\n")' in do_quit
    assert "Crash_rentsave(ch, 0)" in do_quit
    assert "extract_char(ch)" in do_quit
    assert "CON_CLOSE" not in do_quit
    assert do_quit.index("Crash_rentsave(ch, 0)") < do_quit.index("extract_char(ch)")


def test_character_extraction_returns_attached_player_to_menu():
    extract = function_body(
        HANDLER, "void extract_char_final(struct char_data *ch)",
        "void extract_char(struct char_data *ch)",
    )

    state_change = "STATE(ch->desc) = CON_MENU;"
    menu_write = 'write_to_output(ch->desc, "%s", CONFIG_MENU);'
    assert state_change in extract
    assert menu_write in extract
    assert extract.index(state_change) < extract.index(menu_write)


def test_zero_at_character_menu_sends_goodbye_then_closes():
    menu = INTERPRETER[
        INTERPRETER.index("case CON_MENU:"):
        INTERPRETER.index("case CON_CHPWD_GETOLD:")
    ]

    zero = menu[menu.index("case '0':"):menu.index("case '1':")]
    goodbye = 'write_to_output(d, "Goodbye.\\r\\n");'
    close = "STATE(d) = CON_CLOSE;"
    assert goodbye in zero
    assert close in zero
    assert zero.index(goodbye) < zero.index(close)
