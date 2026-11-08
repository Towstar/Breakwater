// Private A9 qualification fixtures, not an application or worker wire contract.
// Every result is produced by the authoritative transport() implementation.
#include "breakwater/transport.hpp"

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

Case advection(const std::string& study, std::size_t cells, double cfl) {
    auto c = base("advection", study, cells, cfl, .5, TransportBoundary::dirichlet_inlet);
    initialize_bump(c);
    c.flow = {{0.0, .25 * c.pipe.area_m2}};
    return c;
}

Case closed_diffusion(const std::string& study, std::size_t cells, double cfl) {
    auto c = base("closed_diffusion", study, cells, cfl, .2, TransportBoundary::closed_no_flux);
    c.pipe.dispersion_m2_h = .05;
    constexpr double mean = .6, amplitude = .3;
    const double beta = std::numbers::pi / c.pipe.length_m;
    const double dx = c.pipe.length_m / static_cast<double>(cells);
    for (std::size_t j = 0; j < cells; ++j)
        c.initial[j] = mean + amplitude *
            (std::sin(beta * static_cast<double>(j + 1) * dx) -
             std::sin(beta * static_cast<double>(j) * dx)) / (beta * dx);
    c.reference_parameters = {{"mean_mg_l", mean}, {"amplitude_mg_l", amplitude}};
    return c;
}

Case open_diffusion(const std::string& study, std::size_t cells, double cfl) {
    auto c = base("open_diffusion", study, cells, cfl, .2, TransportBoundary::dirichlet_inlet);
    c.pipe.dispersion_m2_h = .05;
    constexpr double amplitude = .7;
    const double beta = std::numbers::pi / (2.0 * c.pipe.length_m);
    const double dx = c.pipe.length_m / static_cast<double>(cells);
    for (std::size_t j = 0; j < cells; ++j)
        c.initial[j] = amplitude *
            (std::cos(beta * static_cast<double>(j) * dx) -
             std::cos(beta * static_cast<double>(j + 1) * dx)) / (beta * dx);
    c.reference_parameters = {{"amplitude_mg_l", amplitude}};
    return c;
}

Case mixed_adr(const std::string& study, std::size_t cells, double cfl, double mode_fraction) {
    auto c = base("mixed_adr", study, cells, cfl, .5, TransportBoundary::dirichlet_inlet);
    c.id = id(c.regime, study, cells, cfl, mode_fraction);
    c.pipe.dispersion_m2_h = .1;
    c.pipe.decay_per_h = .1;
    const double beta = mode_fraction * std::numbers::pi / c.pipe.length_m;
    const double alpha = -beta * std::cos(beta * c.pipe.length_m) /
        std::sin(beta * c.pipe.length_m);
    const double velocity = 2.0 * c.pipe.dispersion_m2_h * alpha;
    c.flow = {{0.0, velocity * c.pipe.area_m2}};
    const auto primitive = [&](double x) {
        return std::exp(alpha * (x - c.pipe.length_m)) *
            (alpha * std::sin(beta * x) - beta * std::cos(beta * x)) /
            ((alpha * alpha + beta * beta) * std::sin(beta * c.pipe.length_m));
    };
    const double dx = c.pipe.length_m / static_cast<double>(cells);
    for (std::size_t j = 0; j < cells; ++j)
        c.initial[j] = (primitive(static_cast<double>(j + 1) * dx) -
                        primitive(static_cast<double>(j) * dx)) / dx;
    c.reference_parameters = {{"mode_fraction", mode_fraction}};
    return c;
}

Case source_case(const std::string& regime, double initial, double decay, double cfl,
                 const std::string& study) {
    auto c = base(regime, study, 8, cfl, 1.0, TransportBoundary::closed_no_flux);
    c.pipe.decay_per_h = decay;
    std::fill(c.initial.begin(), c.initial.end(), initial);
    std::fill(c.source.begin(), c.source.end(), .25);
    c.reference_parameters = {{"initial_mg_l", initial}, {"source_mg_l_h", .25}};
    return c;
}

