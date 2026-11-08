// Private A3 science-test fixtures, not an application/worker wire contract.
#include "breakwater/engine.hpp"

#include <cmath>
#include <iomanip>
#include <iostream>
#include <limits>
#include <locale>
#include <sstream>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace {
using namespace breakwater;

struct Case {
    std::string name;
    Network network;
    std::vector<ControlValue> overrides;
    SimulationConfig config;
};

void string(std::ostream& out, const std::string& value) {
    constexpr char hex[] = "0123456789abcdef";
    out << '"';
    for (const unsigned char c : value) {
        if (c == '"') out << "\\\"";
        else if (c == '\\') out << "\\\\";
        else if (c < 0x20) out << "\\u00" << hex[c >> 4] << hex[c & 15];
        else out << static_cast<char>(c);
    }
    out << '"';
}

void number(std::ostream& out, double value) {
    if (!std::isfinite(value)) throw std::runtime_error("Nonfinite native fixture value.");
    out << value;
}

void numbers(std::ostream& out, const std::vector<double>& values) {
    out << '[';
    bool first = true;
    for (const auto value : values) {
        if (!first) out << ',';
        first = false;
        number(out, value);
    }
    out << ']';
}

void network(std::ostream& out, const Network& value) {
    out << "{\"assets\":[";
    bool first = true;
    for (const auto& a : value.assets) {
        if (!first) out << ',';
        first = false;
        out << "{\"id\":"; string(out, a.id);
        out << ",\"label\":"; string(out, a.label);
        out << ",\"kind\":"; string(out, kind_name(a.kind));
        out << ",\"capacity_m3\":"; number(out, a.capacity_m3);
        out << ",\"initial_m3\":"; number(out, a.initial_m3);
        out << ",\"inflow_m3h\":"; number(out, a.inflow_m3h);
        out << ",\"curtailment_m3h\":"; number(out, a.curtailment_m3h);
        out << ",\"curtailment_max_m3h\":"; number(out, a.curtailment_max_m3h);
        out << ",\"cost_per_m3\":"; number(out, a.cost_per_m3);
        out << '}';
    }
    out << "],\"routes\":[";
    first = true;
    for (const auto& r : value.routes) {
        if (!first) out << ',';
        first = false;
        out << "{\"id\":"; string(out, r.id);
        out << ",\"source_id\":"; string(out, r.source_id);
        out << ",\"target_id\":"; string(out, r.target_id);
        out << ",\"capacity_m3h\":"; number(out, r.capacity_m3h);
        out << ",\"setpoint_m3h\":"; number(out, r.setpoint_m3h);
        out << ",\"has_pump\":" << (r.has_pump ? "true" : "false");
        out << ",\"available\":" << (r.available ? "true" : "false");
        out << ",\"cost_per_m3\":"; number(out, r.cost_per_m3);
        out << '}';
    }
    out << "]}";
}

void reference(std::ostream& out, const Case& c, Solver solver) {
    const auto compiled = compile(c.network, c.overrides);
    const auto run = simulate(compiled, c.config, solver);
    const auto validation = validate(compiled, run, c.config);
    out << "{\"tank_ids\":[";
    for (std::size_t i = 0; i < run.tank_ids.size(); ++i) {
        if (i) out << ',';
        string(out, run.tank_ids[i]);
    }
    out << "],\"time_h\":"; numbers(out, run.time_h);
    out << ",\"volumes_m3\":[";
    for (std::size_t i = 0; i < run.volumes_m3.size(); ++i) {
        if (i) out << ',';
        numbers(out, run.volumes_m3[i]);
    }
    out << "],\"cost\":"; number(out, run.cost);
    out << ",\"feasible\":" << (validation.feasible ? "true" : "false") << '}';
}

void fixture(std::ostream& out, const Case& c) {
    out << "{\"name\":"; string(out, c.name);
    out << ",\"network\":"; network(out, c.network);
    out << ",\"overrides\":[";
    for (std::size_t i = 0; i < c.overrides.size(); ++i) {
        if (i) out << ',';
        out << "{\"id\":"; string(out, c.overrides[i].id);
        out << ",\"value\":"; number(out, c.overrides[i].value);
        out << '}';
    }
    out << "],\"config\":{\"horizon_h\":"; number(out, c.config.horizon_h);
    out << ",\"step_h\":"; number(out, c.config.step_h);
    out << "},\"references\":{\"dense_reference\":";
    reference(out, c, Solver::dense_reference);
    out << ",\"sparse\":"; reference(out, c, Solver::sparse);
    out << ",\"affine\":"; reference(out, c, Solver::affine);
    out << "}}";
}

