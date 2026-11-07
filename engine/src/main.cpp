#include "breakwater/engine.hpp"
#include <iomanip>
#include <iostream>
#include <string>

int main(int argc, char** argv) {
    try {
        std::string name = "baseline";
        if (argc == 3 && std::string(argv[1]) == "--case") name = argv[2];
        else if (argc != 1) { std::cerr << "Usage: breakwater_cli [--case healthy|baseline|bad|mixed]\n"; return 2; }
        auto network = breakwater::canonical_scenario();
        if (name != "healthy") network = breakwater::main_pump_failure(network);
        const auto controls = name == "healthy" ? std::vector<breakwater::ControlValue>{} : breakwater::incident_controls(name);
        const auto compiled = breakwater::compile(network, controls);
        const auto run = breakwater::simulate(compiled);
        const auto report = breakwater::validate(compiled, run);
        std::cout << std::setprecision(12) << "SYNTHETIC | " << breakwater::solver_version << " | " << name << "\n"
                  << "Nominal 3 h volume feasibility: " << (report.feasible ? "PASS" : "FAIL") << "\n";
        for (std::size_t i = 0; i < run.tank_ids.size(); ++i) std::cout << run.tank_ids[i] << " final_m3=" << run.volumes_m3[i].back() << "\n";
        for (const auto& limit : report.limits) std::cout << limit.asset_id << " " << limit.limit << " reached_at_h=" << limit.time_h << "\n";
        for (const auto& issue : report.violations) std::cout << issue.code << " " << issue.asset_id << ": " << issue.message << "\n";
        std::cout << "cost_synthetic_units=" << run.cost << "\nmax_conservation_residual_m3=" << report.max_conservation_residual_m3 << "\n";
        return 0; // A successfully evaluated infeasible plan is a valid CLI result.
    } catch (const std::exception& e) { std::cerr << e.what() << '\n'; return 2; }
}
