// Private A4 science-test fixture, not an application/worker wire contract.
#include "breakwater/transport.hpp"

#include <cmath>
#include <iomanip>
#include <iostream>
#include <limits>
#include <locale>
#include <numbers>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
constexpr double length_m = 1.0;
constexpr double area_m2 = 1.0;
constexpr double dispersion_m2_h = 0.1;
constexpr double decay_per_h = 0.1;
constexpr double horizon_h = 1.0;
constexpr double output_step_h = 0.025;
constexpr double b_per_m = 3.0 * std::numbers::pi / 4.0;
constexpr double a_per_m = b_per_m;
constexpr double velocity_m_h = 2.0 * dispersion_m2_h * b_per_m;
constexpr double flow_m3h = velocity_m_h * area_m2;
constexpr double eigenvalue_per_h = decay_per_h + 2.0 * dispersion_m2_h * b_per_m * b_per_m;

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
    if (!std::isfinite(value)) throw std::runtime_error("Nonfinite A4 reference value.");
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

// Integral of the positive initial eigenmode:
// phi(x) = exp(a*(x-L))*sin(b*x)/sin(b*L).
// With a=b=3*pi/(4L), phi(0)=0 and phi'(L)=0. The continuous
// source-free solution is c(x,t)=phi(x)*exp(-eigenvalue*t).
double primitive(double x_m) {
    return std::exp(a_per_m * (x_m - length_m)) *
        (a_per_m * std::sin(b_per_m * x_m) - b_per_m * std::cos(b_per_m * x_m)) /
        ((a_per_m * a_per_m + b_per_m * b_per_m) * std::sin(b_per_m * length_m));
}

void run(std::ostream& out, std::size_t cells, double cfl_safety) {
    breakwater::PipeConfig pipe;
    pipe.length_m = length_m;
    pipe.area_m2 = area_m2;
    pipe.dispersion_m2_h = dispersion_m2_h;
    pipe.decay_per_h = decay_per_h;
    pipe.cells = cells;
    const double dx_m = length_m / static_cast<double>(cells);
    std::vector<double> initial(cells);
    for (std::size_t j = 0; j < cells; ++j) {
        const double left = static_cast<double>(j) * dx_m;
        const double right = static_cast<double>(j + 1) * dx_m;
        initial[j] = (primitive(right) - primitive(left)) / dx_m;
    }
    const std::vector<breakwater::FlowKnot> flow{{0.0, flow_m3h}};
    const std::vector<breakwater::InletKnot> inlet{{0.0, 0.0}};
    breakwater::TransportConfig config;
    config.horizon_h = horizon_h;
    config.output_step_h = output_step_h;
    config.cfl_safety = cfl_safety;
    const auto value = breakwater::transport(pipe, initial, flow, inlet, config);

    out << "{\"cells\":" << cells << ",\"cfl_safety\":";
    number(out, cfl_safety);
    out << ",\"x_m\":"; numbers(out, value.x_m);
    out << ",\"time_h\":"; numbers(out, value.time_h);
    out << ",\"concentration_mg_l\":[";
    for (std::size_t t = 0; t < value.concentration_mg_l.size(); ++t) {
        if (t) out << ',';
        numbers(out, value.concentration_mg_l[t]);
    }
    out << "],\"inventory_mg\":"; numbers(out, value.inventory_mg);
    out << ",\"cumulative_inlet_mg\":"; numbers(out, value.cumulative_inlet_mg);
    out << ",\"cumulative_outlet_mg\":"; numbers(out, value.cumulative_outlet_mg);
    out << ",\"cumulative_source_mg\":"; numbers(out, value.cumulative_source_mg);
    out << ",\"cumulative_decay_mg\":"; numbers(out, value.cumulative_decay_mg);
    out << ",\"mass_balance_residual_mg\":"; numbers(out, value.mass_balance_residual_mg);
    out << ",\"diagnostics\":{\"substeps\":" << value.diagnostics.substeps;
    out << ",\"max_cfl_number\":"; number(out, value.diagnostics.max_cfl_number);
    out << ",\"minimum_concentration_mg_l\":";
    number(out, value.diagnostics.minimum_concentration_mg_l);
    out << ",\"max_mass_balance_residual_mg\":";
    number(out, value.diagnostics.max_mass_balance_residual_mg);
    out << "}}";
}
} // namespace

int main(int argc, char**) {
    if (argc != 1) {
        std::cerr << "Usage: breakwater_physicsnemo_reference > native-transport-reference.json\n";
        return 2;
    }
    try {
        // Buffer first: any failure leaves stdout free of a partial fixture.
        std::ostringstream out;
        out.imbue(std::locale::classic());
        out << std::setprecision(std::numeric_limits<double>::max_digits10)
            << "{\"fixture_version\":1,\"solver_version\":";
        string(out, breakwater::transport_solver_version);
        out << ",\"case\":{\"name\":\"positive_eigenmode_transient\",\"length_m\":";
        number(out, length_m);
        out << ",\"area_m2\":"; number(out, area_m2);
        out << ",\"flow_m3h\":"; number(out, flow_m3h);
        out << ",\"dispersion_m2_h\":"; number(out, dispersion_m2_h);
        out << ",\"decay_per_h\":"; number(out, decay_per_h);
        out << ",\"horizon_h\":"; number(out, horizon_h);
        out << ",\"inlet_mg_l\":0,\"initial_profile\":\"positive_eigenmode\",\"b_per_m\":";
        number(out, b_per_m);
        out << ",\"a_per_m\":"; number(out, a_per_m);
        out << ",\"decay_eigenvalue_per_h\":"; number(out, eigenvalue_per_h);
        out << "},\"runs\":[";
        run(out, 32, 0.9); out << ',';
        run(out, 64, 0.9); out << ',';
        run(out, 128, 0.9); out << ',';
        run(out, 128, 0.45);
        out << "]}\n";
        std::cout << out.str();
        if (!std::cout) throw std::runtime_error("Failed to write A4 reference output.");
        return 0;
    } catch (const std::exception& e) {
        std::cerr << e.what() << '\n';
        return 2;
    }
}
