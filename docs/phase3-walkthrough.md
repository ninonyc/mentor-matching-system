# Phase 3 — Core matching validation

## Install

Copy this archive's contents into your existing project root, keeping paths.
Replace `CMakeLists.txt`; the remaining files are new. Your current source
implementations, development logs, and screenshots are not overwritten.
This update requires the installed Phase 1 and Phase 2 files.

The test harness is `tests/core_validation.cpp`. No production algorithm or
verifier code was changed. No new libraries or package downloads are needed.

## Build and run correctness checks

From your project root in VS Code:

```bash
mkdir -p build
c++ -std=c++17 -O2 -Wall -Wextra -Wpedantic -I algorithm/include algorithm/src/matching.cpp algorithm/src/stability.cpp tests/core_validation.cpp -o build/core_validation
./build/core_validation
```

Expected output:

```text
Exhaustive: 46973 preference profiles; 281723 candidate assignments passed
Randomized: 1000 cases passed (seed 4330)
Mutations: 866 cases x 4 rejection checks passed
Contention: 8 shared-ranking cases passed
ALL PHASE 3 CORRECTNESS CHECKS PASSED
```

Tests throw exceptions on failure, print context, and exit with status 1.
They do not depend on `assert`, which can be removed in release builds.
An invalid command-line option exits with status 2.

## What each layer establishes

| Layer | Inputs | Checks |
| --- | --- | --- |
| Exhaustive | Every strict complete preference profile for 0 <= n <= m <= 3. | Matcher structure and stability; verifier agrees with reference for every complete 1:1 assignment; reported blocking pair actually blocks; result is mentor-optimal. |
| Randomized | 1,000 seeded profiles with 0–60 mentors and 0–m mentees. | Complete reciprocal assignments, exact unmatched count, bounded proposals, stability by two methods, unchanged inputs, repeatable matching. |
| Mutations | 866 eligible random cases with at least two mentees. | Missing assignments and out-of-range partners rejected; duplicate preferences rejected by both verifier and matcher. Four checks per case. |
| Contention | Eight cases with shared rankings; equal groups and excess mentors. | Stability, structure, and exact known proposal count. |
| Benchmark | Four sizes, three ranking families, three repetitions per instance. | Measured matcher/verifier timings, proposal bounds, and known shared-ranking proposal count. |

The original Phase 1 and Phase 2 suites retain the other targeted invalid-input
and edge-case checks. Random generation here deliberately creates valid
complete rankings; malformed-input testing is separate.

## Independent reference check

`position` uses `std::find` to search a preference list directly.
`blocks` compares positions for a candidate pair. `referenceStable` scans
nonassigned pairs. None uses the production verifier's inverse-rank tables.
The reference assumes structurally valid assignments: generated candidates
are valid by construction, and matcher outputs pass `validateStructure` first.

Both implementations must follow the same mathematical blocking-pair
definition, but they compute preferences differently. This reduces the risk
of a shared implementation bug; it does not eliminate every possible bug.

## Exhaustive enumeration

For each small group size, enumerate all permutations for every person's
preference row. For each profile, enumerate all injections from mentees to
mentors: each mentee gets a different mentor, with remaining mentors free.
For group sizes m,n, there are `(n!)^m * (m!)^n` preference profiles and
`m! / (m-n)!` complete assignments per profile (0! = 1).
Across all tested group sizes this gives 46,973 profiles and 281,723 candidates.

Every stable candidate is compared with the computed result mentor by mentor.
No mentor may prefer that candidate over the algorithm's result. Unmatched
is ranked below all mentees. This checks mentor-optimality, beyond stability.

Do not casually raise the exhaustive size limit: factorial growth makes
exhaustive testing impractical quickly. We enumerate complete valid
assignments, not every malformed assignment representation.

## Random generation and reproducibility

The generator uses `std::mt19937` seeded with 4330 and explicit Fisher–Yates
shuffling. Rejection sampling gives bounded integers without modulo bias.
The explicit shuffle avoids platform-dependent `std::shuffle` behavior.
Failures include the trial/seed and complete preference lists for inspection.
The default seed and run sequence are fixed; no seed command-line option
is implemented. A second matching run checks deterministic results.

## Proposal-count stress cases

In the shared-ranking family, all mentors rank mentees 0,1,... and all mentees
rank mentors 0,1,... . For m >= n, the proposal count is exactly:

`m*n - n*(n-1)/2`.

Matched Mentor i makes i+1 proposals; each of the m-n unassigned mentors
makes n proposals. For equal groups the sum is n*(n+1)/2.
This provides a known quadratic-work case rather than relying on wall-clock
time to infer algorithmic behavior. The general upper bound remains m*n.

## Benchmark

Use the optimized binary from the build command above:

```bash
./build/core_validation --benchmark
```

To save YOUR measurements separately:

```bash
./build/core_validation --benchmark > docs/phase3-benchmark-local.csv
```

The included `phase3-benchmark-linux.csv` contains implementation-environment
measurements, not your Mac's results. The benchmark prints CSV with matcher
and verifier median milliseconds over three runs on the SAME input.
Generation and printing are outside the timed intervals. Matcher time includes
its validation, rank construction, and output allocation. Verifier time includes
its validation and rank construction.

Cases: n=100,250,500,1000; random equal-sized groups, shared equal-sized
groups, and shared groups with m=2n. All benchmark outputs must be accepted
by the verifier. Timings vary by machine/load; do not expect identical values
or treat these 12 instances as a statistical performance study. No peak
memory measurement is made. Three medians are descriptive, not pass/fail limits.

Both production functions have O(m+n+m*n) time and auxiliary space bounds.
Few proposals on a random input do not remove the O(m*n) cost of reading
complete rankings and building rank tables. Finite tests support the
implementation; they do not replace the mathematical correctness argument.

## CMake and sanitizers

CMake includes `core_validation` in CTest alongside both previous suites.
When CMake 3.16+ is installed:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
ctest --test-dir build --output-on-failure
```

The benchmark is intentionally separate from CTest and runs only when requested.
CMake/CTest execution has not been verified in the implementation environment.
The direct compiler route was verified. The full correctness suite also passed
with AddressSanitizer and UndefinedBehaviorSanitizer; leak detection was
disabled because of the host's previously observed ptrace limitation.

## Review questions — collected at the end

1. How do preference profiles differ from candidate assignments?
2. Why is the number of complete assignments m!/(m-n)!?
3. How does the linear-search reference differ from the rank-table verifier?
4. Why must matcher outputs pass structural checks before the reference scan?
5. How does exhaustive comparison establish mentor-optimality on tested profiles?
6. Why do stable results on 1,000 random inputs not prove correctness for every input?
7. Why do shared rankings produce m*n - n*(n-1)/2 proposals?
8. Why do low proposal counts not make the whole matcher linear-time?
9. What does the seed reproduce, and why will measured milliseconds still vary?
10. Why keep malformed-input checks, correctness checks, and benchmarks separate?
