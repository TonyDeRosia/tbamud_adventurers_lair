# Adventurer class retirement audit

Implemented on `main`, HEAD `04ed8c2ad5d9e138bf21f091677689949ad48df4`, on top of the existing Study/Tome working-tree changes. No commit, push, checkout, reset, restore, stash, or git clean was performed.

## Playable classes and unchanged behavior

| ID | Class | Selection letter |
|---|---|---|
| 0 | Mage | M |
| 1 | Cleric | C |
| 2 | Thief | T |
| 3 | Warrior | W |
| 4 | Paladin | P |
| 5 | Bard | B |
| 6 | Warlock | K |
| 7 | Druid | D |
| 8 | Mystic | Y |

All nine class definition rows (including practice settings), resource-gain rows, and internal archetype mappings remain unchanged. The complete initialized ability/minimum-level tables for IDs 0-8 were executed and compared to the pre-retirement working tree. Their SHA256 is identical: `3ad14b3bb83a3a29f23b698abddb82a86c2563c934be1d055af201ecb4211327`.

Paladin, Bard and Warlock retain their own persistent IDs and playable identities. Their existing Warrior/Thief/Mage archetype mappings remain unchanged. Creation and SET retain the nine canonical letters. The DG class setter uses the existing name lookup bounded by the now-nine-class count.

## Removed gameplay uses

- `structs.h`: replaced the playable class constant with `RETIRED_PC_CLASS_ID 9`; playable count is nine.
- `class.c`: removed the Adventurer definition, name, abbreviation, Mage archetype mapping, stat-roll case, resource-gain row and fallback, practice-gain exception, Kick-loop exception, and Magic Missile assignment. Recall, Unarmed and Kick still cover all nine real classes.
- `act.other.c`: deleted the Adventurer catalog helper, text, and both skills/spells routing branches. Both commands use the shared formatter with the character's real class, including legitimate Tome abilities.
- `classtrack.c/.h`: removed the retired shell's starter learned-level helper and its declarations. Removed its creation/login/init calls from `class.c`, `players.c`, and `db.c`.
- `classtrack.c`, `spell_parser.c`, `spec_procs.c`: removed the Adventurer identity shortcut. Existing per-character Study learned-level records continue to govern their existing reactive learning behavior. Removed the display fallback that became unreachable after removal of the shell shortcut.
- Unarmed help no longer advertises Adventurer as a class.

Class locking, archetype tracking, their saved fields, and default/customized player titles are retained. `Class: Warrior / Title: the Adventurer` remains valid. Score uses the ordinary `GET_TITLE` value; the default-title implementation is unchanged.

## Legacy class 9 and array safety

The repository-wide search, including ignored `.plr` files, found exactly one class-9 player: `lib/plrfiles/K-O/kraevok.plr` (line 5). All five current player files and all world files match their pre-task hashes. No player file was modified or migrated.

ID 9 is reserved and cannot be selected, assigned by `spell_level`, or returned as a valid class by `is_valid_class`. Static assertions keep the playable table count aligned with the nine-class model.

ASCII loading validates the full numeric value before narrowing it into the stored class byte. A retired/invalid class closes the input file, logs the filename and need for staff correction, and returns `LOAD_CHAR_INVALID_CLASS` before class-dependent post-load XP/stat/ability/effect initialization. No replacement class is guessed. The temporary in-memory object is marked undefined and cannot be saved.

Login handles that distinct error before new-character creation: authenticated users return to the account menu; direct login closes with a clear message. Copyover also reports the retired/invalid class and rejects recovery. Other offline consumers reject negative load results. The legacy binary converter retains its on-disk byte layout and serialized numeric values; any converted class 9 is rejected by the same loader.

Staff recovery is deliberately manual: agree on a real class with the player, preserve a backup, and correct the saved class during controlled offline maintenance. In-game offline SET cannot repair the rejected file because it uses the same safe loader. No automatic reselection or migration was introduced.

Runtime defensive checks cover the shared catalog, spell/practice authorization, Tome access and affinity, Tome acquisition, resource progression, new-ability grants, skillset, and player offhand class checks. Existing guarded name accessors replace direct abbreviation indexing in admin displays and CLASS_ABBR. Existing guarded class name/archetype/practice helpers remain authoritative. Spell metadata retains MAX_CLASSES storage capacity; only IDs 0-8 are valid PC indexes. NPC class constants and combat formulas were not changed.

## Study and Tomes

Study remains registered at ID 260 and is assigned to no mortal class. Tome use routes before Study gating. Ordinary mortal Study requires authorization plus positive proficiency; stale raw percentages cannot authorize use, listing or practice. Tome acquisition still grants missing off-class authorization while preserving higher proficiency, applies the configured cooldown, and consumes only when something is newly acquired. Native known and already Tome-authorized abilities remain skipped. The success formula, Tome persistence and affinity rules for legitimate classes are unchanged. Study remains neutral to archetype scoring.

## Validation

