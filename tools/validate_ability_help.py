#!/usr/bin/env python3
import argparse, pathlib, re, subprocess

GENERIC_MARKERS = [
    "available in the live ability table",
    "Source: Live ability table",
    "This help topic is resolved directly from the current in-game ability name.",
]


def parse_live_abilities(spell_parser_text):
    abilities = []
    for m in re.finditer(r'spello\(([^,]+),\s*"([^"]+)"', spell_parser_text):
        abilities.append((m.group(2).strip(), "Spell", m.group(1).strip()))
    for m in re.finditer(r'skillo(?:_cost)?\(([^,]+),\s*"([^"]+)"', spell_parser_text):
        abilities.append((m.group(2).strip(), "Skill", m.group(1).strip()))
    out = {}
    for name, typ, const in abilities:
        out[name.lower()] = (name, typ, const)
    return sorted(out.values(), key=lambda x: x[0].lower())


def parse_help_entries(help_text):
    entries, cur = [], []
    for line in help_text.splitlines():
        if line.startswith("#") and line[1:].isdigit():
            if cur:
                entries.append("\n".join(cur))
                cur = []
        else:
            cur.append(line)
    if cur:
        entries.append("\n".join(cur))

    by_keyword = {}
    for e in entries:
        lines = e.splitlines()
        if not lines:
            continue
        for kw in lines[0].split():
            by_keyword.setdefault(kw.lower(), []).append(e)
    return by_keyword


def entry_is_generic(entry):
    lower = entry.lower()
    return any(m.lower() in lower for m in GENERIC_MARKERS)


def coverage(abilities, by_keyword):
    total = len(abilities)
    spells_total = sum(1 for _, typ, _ in abilities if typ == "Spell")
    skills_total = sum(1 for _, typ, _ in abilities if typ == "Skill")
    unresolved = []
    generic = []
    static = 0
    static_spells = 0
    static_skills = 0
    for name, typ, _ in abilities:
        keys = [name.lower(), name.lower().replace(" ", "-")]
        matched = []
        for k in keys:
            matched.extend(by_keyword.get(k, []))
        if not matched:
            unresolved.append((name, typ, "missing static topic"))
            continue
        if all(entry_is_generic(e) for e in matched):
            generic.append((name, typ, "only generic/static fallback-style text"))
        else:
            static += 1
            if typ == "Spell":
                static_spells += 1
            else:
                static_skills += 1
    return {
        "total": total,
        "spells_total": spells_total,
        "skills_total": skills_total,
        "static": static,
        "static_spells": static_spells,
        "static_skills": static_skills,
        "unresolved": unresolved,
        "generic": generic,
    }


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--compare-head", action="store_true")
    args = ap.parse_args()

    spell_parser = pathlib.Path("src/spell_parser.c").read_text()
    abilities = parse_live_abilities(spell_parser)
    help_path = pathlib.Path("lib/text/help/help.hlp")
    current_help = help_path.read_text(errors="ignore")

    current = coverage(abilities, parse_help_entries(current_help))

    if args.compare_head:
        base_help = subprocess.check_output(["git", "show", "HEAD:lib/text/help/help.hlp"], text=True)
        base = coverage(abilities, parse_help_entries(base_help))
        print("Before (HEAD):")
        print(f"  total abilities: {base['total']} (spells: {base['spells_total']}, skills: {base['skills_total']})")
        print(
            f"  with detailed static help: {base['static']} "
            f"(spells: {base['static_spells']}, skills: {base['static_skills']})"
        )
        print(f"  unresolved (missing): {len(base['unresolved'])}")
        print(f"  unresolved (generic-only): {len(base['generic'])}")
        print("After (working tree):")

    print(f"  total abilities: {current['total']} (spells: {current['spells_total']}, skills: {current['skills_total']})")
    print(
        f"  with detailed static help: {current['static']} "
        f"(spells: {current['static_spells']}, skills: {current['static_skills']})"
    )
    print(f"  unresolved (missing): {len(current['unresolved'])}")
    print(f"  unresolved (generic-only): {len(current['generic'])}")

    if current["unresolved"] or current["generic"]:
        print("\nRemaining unresolved abilities:")
        for row in current["unresolved"] + current["generic"]:
            print(f"  - {row[0]} [{row[1]}]: {row[2]}")
    else:
        print("\nRemaining generic fallback abilities:")
        print("  - (none)")


if __name__ == "__main__":
    main()
