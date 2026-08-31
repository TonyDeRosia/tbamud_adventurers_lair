#!/usr/bin/env python3
"""Read-only inventory of mobile Bonus XP values and live-kill reward math.

This is intentionally an audit aid, not a migration tool.  It reads ASCII mob
prototypes and emits CSV or a concise summary; it never writes world data.
"""
from __future__ import annotations

import argparse
import csv
import signal
import re
import sys
from collections import Counter, defaultdict
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
MOB_DIR = ROOT / "lib" / "world" / "mob"

# Behave like ordinary Unix text tools when a caller deliberately truncates
# CSV output (for example with head), rather than presenting a traceback.
if hasattr(signal, "SIGPIPE"):
    signal.signal(signal.SIGPIPE, signal.SIG_DFL)


def base_xp(attacker_level: int, victim_level: int) -> int:
    """Mirror mob_kill_base_xp_for_levels() in src/fight.c."""
    delta = victim_level - attacker_level
    if delta <= -15:
        return 1
    if delta <= -10:
        return 3
    if delta <= -8:
        return 5
    if delta <= -5:
        return 15
    if delta <= -3:
        return 40
    if delta == -2:
        return 60
    if delta == -1:
        return 90
    if delta == 0:
        return 120
    if delta == 1:
        return 150
    if delta == 2:
        return 180
    if delta == 3:
        return 220
    if delta == 4:
        return 260
    if delta == 5:
        return 300
    return 350


def parse_mobs() -> list[dict[str, object]]:
    """Read enhanced ASCII mobs: after four tilde strings, line 2 is gold/xp."""
    records: list[dict[str, object]] = []
    for path in sorted(MOB_DIR.glob("*.mob")):
        zone = int(path.stem)
        lines = path.read_text(encoding="utf-8", errors="replace").splitlines()
        i = 0
        while i < len(lines):
            if not lines[i].startswith("#"):
                i += 1
                continue
            if lines[i] == "$":
                break
            try:
                vnum = int(lines[i][1:])
            except ValueError:
                i += 1
                continue
            start = i
            i += 1
            strings = []
            for _ in range(4):
                parts = []
                while i < len(lines):
                    parts.append(lines[i])
                    done = lines[i].endswith("~")
                    i += 1
                    if done:
                        break
                strings.append("\n".join(parts).rstrip("~"))
            if i + 2 >= len(lines):
                raise ValueError(f"truncated mob #{vnum} in {path}")
            flags = lines[i].split()
            stats = lines[i + 1].split()
            gold_exp = lines[i + 2].split()
            if len(flags) < 10 or len(stats) < 1 or len(gold_exp) < 2:
                raise ValueError(f"unexpected record layout for #{vnum} in {path} at line {start + 1}")
            level = int(stats[0])
            bonus_xp = int(gold_exp[1])
            records.append({
                "vnum": vnum,
                "zone": zone,
                "level": level,
                "bonus_xp": bonus_xp,
                "name": strings[1].replace("\n", " ").strip(),
            })
            i += 3
            # E-specs and trigger blocks end at E; resume at the next # marker.
            while i < len(lines) and not lines[i].startswith("#") and lines[i] != "$":
                i += 1
    return records


def classify(record: dict[str, object], zone_nonzero: list[dict[str, object]]) -> tuple[str, str]:
    """Conservative, evidence-only classification; never treats magnitude as proof."""
    xp = int(record["bonus_xp"])
    level = int(record["level"])
    if xp == 0:
        return "NORMAL", "zero additive Bonus XP"
    if int(record["vnum"]) == 16400 and xp == 7:
        return "INTENTIONAL_CUSTOM", "known low custom additive value; equal-level preview is 127"
    legacy_value = level * level * 100
    same_formula = [r for r in zone_nonzero if int(r["bonus_xp"]) == int(r["level"]) ** 2 * 100]
    if xp == legacy_value and len(same_formula) >= 3 and len(same_formula) * 2 >= len(zone_nonzero):
        return "HIGH_CONFIDENCE_LEGACY", "matches level^2*100 and this zone is formula-dominant"
    if xp == legacy_value:
        return "AMBIGUOUS", "matches a possible legacy level^2*100 value but lacks zone-wide corroboration"
    return "AMBIGUOUS", "nonzero additive value; source data alone does not establish intent"


def emit(records: list[dict[str, object]], csv_mode: bool) -> None:
    by_zone: dict[int, list[dict[str, object]]] = defaultdict(list)
    for record in records:
        if int(record["bonus_xp"]) != 0:
            by_zone[int(record["zone"])].append(record)
    rows = []
    for record in records:
        category, reason = classify(record, by_zone[int(record["zone"])])
        rows.append({**record, "classification": category, "reason": reason,
                     "equal_level_live_xp": base_xp(int(record["level"]), int(record["level"])),
                     "equal_level_total_xp": base_xp(int(record["level"]), int(record["level"])) + int(record["bonus_xp"])})
    if csv_mode:
        writer = csv.DictWriter(sys.stdout, fieldnames=[
            "vnum", "zone", "level", "bonus_xp", "equal_level_live_xp",
            "equal_level_total_xp", "classification", "reason", "name"])
        writer.writeheader()
        writer.writerows(rows)
        return
    counts = Counter(row["classification"] for row in rows)
    nonzero = [row for row in rows if int(row["bonus_xp"]) != 0]
    print(f"mob prototypes: {len(rows)}")
    print(f"nonzero Bonus XP: {len(nonzero)}")
    print("classification counts: " + ", ".join(f"{key}={counts[key]}" for key in sorted(counts)))
    print("top 25 additive Bonus XP values:")
    for row in sorted(nonzero, key=lambda item: int(item["bonus_xp"]), reverse=True)[:25]:
        print(f"  #{row['vnum']} zone {row['zone']} level {row['level']}: +{row['bonus_xp']} "
              f"(equal-level total {row['equal_level_total_xp']}) [{row['classification']}] {row['reason']}")


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--csv", action="store_true", help="emit every prototype as CSV")
    args = parser.parse_args()
    emit(parse_mobs(), args.csv)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
