# AI Actor legacy NPC integration: Phase 1

Phase 1 adds a read-only `legacy_behavior` query layer.  It classifies special
procedures, service bindings, DG prototype attachments, and behavior flags into
domain masks; it does not dispatch, suppress, migrate, or persist any behavior.

MEDIT now shows a compact Legacy Behavior block and a DG Script count.  AI Actor
Diagnostics contains a separate **LEGACY INTEGRATION** block, and `stat mob`
shows the same data plus the per-instance AI runtime-state status.  Shop, quest,
guild, postmaster, receptionist, cryogenicist, Mayor, and unknown custom special
functions are identified from existing authoritative tables and function
pointers.  Mayor 3105 is identified as a hard-coded assignment and explains its
routine/movement/speech/door domains and shared static route-state hazard.

The current ordering is documented only: special procedures run before AI Actor;
an eligible AI tick can suppress the later legacy mobile tail.  Warning counts
include only WARNING/HIGH RISK legacy findings, never INFO.  DG output currently
reports attachment counts; detailed trigger names/types remain available through
the existing DG editor.  Loadout/reset provenance is intentionally not inferred
because zone resets and runtime equipment are not safely attributable here.

**Runtime NPC behavior was not changed in Phase 1.**  The Phase 2 seam is the
domain mask returned by `legacy_ai_domains()` and the metadata registry: a future
arbitrator may consume those values at an explicitly designed ownership boundary,
without changing this discovery layer.

See also: [Phase 2A conservative mobile-pulse arbitration](AI_ACTOR_LEGACY_INTEGRATION_PHASE2A.md).
