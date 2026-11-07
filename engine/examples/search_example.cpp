#include "breakwater/search.hpp"
#include <iomanip>
#include <iostream>
#include <string>

int main(int argc, char** argv) {
    try {
        const bool infeasible = argc == 2 && std::string(argv[1]) == "--infeasible";
        if (argc != 1 && !infeasible) {
            std::cerr << "Usage: breakwater_search_example [--infeasible]\n"; return 2;
        }
        const auto incident = breakwater::main_pump_failure(breakwater::canonical_scenario());
        const std::vector<breakwater::SearchAxis> axes = infeasible ?
            std::vector<breakwater::SearchAxis>{{"route:bypass:flow", {0.0, 25.0}}} :
            std::vector<breakwater::SearchAxis>{
                {"route:bypass:flow", {0, 5, 10, 15, 20, 25}},
                {"route:backup_pump:flow", {16, 25, 35}},
                {"route:truck_route:flow", {0, 5, 10, 15, 20, 22}},
                {"source:source_a:curtail", {0, 5, 10, 15}}};
        const auto result = breakwater::search_candidates(incident, axes);
        const auto& d = result.diagnostics;
        std::cout << std::setprecision(12) << "SYNTHETIC | " << breakwater::solver_version
                  << " | nominal 3 h finite candidate search\n"
                  << "candidates=" << d.candidates_evaluated << " screen_survivors=" << d.screen_passes
                  << " constraint_rejections=" << d.constraint_screen_rejections
                  << " numerical_rejections=" << d.numerical_screen_rejections
                  << " reference_checks=" << d.reference_checks << " reference_rejections=" << d.reference_rejections << '\n';
        if (result.status == breakwater::SearchStatus::no_feasible_plan_found) {
            std::cout << "NO_FEASIBLE_PLAN_FOUND among evaluated candidates; not a global infeasibility proof.\n"; return 0;
        }
        for (const auto& candidate : result.recommendations) {
            std::cout << "candidate=" << candidate.candidate_index << " reference_validated=true cost=" << candidate.run.cost
                      << " synthetic_units minimum_headroom=" << candidate.validation.minimum_headroom_m3 << " m3\n";
            for (const auto& value : candidate.overrides) std::cout << "  " << value.id << '=' << value.value << " m3/h\n";
            for (std::size_t i = 0; i < candidate.run.tank_ids.size(); ++i)
                std::cout << "  " << candidate.run.tank_ids[i] << " final=" << candidate.run.volumes_m3[i].back() << " m3\n";
        }
        std::cout << "Best among evaluated candidates. Quality/stress robustness not evaluated.\n";
        return 0;
    } catch (const std::exception& e) { std::cerr << e.what() << '\n'; return 2; }
}
