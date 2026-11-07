// A5: measured production kernels and full search, with independent correctness gates.
#include "workloads.hpp"
#include "timing.hpp"
#include "../src/forecast_kernels.hpp"
#include "../src/search_kernel.hpp"

#include <bit>
#include <cstdint>
#include <cstdlib>
#include <fstream>
#include <functional>
#include <iomanip>
#include <iostream>
#include <limits>
#include <locale>
#include <numeric>
#include <set>
#include <sstream>
#include <thread>

namespace {
using namespace breakwater;
namespace bench = breakwater::bench;
namespace detail = breakwater::detail;
volatile double consumed = 0.0;

#if defined(_MSC_VER)
__declspec(noinline)
#elif defined(__GNUC__)
__attribute__((noinline))
#endif
void consume(std::span<const double> data) {
    double sum = 0;
    for (std::size_t i = 0; i < data.size(); ++i) sum += data[i] * static_cast<double>(1+i%7);
    consumed = sum; // Observable full trajectory read; identical dense/sparse overhead.
}

bool close(double a, double b) {
    return std::isfinite(a) && std::isfinite(b) &&
        std::abs(a-b) <= 1e-9+1e-10*std::max(std::abs(a),std::abs(b));
}

std::string processor_identifier() {
#ifdef _MSC_VER
    char* value=nullptr;std::size_t length=0;
    if(_dupenv_s(&value,&length,"PROCESSOR_IDENTIFIER")!=0 || !value)return "unavailable";
    const std::string result(value);std::free(value);return result;
#else
    const auto* value=std::getenv("PROCESSOR_IDENTIFIER");return value?value:"unavailable";
#endif
}

struct NumericBatch {
    std::size_t rows=0, tanks=0, edge_count=0;
    std::vector<double> initial, in, out, incidence;
    std::vector<InternalEdge> edges;
    detail::StorageRatesView view(std::size_t row) const {
        return {std::span(in).subspan(row*tanks,tanks), std::span(out).subspan(row*tanks,tanks),
                std::span(edges).subspan(row*edge_count,edge_count)};
    }
    std::size_t bytes() const {
        return (initial.size()+in.size()+out.size()+incidence.size())*sizeof(double)+edges.size()*sizeof(InternalEdge);
    }
};

NumericBatch pack(const Network& network, const std::vector<CandidateControls>& candidates) {
    const auto baseline=compile(network);
    NumericBatch data;
    data.rows=candidates.size(); data.tanks=baseline.tank_ids.size(); data.edge_count=baseline.edges.size();
    data.initial=baseline.initial_m3;
    data.in.reserve(data.rows*data.tanks); data.out.reserve(data.rows*data.tanks);
    data.edges.reserve(data.rows*data.edge_count);
    for (const auto& candidate:candidates) {
        const auto compiled=compile(network,candidate.overrides);
        if (compiled.tank_ids!=baseline.tank_ids || compiled.edges.size()!=data.edge_count)
            throw std::logic_error("Candidate changed benchmark topology");
        for (std::size_t e=0;e<data.edge_count;++e)
            if (compiled.edges[e].source!=baseline.edges[e].source || compiled.edges[e].target!=baseline.edges[e].target)
                throw std::logic_error("Candidate changed edge order");
        data.in.insert(data.in.end(),compiled.external_in_m3h.begin(),compiled.external_in_m3h.end());
        data.out.insert(data.out.end(),compiled.external_out_m3h.begin(),compiled.external_out_m3h.end());
        data.edges.insert(data.edges.end(),compiled.edges.begin(),compiled.edges.end());
    }
    data.incidence.resize(data.tanks*data.edge_count);
    detail::build_incidence(data.view(0),data.incidence);
    return data;
}

struct TrajectoryWorkspace {
    std::vector<double> time, state, rates, trajectory;
    TrajectoryWorkspace(const NumericBatch& data,SimulationConfig config) {
        const auto steps=static_cast<std::size_t>(std::ceil(config.horizon_h/config.step_h));
        for (std::size_t k=0;k<=steps;++k) time.push_back(std::min(static_cast<double>(k)*config.step_h,config.horizon_h));
        state.resize(data.tanks); rates.resize(data.tanks); trajectory.resize(data.tanks*time.size());
    }
    void rollout(const NumericBatch& data,std::size_t row,bool dense) {
        std::copy(data.initial.begin(),data.initial.end(),state.begin());
        for (std::size_t i=0;i<data.tanks;++i) trajectory[i*time.size()]=state[i];
        const auto view=data.view(row);
        for (std::size_t k=1;k<time.size();++k) {
            if (dense) detail::dense_rates(view,data.incidence,rates);
            else detail::sparse_rates(view,rates);
            detail::euler_update(state,rates,time[k]-time[k-1]);
            for (std::size_t i=0;i<data.tanks;++i) trajectory[i*time.size()+k]=state[i];
        }
    }
    std::size_t bytes() const {return (time.size()+state.size()+rates.size()+trajectory.size())*sizeof(double);}
};

struct Phase {
    std::string name, scope, allocation_scope;
    std::size_t rows=0, iterations=1;
    std::function<void()> run;
    std::vector<double> samples;
};

std::string fingerprint(const std::vector<CandidateControls>& candidates) {
    std::uint64_t hash=14695981039346656037ULL;
    auto add=[&](unsigned char c){hash^=c;hash*=1099511628211ULL;};
    for (const auto& row:candidates) for (const auto& c:row.overrides) {
        for (const unsigned char ch:c.id) add(ch);
        add(0);
        const auto bits=std::bit_cast<std::uint64_t>(c.value);
        for (int shift=0;shift<64;shift+=8) add(static_cast<unsigned char>((bits>>shift)&255));
    }
    std::ostringstream out;out<<std::hex<<hash;return out.str();
}

struct Retained {CompiledNetwork network;SimulationResult run;};

void workload_report(std::ostream& json,const bench::Workload& workload,std::size_t count,
                     std::size_t repeats,std::size_t warmups,bool verify_only) {
    const SimulationConfig config{3.0,.025};
    const SearchOptions options{config,5};
    const auto candidate_rows=bench::candidates(workload,count);
    std::set<std::array<double,4>> unique;
    for(const auto& row:candidate_rows) {
        std::array<double,4> values{};
        if(row.overrides.size()!=4)throw std::logic_error("Expected four controls per benchmark row");
        for(std::size_t i=0;i<4;++i) {
            if(row.overrides[i].id!=workload.varying_control_ids[i])throw std::logic_error("Control ID/order mismatch");
            values[i]=row.overrides[i].value;
        }
        unique.insert(values);
    }
    if(unique.size()!=count)throw std::logic_error("Duplicate benchmark candidates");
    const auto prep_start=bench::Clock::now();
    const auto data=pack(workload.network,candidate_rows);
    const double preparation_ms=std::chrono::duration<double,std::milli>(bench::Clock::now()-prep_start).count();
    detail::Prepared prepared(workload.network);
    detail::CandidateSet set;
    for(const auto& id:workload.varying_control_ids)set.bindings.push_back(prepared.find_control(id));
    for(const auto& row:candidate_rows) {
        detail::Values values{};
        for(std::size_t j=0;j<4;++j){prepared.check_value(set.bindings[j],row.overrides[j].value);values[j]=row.overrides[j].value;}
        set.values.push_back(values);
    }
    detail::Workspace screen_workspace(prepared);
    TrajectoryWorkspace trajectory(data,config);
    std::vector<std::pair<double,std::size_t>> feasible;
    std::size_t screen_passes=0;
    double max_trajectory_error=0;
    // Correctness is outside all measured phases; every candidate is reference-checked.
    for(std::size_t row=0;row<count;++row) {
        const auto compiled=compile(workload.network,candidate_rows[row].overrides);
        const auto reference=simulate(compiled,config,Solver::dense_reference);
        const auto validation=validate(compiled,reference,config);
        if(validation.feasible)feasible.emplace_back(reference.cost,row);
        for(const bool dense:{true,false}) {
            trajectory.rollout(data,row,dense);
            for(std::size_t i=0;i<data.tanks;++i)for(std::size_t k=0;k<trajectory.time.size();++k) {
                const auto actual=trajectory.trajectory[i*trajectory.time.size()+k],expected=reference.volumes_m3[i][k];
                if(!close(actual,expected))throw std::logic_error("Sampled benchmark/public reference disagreement");
                max_trajectory_error=std::max(max_trajectory_error,std::abs(actual-expected));
            }
        }
        std::vector<double> endpoint(data.tanks);
        evaluate_endpoints(compiled,config.horizon_h,endpoint);
        for(std::size_t i=0;i<data.tanks;++i) {
            if(!close(endpoint[i],reference.volumes_m3[i].back()))throw std::logic_error("Affine endpoint mismatch");
            max_trajectory_error=std::max(max_trajectory_error,std::abs(endpoint[i]-reference.volumes_m3[i].back()));
        }
        double cost=0;
        const auto screen=detail::screen_candidate(prepared,set,set.values[row],config.horizon_h,screen_workspace,cost);
        if(screen==detail::Screen::numerical_failure || !close(cost,reference.cost))throw std::logic_error("Screen arithmetic mismatch");
        if(screen==detail::Screen::potential)++screen_passes;
        if(validation.feasible && screen!=detail::Screen::potential)throw std::logic_error("Screen discarded a reference-feasible plan");
    }
    std::sort(feasible.begin(),feasible.end());
    if(feasible.empty() || feasible.size()==count)throw std::logic_error("Workload must exercise feasible and infeasible candidates");
    const auto search=evaluate_candidates(workload.network,candidate_rows,options);
    if(search.diagnostics.candidates_evaluated!=count || search.diagnostics.screen_passes!=screen_passes ||
       search.recommendations.size()!=std::min<std::size_t>(5,feasible.size()))throw std::logic_error("Public search counts mismatch");
    std::vector<Retained> retained;
    for(std::size_t i=0;i<search.recommendations.size();++i) {
        const auto& recommendation=search.recommendations[i];
        if(recommendation.candidate_index!=feasible[i].second || !close(recommendation.run.cost,feasible[i].first) ||
           !recommendation.validation.feasible)throw std::logic_error("Independent full-batch ranking mismatch");
        retained.push_back({compile(workload.network,recommendation.overrides),recommendation.run});
    }

    std::vector<Phase> phases;
    phases.push_back({"dense_sampled_kernel","all candidates, 120 dense incidence/Euler steps, full trajectory writes and consumption",
        "prebuilt topology/numeric inputs and reusable workspace; no allocation instrumentation",count,1,[&]{
            for(std::size_t row=0;row<count;++row){trajectory.rollout(data,row,true);consume(trajectory.trajectory);}
        },{}});
    phases.push_back({"sparse_sampled_kernel","same samples/outputs/consumption as dense; sparse edge updates",
        "same preallocation as dense; no allocation instrumentation",count,1,[&]{
            for(std::size_t row=0;row<count;++row){trajectory.rollout(data,row,false);consume(trajectory.trajectory);}
        },{}});
    phases.push_back({"affine_screen_kernel","actual production endpoint/storage/shared-destination/cost screen; no sampled trajectories or final validation",
        "prepared production topology/candidate buffers/workspace; no allocation instrumentation",count,1,[&]{
            double checksum=0;
            for(std::size_t row=0;row<count;++row){double cost=0;const auto s=detail::screen_candidate(prepared,set,set.values[row],config.horizon_h,screen_workspace,cost);checksum+=cost+static_cast<int>(s);}
            consume(std::span(&checksum,1));
        },{}});
    phases.push_back({"reference_validate_retained","independent validate() on retained dense runs; excludes compile/simulate",
        "validator map/report allocations included",retained.size(),1,[&]{
            double checksum=0;
            for(const auto& row:retained){const auto report=validate(row.network,row.run,config);if(!report.feasible)throw std::logic_error("Retained validation changed");checksum+=report.minimum_headroom_m3+report.max_conservation_residual_m3;}
            consume(std::span(&checksum,1));
        },{}});
    phases.push_back({"public_search_total","evaluate_candidates(): preparation, bounds, screen, ranking, retained compile/dense simulation/validation and result materialization",
        "all public API allocations and destruction included",count,1,[&]{
            const auto result=evaluate_candidates(workload.network,candidate_rows,options);
            if(result.diagnostics.candidates_evaluated!=count || result.recommendations.size()!=retained.size())throw std::logic_error("Timed search changed counts");
            double checksum=0;for(const auto& row:result.recommendations)checksum+=row.run.cost+static_cast<double>(row.candidate_index);
            consume(std::span(&checksum,1));
        },{}});
    if(!verify_only) {
        for(auto& phase:phases) {
            for(std::size_t i=0;i<warmups;++i)phase.run();
            phase.iterations=bench::calibrate(phase.run,10.0,1024);
        }
        for(std::size_t repetition=0;repetition<repeats;++repetition)
            for(std::size_t offset=0;offset<phases.size();++offset) {
                auto& phase=phases[(offset+repetition)%phases.size()];
                phase.samples.push_back(bench::sample_ms(phase.run,phase.iterations));
            }
    }
    std::cerr<<workload.name<<" candidates="<<count<<" verified="<<count<<" feasible="<<feasible.size()
             <<" screen_passes="<<screen_passes<<" reference_checks="<<search.diagnostics.reference_checks<<'\n';
    json<<"{\"workload\":";bench::json_string(json,workload.name);
    json<<",\"candidates\":"<<count<<",\"distinct_candidates\":"<<unique.size()<<",\"candidate_fnv1a64\":";
    bench::json_string(json,fingerprint(candidate_rows));
    json<<",\"assets\":"<<workload.network.assets.size()<<",\"routes\":"<<workload.network.routes.size()
        <<",\"tanks\":"<<data.tanks<<",\"internal_edges\":"<<data.edge_count<<",\"samples\":"<<trajectory.time.size()
        <<",\"horizon_h\":3,\"step_h\":0.025,\"prepared_numeric_bytes\":"<<data.bytes()
        <<",\"trajectory_workspace_bytes\":"<<trajectory.bytes()<<",\"preparation_once_ms\":"<<preparation_ms
        <<",\"max_forward_error_m3\":"<<max_trajectory_error<<",\"all_candidates_reference_checked\":true"
        <<",\"reference_feasible_candidates\":"<<feasible.size()<<",\"screen_passes\":"<<screen_passes
        <<",\"reference_checks_in_search\":"<<search.diagnostics.reference_checks
        <<",\"reference_rejections_in_search\":"<<search.diagnostics.reference_rejections
        <<",\"recommendations\":"<<retained.size()<<",\"best_evaluated_cost\":"<<feasible.front().first
        <<",\"best_candidate_index\":"<<feasible.front().second<<",\"phases\":[";
    if(!verify_only)for(std::size_t i=0;i<phases.size();++i) {
        if(i)json<<',';const auto& phase=phases[i];const auto s=bench::summarize(phase.samples);
        json<<"{\"name\":";bench::json_string(json,phase.name);json<<",\"scope\":";bench::json_string(json,phase.scope);
        json<<",\"allocation_scope\":";bench::json_string(json,phase.allocation_scope);
        json<<",\"rows_per_batch\":"<<phase.rows<<",\"iterations_per_sample\":"<<phase.iterations
            <<",\"minimum_ms\":"<<s.minimum_ms<<",\"median_ms\":"<<s.median_ms<<",\"mean_ms\":"<<s.mean_ms
            <<",\"p90_ms\":"<<s.p90_ms<<",\"maximum_ms\":"<<s.maximum_ms<<",\"stddev_ms\":"<<s.stddev_ms
            <<",\"median_ns_per_row\":"<<s.median_ms*1e6/static_cast<double>(phase.rows)<<",\"samples_ms\":[";
        for(std::size_t k=0;k<phase.samples.size();++k){if(k)json<<',';json<<phase.samples[k];}json<<"]}";
    }
    json<<"]}";
}
} // namespace