Case renamed() {
    auto n = main_pump_failure(canonical_scenario());
    std::unordered_map<std::string, std::string> names;
    for (std::size_t i = 0; i < n.assets.size(); ++i)
        names.emplace(n.assets[i].id, "asset:\"\\\n" + std::to_string(i));
    for (std::size_t i = 0; i < n.routes.size(); ++i)
        names.emplace(n.routes[i].id, "route:\"\\\t" + std::to_string(i));
    std::vector<ControlValue> controls = {
        {"source:" + names.at("source_a") + ":curtail", 7.0},
        {"route:" + names.at("bypass") + ":flow", 13.0},
        {"route:" + names.at("truck_route") + ":flow", 11.0},
        {"route:" + names.at("backup_pump") + ":flow", 26.0}
    };
    for (auto& a : n.assets) a.id = names.at(a.id);
    for (std::size_t i = 0; i < n.routes.size(); ++i) {
        auto& r = n.routes[i];
        r.id = names.at(r.id);
        r.source_id = names.at(r.source_id);
        r.target_id = names.at(r.target_id);
        r.cost_per_m3 = i == 0 ? 11.0 : (i == 1 ? 7.0 : 1.25);
    }
    return {"renamed_costed_interior", std::move(n), std::move(controls), {1.3, 0.2}};
}

Network branching() {
    Network n;
    n.assets = {
        {"west", "West tank", AssetKind::tank, 1000.0, 100.0},
        {"buffer", "Buffer", AssetKind::tank, 500.0, 100.0},
        {"south", "South tank", AssetKind::tank, 600.0, 100.0},
        {"production", "Production", AssetKind::source, 0.0, 0.0, 20.0, 2.0, 8.0, 9.0},
        {"secondary", "Secondary", AssetKind::source, 0.0, 0.0, 11.0, 1.0, 3.0, 4.0},
        {"shared", "Shared treatment", AssetKind::treatment, 50.0, 0.0, 0.0, 0.0, 0.0, 3.0},
        {"trucks", "Shared trucking", AssetKind::trucking, 20.0, 0.0, 0.0, 0.0, 0.0, 17.0}
    };
    n.routes = {
        {"feed", "production", "west", 20.0, 20.0, false, true, 2.0},
        {"second_feed", "secondary", "buffer", 11.0, 11.0, false, true, 1.5},
        {"west_buffer", "west", "buffer", 15.0, 7.0, false, true, 0.5},
        {"west_south", "west", "south", 12.0, 4.0, false, true, 0.75},
        {"buffer_south", "buffer", "south", 20.0, 5.0, true, true, 1.25},
        {"south_treat", "south", "shared", 20.0, 6.0, true, true, 2.0},
        {"buffer_treat", "buffer", "shared", 15.0, 4.0, false, true, 1.5},
        {"west_truck", "west", "trucks", 10.0, 3.0, false, true, 2.0},
        {"buffer_truck", "buffer", "trucks", 10.0, 2.0, false, true, 3.0}
    };
    return n;
}

