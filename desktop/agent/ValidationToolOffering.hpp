#pragma once
#include <QJsonArray>
#include <QJsonObject>
#include <QSet>
#include <optional>

namespace breakwater::desktop::agent {
// Model guidance only. Dispatch and exact-evidence checks remain authoritative.
// An incomplete/oversized catalogue keeps the original schema rather than
// hiding candidates that did not fit the bounded presentation.
inline std::optional<QJsonObject> validationToolOffering(QJsonObject definition,
        const QString& identity, const QJsonArray& targets, bool complete) {
    if (!complete || targets.size() > 256) return definition;
    QSet<QString> seen;
    for (const auto& item : targets) {
        if (!item.isString() || item.toString().isEmpty() || item.toString().size() > 256 || seen.contains(item.toString()))
            return definition;
        seen.insert(item.toString());
    }
    if (targets.isEmpty()) return std::nullopt;
    auto function = definition.value("function").toObject();
    auto parameters = function.value("parameters").toObject();
    auto properties = parameters.value("properties").toObject();
    auto schema = properties.value(identity).toObject();
    schema["type"] = "string"; schema["enum"] = targets;
    properties[identity] = schema; parameters["properties"] = properties;
    function["parameters"] = parameters; definition["function"] = function;
    return definition;
}
}