int main(int argc,char** argv) {
    try {
        bool verify_only=false;std::vector<std::size_t> counts{100,1000,10000};
        std::size_t repeats=7,warmups=2;std::string output;
        for(int i=1;i<argc;++i) {
            const std::string arg=argv[i];
            if(arg=="--verify-only")verify_only=true;
            else if((arg=="--count" || arg=="--repeats" || arg=="--warmups" || arg=="--output") && i+1<argc) {
                const std::string value=argv[++i];
                if(arg=="--output")output=value;
                else {std::size_t consumed_chars=0;const auto n=std::stoull(value,&consumed_chars);if(consumed_chars!=value.size())throw std::invalid_argument("Expected unsigned integer");
                    if(arg=="--count")counts={static_cast<std::size_t>(n)};
                    else if(arg=="--repeats")repeats=static_cast<std::size_t>(n);else warmups=static_cast<std::size_t>(n);}
            } else throw std::invalid_argument("Usage: breakwater_benchmark [--count 100|1000|10000] [--repeats 3..31] [--warmups 1..10] [--verify-only] [--output file.json]");
        }
        if(repeats<3 || repeats>31 || warmups<1 || warmups>10)throw std::invalid_argument("Invalid repeat/warmup bounds");
        for(const auto n:counts)if(n!=100 && n!=1000 && n!=10000)throw std::invalid_argument("Candidate count must be 100,1000,10000");
        if(BREAKWATER_BENCH_CHECKED && !verify_only)throw std::invalid_argument("Checked build: use --verify-only; sanitizer timings are not performance evidence");
#ifndef NDEBUG
        if(!verify_only)throw std::invalid_argument("Performance measurement requires an optimized non-Debug build");
#endif
        std::ostringstream json;json.imbue(std::locale::classic());json<<std::setprecision(17);
        json<<"{\"benchmark_version\":1,\"status\":\"passed\",\"method\":";
        bench::json_string(json,verify_only?"correctness_only":"optimized_serial_precise_fp");
        json<<",\"configuration\":";bench::json_string(json,BREAKWATER_BENCH_CONFIGURATION);
        json<<",\"checked_build\":"<<(BREAKWATER_BENCH_CHECKED?"true":"false")<<",\"cpp_standard\":2020,\"compiler\":";
#ifdef _MSC_FULL_VER
        bench::json_string(json,"MSVC "+std::to_string(_MSC_FULL_VER));
#else
        bench::json_string(json,__VERSION__);
#endif
        json<<",\"logical_processors_reported\":"<<std::thread::hardware_concurrency()<<",\"processor_identifier\":";
        bench::json_string(json,processor_identifier());
        json<<",\"steady_clock\":"<<(bench::Clock::is_steady?"true":"false")<<",\"warmup_batches\":"<<warmups
            <<",\"repeats\":"<<repeats<<",\"calibration_target_ms\":10,\"maximum_iterations_per_sample\":1024"
            <<",\"phase_order\":\"rotated per repetition\",\"quantile\":\"p90 nearest rank; stddev population\""
            <<",\"notes\":\"Kernel setup excluded. Dense/sparse have identical samples, writes and full-buffer checksum consumption. Affine avoids trajectories. Retained validation is five runs, not the full batch. Overlapping phases do not sum to total search. No global optimum or universal speedup claim.\",\"results\":[";
        bool first=true;
        for(const auto& workload:bench::workloads())for(const auto count:counts){if(!first)json<<',';first=false;workload_report(json,workload,count,repeats,warmups,verify_only);}
        json<<"]}\n";
        if(output.empty())std::cout<<json.str();
        else {std::ofstream file(output);file<<json.str();if(!file)throw std::runtime_error("Cannot write benchmark output");}
        return 0;
    }catch(const std::exception& error){std::cerr<<"Benchmark failed: "<<error.what()<<'\n';return 2;}
}
