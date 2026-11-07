#include "breakwater/transport.hpp"
#include <cmath>
#include <fstream>
#include <iomanip>
#include <iostream>

int main(int argc, char** argv) {
    try {
        if (argc != 1 && !(argc == 3 && std::string(argv[1]) == "--csv")) {
            std::cerr << "Usage: breakwater_transport_example [--csv output.csv]\n"; return 2;
        }
        const breakwater::PipeConfig pipe;
        const std::vector<double> initial(pipe.cells, 0.2);
        const std::vector<breakwater::FlowKnot> flow{{0.0, 20.0}, {0.4, 15.0}};
        std::vector<breakwater::InletKnot> inlet;
        for (int i = 0; i <= 40; ++i) {
            const double t = static_cast<double>(i) * 0.025;
            const double pulse_coordinate = (t - 0.25) / 0.08;
            inlet.push_back({t, 0.2 + 0.4 * std::exp(-pulse_coordinate * pulse_coordinate)});
        }
        breakwater::TransportConfig config; config.horizon_h = 1.0;
        const auto run = breakwater::transport(pipe, initial, flow, inlet, config);
        std::cout << std::setprecision(12) << "SYNTHETIC | " << run.solver_version << " | single-pipe tracer reference\n"
                  << "cells=" << pipe.cells << " frames=" << run.time_h.size() << " substeps=" << run.diagnostics.substeps
                  << " maximum_cfl=" << run.diagnostics.max_cfl_number << '\n'
                  << "final_inventory_mg=" << run.inventory_mg.back()
                  << " signed_inlet_exchange_mg=" << run.cumulative_inlet_mg.back()
                  << " outlet_exchange_mg=" << run.cumulative_outlet_mg.back()
                  << " decayed_mg=" << run.cumulative_decay_mg.back() << '\n'
                  << "maximum_mass_residual_mg=" << run.diagnostics.max_mass_balance_residual_mg
                  << " minimum_concentration_mg_l=" << run.diagnostics.minimum_concentration_mg_l
                  << " final_outlet_cell_mg_l=" << run.concentration_mg_l.back().back() << '\n';
        if (argc == 3) {
            std::ofstream csv(argv[2]);
            if (!csv) throw std::runtime_error("Cannot open requested CSV output.");
            csv << std::setprecision(17) << "time_h,x_m,concentration_mg_l\n";
            for (std::size_t t = 0; t < run.time_h.size(); ++t)
                for (std::size_t j = 0; j < run.x_m.size(); ++j)
                    csv << run.time_h[t] << ',' << run.x_m[j] << ',' << run.concentration_mg_l[t][j] << '\n';
            if (!csv) throw std::runtime_error("CSV write failed.");
            std::cout << "Wrote computed time-distance concentration grid to " << argv[2] << '\n';
        }
        std::cout << "Scalar transport proxy; network-wide chemistry and learned model validation are separate work.\n";
        return 0;
    } catch (const std::exception& e) { std::cerr << e.what() << '\n'; return 2; }
}
