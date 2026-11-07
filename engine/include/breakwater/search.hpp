#pragma once
#include "breakwater/engine.hpp"

namespace breakwater {

inline constexpr std::size_t max_search_controls = 4;
inline constexpr std::size_t max_search_candidates = 10000;
inline constexpr std::size_t max_search_recommendations = 5;

struct SearchAxis {
    std::string control_id;
    std::vector<double> values;
};

struct CandidateControls { std::vector<ControlValue> overrides; };

struct SearchOptions {
    SimulationConfig config;
    std::size_t result_limit = max_search_recommendations;
};

enum class SearchStatus { recommendations_ready, no_feasible_plan_found };

struct SearchDiagnostics {
    std::size_t candidates_evaluated = 0;
    std::size_t screen_passes = 0; // Potential survivors, not validated feasible candidates.
    std::size_t constraint_screen_rejections = 0;
    std::size_t numerical_screen_rejections = 0;
    std::size_t reference_checks = 0;
    std::size_t reference_rejections = 0;
};

struct RankedCandidate {
    std::size_t candidate_index = 0;
    std::vector<ControlValue> overrides;
    SimulationResult run;
    ValidationReport validation;
};

struct SearchResult {
    SearchStatus status = SearchStatus::no_feasible_plan_found;
    SearchDiagnostics diagnostics;
    std::vector<RankedCandidate> recommendations;
};

// All input is validated before evaluation. Malformed requests throw invalid_argument.
// Grid order is input order, with the last axis varying fastest; no axes means one
// unchanged candidate. Repeated values are permitted and count toward the 10000 cap.
// Only actual dense-reference-validated recommendations are returned, ordered by
// computed cost then candidate_index. This is finite candidate search, not a global
// optimum or proof of infeasibility. Nonfinite forecasts/cost are rejected and counted.
SearchResult search_candidates(const Network& network, std::span<const SearchAxis> axes,
                               SearchOptions options = {});

// Nonempty explicit batch, at most 10000 candidates and four distinct control IDs
// across the entire batch. Omitted controls retain the immutable input's values.
SearchResult evaluate_candidates(const Network& network,
                                 std::span<const CandidateControls> candidates,
                                 SearchOptions options = {});

} // namespace breakwater