std::vector<Case> cases() {
    std::vector<Case> result;
    for (const auto* name : {"healthy", "baseline", "bad", "mixed"}) {
        const bool healthy = std::string(name) == "healthy";
        result.push_back({name, healthy ? canonical_scenario() : main_pump_failure(canonical_scenario()),
                          healthy ? std::vector<ControlValue>{} : incident_controls(name), {}});
    }
    result.push_back(renamed());
    result.push_back({"branching_shared", branching(), {
        {"source:production:curtail", 3.0}, {"route:west_buffer:flow", 8.0},
        {"route:buffer_south:flow", 6.0}, {"route:south_treat:flow", 9.0}}, {2.7, 0.23}});

    Network single;
    single.assets = {
        {"only", "Only tank", AssetKind::tank, 10.0, 6.0},
        {"source", "Source", AssetKind::source, 0.0, 0.0, 2.0, 0.25, 1.0, 5.0},
        {"sink", "Sink", AssetKind::treatment, 5.0, 0.0, 0.0, 0.0, 0.0, 2.0}
    };
    single.routes = {
        {"in", "source", "only", 2.0, 2.0, false, true, 1.0},
        {"out", "only", "sink", 4.0, 2.5, true, true, 0.5}
    };
    result.push_back({"one_tank_no_internal", std::move(single), {}, {3.0, 0.07}});

    Network isolated;
    isolated.assets = {{"isolated", "No controls", AssetKind::tank, 10.0, 6.0}};
    result.push_back({"zero_controls", std::move(isolated), {}, {0.07, 0.2}});

    Network equality;
    equality.assets = {
        {"full", "Reaches capacity", AssetKind::tank, 10.0, 4.0},
        {"source", "Source", AssetKind::source, 0.0, 0.0, 3.0, 0.0, 1.0}
    };
    equality.routes = {{"feed", "source", "full", 3.0, 3.0}};
    result.push_back({"capacity_equality", std::move(equality), {}, {2.0, 0.3}});

    Network negative;
    negative.assets = {
        {"drains", "Draining tank", AssetKind::tank, 10.0, 1.0},
        {"sink", "Sink", AssetKind::trucking, 2.0}
    };
    negative.routes = {{"out", "drains", "sink", 2.0, 2.0, true}};
    result.push_back({"unclipped_negative", std::move(negative), {}, {3.0, 0.4}});

    // Native structural inspection tolerates this stored default; an explicit
    // override above the exact catalog bound would still be rejected by compile.
    Network stored_tolerance;
    stored_tolerance.assets = {
        {"tank", "Stored tolerance", AssetKind::tank, 10.0, 6.0},
        {"sink", "Sink", AssetKind::treatment, 3.0}
    };
    stored_tolerance.routes = {{"out", "tank", "sink", 2.0, 2.0 + 5e-10, true}};
    result.push_back({"stored_tolerance", std::move(stored_tolerance), {}, {1.0, 0.3}});

    auto overloaded = branching();
    for (auto& a : overloaded.assets) if (a.id == "shared") a.capacity_m3 = 8.0;
    result.push_back({"shared_destination_overload", std::move(overloaded), {}, {0.9, 0.13}});

    // A-001 regressions: preserve every rounded requested interior multiple and
    // finish at the exact requested horizon, including a one-ULP terminal tail.
    Network stationary;
    stationary.assets = {{"grid-tank", "Stationary grid fixture", AssetKind::tank, 1.0, .5}};
    result.push_back({"grid_short_quotient", stationary, {}, {.9000000000000001, .1}});
    result.push_back({"grid_duplicate_quotient", stationary, {}, {.30000000000000004, .1}});
    result.push_back({"grid_subnormal_horizon", stationary, {}, {std::numeric_limits<double>::denorm_min(), 24.0}});
    result.push_back({"grid_update_limit", stationary, {}, {1.0, .0001}});
    result.push_back({"grid_update_limit_rounded_quotient", stationary, {}, {.0082, 8.2e-7}});

    const double q = std::ldexp(1.0, 50);
    Network high_flow;
    high_flow.assets = {{"capacity-tank", "Requested endpoint overflow", AssetKind::tank, .9*q, 0.0},
                       {"fixed-source", "Fixed source", AssetKind::source, 0.0, 0.0, q}};
    high_flow.routes = {{"fixed-feed", "fixed-source", "capacity-tank", q, 0.0}};
    result.push_back({"grid_requested_capacity_overflow", std::move(high_flow), {}, {.9000000000000001, .1}});
    return result;
}
} // namespace

int main(int argc, char**) {
    if (argc != 1) {
        std::cerr << "Usage: breakwater_rollout_reference > native-reference.json\n";
        return 2;
    }
    try {
        // Buffer first so failed native execution never emits a partial fixture.
        std::ostringstream out;
        out.imbue(std::locale::classic());
        out << std::setprecision(std::numeric_limits<double>::max_digits10)
            << "{\"fixture_version\":1,\"solver_version\":";
        string(out, breakwater::solver_version);
        out << ",\"cases\":[";
        const auto values = cases();
        for (std::size_t i = 0; i < values.size(); ++i) {
            if (i) out << ',';
            fixture(out, values[i]);
        }
        out << "]}\n";
        std::cout << out.str();
        if (!std::cout) throw std::runtime_error("Failed to write native fixture output.");
        return 0;
    } catch (const std::exception& e) {
        std::cerr << e.what() << '\n';
        return 2;
    }
}
