# Adventurers Lair Mage-Leaning Skill/Spell Audit (2026-03-18)

This audit was performed against current `src/spells.h`, `src/spell_parser.c`, `src/magic.c`, `src/spells.c`, `src/class.c`, and command handlers.

## Legend
- **already existed**: implemented and wired
- **completed**: existed but patch completed missing usability piece
- **newly implemented**: did not exist before this patch
- **deferred for later**: not implemented in this patch

## Patch impact summary
- Added compatibility aliases (no new spell numbers) so common variants map to existing stable implementations:
  - `colour spray` -> `color spray`
  - `detect invis` -> `detect invisibility`
  - `invis` -> `invisibility`
  - `night vision` -> `infravision`
  - `shield` -> `armor`
  - `underwater breathing` -> `waterwalk`
  - `hunt` -> `track`

## Full requested list classification

| Entry | Classification | Status | Notes |
|---|---|---|---|
| Blink | missing | deferred for later | No spell id or implementation found. |
| Dagger | missing | deferred for later | No weapon proficiency skill found. |
| Dodge | missing | deferred for later | No dodge skill id/handler found. |
| Exotic | missing | deferred for later | No exotic weapon proficiency skill found. |
| Hide | fully implemented | already existed | `SKILL_HIDE` exists and is assigned. |
| Magic missile | fully implemented | already existed | `SPELL_MAGIC_MISSILE` in parser/mag_damage. |
| Recall | fully implemented | already existed | `SKILL_RECALL` command + `spell_recall`. |
| Sneak | fully implemented | already existed | `SKILL_SNEAK` exists and is assigned. |
| Spear | missing | deferred for later | No spear proficiency skill found. |
| Scrolls | missing | deferred for later | No scroll proficiency skill found. |
| Shield | partially implemented | completed | Alias now maps to `SPELL_ARMOR`. |
| Staves | missing | deferred for later | No staves proficiency skill found. |
| Wands | missing | deferred for later | No wands proficiency skill found. |
| Chill touch | fully implemented | already existed | Damage + affect logic present. |
| Continual light | missing | deferred for later | Not present in spell tables. |
| Whip | missing | deferred for later | No whip proficiency skill found. |
| Detect invis | partially implemented | completed | Alias now maps to `SPELL_DETECT_INVIS`. |
| Detect magic | fully implemented | already existed | `SPELL_DETECT_MAGIC` exists. |
| Sleep | fully implemented | already existed | Affect logic present with save checks. |
| Underwater breathing | partially implemented | completed | Alias maps to `SPELL_WATERWALK` (closest utility). |
| Night vision | partially implemented | completed | Alias maps to `SPELL_INFRAVISION`. |
| Blur | missing | deferred for later | No blur spell id/logic found. |
| Burning hands | fully implemented | already existed | Damage logic in `mag_damage`. |
| Spook | missing | deferred for later | No spell id/logic found. |
| Rune of ix | missing | deferred for later | No spell id/logic found. |
| Avoidance | missing | deferred for later | No spell id/logic found. |
| Banshee wail | missing | deferred for later | No spell id/logic found. |
| Weaken | missing | deferred for later | No weaken spell id/logic found. |
| Word of recall | fully implemented | already existed | `SPELL_WORD_OF_RECALL` manual handler. |
| Detect hidden | missing | deferred for later | No detect hidden flag affect found. |
| Mystic might | missing | deferred for later | No spell id/logic found. |
| Poison | fully implemented | already existed | Affect logic in `mag_affects`. |
| Identify | fully implemented | already existed | Manual identify implementation exists. |
| Meditation | missing | deferred for later | No meditation skill id found. |
| Knock | missing | deferred for later | No knock spell id/logic found. |
| Shocking grasp | fully implemented | already existed | Damage logic present. |
| Invis | partially implemented | completed | Alias now maps to `SPELL_INVISIBLE`. |
| Sense anger | missing | deferred for later | No spell id/logic found. |
| Blindness | fully implemented | already existed | Affect logic present with saves. |
| Colour spray | partially implemented | completed | Alias now maps to `SPELL_COLOR_SPRAY`. |
| Dispel magic | missing | deferred for later | No `dispel magic` spell id found. |
| Ventriloquate | fully implemented | already existed | `SPELL_VENTRILOQUATE` declared/registered. |
| Wither | missing | deferred for later | No spell id/logic found. |
| Harden body | missing | deferred for later | No spell id/logic found. |
| Minor creation | missing | deferred for later | No spell id/logic found. |
| Prismatic spray | missing | deferred for later | No spell id/logic found. |
| Haste | missing | deferred for later | No haste spell id/logic found. |
| Fly | fully implemented | already existed | Affect logic present. |
| Fireball | fully implemented | already existed | Damage logic present. |
| Cancellation | missing | deferred for later | No spell id/logic found. |
| Fast healing | missing | deferred for later | No spell id/logic found. |
| Magical rush | missing | deferred for later | No spell id/logic found. |
| Vampiric touch | missing | deferred for later | No spell id/logic found. |
| Charm person | fully implemented | already existed | Manual charm with follower logic exists. |
| Teleport behind | missing | deferred for later | No spell id/logic found. |
| Scry | missing | deferred for later | No spell id/logic found. |
| Absorb | missing | deferred for later | No spell id/logic found. |
| Enchant weapon | fully implemented | already existed | Manual item enchant implementation exists. |
| Perception | missing | deferred for later | No spell id/logic found. |
| Web | missing | deferred for later | No spell id/logic found. |
| Lightning bolt | fully implemented | already existed | Damage logic present. |
| Enchant armor | missing | deferred for later | No spell id/logic found. |
| Scribe | missing | deferred for later | No spell id/logic found. |
| True seeing | missing | deferred for later | No spell id/logic found. |
| Flame arrow | missing | deferred for later | No spell id/logic found. |
| Locate object | fully implemented | already existed | Manual locate object exists. |
| Stone skin | missing | deferred for later | No spell id/logic found. |
| Acid blast | missing | deferred for later | No spell id/logic found. |
| Fire | missing | deferred for later | No distinct `fire` spell id/logic found. |
| Teleport | fully implemented | already existed | Manual teleport exists. |
| Cone of cold | missing | deferred for later | No spell id/logic found. |
| Gate | missing | deferred for later | No spell id/logic found. |
| Flaming sphere | missing | deferred for later | No spell id/logic found. |
| Major creation | missing | deferred for later | No spell id/logic found. |
| Conjure elemental | missing | deferred for later | No spell id/logic found. |
| Sustenance | missing | deferred for later | No spell id/logic found. |
| Talon | missing | deferred for later | No spell id/logic found. |
| Portal | missing | deferred for later | No spell id/logic found. |
| Ice cloud | missing | deferred for later | No spell id/logic found. |
| Force bolt | missing | deferred for later | No spell id/logic found. |
| Wraith form | missing | deferred for later | No spell id/logic found. |
| Mist form | missing | deferred for later | No spell id/logic found. |
| Shard of ice | missing | deferred for later | No spell id/logic found. |
| Warmth | missing | deferred for later | No spell id/logic found. |
| Fire breath | missing | deferred for later | No spell id/logic found. |
| Shock aura | missing | deferred for later | No spell id/logic found. |
| Antimagic shell | missing | deferred for later | No spell id/logic found. |
| Acidproof | missing | deferred for later | No spell id/logic found. |
| Scorch | missing | deferred for later | No spell id/logic found. |
| Enchanters focus | missing | deferred for later | No spell id/logic found. |
| Awakening | missing | deferred for later | No spell id/logic found. |
| Acid wave | missing | deferred for later | No spell id/logic found. |
| Shockproof | missing | deferred for later | No spell id/logic found. |
| Translocate | missing | deferred for later | No spell id/logic found. |
| Holy mirror | missing | deferred for later | No spell id/logic found. |
| Solidify | missing | deferred for later | No spell id/logic found. |
| Disrupt | missing | deferred for later | No spell id/logic found. |
| Balefire | missing | deferred for later | No spell id/logic found. |
| Lightspeed | missing | deferred for later | No spell id/logic found. |
| Grey aura | missing | deferred for later | No spell id/logic found. |
| Wayfind | missing | deferred for later | No spell id/logic found. |
| Lightning strike | missing | deferred for later | No spell id/logic found. |
| Miasma | missing | deferred for later | No spell id/logic found. |
| Camp | missing | deferred for later | No spell id/logic found. |
| Toxic cloud | missing | deferred for later | No spell id/logic found. |
| Globe of invulnerability | missing | deferred for later | No spell id/logic found. |
| Acid stream | missing | deferred for later | No spell id/logic found. |
| Banishment | missing | deferred for later | No spell id/logic found. |
| Immolate | missing | deferred for later | No spell id/logic found. |
| Chaos portal | missing | deferred for later | No spell id/logic found. |
| Cure blindness | fully implemented | already existed | `SPELL_CURE_BLIND` + unaffect logic. |
| Dual wield | fully implemented | already existed | `SKILL_DUAL_WIELD` exists/assigned. |
| Enhanced damage | missing | deferred for later | No enhanced damage skill id found. |
| Hunt | partially implemented | completed | Alias maps to `SKILL_TRACK`. |
| Locate animal | missing | deferred for later | No spell id/logic found. |
| Locate corpse | missing | deferred for later | No spell id/logic found. |
| Sanctuary | fully implemented | already existed | Affect logic exists. |
| Second attack | missing | deferred for later | No second attack skill id found. |
| Summon | fully implemented | already existed | Manual summon exists with checks. |
| Third attack | missing | deferred for later | No third attack skill id found. |

## Recommended behavior notes for custom/nonstandard entries (deferred set)

These should be implemented in a follow-up in grouped batches to reduce risk:
1. **Defensive arcane buffs**: Rune of ix, Avoidance, Harden body, Absorb, Antimagic shell, Holy mirror, Solidify, Grey aura.
2. **Mage offensive set**: Banshee wail, Spook, Wither, Fire, Flaming sphere, Talon, Shard of ice, Scorch, Disrupt, Balefire, Miasma.
3. **Mobility/transport**: Teleport behind, Portal, Gate, Chaos portal, Translocate, Wayfind.
4. **Summon/form suite**: Conjure elemental, Wraith form, Mist form, Banishment.
5. **Utility/crafting**: Scribe, Minor creation, Major creation, Sustenance, Awakening, Perception, Sense anger.

