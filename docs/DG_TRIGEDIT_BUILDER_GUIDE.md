# DG TRIGEDIT Builder Guide

## Prototype versus attachment

A trigger VNUM is one reusable **prototype**. MEDIT, OEDIT, and REDIT store ordered references (attachments) to that prototype; they do not copy its script. Editing a prototype affects every saved parent that references it and future instances. Detaching removes one reference and does not delete the prototype. Use `Y) References` before changing a shared trigger.

## Creating and editing

1. Run `trigedit <vnum>` in a zone you may edit. A new prototype defaults to Mobile, Greet, narg 100, and a placeholder command.
2. Main menu: `1` name; `2` intended family (`0` Mobile, `1` Object, `2` Room); `3` event flags; `4` numeric argument; `5` phrase; `6` commands; `W` copy an existing trigger into this VNUM; `Y` read-only references; `H` field help; `Q` quit.
3. Event numbers retain their on-disk bit positions; reserved/UNUSED positions are hidden and rejected. Multiple flags are possible only when their narg/phrase meanings are compatible. Changing family explicitly clears flags because bit meanings differ.
4. In the shared text editor, use `/h` for the installed command list, `/s` to accept, and `/a` to restore the pre-edit buffer. It supports list, insert, edit/replace and delete operations described by `/h`. The total script buffer is `MAX_CMD_LENGTH`; over-limit input is refused. Blank physical lines are not retained as separate compiled commands.
5. `Q`, then `Y` writes the prototype and trigger zone file; `N` discards; `A` returns to editing. Reopen with `trigedit <vnum>`. Commands are case-insensitive at TRIGEDIT menus.

Invalid attach values and malformed or negative numeric arguments are rejected rather than silently becoming zero. Narg is event-specific: consult the capability matrix; it is not always a percentage. In particular, object Command uses scope bits (1 equipped, 2 inventory, 4 room), Bribe uses a minimum coin amount, Speech/Act select match mode, HitPrcnt is a threshold, and Time is a game hour.

## Attaching, inspecting, editing, and detaching

Open the parent (`medit`, `oedit`, or `redit`) and select its Scripts entry. `N` prompts for strict `slot,VNUM`; slot is 1 through count+1. The shared editor verifies existence, family, builder zone permission, and insertion range. Duplicate attachments remain allowed by explicit legacy policy. Select a numbered attachment to inspect its prototype, then choose the displayed prototype-edit action if desired (nested OLC is deliberately guarded). `X` selects detach, and confirmation explains that only the attachment is removed. Quit/save the parent to persist its attachment list.

* MEDIT accepts Mobile prototypes only.
* OEDIT accepts Object prototypes only.
* REDIT accepts World/Room prototypes only.

Parent inspection is read-only until an explicit edit/detach action. Attach and confirmed detach mark the parent dirty; inspection does not. Parent disk serialization writes `T <vnum>` references and reload resolves them through `assign_triggers`.

## Matching and execution essentials

Command phrases are case-insensitive command prefixes; `*` observes/intercepts all commands. A nonzero script-driver return intercepts where the event hook honors return values. Speech/Act use substring mode at narg 0, or a word/quoted-phrase list at nonzero narg. Random/chance events use `rand_number(1,100) <= narg`. Time is exact in-game hour equality. See [the capability matrix](DG_TRIGGER_CAPABILITY_MATRIX.md) for every event, context, partial support, and dispatch function, and [the manual catalogue](DG_TRIGGER_MANUAL_VERIFICATION.md) before promoting content.

## Safe change checklist

1. Inspect `Y) References` and record consumers.
2. Copy to a test VNUM when semantics change.
3. Test positive and stated negative cases in a designated builder zone.
4. Save/reopen TRIGEDIT and the parent editor.
5. Reboot a test server to prove disk reload before production use.
