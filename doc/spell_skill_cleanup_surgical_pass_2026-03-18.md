# Adventurers Lair tbaMUD - Surgical Cleanup Pass (2026-03-18)

## Scope
This note records the final narrow fixes applied in this surgical pass only:
1. Real haste detection label in `SKILL_APPRAISE_ENEMY`
2. General undead detection path in `SKILL_APPRAISE_ENEMY`
3. `MAX_SKILLS` stale boundary comment update in `src/spells.h`

## Files changed
- `src/act.offensive.c`
- `src/spells.h`
- `doc/spell_skill_cleanup_surgical_pass_2026-03-18.md`

## Phase 1 verification (actual code state before fixes)
- `AFF_HASTE` does not exist in `src/structs.h`.
- No `SPELL_HASTE` entry exists in spell constants/parser.
- The implemented haste-style buff path in current code is `SPELL_ADRENALINE_SURGE`.
- Existing undead handling was split across:
  - spell logic helper using vampire race (`RACE_VAMPIRE`) in `src/spells.c`
  - NPC undead class checks (`GET_CLASS(...) == CLASS_UNDEAD`) in runtime logic (`src/act.informative.c`, `src/fight.c`).
- `MAX_SKILLS` is `260` in `src/structs.h`; the `src/spells.h` trailing skills comment was converted to reference `MAX_SKILLS` symbolically instead of hard-coding a number.

## Surgical fixes applied
### 1) Appraise real haste detection
- Added `appraise_is_hasted()` in `src/act.offensive.c`.
- It uses `affected_by_spell(vict, SPELL_ADRENALINE_SURGE)` as the real in-code haste-style detection path.
- `SKILL_APPRAISE_ENEMY` now emits:
  - `Flag: hasted.`
- Existing `AFF_ADRENALINE` label remains separate and unchanged:
  - `Flag: adrenaline surge.`

### 2) Appraise general undead detection
- Added `appraise_is_undead()` in `src/act.offensive.c` to align appraise with existing live undead handling paths already present in code:
  - `GET_RACE(vict) == RACE_VAMPIRE`
  - `IS_NPC(vict) && GET_CLASS(vict) == CLASS_UNDEAD`
- Replaced the prior vampire-only appraise line with:
  - `Flag: undead.` when `appraise_is_undead()` is true.

### 3) MAX_SKILLS comment correction
- Updated stale hard-coded comment in `src/spells.h`:
  - from `MAX_SKILLS (260)`
  - to `MAX_SKILLS (see structs.h)`
- This now tracks real code truth without becoming stale on future limit changes.

## Compile result
Command run:
- `make -C src -j4`

Result:
- Success (no compile errors from this pass).

## Documentation consistency confirmation
This note reflects the actual post-pass code state for the three targeted items above and supersedes stale statements for this scope.
