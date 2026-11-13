#include "stability.hpp"

#include <iostream>
#include <stdexcept>

namespace {
using mentoring::MatchingResult;
using mentoring::PreferenceLists;
using mentoring::VerificationStatus;
std::size_t checks = 0;

void expect(const PreferenceLists& mentors, const PreferenceLists& mentees,
            const MatchingResult& assignment, VerificationStatus status) {
    const auto result = mentoring::verifyStability(mentors, mentees, assignment);
    if (result.status != status
        || result.isReasonable() != (status == VerificationStatus::Stable)
        || result.blocking_pair.has_value() != (status == VerificationStatus::Unstable)) {
        throw std::runtime_error("Unexpected verification result: " + result.explanation);
    }
    ++checks;
}
} // namespace

int main() {
    try {
        const PreferenceLists mentors = {{0, 1}, {1, 0}};
        const PreferenceLists mentees = {{1, 0}, {0, 1}};
        const MatchingResult identity = {{0, 1}, {0, 1}, 0};
        const MatchingResult swapped = {{1, 0}, {1, 0}, 0};
        // Both are stable, though only one is mentor-optimal.
        expect(mentors, mentees, identity, VerificationStatus::Stable);
        expect(mentors, mentees, swapped, VerificationStatus::Stable);
        // A preference change can create a blocking pair in an existing matching.
        expect({{1, 0}, {1, 0}}, mentees, identity, VerificationStatus::Unstable);
        expect({}, {}, {}, VerificationStatus::Stable);
        expect({{}, {}}, {}, {{std::nullopt, std::nullopt}, {}, 0}, VerificationStatus::Stable);
        expect({{0}}, {{0}}, {{0}, {0}, 0}, VerificationStatus::Stable);
        expect({{0}, {0}}, {{0, 1}}, {{0, std::nullopt}, {0}, 0}, VerificationStatus::Stable);
        const MatchingResult unmatched = {{std::nullopt, 0}, {1}, 0};
        expect({{0}, {0}}, {{0, 1}}, unmatched, VerificationStatus::Unstable);
        const auto blocked = mentoring::verifyStability({{0}, {0}}, {{0, 1}}, unmatched);
        if (blocked.blocking_pair->mentor != 0 || blocked.blocking_pair->mentee != 0) {
            throw std::runtime_error("Wrong blocking-pair IDs");
        }
        auto changed_count = identity;
        changed_count.proposal_count = 999;
        expect(mentors, mentees, changed_count, VerificationStatus::Stable);

        expect({{0}}, {{0}, {0}}, {}, VerificationStatus::InvalidPreferences);
        expect({{0}, {0, 1}}, mentees, identity, VerificationStatus::InvalidPreferences);
        expect({{0, 0}, {1, 0}}, mentees, identity, VerificationStatus::InvalidPreferences);
        expect({{0, 2}, {1, 0}}, mentees, identity, VerificationStatus::InvalidPreferences);
        expect(mentors, {{0, 0}, {0, 1}}, identity, VerificationStatus::InvalidPreferences);
        expect(mentors, {{0}, {0, 1}}, identity, VerificationStatus::InvalidPreferences);
        expect(mentors, {{0, 2}, {0, 1}}, identity, VerificationStatus::InvalidPreferences);
        expect(mentors, mentees, {{0}, {0, 1}, 0}, VerificationStatus::InvalidAssignment);
        expect(mentors, mentees, {{0, 1}, {0}, 0}, VerificationStatus::InvalidAssignment);
        expect(mentors, mentees, {{2, 1}, {0, 1}, 0}, VerificationStatus::InvalidAssignment);
        expect(mentors, mentees, {{0, 1}, {2, 1}, 0}, VerificationStatus::InvalidAssignment);
        expect(mentors, mentees, {{0, 1}, {0, std::nullopt}, 0}, VerificationStatus::InvalidAssignment);
        expect(mentors, mentees, {{std::nullopt, 1}, {0, 1}, 0}, VerificationStatus::InvalidAssignment);
        expect(mentors, mentees, {{0, 0}, {0, 1}, 0}, VerificationStatus::InvalidAssignment);
        expect(mentors, mentees, {{0, 1}, {0, 0}, 0}, VerificationStatus::InvalidAssignment);
        expect(mentors, mentees, {{0, 1}, {1, 0}, 0}, VerificationStatus::InvalidAssignment);
        std::cout << "Passed " << checks << " verifier scenarios and blocking-pair ID check\n";
    } catch (const std::exception& error) {
        std::cerr << "FAILED: " << error.what() << '\n';
        return 1;
    }
}
