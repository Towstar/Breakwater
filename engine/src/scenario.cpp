#include "breakwater/engine.hpp"
#include <algorithm>
#include <stdexcept>

namespace breakwater {

Network canonical_scenario() {
    Network n;
    n.assets = {
        {"tank_a", "Tank A", AssetKind::tank, 120.0, 80.0},
        {"tank_b", "Tank B", AssetKind::tank, 130.0, 114.0},
        {"source_a", "Production A", AssetKind::source, 0.0, 0.0, 52.0, 0.0, 15.0, 250.0},
        {"source_b", "Production B", AssetKind::source, 0.0, 0.0, 16.0, 0.0, 0.0, 250.0},
        {"treatment", "Treatment", AssetKind::treatment, 60.0},
        {"trucking", "Emergency trucking", AssetKind::trucking, 22.0, 0.0, 0.0, 0.0, 0.0, 80.0}
    };
    n.routes = {
        {"feed_a", "source_a", "tank_a", 52.0, 52.0},
        {"feed_b", "source_b", "tank_b", 16.0, 16.0},
        {"main_pump", "tank_a", "treatment", 55.0, 44.0, true},
        {"backup_pump", "tank_b", "treatment", 35.0, 16.0, true},
        {"bypass", "tank_a", "tank_b", 25.0, 0.0},
        {"truck_route", "tank_a", "trucking", 22.0, 0.0}
    };
    return n;
}

Network main_pump_failure(const Network& input, const std::string& route_id) {
    Network n = input;
    auto it = std::find_if(n.routes.begin(), n.routes.end(), [&](const Route& r) { return r.id == route_id; });
    if (it == n.routes.end() || !it->has_pump) throw std::invalid_argument("Select an existing pump route.");
    it->available = false;
    it->setpoint_m3h = 0.0;
    return n;
}

std::vector<ControlValue> incident_controls(const std::string& name) {
    if (name == "baseline") return {};
    if (name == "bad") return {{"route:bypass:flow", 25.0}, {"route:truck_route:flow", 22.0}, {"source:source_a:curtail", 5.0}, {"route:backup_pump:flow", 35.0}};
    if (name == "mixed") return {{"route:bypass:flow", 20.0}, {"route:truck_route:flow", 22.0}, {"source:source_a:curtail", 10.0}, {"route:backup_pump:flow", 35.0}};
    throw std::invalid_argument("Unknown case. Use healthy, baseline, bad or mixed.");
}

} // namespace breakwater
