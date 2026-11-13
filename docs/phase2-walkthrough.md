# Phase 2 — Independent stability verification

## Scope

This delivery adds a verifier; Phase 1's matching implementation and demo are unchanged.
It does not implement random generation, benchmarks, database, API, or UI.
The verifier evaluates the preference lists supplied at the time of the call.
After preferences change, verify the existing assignment against the new lists.

## Install the update

This archive contains only new or changed Phase 2 files. Copy its contents
into your existing project root, keeping the paths. Replace `CMakeLists.txt`.
No screenshots, Phase 1 documentation, `.git` directory, or existing development
log are included, so your local documentation and screenshots remain intact.
Merge `docs/phase2-development-log.md` into your main log if you prefer one log.
It records environment checks separately from your own future local checks.

## Files

| File | Responsibility |
| --- | --- |
| `algorithm/include/stability.hpp` | Status, blocking-pair data, and public function declaration. |
| `algorithm/src/stability.cpp` | Independent preference/assignment validation and blocking-pair scan. |
| `algorithm/src/stability_demo.cpp` | Demonstrate a stable result and a deliberately unstable assignment. |
| `tests/stability_tests.cpp` | Focused verifier checks, including alternate stable solutions. |
| `CMakeLists.txt` | Add verifier to the core library and add demo/test targets. |

## Return value

A Boolean alone loses the reason a result was rejected. `VerificationResult`
has four statuses and an explanation:

| Status | Meaning | `isReasonable()` |
| --- | --- | --- |
| `InvalidPreferences` | Counts or preference lists violate the agreed input rules. | false |
| `InvalidAssignment` | Assignment sizes, IDs, completeness, or reciprocity are wrong. | false |
| `Unstable` | Assignment is valid, but a blocking pair exists. | false |
| `Stable` | Assignment is valid and has no blocking pair. | true |

`blocking_pair` contains mentor and mentee IDs only for `Unstable`.
The function returns the first problem, not a list of every problem.
Ordinary malformed input produces a result; allocation failures may throw.

## Reading the implementation

1. `completeRankings` independently checks complete permutations of IDs.
   It does not trust Phase 1 validation or call Phase 1 helpers.
2. Assignment vectors must have the right sizes. All partner IDs are checked
   before indexing, and every mentee must be assigned.
3. Reciprocal records must agree: if mentor a names mentee b, b must name a,
   and vice versa. This also rules out duplicate assignments.
4. Ordered lists become inverse-rank tables for both sides. Lower rank means
   higher preference. These tables are reconstructed from the supplied inputs.
5. Every nonassigned mentor-mentee pair is examined.

A pair blocks the assignment only when BOTH conditions hold:

- The mentor is unassigned, or prefers this mentee to their assigned mentee.
- The mentee prefers this mentor to their assigned mentor.

The `||` operator short-circuits: for an unassigned mentor, the right side
is not evaluated, so their empty optional is never dereferenced. All mentees
are assigned before the blocking-pair scan begins.

Unlike matching, verification makes no proposals and changes no assignments.
It ignores `proposal_count`, because proposal history cannot establish whether
an arbitrary assignment is stable. Alternate stable matchings must pass even
if the mentor-proposing matcher would return a different one.

## Example

Two mentors both rank the only mentee first. The mentee ranks Mentor 0 above
Mentor 1. Assigning Mentor 1 and leaving Mentor 0 free is a valid complete
one-to-one assignment, but unstable: Mentor 0 wants a partner and Mentee 0
prefers Mentor 0. The verifier reports the pair (Mentor 0, Mentee 0).

## Build and run

Run from the project root in your VS Code terminal:

```bash
mkdir -p build
c++ -std=c++17 -Wall -Wextra -Wpedantic -I algorithm/include algorithm/src/matching.cpp algorithm/src/stability.cpp algorithm/src/stability_demo.cpp -o build/stability_demo
./build/stability_demo
```

Expected output:

```text
Computed assignment reasonable: true
Valid one-to-one assignment; no blocking pair exists
Manual assignment reasonable: false
Blocking pair: Mentor 0 and Mentee 0
```

Run the independent verifier tests (notice that `matching.cpp` is not linked):

```bash
c++ -std=c++17 -Wall -Wextra -Wpedantic -I algorithm/include algorithm/src/stability.cpp tests/stability_tests.cpp -o build/stability_tests
./build/stability_tests
```

Expected: `Passed 25 verifier scenarios and blocking-pair ID check`.

If CMake 3.16+ is installed, the existing configure/build commands also build
the new targets. `ctest --test-dir build --output-on-failure` runs both suites.
CMake execution was not verified in the implementation environment.

## Complexity

Validation, inverse-rank construction, and scanning at most m*n pairs take
O(m+n+m*n) time. The rank tables and temporary validation state use
O(m+n+m*n) auxiliary space. Empty groups are supported as in Phase 1.

## Review questions — collected at the end

1. Why must assignment IDs be validated before the reciprocal checks?
2. How do reciprocal checks prevent two mentees sharing a mentor?
3. Why does an unassigned mentor still need to participate in the scan?
4. Why is one person's preference insufficient to make a blocking pair?
5. Why must a different stable matching pass even if the matcher never outputs it?
6. Why is `proposal_count` irrelevant to this check?
7. How can a preference change leave the existing assignment stable?
8. Why are malformed assignments distinguished from valid but unstable ones?
