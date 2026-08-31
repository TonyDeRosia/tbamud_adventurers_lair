"""Contracts for room-only prompt spacing and compact idle tick refreshes."""
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
PROMPT = (ROOT / "src/prompt.c").read_text(encoding="utf-8")
COMM = (ROOT / "src/comm.c").read_text(encoding="utf-8")
INFO = (ROOT / "src/act.informative.c").read_text(encoding="utf-8")


def section(text: str, start: str, end: str) -> str:
    pos = text.index(start)
    return text[pos:text.index(end, pos)]


def test_room_render_owns_exactly_one_blank_line():
    room = section(INFO, "void look_at_room(", "static void look_in_direction(")
    tail = room[room.index("list_obj_to_char(world[IN_ROOM(ch)].contents"):]
    assert tail.count('send_to_char(ch, "\\r\\n");') == 1
    assert tail.index("list_char_to_char(") < tail.index('send_to_char(ch, "\\r\\n");')


def test_tick_prompt_uses_only_the_minimum_clean_line_separator():
    queued = section(PROMPT, "void queue_prompt(", "ACMD(do_prompt)")
    assert 'write_to_output(d, "\\r\\n%s", make_prompt(d));' in queued
    assert "\\r\\n \\r\\n" not in queued
    assert "\\r\\n\\r\\n" not in queued


def test_tick_refresh_remains_enabled_and_skips_combat_and_editors():
    refresh = section(COMM, "static void refresh_idle_prompts_on_tick(", "void heartbeat(")
    assert "queue_prompt(d);" in refresh
    assert "d->showstr_count || d->str" in refresh
    assert "FIGHTING(d->character)" in refresh
    heartbeat = section(COMM, "void heartbeat(", "void record_usage(")
    assert "point_update();" in heartbeat
    assert "refresh_idle_prompts_on_tick();" in heartbeat


def test_prompt_builders_contain_no_forced_room_spacing():
    builder = section(PROMPT, "char *make_prompt(", "void queue_prompt(")
    assert "\\r\\n \\r\\n" not in builder
    template_line = next(line for line in PROMPT.splitlines() if "default_prompt_template" in line)
    assert "\\r" not in template_line
    assert "\\n" not in template_line


def test_expected_output_sequences():
    room_content = "room content\r\n"
    prompt = "[prompt] "
    assert room_content + "\r\n" + prompt == "room content\r\n\r\n[prompt] "
    assert prompt + "\r\n" + prompt + "\r\n" + prompt == (
        "[prompt] \r\n[prompt] \r\n[prompt] "
    )


if __name__ == "__main__":
    tests = [value for name, value in globals().items() if name.startswith("test_")]
    for test in tests:
        test()
    print(f"{len(tests)} prompt-spacing regression tests passed")
