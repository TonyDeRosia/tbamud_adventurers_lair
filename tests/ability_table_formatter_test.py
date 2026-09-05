#!/usr/bin/env python3
"""Output-oriented contracts for the shared spell/skill ability table layout."""
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
SOURCE = (ROOT / "src" / "act.other.c").read_text(encoding="utf-8")
SPELLS = (ROOT / "src" / "spell_parser.c").read_text(encoding="utf-8")


def render(rows):
    """Plain-text model of the C table renderer for representative rows."""
    left_width = max((len(name) for _, name, _ in rows[::2]), default=0)
    right_width = max((len(name) for _, name, _ in rows[1::2]), default=0)
    out, index = [], 0
    while index < len(rows):
        level = rows[index][0]
        group = []
        while index < len(rows) and rows[index][0] == level:
            group.append(rows[index])
            index += 1
        for offset in range(0, len(group), 2):
            cells = []
            for col, (_, name, pct) in enumerate(group[offset:offset + 2]):
                width = left_width if col == 0 else right_width
                score = "[ -- ]" if pct < 0 else f"[{pct:3d}%]"
                cells.append(f"{name:<{width}} {score}")
            prefix = f"Level {level:<2}: " if offset == 0 else "          "
            out.append(prefix + "  ".join(cells))
    return "\n".join(out)


def test_shared_renderer_and_no_legacy_truncation_branch():
    start = SOURCE.index("void show_ability_table_aligned")
    body = SOURCE[start:SOURCE.index("static void show_ability_filter_help", start)]
    assert "name_width" not in body
    assert "%-*.*s" not in body
    assert "complete names and percentage cells fit" in body


def test_long_names_percentages_grouping_and_odd_row_output():
    output = render([
        (1, "recall", 100), (1, "study", 1), (1, "unarmed", -1),
        (100, "sovereign pressure", 100), (100, "supreme caster discipline", 100),
        (100, "tactical spell memory", 75),
    ])
    assert "\n\n" not in output
    for name in ("sovereign pressure", "supreme caster discipline", "tactical spell memory"):
        assert name in output
    assert output.count("Level 1 ") == 1
    assert output.count("Level 100") == 1
    assert "[100%]" in output and "[  1%]" in output and "[ -- ]" in output
    assert output.splitlines()[-1].startswith("          tactical spell memory")


def test_current_skill_names_and_spell_path_remain_present():
    assert "supreme caster discipline" in SPELLS
    assert "tactical spell memory" in SPELLS
    assert "show_ability_table_aligned(ch, 0, show_all, filter);" in SOURCE
    assert "show_ability_table_aligned(ch, 1, show_all, filter);" in SOURCE


def test_practice_and_skill_commands_keep_their_existing_table_paths():
    practice = SOURCE[SOURCE.index("ACMD(do_practice)"):SOURCE.index("ACMD(do_buypractice)")]
    skills = SOURCE[SOURCE.index("ACMD(do_skills)"):SOURCE.index("ACMD(do_spellbook)")]
    assert "list_known_abilities(ch);" in practice
    assert "show_ability_table_aligned(ch, 0, show_all, filter);" in skills


if __name__ == "__main__":
    tests = [value for name, value in globals().items() if name.startswith("test_")]
    for test in tests:
        test()
    print(f"{len(tests)} ability table formatter regression tests passed")
