# Phase 1: mentor-proposing deferred acceptance

## Input and output

IDs are zero-based vector indices, separately for each group. Mentor 0 and
mentee 0 are different people. These are temporary algorithm IDs; permanent
user IDs will be mapped later. Input rows list IDs from best to worst; they
do not contain numerical scores. Everyone is acceptable, and being matched
is preferred to being unmatched. Rankings stay fixed throughout a run.

`MatchingResult` contains assignment vectors in both directions and a proposal
count. `std::optional<std::size_t>` holds either a partner ID or no value.
An empty optional is unassigned; ID 0 is a valid partner.

## Data structures

| Structure | Meaning |
| --- | --- |
| `mentor_preferences[a][k]` | Mentee at position k in mentor a's ranking. |
| `mentee_rank[b][a]` | Position of mentor a in mentee b's ranking; smaller is better. |
| `next_proposal[a]` | Next unused preference-list position for mentor a. |
| `available` | Queue of unassigned mentors with untried mentees. |
| `mentor_to_mentee` / `mentee_to_mentor` | The same tentative pairs viewed in either direction. |

The inverse rank lookup makes each comparison constant time rather than
searching a mentee's whole list. A queue is a scheduling choice, not a
requirement of the mathematical algorithm.

## Demo trace

Mentor preferences: M0: 0,1,2; M1: 0,1,2; M2: 1,0,2; M3: 0,1,2.
Mentee preferences: E0: 1,0,3,2; E1: 0,2,1,3; E2: 2,3,0,1.
The initial mentor queue is 0,1,2,3. Each row is one proposal.

| Step | Proposal | Decision |
| --- | --- | --- |
| 1 | M0 to E0 | E0 is free; hold M0. |
| 2 | M1 to E0 | E0 prefers M1; release M0 to the queue. |
| 3 | M2 to E1 | E1 is free; hold M2. |
| 4 | M3 to E0 | E0 keeps M1; M3 returns to the queue. |
| 5 | M0 to E1 | E1 prefers M0; release M2 to the queue. |
| 6 | M3 to E1 | E1 keeps M0; M3 returns to the queue. |
| 7 | M2 to E0 | E0 keeps M1; M2 returns to the queue. |
| 8 | M3 to E2 | E2 is free; hold M3. |
| 9 | M2 to E2 | E2 prefers M2; release M3, whose list is exhausted. |

Final result: M0–E1, M1–E0, M2–E2; M3 unassigned. Nine proposals.
Assignments are tentative during the loop and final when the queue is empty.

## Why it terminates

A mentor advances their proposal position immediately, so they never approach
the same mentee twice. There are at most m*n distinct proposals. A rejected
or displaced mentor rejoins the queue only when untried mentees remain.
We do not require every mentor to be matched.

## Why every mentee is matched

Once a mentee receives a proposal, they always retain some mentor. If a mentee
were unmatched at the end, every unmatched mentor would still have to propose
to them before exhausting their list. Since m >= n, an unmatched mentee implies
an unmatched mentor exists. That mentor could not finish without proposing to
that mentee, who would accept. This is a contradiction.

## Why the result is stable

Suppose mentor M prefers mentee E over their final assignment, or M is unmatched.
M must have proposed to E before reaching the final result. E rejected M or
later replaced M with a preferred mentor. A mentee only improves their tentative
partner, so E's final partner is preferred to M. They cannot be a blocking pair.
This argument depends on complete strict rankings and all pairs being acceptable.
The separate implementation checking stability is deferred to Phase 2.

Mentor-proposing deferred acceptance is mentor-optimal among stable matchings.
It does not maximize total satisfaction or guarantee everyone's first choice.

## Cost and validation

Preference validation and the rank table require O(m*n) work when both sides
are nonempty. Each of at most m*n proposals takes O(1) work. Including empty
groups and output setup, time and space bounds are O(m+n+m*n).

Row length, duplicate IDs, out-of-range IDs, and m < n are rejected with
`std::invalid_argument`. An ordered row inherently provides distinct rank
positions; duplicate participant IDs are forbidden. Empty groups are supported:
with n = 0, all mentors remain unmatched and make zero proposals.

## Review order

1. Read `matching.hpp` and identify how IDs and unassigned states are represented.
2. Run `demo.cpp`, then trace the nine proposals above by hand.
3. Read input validation and the inverse-rank construction.
4. Read the proposal loop and follow changes to both assignment vectors.
5. Run the smoke checks. Discuss questions before starting Phase 2.

## Bringing the changes into your local repository

Extract the returned archive somewhere separate from your existing checkout.
Copy its contents into your existing project root, including updated tracked
files. Keep your existing `.git` directory; the archive contains no Git history.
Review the diff in VS Code before committing. No changes have been pushed to GitHub.
