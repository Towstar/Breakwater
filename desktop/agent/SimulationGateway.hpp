#pragma once

#include "breakwater/engine.hpp"
#include <QJsonArray>
#include <QJsonObject>
#include <QString>
#include <atomic>
#include <map>
#include <mutex>
#include <optional>

namespace breakwater::desktop::agent {

// A session owns one immutable physical snapshot. Invalidation is safe while a
// numerical job is running; completed work remains available as evidence only.
class SimulationGateway final {
public:
    SimulationGateway(const Network& network, const QString& physicalHash, double horizonH = 3.0);
    QString snapshotId() const { return snapshot_id_; }
    QString physicalHash() const { return physical_hash_; }
    double horizonH() const { return config_.horizon_h; }
    void invalidate();
    bool isCurrent() const { return current_.load(); }
    QJsonObject execute(const QString& toolName, const QJsonObject& arguments);
    QJsonObject recommendation(const QString& runId) const;
    QJsonObject stagedPlan() const;
    std::vector<ControlValue> stagedControls() const;
    QJsonArray runs() const;
    QJsonObject runDetails(const QString& runId) const;
    static QJsonArray toolDefinitions();

private:
    struct StoredRun {
        QString id;
        QString candidate_id;
        QString created_at;
        CompiledNetwork compiled;
        SimulationResult result;
        ValidationReport reported_validation;
        std::optional<ValidationReport> explicit_validation;
        QString validation_id;
        std::vector<ControlValue> overrides;
    };
    const Network network_;
    const QString snapshot_id_;
    const QString physical_hash_;
    const SimulationConfig config_;
    std::atomic<bool> current_{true};
    mutable std::mutex mutex_;
    std::map<QString, StoredRun> runs_;
    bool network_inspected_ = false;
    bool controls_inspected_ = false;
    QString staged_run_id_;

    QJsonObject summary(const StoredRun& run) const;
    QJsonObject recommendationLocked(const QString& runId) const;
    StoredRun compute(const QString& candidateId, const std::vector<ControlValue>& controls) const;
    std::vector<ControlValue> parseControls(const QJsonArray& controls) const;
};

} // namespace breakwater::desktop::agent
