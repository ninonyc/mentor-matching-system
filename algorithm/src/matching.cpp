#include "matching.hpp"

#include <queue>
#include <stdexcept>
#include <string>

namespace mentoring {
namespace {

// Every row must be a permutation of IDs 0 through opposite_count - 1.
void validatePreferences(const PreferenceLists& preferences,
                         std::size_t opposite_count,
                         const std::string& side) {
    for (std::size_t person = 0; person < preferences.size(); ++person) {
        const auto& list = preferences[person];
        const std::string label = side + " " + std::to_string(person);
        if (list.size() != opposite_count) {
            throw std::invalid_argument(label + ": must rank everyone exactly once");
        }
        std::vector<bool> seen(opposite_count, false);
        for (std::size_t id : list) {
            if (id >= opposite_count) {
                throw std::invalid_argument(label + ": preference ID is out of range");
            }
            if (seen[id]) {
                throw std::invalid_argument(label + ": duplicate preference ID");
            }
            seen[id] = true;
        }
    }
}

} // namespace

MatchingResult matchMentors(const PreferenceLists& mentor_preferences,
                           const PreferenceLists& mentee_preferences) {
    const std::size_t m = mentor_preferences.size();
    const std::size_t n = mentee_preferences.size();
    if (m < n) {
        throw std::invalid_argument("There must be at least as many mentors as mentees");
    }
    validatePreferences(mentor_preferences, n, "Mentor");
    validatePreferences(mentee_preferences, m, "Mentee");

    MatchingResult result;
    result.mentor_to_mentee.resize(m); // All optional values start empty.
    result.mentee_to_mentor.resize(n);
    if (n == 0) {
        return result;
    }

    // preference[mentee][position] gives a mentor ID.
    // rank[mentee][mentor] gives that mentor's position (lower is better).
    PreferenceLists mentee_rank(n, std::vector<std::size_t>(m));
    for (std::size_t mentee = 0; mentee < n; ++mentee) {
        for (std::size_t position = 0; position < m; ++position) {
            mentee_rank[mentee][mentee_preferences[mentee][position]] = position;
        }
    }

    std::vector<std::size_t> next_proposal(m, 0);
    std::queue<std::size_t> available;
    for (std::size_t mentor = 0; mentor < m; ++mentor) {
        available.push(mentor);
    }

    while (!available.empty()) {
        const std::size_t mentor = available.front();
        available.pop();
        const std::size_t mentee = mentor_preferences[mentor][next_proposal[mentor]];
        ++next_proposal[mentor]; // Advance even when rejected; never repeat a proposal.
        ++result.proposal_count;

        const auto current = result.mentee_to_mentor[mentee];
        if (!current.has_value()) {
            result.mentor_to_mentee[mentor] = mentee;
            result.mentee_to_mentor[mentee] = mentor;
        } else if (mentee_rank[mentee][mentor] < mentee_rank[mentee][*current]) {
            // The mentee prefers the new proposer. Release their previous mentor.
            result.mentor_to_mentee[*current].reset();
            result.mentor_to_mentee[mentor] = mentee;
            result.mentee_to_mentor[mentee] = mentor;
            if (next_proposal[*current] < n) {
                available.push(*current);
            }
        } else {
            // Rejected mentors return only if they still have someone to approach.
            if (next_proposal[mentor] < n) {
                available.push(mentor);
            }
        }
    }
    return result;
}

} // namespace mentoring
