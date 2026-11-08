// Private F1b candidate exporter. Case builders/serialization copied from frozen F1.
// Independent Python expectations validate every physical input and identity.
#include "front_advection.hpp"

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <iostream>
#include <limits>
#include <locale>
#include <numbers>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace {
using namespace breakwater;

struct Case {
    std::string id, regime, study;
    PipeConfig pipe;
    TransportConfig config;
    std::vector<double> initial, source;
    std::vector<FlowKnot> flow;
    std::vector<InletKnot> inlet;
    std::vector<std::pair<std::string, double>> reference_parameters;
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
    if (!std::isfinite(value)) throw std::runtime_error("Nonfinite A9 fixture value.");
    out << value;
}

void numbers(std::ostream& out, const std::vector<double>& values) {
    out << '[';
    for (std::size_t i = 0; i < values.size(); ++i) {
        if (i) out << ',';
        number(out, values[i]);
    }
    out << ']';
}

std::string id(const std::string& regime, const std::string& study,
               std::size_t cells, double cfl, double mode_fraction = 0.0) {
    std::ostringstream out;
    out.imbue(std::locale::classic());
    out << regime << '.' << study;
    if (mode_fraction != 0.0) out << ".mode" << mode_fraction;
    out << ".n" << cells << ".cfl" << cfl;
    return out.str();
}

Case base(const std::string& regime, const std::string& study, std::size_t cells,
          double cfl, double horizon, TransportBoundary boundary) {
    Case c;
    c.id = id(regime, study, cells, cfl);
    c.regime = regime; c.study = study;
    c.pipe = {1.0, .2, 0.0, 0.0, cells};
    c.config.horizon_h = horizon;
    c.config.output_step_h = horizon;
    c.config.cfl_safety = cfl;
    c.config.boundary = boundary;
    c.initial.assign(cells, 0.0);
    c.source.assign(cells, 0.0);
    c.flow = {{0.0, 0.0}};
    if (boundary == TransportBoundary::dirichlet_inlet) c.inlet = {{0.0, 0.0}};
    return c;
}

double bump_primitive(double x) {
    constexpr double start = .125, width = .5;
    // Bound the integration domain to the support of the analytic initial bump.
    // This does not clip a simulated concentration or any engine output.
    const double z = std::clamp(x - start, 0.0, width);
    return z / 2.0 - width * std::sin(2.0 * std::numbers::pi * z / width) /
        (4.0 * std::numbers::pi);
}

void initialize_bump(Case& c) {
    const double dx = c.pipe.length_m / static_cast<double>(c.pipe.cells);
    for (std::size_t j = 0; j < c.pipe.cells; ++j)
        c.initial[j] = (bump_primitive(static_cast<double>(j + 1) * dx) -
                        bump_primitive(static_cast<double>(j) * dx)) / dx;
    c.reference_parameters = {{"bump_start_m", .125}, {"bump_width_m", .5}};
}

Case scheduled_inlet(std::size_t cells) {
    auto c = base("scheduled_inlet", "space", cells, .1, .6,
                  TransportBoundary::dirichlet_inlet);
    c.config.output_step_h = .05;
    c.flow = {{0.0, .4 * c.pipe.area_m2},
              {.137, 0.0}, {.293, .7 * c.pipe.area_m2}};
    c.inlet = {{0.0, 0.0}, {.11, 1.0}, {.29, .2}, {.47, .8}, {.63, 0.0}};
    c.reference_parameters = {{"initial_mg_l", 0.0}};
    return c;
}

Case front_initial(const std::string& regime, const std::string& study,
                   std::size_t cells, double cfl) {
    auto c = base(regime, study, cells, cfl, 4.0, TransportBoundary::dirichlet_inlet);
    c.config.output_step_h = .0125;
    c.flow = {{0.0, .25 * c.pipe.area_m2}};
    initialize_bump(c);
    if (regime == "front_box") {
        const double dx = c.pipe.length_m / static_cast<double>(cells);
        for (std::size_t j = 0; j < cells; ++j) {
            // Exact geometric overlap of the unit box with a cell, not clipping
            // of a simulated concentration. No solver output is changed.
            const double left = std::max(static_cast<double>(j) * dx, .125);
            const double right = std::min(static_cast<double>(j + 1) * dx, .625);
            c.initial[j] = std::max(0.0, right - left) / dx;
        }
    }
    return c;
}

std::vector<Case> front_cases() {
    std::vector<Case> all;
    all.reserve(22);
    for (const auto* regime : {"front_smooth", "front_box"}) {
        for (const std::size_t cells : {64u, 128u, 256u, 512u})
            all.push_back(front_initial(regime, "space", cells, .02));
        for (const double cfl : {.4, .2, .1, .05})
            all.push_back(front_initial(regime, "time", 128, cfl));
    }
    for (const std::size_t cells : {128u, 256u, 512u}) all.push_back(scheduled_inlet(cells));
    for (const std::size_t cells : {128u, 256u, 512u}) {
        auto c = scheduled_inlet(cells);
        c.study = "extended_space";
        c.id = id(c.regime, c.study, cells, c.config.cfl_safety);
        c.config.horizon_h = 2.2;
        c.config.output_step_h = .0125;
        all.push_back(std::move(c));
    }
    if (all.size() != 22) throw std::logic_error("F1 matrix must contain exactly 22 cases.");
    return all;
}

