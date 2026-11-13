#include "matching.hpp"
#include "stability.hpp"

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <functional>
#include <iomanip>
#include <iostream>
#include <numeric>
#include <random>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
using mentoring::MatchingResult;
using mentoring::PreferenceLists;
using mentoring::VerificationStatus;
constexpr std::uint32_t seed = 4330;

void require(bool condition, const std::string& message) {
    if (!condition) { throw std::runtime_error(message); }
}

// This reference check uses linear list searches, not the production rank tables.
std::size_t position(const std::vector<std::size_t>& list, std::size_t person) {
    return static_cast<std::size_t>(std::find(list.begin(), list.end(), person) - list.begin());
}

bool blocks(const PreferenceLists& mentors, const PreferenceLists& mentees,
            const MatchingResult& a, std::size_t mentor, std::size_t mentee) {
    if (a.mentor_to_mentee[mentor] == mentee) { return false; }
    const auto partner = a.mentor_to_mentee[mentor];
    const bool mentor_wants = !partner
        || position(mentors[mentor], mentee) < position(mentors[mentor], *partner);
    return mentor_wants
        && position(mentees[mentee], mentor)
            < position(mentees[mentee], *a.mentee_to_mentor[mentee]);
}

bool referenceStable(const PreferenceLists& mentors, const PreferenceLists& mentees,
                     const MatchingResult& a) {
    // Called only after structural validation, or for enumerated valid assignments.
    for (std::size_t mentor = 0; mentor < mentors.size(); ++mentor) {
        for (std::size_t mentee : mentors[mentor]) {
            if (blocks(mentors, mentees, a, mentor, mentee)) { return false; }
        }
    }
    return true;
}

void validateStructure(const MatchingResult& a, std::size_t m, std::size_t n) {
    require(a.mentor_to_mentee.size() == m && a.mentee_to_mentor.size() == n,
            "Incorrect output dimensions");
    std::vector<bool> used(m, false);
    for (std::size_t mentee = 0; mentee < n; ++mentee) {
        const auto mentor = a.mentee_to_mentor[mentee];
        require(mentor.has_value() && *mentor < m, "Missing or invalid mentor");
        require(!used[*mentor], "A mentor has multiple mentees");
        used[*mentor] = true;
        require(a.mentor_to_mentee[*mentor] == mentee, "Assignment directions disagree");
    }
    std::size_t free_count = 0;
    for (std::size_t mentor = 0; mentor < m; ++mentor) {
        const auto mentee = a.mentor_to_mentee[mentor];
        if (!mentee) { ++free_count; }
        else {
            require(*mentee < n, "Invalid mentee ID");
            require(a.mentee_to_mentor[*mentee] == mentor, "Extraneous mentor assignment");
        }
    }
    require(free_count == m - n, "Incorrect unmatched count");
    require(a.proposal_count <= m * n && a.proposal_count >= n,
            "Proposal count outside theoretical bounds");
}

std::size_t mentorRank(const PreferenceLists& mentors, const MatchingResult& a,
                       std::size_t mentor) {
    return a.mentor_to_mentee[mentor]
        ? position(mentors[mentor], *a.mentor_to_mentee[mentor]) : mentors[mentor].size();
}

void dump(const char* name, const PreferenceLists& lists) {
    std::cerr << name << " = {\n";
    for (const auto& list : lists) {
        std::cerr << "  {";
        for (std::size_t id : list) { std::cerr << id << ','; }
        std::cerr << "},\n";
    }
    std::cerr << "}\n";
}

std::uint64_t candidate_count = 0;

