#pragma once

#include "breakwater/transport.hpp"
#include <QJsonObject>
#include <QString>

namespace breakwater::desktop::quality {

// Independent physical/numerical inputs for the selected-pipe scalar model.
// Neither editor coordinates nor native tank-volume controls supply geometry.
struct PipeParameters {
    PipeConfig pipe{};
    double initial_mg_l = 0.0;
    double inlet_mg_l = 1.0;
    double source_mg_l_h = 0.0;
    double output_step_h = 0.025;
    double cfl_safety = 0.9;
    TransportBoundary boundary = TransportBoundary::dirichlet_inlet;
};

// A serialized entry is complete and strict; an absent project route entry uses
// the defaults above. Invalid/nonfinite inputs throw std::invalid_argument.
QJsonObject pipe_parameters_json(const PipeParameters& parameters);
PipeParameters parse_pipe_parameters(const QJsonObject& object);
QString pipe_parameters_hash(const PipeParameters& parameters);

} // namespace breakwater::desktop::quality
