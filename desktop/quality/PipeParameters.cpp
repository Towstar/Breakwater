#include "PipeParameters.hpp"

#include <QCryptographicHash>
#include <QJsonDocument>
#include <QSet>
#include <cmath>
#include <stdexcept>

namespace breakwater::desktop::quality {
namespace {
void require(bool condition, const char* message) {
    if (!condition) throw std::invalid_argument(message);
}
void validate(const PipeParameters& p) {
    require(std::isfinite(p.pipe.length_m) && p.pipe.length_m > 0.0 &&
        std::isfinite(p.pipe.area_m2) && p.pipe.area_m2 > 0.0,
        "Pipe length and cross-section must be positive finite values.");
    for (const auto value : {p.pipe.dispersion_m2_h, p.pipe.decay_per_h,
             p.initial_mg_l, p.inlet_mg_l, p.source_mg_l_h})
        require(std::isfinite(value) && value >= 0.0, "Concentration, dispersion, decay and source must be finite and nonnegative.");
    require(p.pipe.cells >= 2 && p.pipe.cells <= max_transport_cells, "Pipe cells must be in the native range 2..4096.");
    require(std::isfinite(p.output_step_h) && p.output_step_h > 0.0,
        "Transport output step must be positive and finite.");
    require(std::isfinite(p.cfl_safety) && p.cfl_safety > 0.0 && p.cfl_safety <= 1.0,
        "Transport CFL safety must be in (0, 1].");
    require(p.boundary == TransportBoundary::dirichlet_inlet || p.boundary == TransportBoundary::closed_no_flux,
        "Unknown transport boundary.");
}
double number(const QJsonObject& object, const char* field) {
    const auto value = object.value(QLatin1String(field));
    require(value.isDouble() && std::isfinite(value.toDouble()), "Pipe parameter fields must be finite numbers.");
    return value.toDouble();
}
}

QJsonObject pipe_parameters_json(const PipeParameters& p) {
    validate(p); // QJson would otherwise turn a nonfinite C++ number into null.
    return {{"length_m", p.pipe.length_m}, {"area_m2", p.pipe.area_m2},
        {"dispersion_m2_h", p.pipe.dispersion_m2_h}, {"decay_per_h", p.pipe.decay_per_h},
        {"cells", static_cast<qint64>(p.pipe.cells)}, {"initial_mg_l", p.initial_mg_l},
        {"inlet_mg_l", p.inlet_mg_l}, {"source_mg_l_h", p.source_mg_l_h},
        {"output_step_h", p.output_step_h}, {"cfl_safety", p.cfl_safety},
        {"boundary", p.boundary == TransportBoundary::dirichlet_inlet ? "dirichlet_inlet" : "closed_no_flux"}};
}

PipeParameters parse_pipe_parameters(const QJsonObject& object) {
    static const QSet<QString> fields{"length_m", "area_m2", "dispersion_m2_h", "decay_per_h", "cells",
        "initial_mg_l", "inlet_mg_l", "source_mg_l_h", "output_step_h", "cfl_safety", "boundary"};
    require(object.size() == fields.size(), "A pipe parameter entry must contain exactly the documented fields.");
    for (auto it = object.begin(); it != object.end(); ++it)
        require(fields.contains(it.key()), "Unknown pipe parameter field.");
    PipeParameters p;
    p.pipe.length_m = number(object, "length_m"); p.pipe.area_m2 = number(object, "area_m2");
    p.pipe.dispersion_m2_h = number(object, "dispersion_m2_h"); p.pipe.decay_per_h = number(object, "decay_per_h");
    const auto cells = number(object, "cells");
    require(cells >= 2.0 && cells <= static_cast<double>(max_transport_cells) && std::floor(cells) == cells,
        "Pipe cells must be an integer in 2..4096.");
    p.pipe.cells = static_cast<std::size_t>(cells);
    p.initial_mg_l = number(object, "initial_mg_l"); p.inlet_mg_l = number(object, "inlet_mg_l");
    p.source_mg_l_h = number(object, "source_mg_l_h"); p.output_step_h = number(object, "output_step_h");
    p.cfl_safety = number(object, "cfl_safety");
    const auto boundary = object.value("boundary");
    require(boundary.isString(), "Transport boundary must be a string.");
    if (boundary.toString() == "dirichlet_inlet") p.boundary = TransportBoundary::dirichlet_inlet;
    else if (boundary.toString() == "closed_no_flux") p.boundary = TransportBoundary::closed_no_flux;
    else throw std::invalid_argument("Unknown transport boundary.");
    validate(p);
    return p;
}

QString pipe_parameters_hash(const PipeParameters& p) {
    const QJsonObject envelope{{"parameter_version", 1}, {"parameters", pipe_parameters_json(p)}};
    return QString::fromLatin1(QCryptographicHash::hash(QJsonDocument(envelope).toJson(QJsonDocument::Compact),
        QCryptographicHash::Sha256).toHex());
}

} // namespace breakwater::desktop::quality
