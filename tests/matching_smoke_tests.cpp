#include "matching.hpp"

#include <iostream>
#include <stdexcept>
#include <string>

namespace {
void require(bool condition, const std::string& message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

void expectInvalid(const mentoring::PreferenceLists& mentors,
                   const mentoring::PreferenceLists& mentees) {
    try {
        mentoring::matchMentors(mentors, mentees);
    } catch (const std::invalid_argument&) {
        return;
    }
    throw std::runtime_error("Expected invalid_argument");
}
} // namespace

int main() {
    try {
        const auto empty = mentoring::matchMentors({}, {});
        require(empty.mentor_to_mentee.empty() && empty.mentee_to_mentor.empty()
                && empty.proposal_count == 0, "Empty groups");

        const auto no_mentees = mentoring::matchMentors({{}, {}}, {});
        require(no_mentees.mentor_to_mentee.size() == 2
                && !no_mentees.mentor_to_mentee[0]
                && !no_mentees.mentor_to_mentee[1], "No mentees");

        const auto single = mentoring::matchMentors({{0}}, {{0}});
        require(single.mentor_to_mentee[0] == 0 && single.mentee_to_mentor[0] == 0,
                "One pair");

        // Both sides prefer their own index; no replacement is needed.
        const auto equal = mentoring::matchMentors({{0, 1}, {1, 0}}, {{0, 1}, {1, 0}});
        require(equal.mentor_to_mentee[0] == 0 && equal.mentor_to_mentee[1] == 1,
                "Equal group sizes");

        // This case exercises replacement, rejection, and exhausted preference lists.
        const auto result = mentoring::matchMentors(
            {{0, 1, 2}, {0, 1, 2}, {1, 0, 2}, {0, 1, 2}},
            {{1, 0, 3, 2}, {0, 2, 1, 3}, {2, 3, 0, 1}});
        require(result.mentor_to_mentee[0] == 1 && result.mentor_to_mentee[1] == 0
                && result.mentor_to_mentee[2] == 2 && !result.mentor_to_mentee[3],
                "Unequal groups: mentor assignments");
        require(result.mentee_to_mentor[0] == 1 && result.mentee_to_mentor[1] == 0
                && result.mentee_to_mentor[2] == 2, "Reverse assignments");
        require(result.proposal_count == 9, "Expected proposal count");

        expectInvalid({{0, 1}}, {{0}, {0}}); // Too few mentors.
        expectInvalid({{0}, {0, 1}}, {{0, 1}, {1, 0}}); // Missing rank.
        expectInvalid({{0, 0}, {0, 1}}, {{0, 1}, {1, 0}}); // Duplicate.
        expectInvalid({{0}, {1}}, {{0, 1}}); // Mentor's out-of-range ID.
        expectInvalid({{0}, {0}}, {{0, 0}}); // Mentee's duplicate ID.
        expectInvalid({{0}, {0}}, {{0, 2}}); // Mentee's out-of-range ID.
        expectInvalid({{0}, {0}}, {{0}}); // Incomplete mentee list.
        std::cout << "Passed 12 Phase 1 smoke scenarios\n";
    } catch (const std::exception& error) {
        std::cerr << "FAILED: " << error.what() << '\n';
        return 1;
    }
}
