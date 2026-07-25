# MEDIT: legacy-first mob behavior

MEDIT's classic fields and the legacy mobile runtime are the authoritative mob
behavior system. NPC flags, special procedures, shops, and DG Scripts are not
compatibility aids and are not replaced by AI Actor.

## Builder workflow

The main screen retains positions, attack type, stats, **NPC Flags**, **AFF
Flags**, loadout/loot, and **DG Scripts**. Use **L) Legacy Behavior** to inspect
what the existing runtime does. Use **I) AI Actor Extensions** only after the
legacy sources have been reviewed.

### Canonical NPC flags

`SENTINEL`, `STAY_ZONE`, aggression flags, `MEMORY`, `HELPER`, `WIMPY`, and
`SCAVENGER` remain the only MEDIT storage for their legacy behaviors. Changes in
the Legacy Behavior submenus toggle those same flag bits; there is no parallel
legacy-behavior record in AI configuration. `mobile_activity()` remains the
runtime consumer.

### Special procedures

The Legacy Behavior screen identifies a special only from the mobile's assigned
C function pointer. Names, keywords, descriptions, and roles are never used as
proof of assignment. An unassigned mob whose displayed name contains “Mayor”
gets a warning that it has no Mayor behavior, not a false assignment.

The central special catalog records a function pointer, display name, purpose,
owned domains, visible capabilities, editability, and coexistence guidance. An
unknown function remains visibly unknown and conservatively locks overlapping AI
ownership rather than receiving an inferred identity.

The classic Mayor special is displayed as read-only. Its catalog describes its
timed wake/sleep cycle, route, gate actions, speeches, posture changes, and waits.
MEDIT does not migrate, duplicate, or rewrite that routine.

### DG Scripts and services

Attached DG Scripts are an external legacy authority. The Legacy screen reports
the attachment count and Diagnostics reports them as a source. Speech or other
arbitrary scripted behavior is not guessed from trigger text, so a builder must
inspect the attached triggers before adding an overlapping extension. Shops,
questmasters, guildmasters, and other service specials likewise remain owned by
their legacy systems.

## One authority per domain

MEDIT reports ownership for movement, routine, posture, speech, combat
initiation/tactics, memory, assistance, fleeing, scavenging/object interaction,
doors, scripts, and services. The supplemental AI menu marks legacy-owned
movement, schedules, combat targeting, scavenging, memory bases, speech, and
door interaction unavailable or extended-only. It does not link to duplicate
editors for canonical NPC flags.

AI personality, relationship nuance beyond `MEMORY`, perception, and social
style are additive. Dynamic dialogue is available only when neither an assigned
special nor attached DG Scripts owns speech. Runtime arbitration continues to
preserve legacy behavior; MEDIT never silently assigns AI ownership.

## Diagnostics, saving, and limitations

The preview is derived from actual flags, the actual function pointer, attached
trigger prototypes, and the stored AI flag/configuration. Diagnostics include
assignment origin, domains, collisions, unknown specials, runtime ordering, and
the fact that saved prototype changes normally require reload/respawn to affect
instances.

Hard-coded specials are currently read-only. DG Scripts are treated as owning an
arbitrary domain because safely classifying trigger commands requires builder
inspection. Some direct/custom specials may lack catalog metadata and are shown
as unknown rather than guessed. Future work may add metadata or migrate an
individual special, but migration is optional and is never required merely to
keep using legacy MEDIT.
