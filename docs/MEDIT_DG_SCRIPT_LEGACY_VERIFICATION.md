# MEDIT DG Script legacy verification

## Builder Academy acceptance workflow (run this first)

Prerequisites: use a builder with permission for zone 0, make a disposable copy
or arrange to discard the edit, and ensure no other descriptor is editing the
same mobile. At the normal game prompt enter:

```text
nohassle
goto 13
north
goto 18
medit 14
S
N
1, 1
N
2, 3
X
Q
Y
```

`nohassle` must report that NOHASSLE is **off** before runtime tests. Rooms 18,
20, 21, 22, and 23 should be visited in order (use `goto 18`, then the available
hallway exits) and their Academy instructions read without editing them.

Expected editor checkpoints:

* `S` displays `Script Editor` and `Trigger List:`.
* `N` displays `Please enter position, vnum   (ex: 1, 200):`.
* Before saving, slots 1 and 2 show trigger VNUMs 1 and 3, respectively.
* `X` returns to the MEDIT main menu, whose DG Scripts field remains set.
* `Q`, `Y` performs the ordinary MEDIT save; `X` does not save by itself.

Reopen with `medit 14`, select `S`, and confirm the same order. Exit with `X`,
then `Q` without changing anything. Run `stat questmaster`, `tstat 1`, and
`tstat 3`; STAT should number both attached runtime triggers and each TSTAT must
show the authored Greet or Receive trigger.

## Ogre and runtime checks

```text
medit 16
S
N
1, 2
X
Q
Y
medit 16
S
```

Expect slot 1 to contain VNUM 2 (Mobile: Death). Exit without changes. With
NOHASSLE off, walk from room 18 through the quest rooms: entering the
questmaster from the south must run the Greet dialogue; killing the ogre must
load the wings and reload the ogre; giving a wrong object to the questmaster
must say it is unwanted and retain the object (`return 0`); giving the authored
wings must produce thanks, the gold message/reward, and purge the wings.

At room 20, run:

```text
stat gateguard
tstat 4
tstat 5
tstat 7
tstat 8
give 10 coins guard
north
```

Expect the guard to say `Thank you.`, unlock and open the gateway, and permit
north until the authored delayed close/lock commands execute.

## Ordering, deletion, and discard

On a disposable mobile with three valid Mobile triggers, use `N` to insert at
slot 1, append at slot `count + 1`, and insert at slot 2. Save and reopen; order
must not change. Select `D`, enter `2`, confirm `Y`, save, and reopen; only the
middle attachment must be gone. Repeat an add and delete, leave the Script
Editor with `X`, leave MEDIT with `Q`, answer `N`, then reopen: the original
prototype list must be intact.

Try a nonexistent VNUM and an Object/Room trigger in MEDIT. Both must be rejected
without changing the list. If a saved reference becomes missing after its
prototype is removed, the menu must show `<missing trigger prototype>` and `D`
must still permit detaching that slot. `I` provides read-only inspection; `E`
explains the safe separate `trigedit <vnum>` workflow; `H` repeats the legacy
grammar.

These runtime scenarios require an interactive server and intentionally remain
manual; automated tests trace the production editor/save/load/runtime functions
and protect the checked-in Academy prototypes.
