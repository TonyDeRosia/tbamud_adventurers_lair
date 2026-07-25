# MEDIT: legacy-first builder workflow

Classic MEDIT remains the primary mobile builder interface. Behavior is inspected
where it is configured and executed; there is no required generalized “Legacy
Behavior” editing layer.

* **NPC Flags** own flag-backed mobile activity. The canonical bit editor remains
  the writer, and selected MEMORY, SCAVENGER, and SENTINEL flags receive concise
  authority/help text.
* **DG Scripts** own trigger-backed behavior. Attached entries show prototype
  facts, and numeric selection opens conservative read-only inspection. Optional
  metadata exists for known trigger 3011; unknown scripts are explicitly reported
  as arbitrary instead of being guessed.
* **Special Procedure** is a direct read-only MEDIT entry based on the assigned C
  function pointer. Names and descriptions never assign or classify a special.
* **AI Actor Extensions** are supplemental. Their reachable menu is limited to
  additive personality, identity/role, and advanced perception; legacy-owned
  domains have no duplicate editor there.
* **Effective Behavior Preview** is a direct, read-only summary derived from
  canonical flags, the assigned function pointer, attached scripts, and existing
  AI state.

One authority owns each behavior domain: NPC mobile activity, an assigned special,
a DG Script, a shop/service, an AI-only extension, none, or unknown external
behavior. Legacy sources remain authoritative and are never disabled because AI
data exists. Existing historical modes and persisted AI fields may remain compiled
for compatibility, but duplicate legacy controls are not reachable from the
primary MEDIT workflow.

The shared nested DG menu routes commands case-insensitively in MEDIT, OEDIT, and
REDIT: `n/N` attaches, `x/X` detaches, `q/Q` restores the parent, and a displayed
number inspects that trigger. Merely opening or inspecting the menu does not dirty
a record; only successful attachment or detachment does.

## Official TBA MUD Baseline

Official TBA MUD `master` is the behavioral and structural baseline for Oasis
MEDIT and its shared DG editor.  Adventurer's Lair preserves that numbered and
lettered workflow where practical and extends it with small additive entries
rather than replacing it with a dashboard or wizard.  Pet Price, Loadout/Loot,
Special Procedure inspection, AI Actor Extensions, and Effective Behavior Preview
sit beside the classic fields.

Legacy NPC flags, actual assigned special-function pointers, services, and DG
Scripts remain authoritative.  AI Actor does not replace those systems.  The
special, effective behavior, and trigger inspection screens are custom read-only
quality-of-life additions and do not dirty a prototype.

Older broad-AI tests were changed because they confused **data compatibility**
with **menu reachability**.  Historical schedule, patrol, combat, dialogue,
memory, and social structures remain compiled and are still copied, loaded,
validated, and available to runtime arbitration for saved mobs.  Preserving those
fields prevents data loss; it does not promise a builder-facing editor.  The
current root exposes only additive personality, identity/role, and perception.
Primary-parser contract tests enumerate those transitions, reject stale submenu
links and malformed fallthrough, and prove that undocumented input redraws the
same menu.  This prevents duplicate paths without deleting compatibility data.
