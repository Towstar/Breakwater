#pragma once

#include "breakwater/search.hpp"

namespace breakwater::detail {
// Private baseline used by differential tests/benchmarks. It shares production
// request validation, candidate materialization, ranking and finalist validation.
SearchResult evaluate_candidates_route_walk(const Network& network,
    std::span<const CandidateControls> candidates, SearchOptions options = {});
}
