# Full Python suite baseline

## Commands

Run collection independently before interpreting execution failures:

```sh
python3 -m pytest --collect-only -q
```

Run the complete Python suite with:

```sh
python3 -m pytest -q
```

The focused legacy-first group is:

```sh
python3 tests/medit_legacy_first_test.py
python3 tests/medit_mode_routing_guardrail_test.py
python3 tests/legacy_behavior_metadata_test.py
python3 tests/ai_actor_schedule_regression_test.py
python3 tests/ai_actor_builder_first_menu_test.py
python3 tests/ai_actor_menu_usability_test.py
python3 tests/ai_actor_legacy_arbitration_phase2a_test.py
python3 tests/ai_actor_legacy_arbitration_phase2b_test.py
```

The focused DG group is:

```sh
python3 tests/dg_menu_contract_test.py
python3 tests/dg_attachment_workflow_test.py
```

Builder Cross-Reference has its own regression command:

```sh
python3 tests/builder_references_test.py
```

World reset integrity remains a required adjacent check:

```sh
python3 tests/world_reset_integrity_test.py
```

## Builds

The supported Make build is:

```sh
make -C src clean
make -C src -j2
```

When the checkout provides a root `CMakeLists.txt`, also use:

```sh
cmake -S . -B build
cmake --build build -j2
```

A successful baseline means collection exits zero, the full pytest command exits
zero, every focused direct-script command exits zero, and the normal Make build is
free of compiler warnings. The optional CMake build must complete; known unrelated
legacy utility warnings are tracked in `doc/full_suite_stabilization_audit.md` and
must not be confused with a clean Make baseline. `git diff --check` must also be clean.

## Test contract policy

Behavioral and structural assertions are preferred over searching an entire
source file for a word. Menu contracts should isolate the function that displays
the menu and the parser case that handles its input. Runtime tests should exercise
selection, copying, persistence, validation, or arbitration where a harness is
practical. Text assertions remain appropriate for genuinely user-visible menu
labels, mode transitions, stable serialization tokens, and authority notices,
but arbitrary comments, function order, and the mere presence of a historical
word are not contracts.

The primary AI Actor Extensions menu is additive-only: Personality, Identity /
Role, and Advanced Perception. Historical communication, dialogue, routine,
schedule, patrol, movement, memory, threat, combat, fleeing, assistance,
scavenging, and ownership data may remain compiled, copied, loaded, validated,
and consumed at runtime. That compatibility does not make their historical
builder editors reachable. A compatibility symbol must not be removed merely
because the primary menu does not call it; persistence and runtime call sites
must first be audited and removal proven safe.

Warnings are defects to classify, not output to suppress. Fix the source when a
warning identifies ambiguous control flow, type, format, prototype, or lifetime
behavior. Narrow attributes or guards are acceptable only for documented,
intentional compatibility code after call-site analysis. Do not add file-wide
pragmas, blanket warning disables, or dummy casts to manufacture a clean build.
Compiler notes that are not warnings should be recorded when relevant but must
not be mislabeled as source failures.
