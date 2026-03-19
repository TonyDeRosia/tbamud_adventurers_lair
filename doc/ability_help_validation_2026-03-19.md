# Ability Help Validation (2026-03-19)

This pass re-ran a full spell/skill help coverage audit against live abilities from `src/spell_parser.c` and static help entries in `lib/text/help/help.hlp`.

## Commands run

- `make -C src -j4`
- `python3 tools/validate_ability_help.py`

## Coverage summary

- Total live spells found: **226**
- Total live skills found: **29**
- Total live abilities found: **255**
- Generic fallback-only abilities before this pass (HEAD): **0**
- Generic fallback-only abilities after this pass: **0**
- Missing static help entries after this pass: **0**

## Remaining generic fallback abilities

- **None**

## Notes

- The validator now reports spell/skill split counts directly and prints an explicit `(none)` line when no generic fallback abilities remain.
