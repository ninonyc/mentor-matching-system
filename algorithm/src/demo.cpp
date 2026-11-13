#include "matching.hpp"

#include <exception>
#include <iostream>

int main() {
    const mentoring::PreferenceLists mentors = {
        {0, 1, 2}, // Mentor 0 prefers mentee 0, then 1, then 2.
        {0, 1, 2},
        {1, 0, 2},
        {0, 1, 2}
    };
    const mentoring::PreferenceLists mentees = {
        {1, 0, 3, 2}, // Mentee 0 prefers mentor 1 most.
        {0, 2, 1, 3},
        {2, 3, 0, 1}
    };
    try {
        const auto result = mentoring::matchMentors(mentors, mentees);
        for (std::size_t mentor = 0; mentor < result.mentor_to_mentee.size(); ++mentor) {
            std::cout << "Mentor " << mentor << " -> ";
            if (result.mentor_to_mentee[mentor].has_value()) {
                std::cout << "Mentee " << *result.mentor_to_mentee[mentor];
            } else {
                std::cout << "Unassigned";
            }
            std::cout << '\n';
        }
        std::cout << "Proposals: " << result.proposal_count << '\n';
    } catch (const std::exception& error) {
        std::cerr << "Matching failed: " << error.what() << '\n';
        return 1;
    }
}
