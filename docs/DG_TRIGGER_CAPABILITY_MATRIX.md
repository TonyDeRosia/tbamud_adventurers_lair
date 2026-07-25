# DG Trigger Capability Matrix

## Audit scope and method

This matrix is derived from the current tree: bit definitions (`dg_scripts.h`), names (`constants.c`), TRIGEDIT parser/serialization (`dg_olc.c`), disk loader (`db.c`), parent attachment editor, call sites, and executable dispatch bodies (`dg_triggers.c`). Official upstream was requested as a comparison; network access returned HTTP 403, and no local upstream archive exists, so upstream parity remains explicitly unverified. The local code is authoritative.

**Classification rule.** The prototype is builder-visible and creatable; fields serialize/load generically; the correct parent editor validates its attach type; and a real call site reaches a dispatch function and `script_driver`. “Automated” below means the repository contract test traces those layers. It is not falsely described as a live game integration test. “Live” points to the disposable recipe catalogue. Global is a modifier bit, not an independently dispatched event. Reserved bit 18 (mobile), gaps in object/world layouts, and bit 21+ are not trigger types.

## Common persistence and workflow

All rows share `trigedit_parse` → `trig_data` (`attach_type`, `trigger_type`, `narg`, `arglist`, ordered `cmdlist`) → `trigedit_save` ASCII records → trigger boot loader → `assign_triggers`. MEDIT/OEDIT/REDIT use the shared DG attachment workflow, which checks prototype existence, attach type, zone permission, supports inspection/references and confirmed detach, and marks the parent dirty. TRIGEDIT now rejects reserved selections and malformed attach/numeric input; changing family explicitly clears selected flags because equal bit positions mean different events.

## Capability matrix

