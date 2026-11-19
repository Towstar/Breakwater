#pragma once
#include "ProjectController.hpp"
#include <QJsonObject>

namespace breakwater::desktop::science {
struct StoredScienceSource {
    std::shared_ptr<const CapturedForecast> capture;
    QJsonObject origin;
    QString error;
};
// Reconstructs an immutable stored candidate off the GUI thread, independently
// checks its full trajectory against the native solver, and retains its original
// samples. No editor or gateway inspection/approval state is written.
StoredScienceSource capture_stored_science(const QJsonObject& archive,const QString& runId);
}
