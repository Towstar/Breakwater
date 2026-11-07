#include "ProjectDocument.hpp"
#include <QCryptographicHash>
#include <QJsonArray>
#include <QJsonDocument>
#include <QSet>
#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace breakwater::desktop {
namespace {
QString q(const std::string& s) { return QString::fromStdString(s); }
double number(const QJsonObject& o, const QString& key, double fallback = 0.0) {
    if (!o.contains(key)) return fallback;
    if (!o.value(key).isDouble() || !std::isfinite(o.value(key).toDouble())) throw std::invalid_argument("Expected finite numeric field: " + key.toStdString());
    return o.value(key).toDouble();
}
std::string text(const QJsonObject& o, const QString& key) {
    if (!o.value(key).isString() || o.value(key).toString().size() > 256) throw std::invalid_argument("Expected string field: " + key.toStdString());
    return o.value(key).toString().toStdString();
}
bool boolean(const QJsonObject& o, const QString& key, bool fallback) {
    if (!o.contains(key)) return fallback;
    if (!o.value(key).isBool()) throw std::invalid_argument("Expected boolean field: " + key.toStdString());
    return o.value(key).toBool();
}
}

QJsonObject network_json(const Network& n) {
    QJsonArray assets, routes;
    for (const auto& a : n.assets) {
        QJsonObject o{{"id", q(a.id)}, {"label", q(a.label)}, {"kind", q(kind_name(a.kind))}};
        if (a.kind == AssetKind::tank) { o["capacity_m3"] = a.capacity_m3; o["initial_m3"] = a.initial_m3; }
        else if (a.kind == AssetKind::source) {
            o["inflow_m3h"] = a.inflow_m3h; o["curtailment_m3h"] = a.curtailment_m3h;
            o["curtailment_max_m3h"] = a.curtailment_max_m3h; o["cost_per_m3"] = a.cost_per_m3;
        } else { o["capacity_m3h"] = a.capacity_m3; o["cost_per_m3"] = a.cost_per_m3; }
        assets.append(o);
    }
    for (const auto& r : n.routes) routes.append(QJsonObject{
        {"id", q(r.id)}, {"source_id", q(r.source_id)}, {"target_id", q(r.target_id)},
        {"capacity_m3h", r.capacity_m3h}, {"setpoint_m3h", r.setpoint_m3h},
        {"has_pump", r.has_pump}, {"available", r.available}, {"cost_per_m3", r.cost_per_m3}});
    return {{"assets", assets}, {"routes", routes}};
}
QJsonObject project_json(const ProjectDocument& p) {
    QJsonObject positions;
    for (auto it = p.positions.cbegin(); it != p.positions.cend(); ++it)
        positions[it.key()] = QJsonObject{{"x", it.value().x()}, {"z", it.value().z()}};
    QJsonObject result{{"version", project_version}, {"synthetic", true}, {"network", network_json(p.network)},
        {"layout", QJsonObject{{"positions", positions}, {"camera", QJsonObject{{"yaw", p.camera_yaw}, {"pitch", p.camera_pitch}, {"distance", p.camera_distance}, {"x", p.camera_x}, {"z", p.camera_z}}}}}};
    if (!p.pipe_parameters.isEmpty()) {
        QJsonObject routes;
        for (auto it = p.pipe_parameters.cbegin(); it != p.pipe_parameters.cend(); ++it) routes[it.key()] = quality::pipe_parameters_json(it.value());
        result["pipe_transport"] = QJsonObject{{"version", 1}, {"routes", routes}};
    }
    return result;
}
ProjectDocument parse_project(const QJsonObject& root) {
    if (number(root, "version", -1.0) != project_version || !root.value("network").isObject()) throw std::invalid_argument("Unsupported or missing project version/network.");
    const auto n = root.value("network").toObject();
    if (!n.value("assets").isArray() || !n.value("routes").isArray()) throw std::invalid_argument("Network needs asset and route arrays.");
    const auto assets = n.value("assets").toArray(), routes = n.value("routes").toArray();
    if (assets.size() > 128 || routes.size() > 256) throw std::invalid_argument("Project exceeds base network size limits.");
    ProjectDocument p;
    for (const auto& value : assets) {
        if (!value.isObject()) throw std::invalid_argument("Asset must be an object.");
        const auto o = value.toObject();
        Asset a; a.id = text(o, "id"); a.label = text(o, "label");
        const auto kind = parse_kind(text(o, "kind"));
        if (!kind) throw std::invalid_argument("Unknown asset kind.");
        a.kind = *kind;
        a.capacity_m3 = number(o, a.kind == AssetKind::tank ? "capacity_m3" : "capacity_m3h");
        a.initial_m3 = number(o, "initial_m3"); a.inflow_m3h = number(o, "inflow_m3h");
        a.curtailment_m3h = number(o, "curtailment_m3h"); a.curtailment_max_m3h = number(o, "curtailment_max_m3h"); a.cost_per_m3 = number(o, "cost_per_m3");
        p.network.assets.push_back(a);
    }
    for (const auto& value : routes) {
        if (!value.isObject()) throw std::invalid_argument("Route must be an object.");
        const auto o = value.toObject();
        p.network.routes.push_back({text(o, "id"), text(o, "source_id"), text(o, "target_id"), number(o, "capacity_m3h"),
            number(o, "setpoint_m3h"), boolean(o, "has_pump", false), boolean(o, "available", true), number(o, "cost_per_m3")});
    }
    // Structural drafts are allowed, but IDs must be usable for stable scene references.
    QSet<QString> ids;
    for (const auto& a : p.network.assets) {
        if (a.id.empty() || ids.contains(q(a.id))) throw std::invalid_argument("Duplicate/empty asset ID.");
        ids.insert(q(a.id));
    }
    for (const auto& r : p.network.routes) {
        if (r.id.empty() || ids.contains(q(r.id))) throw std::invalid_argument("Duplicate/empty route ID.");
        ids.insert(q(r.id));
    }
    if (root.contains("pipe_transport")) {
        if (!root.value("pipe_transport").isObject()) throw std::invalid_argument("Pipe transport parameters must be an object.");
        const auto transport = root.value("pipe_transport").toObject();
        if (transport.size() != 2 || number(transport, "version", -1.0) != 1.0 || !transport.value("routes").isObject())
            throw std::invalid_argument("Unsupported pipe transport parameter version/structure.");
        const auto entries = transport.value("routes").toObject();
        if (entries.size() > static_cast<qsizetype>(p.network.routes.size())) throw std::invalid_argument("Too many pipe parameter entries.");
        for (auto it = entries.begin(); it != entries.end(); ++it) {
            const bool exists = std::any_of(p.network.routes.begin(), p.network.routes.end(), [&](const Route& r) { return q(r.id) == it.key(); });
            if (!exists || !it.value().isObject()) throw std::invalid_argument("Pipe parameters require an existing route.");
            p.pipe_parameters[it.key()] = quality::parse_pipe_parameters(it.value().toObject());
        }
    }
    const auto layout = root.value("layout").toObject();
    const auto positions = layout.value("positions").toObject();
    int index = 0;
    for (const auto& a : p.network.assets) {
        const auto position = positions.value(q(a.id)).toObject();
        const double x = number(position, "x", (index % 5) * 100.0), z = number(position, "z", (index / 5) * 100.0);
        if (std::abs(x) > 10000.0 || std::abs(z) > 10000.0) throw std::invalid_argument("Layout coordinates exceed 10000 units.");
        p.positions[q(a.id)] = QVector3D(static_cast<float>(x), 0.0F, static_cast<float>(z)); ++index;
    }
    const auto camera = layout.value("camera").toObject();
    p.camera_yaw = number(camera, "yaw", 35.0); p.camera_pitch = number(camera, "pitch", -55.0); p.camera_distance = number(camera, "distance", 720.0);
    p.camera_x = number(camera, "x"); p.camera_z = number(camera, "z");
    if (p.camera_pitch < -85.0 || p.camera_pitch > -15.0 || p.camera_distance < 250.0 || p.camera_distance > 1600.0 || std::abs(p.camera_x) > 10000.0 || std::abs(p.camera_z) > 10000.0)
        throw std::invalid_argument("Camera settings outside supported bounds.");
    return p;
}
ProjectDocument example_project() {
    ProjectDocument p; p.network = canonical_scenario();
    p.positions = {{"tank_a", {-100, 0, 30}}, {"tank_b", {80, 0, 30}}, {"source_a", {-240, 0, 30}},
        {"source_b", {80, 0, 190}}, {"treatment", {100, 0, -130}}, {"trucking", {-120, 0, -150}}};
    return p;
}
QString physics_hash(const ProjectDocument& p) {
    return QString::fromLatin1(QCryptographicHash::hash(QJsonDocument(network_json(p.network)).toJson(QJsonDocument::Compact), QCryptographicHash::Sha256).toHex());
}
}
