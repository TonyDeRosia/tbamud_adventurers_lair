# Spell and Skill Repair Report (2026-03-18)

## A. Repair scope
This pass targeted unresolved entries from `doc/spell_skill_verification_2026-03-18.md` in the following categories:
- Parser-wired but not implemented
- Implemented but not reachable
- Broken by missing support hook
- Partially implemented passive/support behavior

No existing spells/skills were removed or renamed.

## B. Repaired items

### B1) Reachability repairs (implemented but not reachable)

| Constant(s) | Previous classification | Exact issue | Exact fix applied | Files changed |
|---|---|---|---|---|
| `SPELL_FIREBOLT`, `SPELL_FLAME_ARROW`, `SPELL_FROSTBITE`, `SPELL_VOLTAIC_BOLT`, `SPELL_ACID_BLAST`, `SPELL_WEB`, `SPELL_STONE_SKIN` | Implemented but not reachable | Parser/implementation existed, but no mortal learn path. | Added explicit `spell_level(...)` assignments for class progression. | `src/class.c` |
| `SPELL_SHADOW_BOLT`, `SPELL_VAMPIRIC_TOUCH`, `SPELL_FEAR`, `SPELL_MARK_OF_DEATH`, `SPELL_BLOODLUST` | Implemented but not reachable | Implemented spell logic had no class learnability metadata. | Added Warlock progression entries via `spell_level(...)`. | `src/class.c` |
| `SPELL_SILENCE`, `SPELL_TRUE_SEEING`, `SPELL_GREATER_HEAL`, `SPELL_CLEANSE` | Implemented but not reachable | Fully implemented and parser-wired, but no assignment in class tables. | Added Cleric learn-level assignments. | `src/class.c` |
| `SPELL_BARKSKIN`, `SPELL_GIANT_STRENGTH`, `SPELL_ADRENALINE_SURGE`, `SPELL_CLARITY` | Implemented but not reachable | Functional spells were dead content due to missing learnability rows. | Added class-level entries (Druid/Warrior/Mystic as appropriate). | `src/class.c` |
| `SPELL_BLACK_LANCE` through `SPELL_HUNTERS_INSTINCT` (custom manual block in verification report IDs 160-225) | Parser-wired but not implemented (report) / unreachable in practice | Custom manual spells had implementation and manual dispatch in code, but no class progression path for normal players. | Added placeholder Warlock learnability path with conservative level-gating for each listed spell to make content reachable without redesigning balance. | `src/class.c` |
| `SKILL_OVERLORD_PRESENCE` through `SKILL_LEGION_MASTERY` | Implemented but not reachable | Passive hooks existed, but no class assignment in `init_spell_levels()`. | Added explicit Warlock skill progression entries to make passive block learnable. | `src/class.c` |
| `SKILL_KICK`, `SKILL_RECALL` | Implemented but not reachable (report) | Previously flagged as unreachable in verification report backlog. | Confirmed existing all-class progression rows are already present; no extra code change required in this pass. | `src/class.c` (verification only) |

### B2) Missing support-hook repairs

| Constant | Previous classification | Exact issue | Exact fix applied | Files changed |
|---|---|---|---|---|
| `SKILL_UNDEAD_COMMAND` | Broken by missing support hook | Skill had parser metadata only and no runtime combat effect. | Added combat damage hook: undead NPC followers/master minions gain damage bonus when master has `SKILL_UNDEAD_COMMAND`. | `src/fight.c` |
| `SKILL_TACTICAL_SPELL_MEMORY` | Broken by missing support hook | Skill had parser metadata only and no behavior. | Added two hooks: (1) additional mana-cost reduction in `mag_manacost`, and (2) spell kill mana refund in `damage()` death resolution. | `src/spell_parser.c`, `src/fight.c` |

## C. Still unresolved items
- `spell_information` legacy prototype remains declared-without-definition and not dispatch-wired. This is left unchanged in this repair pass because it is a legacy declaration cleanup task and not required to make the unresolved report backlog abilities playable.

## D. Reachability decisions
- Added a conservative **placeholder Warlock progression path** for the full unresolved custom manual spell block (`SPELL_BLACK_LANCE` through `SPELL_HUNTERS_INSTINCT`) and shadow/overlord passives.
- Added modest low/mid-tier progression placements for previously unreachable core/custom spells (`SPELL_FIREBOLT`, `SPELL_WEB`, `SPELL_SILENCE`, `SPELL_GREATER_HEAL`, etc.) to avoid dead content while preserving overall class identity.
- These entries are explicitly intended as safe reachability wiring and can be rebalanced later without removing functionality.

## E. Compile result
- Command run: `make -C src -j4`
- Result: success.
- Notes: existing unrelated warnings are still present; no new compile errors remain after this pass.
