# Combat math audit (2026-03-20)

## Baseline traced in this codebase

- Combat lineage is still THAC0-oriented (`compute_thaco()`), but live melee hit resolution in `hit()` had been switched to a derived `%` check based on `compute_offensive_hit_value()` vs `compute_evasion()`.
- Damage still follows Circle/tba style additive path:
  - strength damage bonus
  - `damroll`
  - weapon dice (or unarmed fallback)
  - positional multiplier
- Spell damage goes through `mag_damage()`, then save-for-half through `mag_savingthrow()`.

## Local deviations found

1. **High-level THAC0 clamp bug**
   - `compute_thaco()` forced level 50+ attackers to THAC0 `>= 15` via `MIN(__thaco, 15)`, sharply reducing high-level accuracy.

2. **Melee hit path divergence from THAC0 d20**
   - `hit()` used a 1-100 accuracy roll from abstract hit/evasion values rather than THAC0-vs-AC d20 edge handling.

3. **Player unarmed damage flattened**
   - Unarmed player damage path was `rand_number(0, 2)` regardless of level.

4. **Spell reliability pressure on low-tier nukes**
   - `mag_damage()` always called saves with modifier `0`, so low-tier spells had no level-gap reliability aid.

5. **Peacekeeper/helper flee instability**
   - Generic `MOB_WIMPY` auto-flee in `damage()` had no guard for helper-style support NPCs.

## Bug-vs-math classification

- Math issues:
  - THAC0 clamping and percent-hit replacement reduced level-gap dominance.
  - Unarmed damage floor was mathematically too low for high level.
  - Spell save pressure ignored caster-victim level gap.
- Behavior bugs:
  - Helper peacekeeper-like mobs with wimpy could engage then immediately flee once wounded.

## Tunings made (conservative, lineage-preserving)

1. Restored THAC0-centric d20 melee hit resolution with natural 1/20 behavior.
2. Removed the high-level THAC0 clamp bug; preserved lower bound of `1`.
3. Added modest level-gap THAC0 bonus (`(attacker-victim)/3`) to reinforce expected dominance.
4. Replaced fixed `0..2` player unarmed damage with level-scaling dice that remain below weapon expectations.
5. Added modest level-gap save modifier support in `mag_damage()`, with extra reliability for magic missile/chill touch.
6. Added small level scaling term to magic missile/chill touch damage.
7. Prevented helper mobs from triggering generic `MOB_WIMPY` auto-flee path.

## Preserved behavior

- Core additive damage model (`STR + damroll + dice`) remains intact.
- Existing spell list, damage types, and save-for-half structure remain intact.
- Existing evasion/armor helper functions remain available for display/auxiliary systems.
