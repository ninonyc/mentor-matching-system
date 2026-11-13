#include "matching.hpp"
#include "stability.hpp"

#include <iostream>

int main() {
    const mentoring::PreferenceLists mentors = {{0}, {0}};
    const mentoring::PreferenceLists mentees = {{0, 1}};
    const auto computed = mentoring::matchMentors(mentors, mentees);
    const auto checked = mentoring::verifyStability(mentors, mentees, computed);
    std::cout << "Computed assignment reasonable: " << std::boolalpha
              << checked.isReasonable() << '\n';
    std::cout << checked.explanation << '\n';

    // A complete 1:1 assignment can still be unstable: Mentor 0 is free,
    // and Mentee 0 prefers Mentor 0 to their assigned Mentor 1.
    mentoring::MatchingResult deliberately_unstable;
    deliberately_unstable.mentor_to_mentee = {std::nullopt, 0};
    deliberately_unstable.mentee_to_mentor = {1};
    const auto rejected = mentoring::verifyStability(mentors, mentees,
                                                    deliberately_unstable);
    std::cout << "Manual assignment reasonable: " << rejected.isReasonable() << '\n';
    std::cout << rejected.explanation << '\n';
    return checked.isReasonable()
        && rejected.status == mentoring::VerificationStatus::Unstable ? 0 : 1;
}
