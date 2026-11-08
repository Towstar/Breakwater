#pragma once
#include "ProjectDocument.hpp"

namespace breakwater::desktop::telemetry {
QJsonObject sensor_project_context(const ProjectDocument& project);
QJsonObject empty_sensor_configuration(const ProjectDocument& project);
QJsonObject sensor_project_json(const ProjectDocument& project, const QJsonObject& configuration,
                               const QJsonObject& layout);
void validate_sensor_project(const ProjectDocument& project, const QJsonObject& configuration,
                             const QJsonObject& layout);
QJsonObject sensor_anchor(const ProjectDocument& project, const QJsonObject& sensor);
} // namespace breakwater::desktop::telemetry
