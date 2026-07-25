# DG Trigger Manual Verification

> Disposable plan only: do not add these VNUMs to production world files. Reserve a builder test zone and replace `Z` below with its zone prefix. Use trigger VNUMs `Z80` upward, mobile `Z50`, object `Z51`, room `Z52`.

## Common exact setup

For each row create a fresh trigger with `trigedit <VNUM>`, choose the stated family/flag/narg/phrase, replace the placeholder command with the single exact command `* audit <Family> <Trigger>` followed by `say DG-AUDIT-<Family>-<Trigger>` (for room/object commands use the appropriate valid DG echo/send command if `say` is not valid for that owner), save, reopen, and compare all six fields. Attach at slot 1 through the parent Scripts menu, inspect it, save/reopen the parent, perform the action, then detach with confirmation and verify the action is silent. Console/script logs prove the command reached `script_driver`; visible output proves owner command execution. Use deterministic narg 100 except threshold/matching cases.

## Catalogue

### Mobile

|Trigger VNUM|Flag|Narg|Phrase|Positive action / expected output|Required negative case|
|---:|---|---:|---|---|---|
|`Z80`|Greet|100|`<empty>`|enter while visible; expect `DG-AUDIT-Mobile-Greet` once|leave/re-enter while invisible; expect no audit line|
|`Z81`|Entry|100|`<empty>`|force the test mob north then back; expect `DG-AUDIT-Mobile-Entry` once|a player enters while mob stays; expect no audit line|
|`Z82`|Leave|100|`<empty>`|walk away while visible; expect `DG-AUDIT-Mobile-Leave` once|walk away while invisible; expect no audit line|
|`Z83`|Speech|1|`hello "good day"`|say hello; expect `DG-AUDIT-Mobile-Speech` once|say goodbye; expect no audit line|
|`Z84`|Command|0|`auditcmd`|auditcmd alpha; expect `DG-AUDIT-Mobile-Command` once|unrelatedcmd; expect no audit line|
|`Z85`|Fight|100|`<empty>`|fight the mob through two combat pulses; expect `DG-AUDIT-Mobile-Fight` once|stand in room out of combat; expect no audit line|
|`Z86`|HitPrcnt|50|`<empty>`|reduce mob below half while fighting; expect `DG-AUDIT-Mobile-HitPrcnt` once|fight above half; expect no audit line|
|`Z87`|Death|100|`<empty>`|kill a fresh test mob; expect `DG-AUDIT-Mobile-Death` once|kill another prototype; expect no audit line|
|`Z88`|Receive|100|`<empty>`|give token to mob; expect `DG-AUDIT-Mobile-Receive` once|give token elsewhere; expect no audit line|
|`Z89`|Bribe|101|`<empty>`|give 101 coins; expect `DG-AUDIT-Mobile-Bribe` once|give 100 coins; expect no audit line|
|`Z90`|Random|100|`<empty>`|wait for DG random scheduler poll; expect `DG-AUDIT-Mobile-Random` once|wait outside active zone; expect no audit line|
|`Z91`|Time|12|`<empty>`|wait/set game hour 12; expect `DG-AUDIT-Mobile-Time` once|hour 11; expect no audit line|
|`Z92`|Load|100|`<empty>`|load a new mob instance; expect `DG-AUDIT-Mobile-Load` once|merely enter existing mob room; expect no audit line|
|`Z93`|Act|1|`arrives`|produce a matching act message; expect `DG-AUDIT-Mobile-Act` once|nonmatching act; expect no audit line|
|`Z94`|Memory|100|`<empty>`|use mremember then re-enter/encounter; expect `DG-AUDIT-Mobile-Memory` once|actor absent from memory; expect no audit line|
|`Z95`|Cast|100|`<empty>`|cast on mob; expect `DG-AUDIT-Mobile-Cast` once|cast only in room; expect no audit line|
|`Z96`|Door|100|`<empty>`|open the local door; expect `DG-AUDIT-Mobile-Door` once|open door elsewhere; expect no audit line|
|`Z97`|Damage|100|`<empty>`|damage mob; expect `DG-AUDIT-Mobile-Damage` once|miss/no damage; expect no audit line|

### Object

