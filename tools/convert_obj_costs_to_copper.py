#!/usr/bin/env python3
"""One-time converter for object costs from gold units to copper.

The game now treats `Cost:` values in object files as copper, so existing
world files that store gold values must be multiplied by the configured
copper-per-gold rate.

Safeguards:
- Costs greater than or equal to the skip threshold (default: 100000) are
  treated as already converted and left alone.
- Each modified file is copied to `<original>.gold_cost.bak` before writing,
  and existing backups will cause the file to be skipped to prevent a
  double-conversion.
"""
from __future__ import annotations

import argparse
import pathlib
import re
import shutil
import sys
from typing import List, Optional, Tuple

COST_PATTERN = re.compile(
    r"^(\s*)(-?\d+)(\s+)(-?\d+)(\s+)(-?\d+)(?:(\s+)(-?\d+))?(?:(\s+)(-?\d+))?(\s*)$"
)


def detect_copper_per_gold(structs_path: pathlib.Path, fallback: int = 1000) -> int:
    """Infer a legacy copper-per-gold rate from src/structs.h, falling back on a default."""
    copper_per_silver = None
    silver_per_gold = None

    if not structs_path.exists():
        return fallback

    define_re = re.compile(r"#define\s+(COPPER_PER_SILVER|SILVER_PER_GOLD)\s+(.+)")
    for line in structs_path.read_text(encoding="utf-8").splitlines():
        match = define_re.match(line.strip())
        if not match:
            continue
        name, value = match.groups()
        value = value.replace("LL", "")
        if name == "COPPER_PER_SILVER":
            try:
                copper_per_silver = int(float(value))
            except ValueError:
                copper_per_silver = None
        elif name == "SILVER_PER_GOLD":
            try:
                silver_per_gold = int(float(value))
            except ValueError:
                silver_per_gold = None

    if copper_per_silver and silver_per_gold:
        return copper_per_silver * silver_per_gold
    return fallback


def parse_numbers(raw_line: str) -> Optional[List[int]]:
    """Return a list of ints if the line is purely numeric; otherwise None."""
    stripped = raw_line.strip()
    if not stripped:
        return None
    parts = stripped.split()
    try:
        return [int(p) for p in parts]
    except ValueError:
        return None


def convert_cost_line(raw_line: str, copper_per_gold: int, skip_threshold: int, allow_convert: bool) -> Tuple[str, bool, bool]:
    """Return (new_line, changed, matched_cost_line)."""
    if not allow_convert:
        return raw_line, False, False

    line = raw_line.rstrip("\n")
    match = COST_PATTERN.match(line)
    if not match:
        return raw_line, False, False

    (
        prefix,
        weight,
        sep_wc,
        cost,
        sep_cr,
        rent,
        sep_rl,
        level,
        sep_lt,
        timer,
        suffix,
    ) = match.groups()

    cost_value = int(cost)
    if cost_value >= skip_threshold:
        return raw_line, False, True

    new_cost = cost_value * copper_per_gold

    new_line = f"{prefix}{weight}{sep_wc}{new_cost}{sep_cr}{rent}"
    if sep_rl and level is not None:
        new_line += f"{sep_rl}{level}"
    if sep_lt and timer is not None:
        new_line += f"{sep_lt}{timer}"
    new_line += f"{suffix}\n"
    return new_line, new_cost != cost_value, True


def backup_file(path: pathlib.Path, backup_suffix: str) -> pathlib.Path:
    backup_path = path.with_suffix(path.suffix + backup_suffix)
    if backup_path.exists():
        return backup_path
    shutil.copy(path, backup_path)
    return backup_path


def convert_file(
    path: pathlib.Path,
    copper_per_gold: int,
    skip_threshold: int,
    backup_suffix: str,
    dry_run: bool,
    skip_if_backup: bool,
) -> Tuple[bool, bool, bool]:
    """Convert one file; return (converted, had_cost, skipped_due_to_backup)."""
    backup_path = path.with_suffix(path.suffix + backup_suffix)
    if skip_if_backup and backup_path.exists() and not dry_run:
        return False, False, True

    changed = False
    had_cost = False
    new_lines = []
    prev_numbers: Optional[List[int]] = None

    for line in path.read_text(encoding="utf-8").splitlines(True):
        numbers = parse_numbers(line)
        is_cost_line = bool(
            numbers
            and len(numbers) in {3, 4, 5}
            and prev_numbers
            and len(prev_numbers) == 4
        )

        new_line, converted, matched = convert_cost_line(
            line, copper_per_gold, skip_threshold, is_cost_line
        )
        new_lines.append(new_line)
        had_cost = had_cost or matched
        changed = changed or converted
        prev_numbers = numbers

    if changed and not dry_run:
        backup_file(path, backup_suffix)
        path.write_text("".join(new_lines), encoding="utf-8")
    return changed, had_cost, False


def iter_object_files(root: pathlib.Path):
    yield from sorted(root.glob("*.obj"))


def main() -> int:
    parser = argparse.ArgumentParser(description="Convert object Cost values from gold to copper.")
    parser.add_argument("--object-dir", default="lib/world/obj", type=pathlib.Path, help="Directory containing .obj files")
    parser.add_argument("--skip-threshold", type=int, default=100000, help="Costs >= threshold are treated as already converted")
    parser.add_argument("--backup-suffix", default=".gold_cost.bak", help="Suffix for backup copies")
    parser.add_argument("--copper-per-gold", type=int, default=None, help="Override copper-per-gold value")
    parser.add_argument("--dry-run", action="store_true", help="Show what would change without writing files")
    parser.add_argument(
        "--ignore-backups",
        action="store_true",
        help="Re-run conversion even if a backup file already exists",
    )
    args = parser.parse_args()

    copper_per_gold = args.copper_per_gold or detect_copper_per_gold(pathlib.Path("src/structs.h"))
    print(f"Using copper_per_gold={copper_per_gold}")

    if not args.object_dir.exists():
        print(f"Object directory {args.object_dir} not found", file=sys.stderr)
        return 1

    total_changed = 0
    processed = 0
    for obj_file in iter_object_files(args.object_dir):
        processed += 1
        changed, had_cost, skipped_backup = convert_file(
            obj_file,
            copper_per_gold,
            args.skip_threshold,
            args.backup_suffix,
            args.dry_run,
            not args.ignore_backups,
        )
        if changed:
            total_changed += 1
            action = "would convert" if args.dry_run else "converted"
            print(f"{action} {obj_file}")
        elif skipped_backup:
            print(f"skipped {obj_file} (backup already exists)")
        elif had_cost:
            print(f"skipped {obj_file} (cost already large enough)")

    print(f"Processed {processed} files; {total_changed} converted.")
    if args.dry_run:
        print("Dry run complete; no files were written.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
