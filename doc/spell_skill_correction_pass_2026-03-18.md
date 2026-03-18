# Adventurers Lair tbaMUD — Focused Spell/Skill Correction Pass (2026-03-18)

## 1) What was fixed in this pass

This pass implemented code corrections for the three targeted backlog items and then re-verified compile status.

- Completed **`SPELL_ENERGY_DRAIN`** manual wiring and implementation.
- Safely resolved dead **`spell_information`** declaration by cleanup.
- Added and wired a fully reachable **`SKILL_APPRAISE_ENEMY`** active skill.
- Updated documentation to reflect the repaired state.

## 2) Exact files changed

- `src/spells.h`
- `src/spell_parser.c`
- `src/spells.c`
- `src/act.offensive.c`
- `src/act.h`
- `src/interpreter.c`
- `src/class.c`
- `doc/spell_skill_correction_pass_2026-03-18.md`

## 3) SPELL_ENERGY_DRAIN status

### Before this pass (confirmed)
- Constant existed in `spells.h`.
- Parser registration existed in `spell_parser.c` and used `MAG_DAMAGE | MAG_MANUAL`.
- No `ASPELL(spell_energy_drain)` prototype in `spells.h`.
- No manual dispatch case in `call_magic()` for `SPELL_ENERGY_DRAIN`.
- No manual implementation function.
- Effect path was only generic damage in `mag_damage()`.

### After this pass
- Added `ASPELL(spell_energy_drain)` prototype.
- Added `call_magic()` manual dispatch case for `SPELL_ENERGY_DRAIN`.
- Added real `spell_energy_drain` manual implementation in `spells.c`.
- Retained existing damage path and added a manual drain/support path:
  - drains target mana/move,
  - partially resisted on save,
  - restores some caster mana/move,
  - applies a short debuff on clean success.

Result: `SPELL_ENERGY_DRAIN` is now fully wired through parser + generic damage + manual spell logic.

## 4) spell_information resolution

### Before this pass (confirmed)
- `ASPELL(spell_information);` existed in `spells.h`.
- No implementation present.
- No manual dispatch hook.
- No runtime references found.

### Resolution applied
- Removed the dead `ASPELL(spell_information);` declaration from `spells.h`.

This was the conservative **Option B** cleanup path for dead declaration-only legacy code.

## 5) SKILL_APPRAISE_ENEMY implementation and wiring

### Before this pass (confirmed)
- No skill constant existed.
- No parser/skill registration existed.
- No command handler or runtime implementation existed.

### Implementation added
- Added `SKILL_APPRAISE_ENEMY` constant in `spells.h`.
- Registered skill metadata in `spell_parser.c` via:
  - `skillo_cost(SKILL_APPRAISE_ENEMY, "appraise enemy", 8)`.
- Added command handler implementation `ACMD(do_appraise_enemy)` in `act.offensive.c`.
- Added command declaration in `act.h`.
- Added player command in `interpreter.c`:
  - `appraise` (abbrev `app`).
- Added class reachability in `class.c`:
  - Thief level 18,
  - Warrior level 28,
  - Warlock level 27.

### Behavior summary
- Active utility skill targeting visible room characters.
- Move cost: 8.
- Cooldown: 2 rounds (uses spell cooldown infrastructure).
- Uses existing visibility (`get_char_vis`) and skill roll logic.
- Outputs immersive, category-based appraisal report (no exact numeric leakage), with tiered detail on better success.

## 6) Compile result

Command run:

- `make -C src -j4`

Result:

- Success (no new compile errors from this pass).

## 7) Remaining limitations

- `SKILL_APPRAISE_ENEMY` uses a compact three-tier quality model (basic/strong/excellent) plus failure rather than a broader multi-tier subsystem.
- Resistance/weakness insights are inference-based from visible effects/flags and current stats; no hidden or exact internal percentages are exposed.
- Existing game-balance tuning (cost/cooldown/class level placement/detail thresholds) may be adjusted later, but the skill is now fully functional and reachable.
