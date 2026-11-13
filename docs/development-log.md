# Development log

## Phase 1 — Matching core implementation

**Date:** November 12, 2026

**Work performed by AI:** Implemented a standalone C++17 mentor-proposing
Gale–Shapley matcher, complete-ranking validation, a hard-coded example,
CMake targets, and 12 smoke scenarios. Added a proposal trace and explanation.
The project author's prompts were not generated or rewritten.

**Validation performed in the implementation environment:**
- Compiled example and smoke checks with C++17 and strict warnings, including `-Werror`.
- Example returned M0–E1, M1–E0, M2–E2, M3 unassigned, with nine proposals.
- All 12 smoke scenarios passed in a normal build.
- All 12 passed with AddressSanitizer and UndefinedBehaviorSanitizer, with leak detection disabled.

**Environment limitation:** LeakSanitizer initially exited because the host
uses ptrace. Reran with `ASAN_OPTIONS=detect_leaks=0`; leak detection was not verified.
CMake was unavailable, so CMake/CTest execution was not verified.

**Remaining:** Author code review and Git commit/push. The author confirmed the local demo run below.
Independent stability verification remains Phase 2. Broad testing and
performance measurements remain Phase 3. No fabricated author test results
or Git commits are recorded.

## Manual test P1-M01 — Local matching demo

**Date:** November 12, 2026

**Tester:** Project author. Result reported by the author in the project conversation.

**Environment:** Author's local Mac / VS Code setup. Exact compiler version was not recorded.

**Test type:** Manual functional test of the provided demonstration case.

**Input:** Four mentors and three mentees using the preference lists in `algorithm/src/demo.cpp`.

**Procedure:** Build and run the demo, then compare its terminal output with the expected output.

**Expected output:**

```text
Mentor 0 -> Mentee 1
Mentor 1 -> Mentee 0
Mentor 2 -> Mentee 2
Mentor 3 -> Unassigned
Proposals: 9
```

**Observed output:** The author confirmed that the actual output matched the expected output above.

**Result:** PASS.

**Scope:** Confirms that the provided demo builds and runs locally and returns
the expected assignments and proposal count for this case. It does not establish
correctness for every input or replace Phase 2's independent stability verifier.
No local execution of the separate smoke-test executable or CMake route was reported.

## Issue P1-I01 — VS Code IntelliSense C++ standard mismatch

**Date:** November 12, 2026

**Reported by:** Project author.

**Symptoms:** Red underlines at `std::optional` in `matching.hpp` and at
assignment fields used in `matching.cpp` and `demo.cpp`. The editor displayed
19 problems even though the terminal demo had compiled and returned the
expected output.

**Diagnosis:** IntelliSense configuration was inconsistent with the project's
C++17 requirement. `std::optional` requires C++17 or later; failures to parse
these declarations caused related diagnostics elsewhere. The exact diagnostic
text and previous editor standard were not captured.

**Action:** Opened `C/C++: Edit Configurations (UI)` in VS Code and selected
`c++17` as the C++ standard. The terminal build already used `-std=c++17`.

**Verification:** The author confirmed that the red underlines disappeared
after this setting change.

**Status:** RESOLVED — editor configuration issue; no algorithm changes.

**Evidence:** The uploaded screenshot `Screenshot 2026-11-12 at 18.57.22.png`
shows the original header diagnostics. The author will add relevant evidence
to `docs/screenshots/`; an after-fix screenshot has not yet been supplied.

**Scope:** This confirms resolution of the reported editor diagnostics.
It is separate from manual functional test P1-M01 and does not establish
stability or correctness on additional matching inputs.
