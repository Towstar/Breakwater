#pragma once

#include "ProjectController.hpp"
#include <QJsonObject>
#include <QString>

namespace breakwater::desktop::science {

inline constexpr qint64 maximumStorageScienceEvidenceBytes = 1024 * 1024;

// Serializes the immutable native capture and a completed comparison. Call away
// from the GUI thread. This creates historical evidence, never an approval.
QJsonObject storage_science_archive(const CapturedForecast& capture, const QJsonObject& evidence);

// Checks the result's bounded v1 format and its internal relationship to the
// enclosing native capture. Imported evidence is not authenticated execution.
bool validate_storage_science_evidence(const QJsonObject& evidence, const QJsonObject& archive,
                                      QString* error = nullptr);

} // namespace breakwater::desktop::science
