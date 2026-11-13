#pragma once

#include "matching.hpp"

#include <optional>
#include <string>

namespace mentoring {

enum class VerificationStatus {
    InvalidPreferences,
    InvalidAssignment,
    Unstable,
    Stable
};

struct BlockingPair {
    std::size_t mentor;
    std::size_t mentee;
};

struct VerificationResult {
    VerificationStatus status;
    std::string explanation;
    std::optional<BlockingPair> blocking_pair;

    // Only a valid, stable assignment is reasonable.
    bool isReasonable() const { return status == VerificationStatus::Stable; }
};

// Independently checks inputs, full 1:1 assignments, and blocking pairs.
// Does not call matchMentors or use proposal_count. Returns the first issue.
// Unmatched mentors prefer any mentee to being unmatched.
// Malformed data is reported as a status; memory allocation may still throw.
// Time and auxiliary space: O(m + n + m*n).
VerificationResult verifyStability(const PreferenceLists& mentor_preferences,
                                   const PreferenceLists& mentee_preferences,
                                   const MatchingResult& assignment);

} // namespace mentoring