| Suite/check | Result |
|---|---|
| adventurer_class_removal_regression_test.py | 7 passed; executes production menu/parser, class tables, saved-class validation and full ability initialization with UBSan |
| class_identity_study_access_regression_test.py | 7 passed; 9 classes, 36 Study Tome acquisitions, stale/native/repeat/cooldown/validation/practice cases; invalid IDs rejected under UBSan |
| tome_system_test.py | Source contracts passed |
| resource_pool_regression_test.py | 7 passed |
| unarmed_proficiency_test.py | 6 passed |
| ability_table_formatter_test.py | 4 passed |
| proficiency_advancement_regression_test.py | 6 passed |
| dual_wield_class_effectiveness_test.py | All checks passed |
| account_character_persistence_test.py | 15 test functions explicitly executed and passed |
| last_room_persistence_regression_test.py | 6 passed |
| concealment_visibility_regression_test.py | 8 passed |
| Clean GNU17 WSL build | Full MUD compiled and linked, exit 0 |
| Isolated full-world boot | Passed on temporary port 46299; game loop and login greeting confirmed; zero startup SYSERRs |
| git diff --check | Passed |

Build command:

```text
wsl bash -lc "cd '/mnt/c/Users/antho/Desktop/TBAMUD/TBAMUD UPDATED' && make -C src clean && make -C src MYFLAGS='-std=gnu17 -Wall -Wno-char-subscripts -Wno-unused-but-set-variable' -j2"
```

Boot used a temporary copy of the library, an unused port, quick boot and new-player restriction. The process, copied data and logs were removed. It did not modify original world/player data or touch a live process. Generated root MUD binary, object files and dependency output are removed after validation as requested; rebuild before deployment.

The new removal suite was added. The existing Study suite was updated without removing its authorization coverage; the formatter test's function boundary and resource test's class set were updated for retirement. No unrelated test assertions were removed.

## Remaining production-source references

There are no `CLASS_ADVENTURER`, `show_adventurer_study_catalog`, or `As an Adventurer` production-source matches. Every remaining case-sensitive `Adventurer` reference is listed below. Generic lowercase prose and project/world names are intentionally preserved.

| Location | Reason retained |
|---|---|
| `src/act.item.c:990` | Project name: Adventurer's Lair. |
| `src/act.item.c:999` | Project name: Adventurer's Lair. |
| `src/classtrack.c:310` | Existing soft-title default, comparison, or explanatory comment; no playable class. |
| `src/classtrack.c:333` | Existing soft-title default, comparison, or explanatory comment; no playable class. |
| `src/classtrack.c:389` | Existing soft-title default, comparison, or explanatory comment; no playable class. |
| `src/classtrack.c:452` | Existing soft-title default, comparison, or explanatory comment; no playable class. |
| `src/classtrack.c:517` | Existing soft-title default, comparison, or explanatory comment; no playable class. |
| `src/db.c:769` | Project name: Adventurer's Lair. |
| `src/db.c:4065` | Project name: Adventurer's Lair. |
| `src/interpreter.c:1409` | World location name: Adventurer's Academia. |
| `src/players.c:876` | Existing normal/soft title default, preserved. |
| `src/quest_rewards.c:1` | Project name: Adventurer's Lair. |
| `src/shop.c:156` | Project name: Adventurer's Lair. |
| `src/structs.h:164` | Reserved legacy PC class ID 9; not playable. |
| `src/structs.h:1173` | Documentation of the preserved soft-title field. |

## Changed files and final working-tree status

The following is the combined status for the earlier Study work plus this retirement. The two mob modifications predate both edits in this task and remain untouched. The historical nested source tree has no modifications. The report itself is also new/untracked.

```text
 M lib/text/help/help.hlp
 M lib/world/mob/164.mob
 M lib/world/mob/186.mob
 M src/act.item.c
 M src/act.other.c
 M src/act.wizard.c
 M src/class.c
 M src/classtrack.c
 M src/classtrack.h
 M src/comm.c
 M src/db.c
 M src/db.h
 M src/interpreter.c
 M src/modify.c
 M src/players.c
 M src/spec_procs.c
 M src/spell_parser.c
 M src/structs.h
 M src/tome.c
 M src/utils.h
 M tests/ability_table_formatter_test.py
 M tests/resource_pool_regression_test.py
?? tests/adventurer_class_removal_regression_test.py
?? tests/class_identity_study_access_regression_test.py
?? docs/ADVENTURER_CLASS_RETIREMENT.md
```

Combat, concealment implementation, score, regeneration, ordinary titles and XP code were compared to their pre-task hashes and remain unchanged. The added offhand guard affects invalid PC classes only. There are no known implementation failures; the legacy player's explicit class selection/correction remains a staff action.

## Live verification after rebuild/restart

Create disposable test characters and confirm exactly `M C T W P B K D Y` are offered. `A` is rejected. Confirm all nine selections work, particularly Paladin, Bard and Warlock.

On Warrior, Paladin, Bard and Warlock test characters run:

```text
score
skills
skills all
spells
spells all
```

Each must use its selected class catalog. No Adventurer-only catalog message should appear. The ordinary title may remain `the Adventurer`.

Before learning Study, run:

```text
study magic missile
tome status
tome list
```

Ordinary Study is denied without consuming resources/cooldown. With a staff-provided Study Tome in inventory, replace the placeholder with its actual keyword:

```text
study <tome-keyword>
skills
tome list
tome status
study magic missile
```

At a trainer run `practice study`. Test both zero proficiency and legacy stale proficiency: acquisition grants authorization, preserves any higher percentage, consumes the Tome and applies its configured cooldown. After cooldown expires, a second Study-only Tome provides nothing new and remains unconsumed.

For immortal SET, use only a disposable test character and test `set <test-character> class p`, then `b`, then `k`; each must select the corresponding real class. `set <test-character> class a` and `set <test-character> class 9` must be rejected.

Selecting the existing class-9 character should report that staff correction is required, preserve the account roster, and never enter creation or the game world.
