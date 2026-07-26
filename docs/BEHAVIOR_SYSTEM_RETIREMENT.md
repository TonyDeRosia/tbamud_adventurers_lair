# NPC behavior subsystem retirement

## Dependency audit and graph

The retired secondary actor framework was audited before removal. Its dependency graph was:

```text
mobile world records
  -> db.c extended-record loader
  -> char_data prototype configuration
  -> genmob.c copy/free/save
  -> MEDIT extension states and menus
  -> profile compiler
  -> per-instance profile, memory, threat, relationship, patrol, and schedule state
  -> mobile_activity arbitration and event hooks
  -> command diagnostics

Builder-facing generators
  -> actor normalization and planning (no implementation exists in this repository)
  -> MEDIT extension navigation
  -> actor validation and preview

runtime events
  -> speech, emote, movement, item, combat, spell, and social hooks
  -> reaction and brain modules
```

The audit covered source/header modules, mobile flags and structures, Oasis states, MEDIT displays and parsing, configuration toggles, world-file records, copy/free paths, heartbeat arbitration, event hooks, commands, test fixtures, tests, and historical documentation. The repository contains no separate BuilderBot program or package; its only integration surface here was the MEDIT workflow and test content.

## Removal plan executed

1. Remove builder entry points and Oasis states.
2. Remove the runtime tick, event hooks, arbitration, profiles, memory, relationships, threats, patrols, schedules, dialogue, and diagnostics.
3. Remove prototype allocation/copy/free and persistence read/write paths.
4. Remove configuration switches, mobile flag, commands, fixtures, source modules, tests, and obsolete documentation.
5. Rebuild and run the remaining MUD and builder-contract suites.

## Migration notes

Legacy extended mobile records are no longer parsed or written. Builders migrating authored behavior must express it through canonical tbaMUD mechanics:

| Retired behavior | Supported replacement |
| --- | --- |
| greeting, warning, ambient or combat speech | DG mobile trigger |
| movement or patrol | DG mobile trigger, mobile flags, or zone resets |
| room/object reactions | DG room or object trigger |
| guard behavior | mobile flags plus DG triggers |
| merchant | shop system |
| trainer | trainer/guild system |
| quest NPC or boss mechanics | quest system plus DG triggers |
| fixed native behavior | special procedure |

World files should be resaved only after removing retired extended records. There is intentionally no compatibility shim: retaining one would preserve dead configuration and a second behavior authority.

## Final architecture

DG Scripts and standard tbaMUD mechanics are the sole supported behavior-authoring path. MEDIT continues to expose ordinary mobile fields and DG trigger attachment; rooms, objects, triggers, shops, quests, specials, and zone resets remain independent canonical systems.