|Family|Trigger|Constant / bit|Builder / create|Configurable fields|Save/load|Valid attachment|Runtime entry point|Dispatch|Automated|Live|Status|Notes|
|---|---|---:|---|---|---|---|---|---|---|---|---|---|
|Mobile|Global|`MTRIG_GLOBAL` / `1 << 0`|Yes / Yes|name, family, flags, narg, phrase, commands|Generic ASCII round trip|Mobile only|call site catalogued in test|`scheduler modifier` → `script_driver`|capability contract|manual catalogue|Partially Supported|modifier; bypasses empty-zone random/time suppression|
|Mobile|Random|`MTRIG_RANDOM` / `1 << 1`|Yes / Yes|name, family, flags, narg, phrase, commands|Generic ASCII round trip|Mobile only|call site catalogued in test|`random_mtrigger` → `script_driver`|capability contract|manual catalogue|Partially Supported|chance 0..100|
|Mobile|Command|`MTRIG_COMMAND` / `1 << 2`|Yes / Yes|name, family, flags, narg, phrase, commands|Generic ASCII round trip|Mobile only|call site catalogued in test|`command_mtrigger` → `script_driver`|capability contract|manual catalogue|Partially Supported|unused; argument is case-insensitive command prefix or *|
|Mobile|Speech|`MTRIG_SPEECH` / `1 << 3`|Yes / Yes|name, family, flags, narg, phrase, commands|Generic ASCII round trip|Mobile only|call site catalogued in test|`speech_mtrigger` → `script_driver`|capability contract|manual catalogue|Partially Supported|0=substring, nonzero=word/quoted-phrase list|
|Mobile|Act|`MTRIG_ACT` / `1 << 4`|Yes / Yes|name, family, flags, narg, phrase, commands|Generic ASCII round trip|Mobile only|call site catalogued in test|`act_mtrigger` → `script_driver`|capability contract|manual catalogue|Partially Supported|0=substring, nonzero=word/phrase list|
|Mobile|Death|`MTRIG_DEATH` / `1 << 5`|Yes / Yes|name, family, flags, narg, phrase, commands|Generic ASCII round trip|Mobile only|call site catalogued in test|`death_mtrigger` → `script_driver`|capability contract|manual catalogue|Partially Supported|chance|
|Mobile|Greet|`MTRIG_GREET` / `1 << 6`|Yes / Yes|name, family, flags, narg, phrase, commands|Generic ASCII round trip|Mobile only|call site catalogued in test|`greet_mtrigger` → `script_driver`|capability contract|manual catalogue|Partially Supported|chance; visible entrant|
|Mobile|Greet-All|`MTRIG_GREET_ALL` / `1 << 7`|Yes / Yes|name, family, flags, narg, phrase, commands|Generic ASCII round trip|Mobile only|call site catalogued in test|`greet_mtrigger` → `script_driver`|capability contract|manual catalogue|Partially Supported|chance; entrant need not be visible|
|Mobile|Entry|`MTRIG_ENTRY` / `1 << 8`|Yes / Yes|name, family, flags, narg, phrase, commands|Generic ASCII round trip|Mobile only|call site catalogued in test|`entry_mtrigger` → `script_driver`|capability contract|manual catalogue|Partially Supported|chance when attached mob enters|
|Mobile|Receive|`MTRIG_RECEIVE` / `1 << 9`|Yes / Yes|name, family, flags, narg, phrase, commands|Generic ASCII round trip|Mobile only|call site catalogued in test|`receive_mtrigger` → `script_driver`|capability contract|manual catalogue|Partially Supported|chance; actor/object variables|
|Mobile|Fight|`MTRIG_FIGHT` / `1 << 10`|Yes / Yes|name, family, flags, narg, phrase, commands|Generic ASCII round trip|Mobile only|call site catalogued in test|`fight_mtrigger` → `script_driver`|capability contract|manual catalogue|Partially Supported|chance each combat trigger pulse|
|Mobile|HitPrcnt|`MTRIG_HITPRCNT` / `1 << 11`|Yes / Yes|name, family, flags, narg, phrase, commands|Generic ASCII round trip|Mobile only|call site catalogued in test|`hitprcnt_mtrigger` → `script_driver`|capability contract|manual catalogue|Partially Supported|HP percentage threshold; checked repeatedly|
|Mobile|Bribe|`MTRIG_BRIBE` / `1 << 12`|Yes / Yes|name, family, flags, narg, phrase, commands|Generic ASCII round trip|Mobile only|call site catalogued in test|`bribe_mtrigger` → `script_driver`|capability contract|manual catalogue|Partially Supported|minimum amount|
|Mobile|Load|`MTRIG_LOAD` / `1 << 13`|Yes / Yes|name, family, flags, narg, phrase, commands|Generic ASCII round trip|Mobile only|call site catalogued in test|`load_mtrigger` → `script_driver`|capability contract|manual catalogue|Partially Supported|chance per instance load|
|Mobile|Memory|`MTRIG_MEMORY` / `1 << 14`|Yes / Yes|name, family, flags, narg, phrase, commands|Generic ASCII round trip|Mobile only|call site catalogued in test|`entry_memory_mtrigger / greet_memory_mtrigger` → `script_driver`|capability contract|manual catalogue|Partially Supported|chance; requires script memory record|
|Mobile|Cast|`MTRIG_CAST` / `1 << 15`|Yes / Yes|name, family, flags, narg, phrase, commands|Generic ASCII round trip|Mobile only|call site catalogued in test|`cast_mtrigger` → `script_driver`|capability contract|manual catalogue|Partially Supported|chance when mob is spell target; argument unused|
|Mobile|Leave|`MTRIG_LEAVE` / `1 << 16`|Yes / Yes|name, family, flags, narg, phrase, commands|Generic ASCII round trip|Mobile only|call site catalogued in test|`leave_mtrigger` → `script_driver`|capability contract|manual catalogue|Partially Supported|chance; visible departing actor|
|Mobile|Door|`MTRIG_DOOR` / `1 << 17`|Yes / Yes|name, family, flags, narg, phrase, commands|Generic ASCII round trip|Mobile only|call site catalogued in test|`door_mtrigger` → `script_driver`|capability contract|manual catalogue|Partially Supported|chance; cmd/direction variables|
|Mobile|Time|`MTRIG_TIME` / `1 << 19`|Yes / Yes|name, family, flags, narg, phrase, commands|Generic ASCII round trip|Mobile only|call site catalogued in test|`time_mtrigger` → `script_driver`|capability contract|manual catalogue|Partially Supported|game hour equality (0..23)|
|Mobile|Damage|`MTRIG_DAMAGE` / `1 << 20`|Yes / Yes|name, family, flags, narg, phrase, commands|Generic ASCII round trip|Mobile only|call site catalogued in test|`damage_mtrigger` → `script_driver`|capability contract|manual catalogue|Partially Supported|chance; actor/victim/damage/attacktype|
|Object|Global|`OTRIG_GLOBAL` / `1 << 0`|Yes / Yes|name, family, flags, narg, phrase, commands|Generic ASCII round trip|Object only|call site catalogued in test|`none` → `script_driver`|capability contract|manual catalogue|Deprecated|defined but runtime does not consult it|
|Object|Random|`OTRIG_RANDOM` / `1 << 1`|Yes / Yes|name, family, flags, narg, phrase, commands|Generic ASCII round trip|Object only|call site catalogued in test|`random_otrigger` → `script_driver`|capability contract|manual catalogue|Partially Supported|chance|
|Object|Command|`OTRIG_COMMAND` / `1 << 2`|Yes / Yes|name, family, flags, narg, phrase, commands|Generic ASCII round trip|Object only|call site catalogued in test|`command_otrigger → cmd_otrig` → `script_driver`|capability contract|manual catalogue|Partially Supported|scope bitmask: equipped=1, inventory=2, room=4; argument prefix/*|
|Object|Timer|`OTRIG_TIMER` / `1 << 5`|Yes / Yes|name, family, flags, narg, phrase, commands|Generic ASCII round trip|Object only|call site catalogued in test|`timer_otrigger` → `script_driver`|capability contract|manual catalogue|Partially Supported|unused; fires at timer expiry before normal expiry processing|
|Object|Get|`OTRIG_GET` / `1 << 6`|Yes / Yes|name, family, flags, narg, phrase, commands|Generic ASCII round trip|Object only|call site catalogued in test|`get_otrigger` → `script_driver`|capability contract|manual catalogue|Partially Supported|chance; room and container get paths call hook|
|Object|Drop|`OTRIG_DROP` / `1 << 7`|Yes / Yes|name, family, flags, narg, phrase, commands|Generic ASCII round trip|Object only|call site catalogued in test|`drop_otrigger` → `script_driver`|capability contract|manual catalogue|Partially Supported|chance; attempted room/container placement uses object hook|
|Object|Give|`OTRIG_GIVE` / `1 << 8`|Yes / Yes|name, family, flags, narg, phrase, commands|Generic ASCII round trip|Object only|call site catalogued in test|`give_otrigger` → `script_driver`|capability contract|manual catalogue|Partially Supported|chance; actor/victim variables|
|Object|Wear|`OTRIG_WEAR` / `1 << 9`|Yes / Yes|name, family, flags, narg, phrase, commands|Generic ASCII round trip|Object only|call site catalogued in test|`wear_otrigger` → `script_driver`|capability contract|manual catalogue|Partially Configurable|unused; actor variable (slot is not exported)|
|Object|Remove|`OTRIG_REMOVE` / `1 << 11`|Yes / Yes|name, family, flags, narg, phrase, commands|Generic ASCII round trip|Object only|call site catalogued in test|`remove_otrigger` → `script_driver`|capability contract|manual catalogue|Partially Supported|unused; before unequip|
|Object|Load|`OTRIG_LOAD` / `1 << 13`|Yes / Yes|name, family, flags, narg, phrase, commands|Generic ASCII round trip|Object only|call site catalogued in test|`load_otrigger` → `script_driver`|capability contract|manual catalogue|Partially Supported|chance per object instance load|
|Object|Cast|`OTRIG_CAST` / `1 << 15`|Yes / Yes|name, family, flags, narg, phrase, commands|Generic ASCII round trip|Object only|call site catalogued in test|`cast_otrigger` → `script_driver`|capability contract|manual catalogue|Partially Supported|chance when object is spell target|
|Object|Leave|`OTRIG_LEAVE` / `1 << 16`|Yes / Yes|name, family, flags, narg, phrase, commands|Generic ASCII round trip|Object only|call site catalogued in test|`leave_otrigger` → `script_driver`|capability contract|manual catalogue|Partially Supported|chance; only room-resident objects observe|
|Object|Consume|`OTRIG_CONSUME` / `1 << 18`|Yes / Yes|name, family, flags, narg, phrase, commands|Generic ASCII round trip|Object only|call site catalogued in test|`consume_otrigger` → `script_driver`|capability contract|manual catalogue|Partially Supported|unused; eat/drink/quaff command variable|
|Object|Time|`OTRIG_TIME` / `1 << 19`|Yes / Yes|name, family, flags, narg, phrase, commands|Generic ASCII round trip|Object only|call site catalogued in test|`time_otrigger` → `script_driver`|capability contract|manual catalogue|Partially Supported|game hour equality (0..23)|
|World|Global|`WTRIG_GLOBAL` / `1 << 0`|Yes / Yes|name, family, flags, narg, phrase, commands|Generic ASCII round trip|Room only|call site catalogued in test|`scheduler modifier` → `script_driver`|capability contract|manual catalogue|Partially Supported|modifier; bypasses empty-zone random/time suppression|
|World|Random|`WTRIG_RANDOM` / `1 << 1`|Yes / Yes|name, family, flags, narg, phrase, commands|Generic ASCII round trip|Room only|call site catalogued in test|`random_wtrigger` → `script_driver`|capability contract|manual catalogue|Partially Supported|chance|
|World|Command|`WTRIG_COMMAND` / `1 << 2`|Yes / Yes|name, family, flags, narg, phrase, commands|Generic ASCII round trip|Room only|call site catalogued in test|`command_wtrigger` → `script_driver`|capability contract|manual catalogue|Partially Supported|unused; case-insensitive command prefix or *|
|World|Speech|`WTRIG_SPEECH` / `1 << 3`|Yes / Yes|name, family, flags, narg, phrase, commands|Generic ASCII round trip|Room only|call site catalogued in test|`speech_wtrigger` → `script_driver`|capability contract|manual catalogue|Partially Supported|0=substring, nonzero=word/phrase list, *=all|
|World|Zone Reset|`WTRIG_RESET` / `1 << 5`|Yes / Yes|name, family, flags, narg, phrase, commands|Generic ASCII round trip|Room only|call site catalogued in test|`reset_wtrigger` → `script_driver`|capability contract|manual catalogue|Partially Supported|chance after zone reset commands|
|World|Enter|`WTRIG_ENTER` / `1 << 6`|Yes / Yes|name, family, flags, narg, phrase, commands|Generic ASCII round trip|Room only|call site catalogued in test|`enter_wtrigger` → `script_driver`|capability contract|manual catalogue|Partially Supported|chance; actor/reverse direction; movement and explicit teleport hooks|
|World|Drop|`WTRIG_DROP` / `1 << 7`|Yes / Yes|name, family, flags, narg, phrase, commands|Generic ASCII round trip|Room only|call site catalogued in test|`drop_wtrigger` → `script_driver`|capability contract|manual catalogue|Partially Supported|chance; actor/object before drop completes|
|World|Cast|`WTRIG_CAST` / `1 << 15`|Yes / Yes|name, family, flags, narg, phrase, commands|Generic ASCII round trip|Room only|call site catalogued in test|`cast_wtrigger` → `script_driver`|capability contract|manual catalogue|Partially Supported|chance; caster/victim/object/spell context|
|World|Leave|`WTRIG_LEAVE` / `1 << 16`|Yes / Yes|name, family, flags, narg, phrase, commands|Generic ASCII round trip|Room only|call site catalogued in test|`leave_wtrigger` → `script_driver`|capability contract|manual catalogue|Partially Supported|chance; actor/outgoing direction|
|World|Door|`WTRIG_DOOR` / `1 << 17`|Yes / Yes|name, family, flags, narg, phrase, commands|Generic ASCII round trip|Room only|call site catalogued in test|`door_wtrigger` → `script_driver`|capability contract|manual catalogue|Partially Supported|chance; local room side, cmd/direction|
|World|Login|`WTRIG_LOGIN` / `1 << 18`|Yes / Yes|name, family, flags, narg, phrase, commands|Generic ASCII round trip|Room only|call site catalogued in test|`login_wtrigger` → `script_driver`|capability contract|manual catalogue|Partially Supported|chance after character placement during enter-game path|
|World|Time|`WTRIG_TIME` / `1 << 19`|Yes / Yes|name, family, flags, narg, phrase, commands|Generic ASCII round trip|Room only|call site catalogued in test|`time_wtrigger` → `script_driver`|capability contract|manual catalogue|Partially Supported|game hour equality (0..23)|

## Numeric argument semantics

|Types|Meaning|Runtime range / zero / negative|Default and builder behavior|
|---|---|---|---|
|Random, Greet*, Entry, Receive, Fight, Death, Load, Cast, Leave, Door, Damage; object Get/Drop/Give; world Reset/Enter/Drop/Login|`rand_number(1,100) <= narg`|0 never; 1–100 percentage; >100 always; negative never (editor rejects new negatives)|new trigger 100; editor accepts nonnegative integer to preserve thresholds/bitmasks|
|HitPrcnt|fires while current HP percentage is <= narg|0 can fire at 0 HP; 1–100 useful; >100 always; negative never|100|
|Bribe|minimum amount (`amount >= narg`)|0 accepts any amount; no artificial 100-coin ceiling; negative rejected|100|
|Speech / Act|boolean matching mode|0 substring; nonzero word/quoted-phrase list|100 (word-list mode)|
|Object Command|scope bit mask|1 equipped, 2 inventory, 4 room; combine with bitwise OR; 0 means no scope|100 is legacy default but selects room bit only plus ignored bits; builder must set 1–7|
|Time (all families)|exact game hour equality|0–23 fires at that hour; >23 safely never fires; negative rejected|100, so builder must configure|
|Timer, Wear, Remove, Consume, ordinary Command|not consumed by that dispatcher|stored but ignored|100|

The editor intentionally no longer clamps every value to 100: that destroyed valid bribe thresholds and object-command scope values. Event-specific safe/useful ranges remain documented rather than migrating historical data.

## Argument phrase semantics

|Types|Meaning and matching|Empty behavior|
|---|---|---|
|Mob/world Command; object Command|Case-insensitive prefix of the parsed command; `*` matches any. Remaining text is `%arg%`; command is `%cmd%`.|logged and skipped|
|Mob/world Speech, Mob Act|Case-insensitive substring when narg=0. With nonzero narg, space-separated words and double-quoted phrases are tested as substrings. Leading `*` matches all.|logged and skipped|
|All other types|Not read by the dispatcher.|allowed and preserved|

No wildcard syntax other than a leading `*` exists. Matching ultimately uses the codebase substring/name helpers; stored case is preserved.

## Scheduling and ordering findings

* Random is polled by `script_trigger_check`; mobile/world checks are zone-activity gated unless Global, while object random is checked for iterated objects. Probability is per poll.
* Time is checked by `script_trigger_check` and equality with the current game hour; it can repeat on scheduler calls during that hour if the scheduler calls more than once.
* Fight and HitPrcnt are invoked from combat processing and may repeat; there is no one-shot latch.
* Death runs in `die()` before the caller continues normal corpse/death processing and may veto via the driver return. Timer runs when the object timer reaches its expiry path. Reset is called after the zone reset-command loop.
* Door hooks receive only operations routed through `do_gen_door`; `cmd_door[subcmd]` is authoritative. Remote-side dispatch is not present.

## Known limits and honest verification status

* Object Global has a name/bit but is commented unused and has no independent behavior: Deprecated. Mobile/world Global only modifies random/time zone gating: Partially Supported as a standalone “type”.
* Object Wear receives `where` in C but does not export it to the script: Partially Configurable context.
* No trigger is Builder Only or Runtime Only among named, non-reserved event bits. No `Finish` trigger exists in this source.
* The Python audit proves source-chain coverage and compiles the real runtime, but the repository has no linkable isolated game-state fixture. Per-event execution is therefore marked manual rather than pretending source inspection is runtime execution.
* Script text uses the shared Oasis text editor with `MAX_CMD_LENGTH` total storage. `/s` saves and `/a` aborts through the shared editor; its `/h` is authoritative for insert/replace/delete/list commands. Save tokenization preserves line order and command text but blank lines are not representable as distinct `cmdlist` elements. Input beyond the configured buffer is rejected by the shared editor; indentation, percent syntax, and ordinary special characters are stored unchanged.