Case scheduled_advection(std::size_t cells) {
    auto c = base("scheduled_advection", "space", cells, .01, .6,
                  TransportBoundary::dirichlet_inlet);
    c.config.output_step_h = .05;
    initialize_bump(c);
    c.flow = {{0.0, .4 * c.pipe.area_m2},
              {.137, 0.0}, {.293, .7 * c.pipe.area_m2}};
    return c;
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

std::vector<Case> cases(bool refined_inlet) {
    constexpr std::size_t meshes[] = {32, 64, 128};
    constexpr double time_cfl[] = {.8, .4, .2, .1};
    std::vector<Case> all;
    all.reserve(45);
    for (const auto cells : meshes) all.push_back(advection("space", cells, .01));
    for (const auto cfl : time_cfl) all.push_back(advection("time", 64, cfl));
    for (const auto cells : meshes) all.push_back(closed_diffusion("space", cells, .01));
    for (const auto cfl : time_cfl) all.push_back(closed_diffusion("time", 64, cfl));
    for (const auto cells : meshes) all.push_back(open_diffusion("space", cells, .01));
    for (const auto cfl : time_cfl) all.push_back(open_diffusion("time", 64, cfl));
    for (const double fraction : {.6, .75, .85})
        for (const auto cells : meshes) all.push_back(mixed_adr("space", cells, .01, fraction));
    for (const auto cfl : time_cfl) all.push_back(mixed_adr("time", 64, cfl, .75));
    for (const double cfl : {.5, .25, .125, .0625})
        all.push_back(source_case("reaction_source", 1.0, 1.0, cfl, "time"));
    all.push_back(source_case("source_only", .1, 0.0, .5, "exact"));
    for (const auto cells : meshes) all.push_back(scheduled_advection(cells));
    const std::size_t inlet_meshes[] = {refined_inlet ? 128u : 32u,
                                       refined_inlet ? 256u : 64u,
                                       refined_inlet ? 512u : 128u};
    for (const auto cells : inlet_meshes) all.push_back(scheduled_inlet(cells));
    if (all.size() != 45) throw std::logic_error("A9 fixture matrix must contain exactly 45 cases.");
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
    const auto r = transport(c.pipe, c.initial, c.flow, c.inlet, c.config, c.source);
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
    out << '}';
}

void rejection(std::ostream& out, const std::string& name, const Case& c,
               const std::string& expected_type) {
    const auto record = [&](const char* type, const std::exception& error) {
        if (expected_type != type)
            throw std::runtime_error("Unexpected rejection category for A9 case: " + name);
        out << "{\"id\":"; string(out, name);
        out << ",\"exception_type\":"; string(out, type);
        out << ",\"message\":"; string(out, error.what());
        out << '}';
    };
    try {
        (void)transport(c.pipe, c.initial, c.flow, c.inlet, c.config, c.source);
    } catch (const std::invalid_argument& error) {
        record("invalid_argument", error); return;
    } catch (const std::runtime_error& error) {
        record("runtime_error", error); return;
    }
    throw std::runtime_error("A9 work-limit request unexpectedly succeeded: " + name);
}

void rejections(std::ostream& out) {
    auto budget = advection("space", 64, .01);
    budget.config.max_substeps = 1;
    rejection(out, "substep_budget", budget, "runtime_error"); out << ',';
    auto frames = advection("space", 64, .01);
    frames.config.horizon_h = 1.0;
    frames.config.output_step_h = 1e-5;
    rejection(out, "output_frame_budget", frames, "invalid_argument"); out << ',';
    auto values = advection("space", 4096, .01);
    values.config.horizon_h = 1.0;
    values.config.output_step_h = .001;
    rejection(out, "output_value_budget", values, "invalid_argument");
}
} // namespace

int main(int argc, char** argv) {
    const bool refined_inlet = argc == 2 && std::string(argv[1]) == "--refined-inlet";
    if (argc != 1 && !refined_inlet) {
        std::cerr << "Usage: breakwater_transport_qualification_reference [--refined-inlet] > transport-qualification.json\n";
        return 2;
    }
    try {
        // A failed native execution must not publish a partial fixture as evidence.
        std::ostringstream out;
        out.imbue(std::locale::classic());
        out << std::setprecision(std::numeric_limits<double>::max_digits10)
            << "{\"fixture_version\":1,\"qualification_variant\":";
        string(out, refined_inlet ? "refined_inlet" : "baseline");
        out << ",\"solver_version\":";
        string(out, transport_solver_version);
        out << ",\"cases\":[";
        const auto all = cases(refined_inlet);
        for (std::size_t i = 0; i < all.size(); ++i) {
            if (i) out << ',';
            fixture(out, all[i]);
        }
        out << "],\"rejections\":[";
        rejections(out);
        out << "]}\n";
        std::cout << out.str();
        if (!std::cout) throw std::runtime_error("Failed to write A9 qualification output.");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 2;
    }
}
