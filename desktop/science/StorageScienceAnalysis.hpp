#pragma once

#include "ProjectController.hpp"
#include <QJsonArray>
#include <QJsonObject>
#include <QString>
#include <memory>

namespace breakwater::desktop::science {

QString sensitivity_ineligible_reason(const Network& network, const ControlDescriptor& control);
QJsonObject storage_science_payload(const CapturedForecast& capture, bool sensitivities, const QJsonArray& controls);
// Pure comparison over immutable native and worker evidence. No QObject or live
// editor state is read, so callers can run it away from the GUI thread.
QJsonObject compare_storage_science(const std::shared_ptr<const CapturedForecast>& capture,
    const QJsonObject& workerResponse, bool sensitivities, const QJsonArray& controls);

} // namespace breakwater::desktop::science
