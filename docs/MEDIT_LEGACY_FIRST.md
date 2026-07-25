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
