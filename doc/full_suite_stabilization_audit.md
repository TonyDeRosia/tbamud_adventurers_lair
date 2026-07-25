# Full-suite stabilization audit

Date: 2026-07-25. Baseline inspected after Builder Cross-Reference commit
`86b12b55c68a1624382a57dc8fccd85f7daf0ce4` (current branch includes its merge).
The captured, unedited initial output is retained for this work session at
`/tmp/original_pytest_output.txt`; the findings below transcribe every distinct
collection error and `legacy_behavior.c` diagnostic from that run.

## Initial `python3 -m pytest -q` result

Pytest stopped during **collection** after 53.60 seconds with five errors; no
collected test executed. There were no syntax, encoding, duplicate-module, path,
or `sys.exit` discovery errors.

| Test or warning | Exact failure | Source location | Expected current architecture | Root cause | Required correction |
|---|---|---|---|---|---|
| `ai_actor_phase1d_usability_test.py` | `AssertionError: This NPC...`; `assert 'This NPC...' in medit` | test line 30; `src/medit.c` current root menu | Only Personality, Identity / Role, and Advanced Perception are reachable | **Obsolete textual assertion** for the superseded broad preview/capability UI | Assert root transitions and retained compatibility fields separately |
| `ai_actor_phase3_usability_test.py` | `AssertionError: AI Actor Perception`; `assert 'AI Actor Perception' in m` | test line 8 | Advanced Perception is reachable; memory, threat, combat, schedule and patrol editors are hidden compatibility code | **Obsolete textual assertion** required every historical screen title | Test the root parser boundary plus runtime/persistence symbols |
| `ai_actor_schedule_editor_test.py` | `AssertionError: AI Actor Schedule Entries`; `assert 'AI Actor Schedule Entries' in s` | test line 8 | Schedule data and runtime stay compatible, with no normal builder transition | **Obsolete textual assertion** treated old editor wording as builder contract | Check structures/runtime helpers and absence of a root transition |
| `ai_actor_visual_hierarchy_test.py` | `AssertionError: Advanced AI Brain`; `assert 'Advanced AI Brain' in s` | test line 6 | Three-entry additive hierarchy | **Obsolete textual assertion** required the removed broad hierarchy merely because words appeared in the file | Bound inspection to the displayed root-menu function |
| compiler warning check | `assert 'warning:' not in result.stdout` | `tests/compiler_warning_regression_test.py:13` | Normal `-Wall` build is clean | **True source defect**: multiple independent statements shared lines after unbraced `if` clauses | Put each independent statement on its own unambiguous line |
| GCC | `warning: this ‘if’ clause does not guard... [-Wmisleading-indentation]` | `src/legacy_behavior.c:61` | Preview input initialization is sequential | `if(!o||!z)return;o[0]=0;if(!m)...` visually implied false guarding | Split guard, initialization and null-mobile branch |
| GCC (four instances) | same `-Wmisleading-indentation` diagnostic | `src/legacy_behavior.c:64-67` | Each flag contributes independently | Two independent flag checks per physical line | One check per line |
| GCC | same diagnostic | `src/legacy_behavior.c:69` | Capability heading is unconditional; script count is conditional | unconditional `add` followed conditional count on one line | Separate statements |
| GCC (three instances) | same diagnostic | `src/legacy_behavior.c:70-72` | MEMORY/HELPER, WIMPY/SCAVENGER, metadata/NOCHARM/NOSUMMON checks are independent | multiple independent checks per physical line | One check per line |

The very large `ai_actor.c:262` compiler messages were GCC **notes**, not
warnings: column tracking was disabled due to the large source/header and GCC
suggested `-flarge-source-files`. They did not trigger `warning:` and represent
neither a `legacy_behavior.c` defect nor a failed diagnostic. No unused static
function, unused variable, sign comparison, format mismatch, missing prototype,
fallthrough, or dead-enum compiler warning was emitted from
`legacy_behavior.c`. After the formatting correction, no warning remains.

## Failure classification

1. **True source regressions:** the nine misleading-indentation sites in
   `legacy_behavior_effective_preview`; behavior was intended to be independent,
   but presentation was unsafe and warning-producing.
