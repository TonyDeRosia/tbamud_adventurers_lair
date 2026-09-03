# Puppet, Mind Control, and connection inspection

## Existing architecture audited

The authoritative checkout is the top-level `src` under `TBAMUD UPDATED`, on
main at initial commit `9262124e2a3b82d36f639104c45a5f19b317c984`.
The nested tracked source directory was not edited.

Legacy SWITCH targets visible characters globally. It sets `d->original` to
the staff body, `d->character` to the target, and attaches the descriptor to the
target. Both bodies retain their rooms. RETURN reverses that transfer.
NPC movement, look, speech, emotes, combat, equipment, and resources therefore
already use the controlled body. NPC guards in ordinary commands continue to
apply. NPCs cannot use normal immortal commands by inheriting their controller's
level, but legacy switched Implementors have an explicit DG-command bypass.
PUPPET disables that bypass. Legacy SWITCH retains its existing policy,
including Implementor access to unoccupied player bodies.

Existing extraction returns switched bodies, disconnect saves the original PC,
and copyover returns before saving. The latter cached the old controlled-body
pointer; it now refreshes that pointer after RETURN. The legacy non-possession
RETURN behavior remains separate from the new explicit escape path.

## Ownership and lifecycle

`control.c` owns a runtime `control_session`, linked from both participating
characters. No relationship or timer is serialized. `switch_to_char` factors
the existing NPC transfer and is shared by SWITCH and PUPPET.

For PC Puppet, the victim retains `victim->desc` and that descriptor's account
fields unchanged. The caster descriptor views the victim through `character`
while retaining `original`. Body output is mirrored before per-connection
protocol encoding. Mirroring requires both descriptors to be playing and all
ownership links to agree. The observer's action input is rejected before
aliases, paging, or editors. Account/login output is never mirrored. Chat input
is suppressed along with other victim actions while either binding is active.

`end_control_session` is the only relationship teardown owner. It cancels the
DG event without double-freeing its payload, restores descriptor ownership,
clears both character links and queued/partial inputs, and frees the session.
If a command ends control reentrantly, session freeing waits for that command
to unwind. Character extraction (including deferred extraction), free_char,
death/raw_kill, disconnect, duplicate-login recovery, copyover, and shutdown
all invoke that cleanup. Expiry uses a DG event, without scanning the world.
Voluntary return displays the original room. The normal descriptor prompt
subsequently uses the restored body.

The original Puppet body is visible and vulnerable, does not autoattack, and
cannot independently execute commands. Any positive post-mitigation damage to
a mortal spell caster's abandoned body breaks Puppet. Damage alone does not
break Mind Control or administrative NPC Puppet. Active participants are not
sent to the idle void. Frozen controllers cannot act through a puppet but can
still release it. Charms cannot overlap; applying a charm affect ends the prior
runtime relationship before the affect is installed. Existing masters and
target followers prevent acquisition, so follower chains are never stolen.

## Administrative Puppet

`puppet <npc>` is available at LVL_IMMORT (101), using world-visible targeting
consistent with SWITCH. GODROOM and private-house restrictions follow SWITCH.
It is NPC-only, requires an unoccupied eligible body and an active controller,
and is guaranteed once structurally legal. No mana, save, skill roll, or timer.
Original room and character attributes remain unchanged.

`unpuppet` or exact `return` releases possession. Nested control is rejected.
NPC command minimum levels and normal capability restrictions still apply.
DG command dispatch, immortal commands, SWITCH, and account ownership are not
granted by the attached descriptor. Command triggers and special-command
interception are skipped for compelled actors; normal movement/room/combat
behavior remains in the engine.

## Player spells

IDs 226 and 227 are Puppet and Mind Control. Both are hostile MAG_MANUAL spells,
local character targets, not-self, requiring standing. Ordinary casting owns
knowledge/proficiency checks, failure, mana, wait state, silence, antimagic,
and environmental checks. The new controller respects the original caster's
post-cast wait even after transferring viewpoint.

| Spell | Mana max/min, level decrement | Mage | Warlock | Mystic |
|---|---|---:|---:|---:|
| Puppet | 90/60, 2 | 60 | 55 | 65 |
| Mind Control | 75/50, 2 | 55 | 45 | 60 |