void exercise(const PreferenceLists& mentors, const PreferenceLists& mentees,
              bool enumerate_candidates, bool repeat) {
    try {
        const auto original_mentors = mentors;
        const auto original_mentees = mentees;
        const auto result = mentoring::matchMentors(mentors, mentees);
        validateStructure(result, mentors.size(), mentees.size());
        require(referenceStable(mentors, mentees, result), "Matcher failed reference stability check");
        require(mentoring::verifyStability(mentors, mentees, result).isReasonable(),
                "Production verifier rejected matcher result");
        require(mentors == original_mentors && mentees == original_mentees, "Inputs changed");
        if (repeat) {
            const auto again = mentoring::matchMentors(mentors, mentees);
            require(again.mentor_to_mentee == result.mentor_to_mentee
                    && again.mentee_to_mentor == result.mentee_to_mentor
                    && again.proposal_count == result.proposal_count, "Nonrepeatable matching");
        }
        if (!enumerate_candidates) { return; }

        // Enumerate every injection from mentees to mentors: all complete 1:1 matchings.
        MatchingResult candidate;
        candidate.mentor_to_mentee.resize(mentors.size());
        candidate.mentee_to_mentor.resize(mentees.size());
        std::size_t stable_count = 0;
        std::function<void(std::size_t)> visit = [&](std::size_t mentee) {
            if (mentee == mentees.size()) {
                ++candidate_count;
                const bool stable = referenceStable(mentors, mentees, candidate);
                const auto checked = mentoring::verifyStability(mentors, mentees, candidate);
                require(checked.status == (stable ? VerificationStatus::Stable : VerificationStatus::Unstable),
                        "Verifier disagreed with independent reference");
                if (!stable) {
                    require(checked.blocking_pair.has_value(), "Missing blocking-pair witness");
                    const auto pair = *checked.blocking_pair;
                    require(pair.mentor < mentors.size() && pair.mentee < mentees.size(),
                            "Out-of-range blocking-pair witness");
                    require(blocks(mentors, mentees, candidate, pair.mentor, pair.mentee),
                            "Reported pair does not block");
                } else {
                    ++stable_count;
                    for (std::size_t mentor = 0; mentor < mentors.size(); ++mentor) {
                        require(mentorRank(mentors, result, mentor) <= mentorRank(mentors, candidate, mentor),
                                "Matcher result is not mentor-optimal");
                    }
                }
                return;
            }
            for (std::size_t mentor = 0; mentor < mentors.size(); ++mentor) {
                if (!candidate.mentor_to_mentee[mentor]) {
                    candidate.mentor_to_mentee[mentor] = mentee;
                    candidate.mentee_to_mentor[mentee] = mentor;
                    visit(mentee + 1);
                    candidate.mentor_to_mentee[mentor].reset();
                    candidate.mentee_to_mentor[mentee].reset();
                }
            }
        };
        visit(0);
        require(stable_count > 0, "No stable matching found in complete enumeration");
    } catch (...) {
        dump("mentor_preferences", mentors);
        dump("mentee_preferences", mentees);
        throw;
    }
}

std::uint64_t exhaustive() {
    std::uint64_t profiles = 0;
    for (std::size_t m = 0; m <= 3; ++m) {
        for (std::size_t n = 0; n <= m; ++n) {
            PreferenceLists mentors(m, std::vector<std::size_t>(n));
            PreferenceLists mentees(n, std::vector<std::size_t>(m));
            for (auto& row : mentors) { std::iota(row.begin(), row.end(), 0); }
            for (auto& row : mentees) { std::iota(row.begin(), row.end(), 0); }
            std::function<void(std::size_t)> fill = [&](std::size_t index) {
                if (index == m + n) {
                    ++profiles;
                    exercise(mentors, mentees, true, false);
                    return;
                }
                auto& row = index < m ? mentors[index] : mentees[index - m];
                do { fill(index + 1); } while (std::next_permutation(row.begin(), row.end()));
            };
            fill(0);
        }
    }
    return profiles;
}

// Rejection sampling plus Fisher-Yates, rather than std::shuffle, makes the
// generated cases repeatable across standard-library implementations.
std::uint32_t bounded(std::mt19937& rng, std::uint32_t bound) {
    const std::uint32_t threshold = (std::uint32_t{0} - bound) % bound;
    std::uint32_t value;
    do { value = static_cast<std::uint32_t>(rng()); } while (value < threshold);
    return value % bound;
}

PreferenceLists preferences(std::size_t people, std::size_t opposite,
                            std::mt19937& rng, bool shared = false) {
    PreferenceLists lists(people, std::vector<std::size_t>(opposite));
    for (auto& row : lists) {
        std::iota(row.begin(), row.end(), 0);
        if (!shared) {
            for (std::size_t remaining = opposite; remaining > 1; --remaining) {
                std::swap(row[remaining - 1], row[bounded(rng, static_cast<std::uint32_t>(remaining))]);
            }
        }
    }
    return lists;
}

void mutationTests(const PreferenceLists& mentors, const PreferenceLists& mentees) {
    const auto valid = mentoring::matchMentors(mentors, mentees);
    auto bad = valid;
    bad.mentee_to_mentor[0].reset();
    require(mentoring::verifyStability(mentors, mentees, bad).status
            == VerificationStatus::InvalidAssignment, "Missing assignment accepted");
    bad = valid;
    bad.mentor_to_mentee[*valid.mentee_to_mentor[0]] = mentees.size();
    require(mentoring::verifyStability(mentors, mentees, bad).status
            == VerificationStatus::InvalidAssignment, "Out-of-range assignment accepted");
    auto bad_preferences = mentors;
    bad_preferences[0][1] = bad_preferences[0][0];
    require(mentoring::verifyStability(bad_preferences, mentees, valid).status
            == VerificationStatus::InvalidPreferences, "Duplicate preferences accepted");
    bool rejected = false;
    try { mentoring::matchMentors(bad_preferences, mentees); }
    catch (const std::invalid_argument&) { rejected = true; }
    require(rejected, "Matcher accepted duplicate preferences");
}