|Trigger VNUM|Flag|Narg|Phrase|Positive action / expected output|Required negative case|
|---:|---|---:|---|---|---|
|`Z98`|Get|100|`<empty>`|get token from room, then from a container; expect `DG-AUDIT-Object-Get` once|get another object; expect no audit line|
|`Z99`|Drop|100|`<empty>`|drop token / put token where command path permits; expect `DG-AUDIT-Object-Drop` once|drop another object; expect no audit line|
|`Z100`|Give|100|`<empty>`|give token to test receiver; expect `DG-AUDIT-Object-Give` once|give another object; expect no audit line|
|`Z101`|Wear|0|`<empty>`|wear wearable token; expect `DG-AUDIT-Object-Wear` once|hold another object; expect no audit line|
|`Z102`|Remove|0|`<empty>`|remove worn token; expect `DG-AUDIT-Object-Remove` once|remove another object; expect no audit line|
|`Z103`|Consume|0|`<empty>`|eat/drink/quaff matching test object; expect `DG-AUDIT-Object-Consume` once|consume another object; expect no audit line|
|`Z104`|Timer|0|`<empty>`|set short timer and wait for expiry; expect `DG-AUDIT-Object-Timer` once|object with timer not expired; expect no audit line|
|`Z105`|Command|7|`auditobj`|use auditobj while room/inventory/equipped in turn; expect `DG-AUDIT-Object-Command` once|scope not present; expect no audit line|
|`Z106`|Load|100|`<empty>`|load a new object instance; expect `DG-AUDIT-Object-Load` once|move existing instance; expect no audit line|
|`Z107`|Random|100|`<empty>`|wait for scheduler poll; expect `DG-AUDIT-Object-Random` once|use object without trigger; expect no audit line|
|`Z108`|Cast|100|`<empty>`|cast on object; expect `DG-AUDIT-Object-Cast` once|cast on holder only; expect no audit line|
|`Z109`|Leave|100|`<empty>`|leave while token is on room floor; expect `DG-AUDIT-Object-Leave` once|leave while carrying token; expect no audit line|
|`Z110`|Time|12|`<empty>`|reach game hour 12; expect `DG-AUDIT-Object-Time` once|hour 11; expect no audit line|

### Room

|Trigger VNUM|Flag|Narg|Phrase|Positive action / expected output|Required negative case|
|---:|---|---:|---|---|---|
|`Z111`|Enter|100|`<empty>`|walk into room; expect `DG-AUDIT-Room-Enter` once|remain in room; expect no audit line|
|`Z112`|Leave|100|`<empty>`|walk out; expect `DG-AUDIT-Room-Leave` once|remain; expect no audit line|
|`Z113`|Speech|1|`hello`|say hello; expect `DG-AUDIT-Room-Speech` once|say goodbye; expect no audit line|
|`Z114`|Command|0|`auditroom`|auditroom alpha; expect `DG-AUDIT-Room-Command` once|unrelated command; expect no audit line|
|`Z115`|Door|100|`<empty>`|open local exit; expect `DG-AUDIT-Room-Door` once|operate remote-side exit; expect no audit line|
|`Z116`|Drop|100|`<empty>`|drop token; expect `DG-AUDIT-Room-Drop` once|drop in another room; expect no audit line|
|`Z117`|Cast|100|`<empty>`|cast in room; expect `DG-AUDIT-Room-Cast` once|cast elsewhere; expect no audit line|
|`Z118`|Random|100|`<empty>`|wait for active-zone poll; expect `DG-AUDIT-Room-Random` once|empty the zone; expect no audit line|
|`Z119`|Time|12|`<empty>`|reach game hour 12; expect `DG-AUDIT-Room-Time` once|hour 11; expect no audit line|
|`Z120`|Zone Reset|100|`<empty>`|reset containing zone; expect `DG-AUDIT-Room-Zone Reset` once|reset another zone; expect no audit line|
|`Z121`|Login|100|`<empty>`|log in with placement in room; expect `DG-AUDIT-Room-Login` once|reconnect path not passing enter-game placement; expect no audit line|

## Context assertions

While running each positive case, temporarily change the script to log/echo the variables documented in the matrix (`%actor%`, `%victim%`, `%object%`, `%direction%`, `%cmd%`, `%arg%`, `%speech%`, `%spell%`, `%spellname%`, `%amount%`, `%damage%`, `%attacktype%`, `%time%`). Confirm UIDs resolve to the exact participants. Repeat with an unrelated trigger on the same owner and confirm it remains silent. Chance cases should additionally be tried at narg 0 (never) and 100 (always); HitPrcnt above/below threshold; Bribe one coin below/at threshold; Object Command at scope masks 1, 2 and 4 separately.

## Round trip and safety record

For one trigger in each family, capture the on-disk trigger record after save, reboot the disposable server, reopen TRIGEDIT and parent editor, and compare VNUM, name, attach family, flags, narg, phrase, exact command order and attachment slot. Test `/a` restores script text and `Q/N` discards prototype changes. Test `Y` references before/after attach. Attempt each VNUM in both wrong parent editors and require `cannot be attached`; attempt malformed `slot,VNUM`, unauthorized-zone VNUM, nonexistent VNUM, and out-of-range slot and require rejection without dirtying the parent.

## Items requiring explicit observation

* Greet ignores entrants the mob cannot see; Greet-All does not. Both exclude self, sleeping/fighting/charmed observers and use the DG target policy (gods allowed by this hook). Entry is the mob moving, not another actor entering.
* Fight and HitPrcnt can repeat on later combat processing; verify exactly once per invocation, not once for the lifetime. Death runs before normal death processing continues.
* Object Leave observes only objects in `room->contents`, not carried/equipped objects. Object command search order is equipped, inventory, then room.
* Room Door is dispatched only on the actor’s local room side. Enter teleport coverage depends on the explicit call site; Login is the enter-game placement call, so separately record reconnect behavior rather than infer it.
* Timer fires on expiry processing. Consume covers eat, drink and quaff. Reset fires after the zone reset command loop.
