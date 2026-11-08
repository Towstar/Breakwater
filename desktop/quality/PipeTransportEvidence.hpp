#pragma once

#include <QJsonObject>
#include <QString>
#include <QtTypes>

namespace breakwater::desktop::quality {

inline constexpr qint64 maximumPipeEvidenceBytes = 1024 * 1024;

// Checks the bounded native-result v1 display format and its internal identities.
// A valid file is historical evidence, not authenticated execution or approval.
bool validate_pipe_transport_evidence(const QJsonObject& evidence, QString* error = nullptr);

} // namespace breakwater::desktop::quality