With proficiency P clamped to 1..100, duration in seconds is:

- Puppet NPC: `10 + floor(50 * P / 100)`, at most 60.
- Puppet PC: `3 + floor(27 * P / 100)`, at most 30.
- Mind Control NPC: `15 + floor(75 * P / 100)`, at most 90.
- Mind Control PC: `3 + floor(27 * P / 100)`, at most 30.

Besides the normal proficiency roll, the target gets the existing SAVING_PARA
save. Then a second roll must pass this bounded score:
`clamp(5, 85, floor(P/2) + 20 + 2*(caster_level-target_level) + caster_INT-target_WIS - PC_penalty)`.
PC_penalty is 15 for player targets and zero for NPCs. Targets over five levels
above the caster, or at immortal level, cannot be bound. Even mastery is not
guaranteed. Immortals using the SPELL get mastery for this calculation but
still face structural restrictions, saves, bounded rolls, and spell duration;
the administrative COMMAND is the guaranteed path.

The legacy saving tables have a shared final defined tier at 40; several lack
any higher entries despite the game's 100 mortal levels. Saving lookups now cap
at that tier rather than logging SYSERRs and falling through to a broken result.
This fixes all callers of that shared lookup, not just control. High-level
characters consequently retain the strong resistance of that final tier.

NPC immunity uses MOB_NOCHARM, MOB_NOKILL, MOB_SPEC, actual assigned special
function pointers, sanctuary, and existing charm/follower relationships. This
protects assigned shop, guild, quest, post, inn, and other service functions
without VNUM lists. Neither spell bypasses peaceful rooms. PC targets require
CONFIG_PK_ALLOWED and PRF_SUMMONABLE, preserving the charm consent convention.
Both endpoints must be suitable, and PC targets must be actively playing.
Failed resistance engages hostility. Successful targets retain hostility
against the caster but cannot harm their controller while bound; normal combat
position restrictions apply until they flee or combat ends.

Mind Control keeps the caster in their own body. `order <target> <command>`
compels that bound target in the same room through the ordinary interpreter.
It does not reuse AFF_CHARM or mutate master/follower pointers. `order all`
remains outside this targeted extension. `unpuppet` also releases domination.
Orders respect the caster delay, PC target delay, and an NPC order cooldown
using the existing pulse clock.

## Compelled command policy

For either player spell, resolved commands are restricted BEFORE command
triggers or specials: directions (including diagonals/abbreviations), look,
scan, exits, score, say, emote, socials, hit/kill, kick, bash, flee, stand, sit,
and rest. Resolution uses the same command ordering and level rules as the
interpreter. Everything else is denied, including casting, inventory transfers,
banking, shops, mail, account/password/deletion, quit/rent/save, aliases, orders,
OLC, DG commands, and administration. Target stats and skills remain the source
of combat behavior. Controlled NPCs cannot bypass disabled PvP through damage
or ordinary attack paths. Damage to the controller is disallowed while bound.

## Socket inspection

`socket [player|descriptor-id] [full]` requires LVL_IMPL (104), a real player,
and their own unbound body. It lists connection ID, owner, connection-state
label, redacted host, room, account name, connection age, idle mud ticks, and
controller/target/original/mode/observer relationships where present. Login
and missing-character descriptors are safe. Host is fully redacted by default;
only an explicit FULL at the same highest privilege reveals the stored host.
No pointer, password, hash, token, or fabricated network detail is printed.
Existing USERS remains available with its preexisting policy.

## Verification

`python3 tests/control_runtime_test.py` links the real engine, recompiling the
control and communication modules with AddressSanitizer and UBSan. The fixture
uses real command dispatch, output buffers, protocol conversion, movement,
spell saves/costs, events, damage, extraction, death, disconnect, and free_char.
Leak reporting is disabled because the short-lived fixture intentionally leaves
its synthetic world allocated; address and UB errors are fatal.

See the task's validation report for final counts, clean build, isolated boot,
existing regression results, limitations, and complete ending Git status.
Authenticated live player sessions were not used. No production world files,
commits, or pushes are part of this change.