void correctness() {
    const auto profiles = exhaustive();
    require(profiles == 46973 && candidate_count == 281723, "Enumeration coverage count changed");
    std::cout << "Exhaustive: " << profiles << " preference profiles; " << candidate_count
              << " candidate assignments passed\n";
    std::mt19937 rng(seed);
    constexpr std::size_t random_cases = 1000;
    std::size_t mutation_cases = 0;
    for (std::size_t trial = 0; trial < random_cases; ++trial) {
        const std::size_t m = bounded(rng, 61);
        const std::size_t n = bounded(rng, static_cast<std::uint32_t>(m + 1));
        const auto mentors = preferences(m, n, rng);
        const auto mentees = preferences(n, m, rng);
        try {
            exercise(mentors, mentees, false, true);
            if (n >= 2) { mutationTests(mentors, mentees); ++mutation_cases; }
        } catch (...) {
            std::cerr << "Random failure: seed=" << seed << ", trial=" << trial << '\n';
            dump("mentor_preferences", mentors);
            dump("mentee_preferences", mentees);
            throw;
        }
    }
    std::cout << "Randomized: " << random_cases << " cases passed (seed " << seed << ")\n";
    std::cout << "Mutations: " << mutation_cases << " cases x 4 rejection checks passed\n";
    // Deliberately contentious preferences have a known exact proposal count.
    for (std::size_t n : {1U, 2U, 10U, 50U}) {
        for (std::size_t m : {n, 2 * n}) {
            const auto mentors = preferences(m, n, rng, true);
            const auto mentees = preferences(n, m, rng, true);
            exercise(mentors, mentees, false, true);
            require(mentoring::matchMentors(mentors, mentees).proposal_count
                    == m * n - n * (n - 1) / 2, "Incorrect shared-ranking proposal count");
        }
    }
    std::cout << "Contention: 8 shared-ranking cases passed\nALL PHASE 3 CORRECTNESS CHECKS PASSED\n";
}

void benchmark() {
    std::mt19937 rng(seed);
    std::cout << "family,mentors,mentees,proposals,proposal_bound,match_ms_median,verify_ms_median\n";
    for (std::size_t n : {100U, 250U, 500U, 1000U}) {
        for (const std::string family : {"random", "shared", "shared_excess"}) {
            const std::size_t m = family == "shared_excess" ? 2 * n : n;
            const auto mentors = preferences(m, n, rng, family != "random");
            const auto mentees = preferences(n, m, rng, family != "random");
            std::vector<double> match_times, verify_times;
            std::size_t proposals = 0;
            for (int repeat = 0; repeat < 3; ++repeat) {
                const auto start = std::chrono::steady_clock::now();
                const auto a = mentoring::matchMentors(mentors, mentees);
                const auto middle = std::chrono::steady_clock::now();
                const auto checked = mentoring::verifyStability(mentors, mentees, a);
                const auto finish = std::chrono::steady_clock::now();
                require(checked.isReasonable(), "Benchmark produced rejected matching");
                require(a.proposal_count <= m * n, "Benchmark exceeded proposal bound");
                if (family != "random") {
                    require(a.proposal_count == m * n - n * (n - 1) / 2,
                            "Benchmark shared-ranking proposal count mismatch");
                }
                proposals = a.proposal_count;
                match_times.push_back(std::chrono::duration<double, std::milli>(middle - start).count());
                verify_times.push_back(std::chrono::duration<double, std::milli>(finish - middle).count());
            }
            std::sort(match_times.begin(), match_times.end());
            std::sort(verify_times.begin(), verify_times.end());
            std::cout << family << ',' << m << ',' << n << ',' << proposals << ',' << m * n
                      << ',' << std::fixed << std::setprecision(3) << match_times[1]
                      << ',' << verify_times[1] << '\n';
        }
    }
}
} // namespace

int main(int argc, char* argv[]) {
    try {
        if (argc == 1) { correctness(); }
        else if (argc == 2 && std::string(argv[1]) == "--benchmark") { benchmark(); }
        else { std::cerr << "Usage: core_validation [--benchmark]\n"; return 2; }
    } catch (const std::exception& error) {
        std::cerr << "FAILED: " << error.what() << '\n';
        return 1;
    }
}