void result(std::ostream& out, const TransportResult& r) {
    out << "{\"solver_version\":"; string(out, r.solver_version);
    out << ",\"x_m\":"; numbers(out, r.x_m);
    out << ",\"time_h\":"; numbers(out, r.time_h);
    out << ",\"concentration_mg_l\":[";
    for (std::size_t i = 0; i < r.concentration_mg_l.size(); ++i) {
        if (i) out << ',';
        numbers(out, r.concentration_mg_l[i]);
    }
    out << "],\"inventory_mg\":"; numbers(out, r.inventory_mg);
    out << ",\"cumulative_inlet_mg\":"; numbers(out, r.cumulative_inlet_mg);
    out << ",\"cumulative_outlet_mg\":"; numbers(out, r.cumulative_outlet_mg);
    out << ",\"cumulative_source_mg\":"; numbers(out, r.cumulative_source_mg);
    out << ",\"cumulative_decay_mg\":"; numbers(out, r.cumulative_decay_mg);
    out << ",\"mass_balance_residual_mg\":"; numbers(out, r.mass_balance_residual_mg);
    out << ",\"diagnostics\":{\"substeps\":" << r.diagnostics.substeps;
    out << ",\"max_cfl_number\":"; number(out, r.diagnostics.max_cfl_number);
    out << ",\"minimum_concentration_mg_l\":"; number(out, r.diagnostics.minimum_concentration_mg_l);
    out << ",\"max_mass_balance_residual_mg\":"; number(out, r.diagnostics.max_mass_balance_residual_mg);
    out << "}}";
}

void fixture(std::ostream& out, const Case& c) {
    const auto experiment = front_experiment::advect(c.pipe, c.initial, c.flow, c.inlet, c.config, c.source);
    const auto& r = experiment.transport;
    out << "{\"id\":"; string(out, c.id);
    out << ",\"regime\":"; string(out, c.regime);
    out << ",\"study\":"; string(out, c.study);
    out << ",\"pipe\":{\"length_m\":"; number(out, c.pipe.length_m);
    out << ",\"area_m2\":"; number(out, c.pipe.area_m2);
    out << ",\"dispersion_m2_h\":"; number(out, c.pipe.dispersion_m2_h);
    out << ",\"decay_per_h\":"; number(out, c.pipe.decay_per_h);
    out << ",\"cells\":" << c.pipe.cells << "},\"config\":{\"horizon_h\":";
    number(out, c.config.horizon_h);
    out << ",\"output_step_h\":"; number(out, c.config.output_step_h);
    out << ",\"cfl_safety\":"; number(out, c.config.cfl_safety);
    out << ",\"boundary\":";
    string(out, c.config.boundary == TransportBoundary::closed_no_flux ?
           "closed_no_flux" : "dirichlet_inlet");
    out << ",\"max_substeps\":" << c.config.max_substeps << "},\"flow\":[";
    for (std::size_t i = 0; i < c.flow.size(); ++i) {
        if (i) out << ',';
        out << "{\"time_h\":"; number(out, c.flow[i].time_h);
        out << ",\"flow_m3h\":"; number(out, c.flow[i].flow_m3h);
        out << '}';
    }
    out << "],\"inlet\":[";
    for (std::size_t i = 0; i < c.inlet.size(); ++i) {
        if (i) out << ',';
        out << "{\"time_h\":"; number(out, c.inlet[i].time_h);
        out << ",\"concentration_mg_l\":"; number(out, c.inlet[i].concentration_mg_l);
        out << '}';
    }
    out << "],\"initial_mg_l\":"; numbers(out, c.initial);
    out << ",\"source_mg_l_h\":"; numbers(out, c.source);
    out << ",\"reference_parameters\":{";
    for (std::size_t i = 0; i < c.reference_parameters.size(); ++i) {
        if (i) out << ',';
        string(out, c.reference_parameters[i].first); out << ':';
        number(out, c.reference_parameters[i].second);
    }
    out << "},\"result\":"; result(out, r);
    out << ",\"execution\":{\"stage_evaluations\":" << experiment.stage_evaluations
        << ",\"cell_stage_updates\":" << experiment.cell_stage_updates
        << ",\"maximum_concentration_mg_l\":";
    number(out, experiment.maximum_concentration_mg_l); out << '}';
    out << '}';
}


std::vector<Case> comparison_cases() {
    auto all = front_cases();
    for (const auto* regime : {"front_smooth", "front_box"})
        for (double cfl : {.08, .04}) all.push_back(front_initial(regime, "time_probe", 512, cfl));
    for (double cfl : {.4, .2}) {
        auto c = scheduled_inlet(512);
        c.study = "time_probe"; c.config.cfl_safety = cfl;
        c.id = id(c.regime,c.study,512,cfl);
        c.config.horizon_h = 2.2; c.config.output_step_h = .0125;
        all.push_back(std::move(c));
    }
    if (all.size() != 28) throw std::logic_error("F1b requires exactly 28 candidate cases.");
    return all;
}
} // namespace
int main(int argc, char**) {
    if (argc != 1) { std::cerr << "Usage: breakwater_front_comparison_reference\n"; return 2; }
    try {
        std::ostringstream out; out.imbue(std::locale::classic());
        out << std::setprecision(std::numeric_limits<double>::max_digits10)
            << "{\"fixture_version\":1,\"qualification_variant\":\"front_comparison\",\"solver_version\":";
        string(out, front_experiment::solver_version); out << ",\"cases\":[";
        const auto all = comparison_cases();
        for (std::size_t i=0; i<all.size(); ++i) { if (i) out << ','; fixture(out,all[i]); }
        out << "]}\n"; std::cout << out.str();
        if (!std::cout) throw std::runtime_error("Failed to publish candidate fixture.");
        return 0;
    } catch (const std::exception& e) { std::cerr << e.what() << '\n'; return 2; }
}
