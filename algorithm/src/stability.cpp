#include "stability.hpp"

#include <vector>

namespace mentoring {
namespace {

// Intentionally independent validation: the verifier cannot assume that the
// matcher ran, or that a supplied assignment came from the matcher at all.
bool completeRankings(const PreferenceLists& lists, std::size_t opposite_count) {
    for (const auto& list : lists) {
        if (list.size() != opposite_count) {
            return false;
        }
        std::vector<bool> seen(opposite_count, false);
        for (std::size_t id : list) {
            if (id >= opposite_count || seen[id]) {
                return false;
            }
            seen[id] = true;
        }
    }
    return true;
}

VerificationResult failure(VerificationStatus status, const std::string& message) {
    return {status, message, std::nullopt};
}

} // namespace

VerificationResult verifyStability(const PreferenceLists& mentor_preferences,
                                   const PreferenceLists& mentee_preferences,
                                   const MatchingResult& assignment) {
    const std::size_t m = mentor_preferences.size();
    const std::size_t n = mentee_preferences.size();
    if (m < n || !completeRankings(mentor_preferences, n)
              || !completeRankings(mentee_preferences, m)) {
        return failure(VerificationStatus::InvalidPreferences,
                       "Require m >= n and complete rankings without duplicates or invalid IDs");
    }

    const auto& mentor_partner = assignment.mentor_to_mentee;
    const auto& mentee_partner = assignment.mentee_to_mentor;
    if (mentor_partner.size() != m || mentee_partner.size() != n) {
        return failure(VerificationStatus::InvalidAssignment,
                       "Assignment vector sizes do not match participant counts");
    }

    // Check ALL IDs before using any of them as vector indices.
    for (const auto& partner : mentor_partner) {
        if (partner && *partner >= n) {
            return failure(VerificationStatus::InvalidAssignment,
                           "A mentor has an out-of-range mentee ID");
        }
    }
    for (const auto& partner : mentee_partner) {
        if (!partner) {
            return failure(VerificationStatus::InvalidAssignment,
                           "Every mentee must have a mentor");
        }
        if (*partner >= m) {
            return failure(VerificationStatus::InvalidAssignment,
                           "A mentee has an out-of-range mentor ID");
        }
    }

    // Both assignment vectors must describe the same one-to-one pairs.
    // Reciprocal agreement also prevents either person appearing in two pairs.
    for (std::size_t mentor = 0; mentor < m; ++mentor) {
        if (mentor_partner[mentor]
            && mentee_partner[*mentor_partner[mentor]] != mentor) {
            return failure(VerificationStatus::InvalidAssignment,
                           "Mentor and mentee assignment records disagree");
        }
    }
    for (std::size_t mentee = 0; mentee < n; ++mentee) {
        if (mentor_partner[*mentee_partner[mentee]] != mentee) {
            return failure(VerificationStatus::InvalidAssignment,
                           "Mentee and mentor assignment records disagree");
        }
    }

    // Convert ordered lists to inverse ranks. A smaller rank is preferred.
    PreferenceLists mentor_rank(m, std::vector<std::size_t>(n));
    PreferenceLists mentee_rank(n, std::vector<std::size_t>(m));
    for (std::size_t mentor = 0; mentor < m; ++mentor) {
        for (std::size_t rank = 0; rank < n; ++rank) {
            mentor_rank[mentor][mentor_preferences[mentor][rank]] = rank;
        }
    }
    for (std::size_t mentee = 0; mentee < n; ++mentee) {
        for (std::size_t rank = 0; rank < m; ++rank) {
            mentee_rank[mentee][mentee_preferences[mentee][rank]] = rank;
        }
    }

    // Examine every nonassigned mentor-mentee pair; do not rerun matching.
    for (std::size_t mentor = 0; mentor < m; ++mentor) {
        for (std::size_t mentee = 0; mentee < n; ++mentee) {
            if (mentor_partner[mentor] == mentee) {
                continue;
            }
            // Short circuiting avoids dereferencing an empty optional.
            const bool mentor_prefers = !mentor_partner[mentor]
                || mentor_rank[mentor][mentee]
                   < mentor_rank[mentor][*mentor_partner[mentor]];
            const bool mentee_prefers = mentee_rank[mentee][mentor]
                < mentee_rank[mentee][*mentee_partner[mentee]];
            if (mentor_prefers && mentee_prefers) {
                return {VerificationStatus::Unstable,
                        "Blocking pair: Mentor " + std::to_string(mentor)
                            + " and Mentee " + std::to_string(mentee),
                        BlockingPair{mentor, mentee}};
            }
        }
    }
    return {VerificationStatus::Stable,
            "Valid one-to-one assignment; no blocking pair exists", std::nullopt};
}

} // namespace mentoring