2. **Obsolete textual assertions:** the four AI Actor modules listed above.
3. **Discovery/collection problems:** import-time assertions in those modules and
   in the compiler warning script aborted collection. They are now pytest test
   functions where changed, so assertions are reported as tests rather than
   collection errors.
4. **Warnings:** only `-Wmisleading-indentation` from `legacy_behavior.c`; all
   corrected without pragmas, attributes, casts, or compiler-option changes.
5. **Unrelated pre-existing failures:** none observed because initial collection
   stopped before execution. The GCC large-source notes remain informational.

## Textual-contract audit

Classification key: **A** source is wrong; **B** obsolete expectation; **C** too
brittle and replaced structurally/behaviorally; **D** compatibility assertion
must remain; **E** duplicate coverage.

| Assertion family / tests | Class | Finding and action |
|---|---:|---|
| Broad Phase 1D labels and `This NPC...` (`ai_actor_phase1d_usability_test.py`) | B/C | Removed whole-file label searches; now slices the actual root menu/parser and verifies compatibility fields separately. |
| Every Phase 3 historical title and arbitrary parser window (`ai_actor_phase3_usability_test.py`) | B/C/D | Replaced with root-transition checks and explicit schedule/patrol/memory/combat structure and validator preservation. |
| Exact schedule list prose (`ai_actor_schedule_editor_test.py`) | B/C/D | Builder wording is no longer a contract; persistent arrays and runtime helpers remain required while root reachability is forbidden. |
| Words anywhere in visual hierarchy (`ai_actor_visual_hierarchy_test.py`) | B/C | Restricted assertions to the actual root display function and rejects numbered obsolete entries there. |
| Additive root labels/transitions (`ai_actor_builder_first_menu_test.py`, `ai_actor_menu_usability_test.py`) | A/D | Still valid structural boundary coverage and intentionally retained. Some label overlap is E, but parser reachability and main-menu routing are distinct. |
| Hidden modes/default root redraw (`medit_mode_routing_guardrail_test.py`) | D | Essential proof that historical parser modes cannot be entered by undocumented normal input. |
| Legacy authority/ownership notices (`medit_legacy_first_test.py`, arbitration Phase 2A/2B tests) | D | Compatibility and runtime-arbitration contracts remain; they do not grant builder reachability. |
| Schedule/runtime regression modules | D | Runtime and persistence behaviors remain valid even though the schedule builder submenu is hidden. |
| Dialogue, creature-sound, diagnostics, Phase 4 and defect regression source checks | D/E | Retain compatibility/runtime coverage; none is interpreted as a root-menu promise. |
| `legacy_behavior.c` metadata and domains (`legacy_behavior_metadata_test.py`) | A/D | Pointer-based metadata, real authority, and diagnostics remain current and must not be weakened. |
| DG menu strings/parser constants and Builder References contracts | A/D | Unrelated current structural/behavioral contracts; no changes made. |
| Warning-free compiler output (`compiler_warning_regression_test.py`) | A | Correct requirement; source was fixed rather than suppressing the test or warnings. |

## Historical AI editor symbol and mode classification

Callers were searched in `src/medit.c`, mode constants in `src/oasis.h`, data in
`src/ai_actor.h`, copy/load/save paths, and runtime helpers in `src/ai_actor.c`.
“Primary” means a transition from `MEDIT_AI_MENU`, not a parser case that can only
exist after restoring old OLC state.

