#pragma once
#include "EogDataset.hpp"
#include <QJsonObject>
#include <QString>
#include <QTemporaryDir>
#include <atomic>
#include <memory>
#include <utility>

namespace breakwater::desktop::eog {
// The private directory is removed only after the last request/history lease
// releases this immutable capture. It is never the source dataset directory.
class Capture final {
public:
    QString directory() const { return directory_owner_->path(); }
    const QJsonObject& manifest() const { return manifest_; }
private:
    Capture(std::shared_ptr<QTemporaryDir> directory, QJsonObject manifest)
        : directory_owner_(std::move(directory)), manifest_(std::move(manifest)) {}
    std::shared_ptr<QTemporaryDir> directory_owner_;
    QJsonObject manifest_;
    friend std::shared_ptr<const Capture> prepare_capture(
        std::shared_ptr<const Dataset>, qint64, const std::shared_ptr<std::atomic_bool>&);
};

// Call outside the UI thread. Throws on cancellation, changed/unsupported source
// files or invalid bounds; no incomplete directory/manifest is published.
std::shared_ptr<const Capture> prepare_capture(
    std::shared_ptr<const Dataset> dataset, qint64 cutoff,
    const std::shared_ptr<std::atomic_bool>& cancellation = {});
} // namespace breakwater::desktop::eog
