# Adventurers Lair tbaMUD - Final Cleanup Pass (2026-03-18)

## Scope
This cleanup pass addresses the remaining targeted spell/skill issues:
1. `spell_information` unresolved declaration status
2. `SKILL_APPRAISE_ENEMY` haste detection
3. `SKILL_APPRAISE_ENEMY` undead detection
4. `SKILL_APPRAISE_ENEMY` summoned-role labeling
5. stale `MAX_SKILLS` boundary comment in `spells.h`
6. stale correction documentation alignment

## Files changed
- `src/act.offensive.c`
- `src/spells.h`
- `doc/spell_skill_cleanup_pass_2026-03-18_final.md` (this report)

## 1) spell_information final resolution status
Current source-of-truth code check shows **no active declaration/definition/reference** for `spell_information` in `src/`.

Verification summary performed in this pass:
- searched declarations/usages in code (`src/`) and found none.
- remaining mentions are historical text in prior docs.

Result:
- `spell_information` is now treated as resolved dead-code cleanup state in the live codebase.
- no additional code removal was required in this pass because the code declaration had already been removed.

## 2) SKILL_APPRAISE_ENEMY corrections
### A) Haste detection correction
- Removed incorrect output mapping that treated `AFF_ADRENALINE` as "hasted".
- Kept detection of the buff, but now labels it accurately as:
  - `Flag: adrenaline surge.`

### B) Undead detection correction
- Replaced class-based check (`GET_CLASS(...) == CLASS_UNDEAD`) in appraise output.
- Appraise undead flag now follows the same actual undead criterion used by spell logic (`RACE_VAMPIRE`).

### C) Summoned role labeling correction
- Role text for NPCs with a master changed from:
  - `summoner`
- to:
  - `summoned servant`

This preserves role signaling while preventing servant/follower entities from being mislabeled as the one doing the summoning.

## 3) MAX_SKILLS stale comment correction
- Updated stale boundary comment in `src/spells.h`:
  - from `MAX_SKILLS (200)`
  - to `MAX_SKILLS (260)`

## 4) Compile result
Command run:
- `make -C src -j4`

Result:
- Build completed successfully after this cleanup pass.

## 5) Remaining limitations (small)
- Historical docs from earlier passes still contain superseded statements about `spell_information` status; this report is the current authoritative cleanup record for the final state.
- Appraise continues to provide only descriptive categories/flags and not exact private numerical stats, by design.
