#pragma once

#include <cstddef>
#include <optional>
#include <vector>

namespace mentoring {

// Row = participant ID; entries = opposite-side IDs, best preference first.
// IDs are zero-based and local to this run (not permanent database IDs).
using PreferenceLists = std::vector<std::vector<std::size_t>>;

struct MatchingResult {
    // nullopt means unassigned. Each index identifies a mentor or mentee.
    std::vector<std::optional<std::size_t>> mentor_to_mentee;
    std::vector<std::optional<std::size_t>> mentee_to_mentor;
    std::size_t proposal_count = 0;
};

// Complete, strict rankings; all partners acceptable; mentors >= mentees.
// Throws std::invalid_argument for malformed preferences or too few mentors.
// Empty groups are supported. Inputs are never modified.
// Time: O(m + n + m*n). Space: O(m + n + m*n), including rank lookup.
MatchingResult matchMentors(const PreferenceLists& mentor_preferences,
                           const PreferenceLists& mentee_preferences);

} // namespace mentoring
