#pragma once
#include "breakwater/engine.hpp"
#include <QJsonObject>
#include <QMap>
#include <QVector3D>

namespace breakwater::desktop {
struct ProjectDocument {
    Network network;
    QMap<QString, QVector3D> positions;
    double camera_yaw = 35.0;
    double camera_pitch = -55.0;
    double camera_distance = 720.0;
    double camera_x = 0.0;
    double camera_z = 0.0;
};
QJsonObject network_json(const Network& network);
QJsonObject project_json(const ProjectDocument& project);
ProjectDocument parse_project(const QJsonObject& object);
ProjectDocument example_project();
QString physics_hash(const ProjectDocument& project);
}
