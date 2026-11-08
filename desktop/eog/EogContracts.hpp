#pragma once
#include "breakwater/eog.hpp"
#include <QByteArray>
#include <QJsonObject>
#include <QString>

namespace breakwater::desktop::eog {
// Native contract validation only. No Python process, numerical provider or authority.
QJsonObject decode_document(const QByteArray& bytes);
void validate_document(const QJsonObject& document, const QString& kind = {},
                       const QJsonObject& context = {});
QByteArray semantic_bytes(const QJsonValue& value);
QString dataset_identity(const QJsonObject& sourceHashes);
QString capture_identity(const QJsonObject& manifest);
QString plan_identity(const QJsonObject& plan);
// Inline tool schema from the embedded public plan contract. Only the four
// application-owned envelope fields are optional; numerical claims stay required.
QJsonObject agent_plan_schema();
} // namespace breakwater::desktop::eog
