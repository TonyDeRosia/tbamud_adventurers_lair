#!/usr/bin/env python3
"""Validate that ability names resolve to static help topics before live fallback.

This mirrors runtime help lookup behavior in:
- load_help() tokenization/min level parsing
- search_help()/search_help_flexible() matching
- do_help() fallback order
"""

from __future__ import annotations

import re
from dataclasses import dataclass
from pathlib import Path
from typing import List, Tuple

ROOT = Path(__file__).resolve().parents[1]
HELP_PATH = ROOT / "lib" / "text" / "help" / "help.hlp"
SPELL_PARSER_PATH = ROOT / "src" / "spell_parser.c"


@dataclass
class HelpEntry:
    keyword: str
    min_level: int


def strn_cmp(a: str, b: str, n: int) -> int:
    al = a[:n].lower()
    bl = b[:n].lower()
    return (al > bl) - (al < bl)


def normalize_help_keyword(src: str) -> str:
    out: List[str] = []
    last_was_sep = True
    for ch in src:
        if ch.isalnum():
            out.append(ch.lower())
            last_was_sep = False
            continue
        if not last_was_sep:
            out.append("-")
            last_was_sep = True

    while out and out[-1] == "-":
        out.pop()
    return "".join(out)


def load_help_keywords() -> List[HelpEntry]:
    lines = HELP_PATH.read_text(encoding="utf-8", errors="ignore").splitlines()
    idx = 0
    entries: List[HelpEntry] = []

    while idx < len(lines):
        key = lines[idx]
        idx += 1

        if key.startswith("$"):
            break

        key = key.lstrip()
        if not key or key.startswith("!"):
            continue

        while idx < len(lines) and not lines[idx].startswith("#"):
            idx += 1

        min_level = 0
        if idx < len(lines):
            m = re.match(r"#(\d+)", lines[idx])
            if m:
                min_level = int(m.group(1))
            idx += 1

        for keyword in key.split():
            entries.append(HelpEntry(keyword=keyword, min_level=min_level))

    entries.sort(key=lambda h: h.keyword.lower())
    return entries


def search_help(entries: List[HelpEntry], argument: str, level: int) -> int:
    if not entries or not argument:
        return -1

    bot = 0
    top = len(entries) - 1
    minlen = len(argument)

    while bot <= top:
        mid = (bot + top) // 2
        chk = strn_cmp(argument, entries[mid].keyword, minlen)

        if chk == 0:
            while mid > 0 and strn_cmp(argument, entries[mid - 1].keyword, minlen) == 0:
                mid -= 1

            while mid < len(entries) and level < entries[mid].min_level:
                mid += 1

            if mid >= len(entries):
                break
            if strn_cmp(argument, entries[mid].keyword, minlen) != 0:
                break
            if level < entries[mid].min_level:
                break
            return mid

        if chk > 0:
            bot = mid + 1
        else:
            top = mid - 1

    return -1


def search_help_flexible(entries: List[HelpEntry], argument: str, level: int) -> int:
    exact = search_help(entries, argument, level)
    if exact != -1:
        return exact

    normalized_arg = normalize_help_keyword(argument)
    if not normalized_arg:
        return -1

    exact = search_help(entries, normalized_arg, level)
    if exact != -1:
        return exact

    for i, entry in enumerate(entries):
        if entry.min_level > level:
            continue
        normalized_key = normalize_help_keyword(entry.keyword)
        if normalized_key and normalized_key == normalized_arg:
            return i

    return -1


def parse_live_ability_names() -> List[str]:
    text = SPELL_PARSER_PATH.read_text(encoding="utf-8", errors="ignore")
    pattern = re.compile(r"\b(?:spello|skillo)\s*\([^\n]*?\"([^\"]+)\"")
    names = [m.group(1).strip() for m in pattern.finditer(text)]

    # Preserve order while deduplicating.
    seen = set()
    unique = []
    for name in names:
        if not name or name in seen:
            continue
        seen.add(name)
        unique.append(name)
    return unique


def main() -> int:
    entries = load_help_keywords()
    abilities = parse_live_ability_names()

    unresolved: List[Tuple[str, str]] = []
    for ability in abilities:
        if search_help_flexible(entries, ability, 0) == -1:
            unresolved.append((ability, normalize_help_keyword(ability)))

    required_examples = [
        "body of effulgent beryl",
        "bear spirit",
        "black lance",
        "total occultation",
        "domain break",
        "miasma",
        "shadow extraction",
        "hunters instinct",
        "appraise enemy",
    ]

    print(f"Loaded help keywords: {len(entries)}")
    print(f"Live abilities checked: {len(abilities)}")

    for example in required_examples:
        status = "STATIC" if search_help_flexible(entries, example, 0) != -1 else "FALLBACK"
        print(f"example: {example:<28} -> {status}")

    if unresolved:
        print(f"\nUnresolved abilities: {len(unresolved)}")
        for ability, normalized in unresolved:
            print(f"  - {ability} (normalized: {normalized})")
        return 1

    print("\nAll live ability names resolve to static help topics at level 0.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