| Symbol or mode family | Current purpose | Callers | Reachability | Persistence dependency | Runtime dependency | Warning status | Action |
|---|---|---|---|---|---|---|---|
| `medit_disp_ai_menu` / `MEDIT_AI_MENU` | Additive root | main MEDIT `I`; its parser | Primary | indirect config | no | clean | Retain |
| personality / trait / preset | Additive personality editing | root command `1` | Primary | personality array copied/saved | response modifiers | clean | Retain |
| role / identity | Additive identity role | root command `2` | Primary | role field | role scoring/dialogue | clean | Retain |
| perception / value | Additive perception | root command `3` | Primary | sensitivity fields | observation/recognition | clean | Retain |
| diagnostics and AI preview | Read-only inspection | root `D`/`P` | Primary, read-only | reads full config | reports runtime state | clean | Retain |
| communication/capabilities/vocalization | Historical authored identity/speech compatibility | historical parsers/displays | Hidden | fields/dialogue persisted | social/vocal runtime | clean | Retain compatibility code |
| social/dialogue | Historical social authoring | historical parser cases | Hidden | dialogue arrays copied/saved | runtime dialogue selection | clean | Retain compatibility code |
| schedule/routine/random/patrol families | Historical routines | historical parser cases and internal display helpers | Hidden | schedules/routes persisted | schedule selection, movement and arbitration | clean | Retain compatibility code |
| movement | Historical movement configuration | historical parser case | Hidden | movement/destination fields | movement runtime | clean | Retain compatibility code |
| memory/threat | Historical relationship/threat configuration | historical parser cases | Hidden | prototype policy fields | instance memory/threat processing | clean | Retain compatibility code |
| combat/targets/flee/assist | Historical reaction configuration | historical parser cases | Hidden | combat policy/weights | combat arbitration | clean | Retain compatibility code |
| ownership/compatibility | Historical ownership diagnostics/editor | historical parser cases | Hidden | ownership fields | arbitration consumes ownership | clean | Retain compatibility code |
| broad behavior and legacy menu displays | Superseded duplicate editor surfaces | historical mode routing only | Dead from current roots | none directly | none directly | clean | Intentionally retained pending a separate proven-safe removal audit |
| schedule/combat validators | Internal validators and previews | runtime/tests/historical code | Internal | validate persisted objects | protects runtime invariants | clean | Retain |
| AI config copy/load/save/validate symbols | Persistence compatibility | DB/genmob/copy paths | Not a menu | essential | supplies runtime config | clean | Retain |

No historical symbol was accidentally reachable, and no symbol was removed.
Lack of a root menu caller alone was deliberately not treated as removal proof.

## Corrections

* Reformatted only the warning-producing independent conditions in
  `legacy_behavior_effective_preview`; output and behavior are unchanged.
* Replaced four obsolete whole-source/menu-word contracts with scoped display and
  parser-transition assertions plus explicit persistence/runtime compatibility
  checks.
* Made the revised modules proper pytest tests while preserving direct-script
  execution for the repository's focused command style.
* Did not change Builder References, DG behavior, persistence fields, runtime
  arbitration, compiler flags, or the current MEDIT architecture.

## Remaining warnings and limitations

The required normal Make build has no `warning:` diagnostics and no remaining
`legacy_behavior.c` warning. The optional CMake build succeeds but exposes these
unrelated pre-existing diagnostics because its targets do not use Make's
`-Wno-unused-but-set-variable` setting:

| Exact warning | Location | Why it remains / defect status | Remediation |
|---|---|---|---|
| `clang-tidy not found. Static analysis disabled.` | `CMakeLists.txt:45` | Environment/tool availability warning, not a C defect | Install clang-tidy when static analysis is desired; no source suppression |
| `variable ‘buf2’ set but not used` | `src/util/autowiz.c:262` | Pre-existing utility cleanup issue, unrelated to legacy-first stabilization | Separate utility cleanup |
| `variable ‘j’ set but not used` | `src/util/plrtoascii.c:162` | Pre-existing utility cleanup issue | Separate utility cleanup |
| ``%s` directive output may be truncated` | `src/act.informative.c:3824` | Pre-existing bounded-format diagnostic; potentially real display truncation | Audit clan-tag length in a separate focused correction |
| `variable ‘buf2’ set but not used`; `variable ‘i’ set but not used` | `src/util/shopconv.c:62`, `:61`, `:79`, `:91`, `:159` | Pre-existing converter cleanup issues | Separate utility cleanup |
| `variable ‘buf2’ set but not used` | `src/util/wld2html.c:507` | Pre-existing utility cleanup issue | Separate utility cleanup |
| `variable ‘load_result’ set but not used` | `src/interpreter.c:1635` | Pre-existing main-server CMake-only diagnostic | Audit the login result handling separately |

GCC also prints informational large-source column-tracking **notes** for
`src/ai_actor.c:262`; these are not `warning:` diagnostics. No pragma or warning
suppression was added. These unrelated CMake findings were not expanded into a
broad cleanup because this phase is constrained to suite stabilization and
`legacy_behavior.c`; they do not prevent either supported build from completing.
