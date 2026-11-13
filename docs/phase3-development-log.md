## Phase 3 — Core matching validation

**Date:** November 12, 2026

**AI implementation:** Added `tests/core_validation.cpp` with exhaustive
small-instance enumeration, an independent linear-search reference check,
seeded random cases, invalid-input/assignment mutations, contention cases,
and a separate benchmark mode. Added a CMake/CTest target.
Production matching and verification code were unchanged.

**Implementation environment:** Linux x86_64; GCC 14.2.0.

**Results actually executed in that environment:**
- Exhaustive: all 46,973 strict complete preference profiles for 0 <= n <= m <= 3.
- Compared verifier decisions with the reference on all 281,723 complete assignments.
- Checked matcher stability, assignment structure, and mentor-optimality for every profile.
- All 1,000 random cases passed, with fixed seed 4330.
- All 866 eligible mutation cases passed four rejection checks each.
- All eight shared-ranking contention cases passed, including exact proposal counts.
- Strict-warning optimized build (`-std=c++17 -O2 -Wall -Wextra -Wpedantic -Werror`) passed.
- Full correctness suite passed under AddressSanitizer/UndefinedBehaviorSanitizer.
  Leak detection disabled for the known host ptrace limitation.
- Repeated optimized correctness run produced the same coverage counts.
- Existing Phase 1 smoke executable: 12 scenarios passed.
- Existing Phase 2 verifier executable: 25 scenarios and blocking-pair ID check passed.

**Benchmark:** 12 instances, three repetitions each, using an optimized
build. See `phase3-benchmark-linux.csv` for measurements. Input generation
excluded; validation and allocations included in each function's timings.
At m=n=1000, shared rankings made exactly 500,500 proposals.
At m=2000,n=1000, shared rankings made exactly 1,500,500 proposals.
No timing thresholds or peak-memory measurements were used.

**Evidence files:** `phase3-correctness-linux.txt`, `phase3-benchmark-linux.csv`.
These are AI-environment evidence, not author-local results.

**Status:** Phase 3 implementation and environment checks complete;
author-local correctness and benchmark runs confirmed below; code review remains pending. CMake/CTest not executed here.
No database/API/UI work or Git commit/push performed.

**Limit:** Exhaustive coverage stops at three mentors. Larger cases are
sampled. This is bounded empirical evidence, not a universal correctness proof.

**Local documentation:** Merge this entry into your main log if desired.
Your existing logs and screenshots are excluded from the update archive.

## Manual test P3-M01 — Local correctness suite

**Date:** November 12, 2026

**Tester/environment:** Project author, local Mac / VS Code terminal.

**Procedure:** Build the optimized C++17 `core_validation` executable using
the supplied command and run it without arguments.

**Expected output:**

```text
Exhaustive: 46973 preference profiles; 281723 candidate assignments passed
Randomized: 1000 cases passed (seed 4330)
Mutations: 866 cases x 4 rejection checks passed
Contention: 8 shared-ranking cases passed
ALL PHASE 3 CORRECTNESS CHECKS PASSED
```

**Observed:** Author confirmed actual output matched expected output.

**Result:** PASS.

## Manual test P3-M02 — Local benchmark

**Date:** November 12, 2026

**Tester/environment:** Project author, local Mac / VS Code terminal.

**Procedure:** Run `./build/core_validation --benchmark`.

**Observed:** Author reported benchmark execution and supplied
`Screenshot 2026-11-12 at 19.35.50.png`. Visible rows agree with expected
proposal counts and bounds. Terminal prompt returned; no failure message
is visible. The CSV header and first row (random, 100 mentors, 100 mentees)
are outside the screenshot.

**Result:** PASS for the reported completed benchmark execution. Visible
measurements are transcribed separately in `phase3-benchmark-mac-visible.csv`.
This is an 11-row partial transcript, not the full 12-instance CSV.

**Selected median measurements, milliseconds:**

| Family | Mentors | Mentees | Proposals | Matching | Verification |
| --- | --- | --- | --- | --- | --- |
| Random | 1000 | 1000 | 7398 | 1.764 | 2.863 |
| Shared rankings | 1000 | 1000 | 500500 | 5.707 | 5.811 |
| Shared rankings, excess mentors | 2000 | 1000 | 1500500 | 13.857 | 12.373 |

**Interpretation:** At the tested sizes, the core completed quickly. Shared
rankings produced the known high proposal counts, all below m*n. Random
rankings required fewer proposals, but reading full rankings and constructing
rank tables still take quadratic work for equal-sized groups. These are
medians of three repetitions of each instance, not universal latency guarantees.

**Evidence:** Author will place the supplied relevant screenshot in
`docs/screenshots/`. Screenshot has not been copied into this archive.

**Remaining:** Code review and any desired complete local CSV export.
No local sanitizer or CMake/CTest run is claimed.
