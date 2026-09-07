from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[1]
SRC = (ROOT / "src" / "interpreter.c").read_text(encoding="utf-8")

def command_names():
    names = []
    for m in re.finditer(r'^\s*\{\s*"([^"]+)"\s*,', SRC, re.M):
        name = m.group(1)
        if name == "RESERVED":
            continue
        names.append(name)
    return names

def resolves_like_live_interpreter(arg: str):
    for command in command_names():
        if not command.startswith(arg):
            continue
        if command == "light" and arg != "light":
            continue
        return command
    return None

def test_source_has_narrow_light_exact_match_exception():
    needle = (
        '!strncmp(complete_cmd_info[cmd].command, arg, length) &&\n'
        '       (strcmp(complete_cmd_info[cmd].command, "light") || !strcmp(arg, "light"))'
    )
    assert needle in SRC

def test_command_table_itself_is_unchanged():
    assert '{ "light"    , "light"   , POS_RESTING , do_light    , 0, 0 },' in SRC
    assert '{ "list"     , "lis"     , POS_STANDING, do_not_here , 0, 0 },' in SRC

def test_li_resolves_to_list():
    assert resolves_like_live_interpreter("li") == "list"

def test_lis_resolves_to_list():
    assert resolves_like_live_interpreter("lis") == "list"

def test_list_resolves_to_list():
    assert resolves_like_live_interpreter("list") == "list"

def test_light_requires_full_word():
    assert resolves_like_live_interpreter("lig") is None
    assert resolves_like_live_interpreter("ligh") is None
    assert resolves_like_live_interpreter("light") == "light"

def test_look_abbreviation_remains_unchanged():
    assert resolves_like_live_interpreter("l") == "look"

def run():
    tests = [
        test_source_has_narrow_light_exact_match_exception,
        test_command_table_itself_is_unchanged,
        test_li_resolves_to_list,
        test_lis_resolves_to_list,
        test_list_resolves_to_list,
        test_light_requires_full_word,
        test_look_abbreviation_remains_unchanged,
    ]

    for test in tests:
        test()

    print(f"{len(tests)} tests passed: LI / LIST / LIGHT abbreviation regression")

if __name__ == "__main__":
    run()