#include "breakwater/eog.hpp"
#include <type_traits>
static_assert(breakwater::eog::version == 1);
static_assert(std::is_default_constructible_v<breakwater::eog::AnalysisRequest>);
static_assert(std::is_default_constructible_v<breakwater::eog::AnalysisResponse>);
void instantiate_eog_dto_contract() {
    const breakwater::eog::AnalysisRequest request;
    const breakwater::eog::AnalysisResponse response;
    (void)request; (void)response;
}
