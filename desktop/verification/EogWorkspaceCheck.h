#pragma once
#include "../ProjectController.hpp"
#include "../agent/ModelTransport.hpp"
#include "../eog/EogAnalysisController.hpp"
#include "../eog/EogDatasetController.hpp"
#include <QCoreApplication>
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QImage>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QKeyEvent>
#include <QLocale>
#include <QMouseEvent>
#include <QPointer>
#include <QQuickItem>
#include <QQuickWindow>
#include <QTemporaryDir>
#include <QTextStream>
#include <QTimer>
#include <algorithm>
#include <array>
#include <cmath>
#include <memory>
#include <stdexcept>

namespace breakwater::desktop {
// Verification-only copy of the existing render fixture. A later facility
// sample extends the playback clock without filling the tank/pump source gaps.
// Production inputs and the repository fixture are never modified.
inline std::unique_ptr<QTemporaryDir> prepareEogWorkspaceFixture(const QString& source) {
    auto temporary = std::make_unique<QTemporaryDir>();
    if (!temporary->isValid()) return {};
    for (const auto* name : {"network.json", "ops.json", "telemetry.csv"})
        if (!QFile::copy(QDir(source).filePath(name), temporary->filePath(name))) return {};
    QFile telemetry(temporary->filePath("telemetry.csv"));
    if (!telemetry.open(QIODevice::ReadWrite)) return {};
    const auto original = telemetry.readAll();
    const QByteArray extension = (original.endsWith('\n') ? QByteArray{} : QByteArray{"\n"})
        + "2026-10-05T00:30:00Z,1791160200,facility-Z,inflow_bpd,17.5\n";
    if (telemetry.write(extension) != extension.size() || !telemetry.flush()) return {};
    return temporary;
}

namespace eog_workspace_check {
// This type deliberately is not a NemotronClient: verification cannot reload
// checkout/user credentials or send HTTP, even if an unintended action starts.
class RejectingModelTransport final : public agent::ModelTransport {
    int attempts_ = 0;
public:
    int requestAttempts() const { return attempts_; }
    void request(const QJsonArray&, const QJsonArray&) override {
        ++attempts_;
        emit requestFailed("ui_verification_model_forbidden", "The read-only UI probe must not request a model.");
    }
    void requestFinalJson(const QJsonArray& messages) override { request(messages, {}); }
    void cancel() override {}
};
class Check final : public QObject {
    QPointer<QQuickWindow> window_;
    QPointer<ProjectController> project_;
    QPointer<eog::EogDatasetController> dataset_;
    QPointer<eog::EogAnalysisController> analysis_;
    std::array<QPointer<RejectingModelTransport>, 3> models_;
    QCoreApplication* app_;
    QTimer timer_; QElapsedTimer elapsed_;
    int phase_ = 0;
    bool finished_ = false;
    QSize requested_;
    QJsonArray checks_;
    QJsonObject observations_, initial_project_;
    QString initial_hash_, initial_run_, dataset_id_;
    quint64 initial_revision_ = 0;
    bool initial_approval_ = false;
    std::unique_ptr<QTemporaryDir> artifacts_;

    static void require(bool condition, const char* code) {
        if (!condition) throw std::runtime_error(code);
    }
    QQuickItem* find(QQuickItem* root, const QString& name) const {
        if (!root) return nullptr;
        if (root->objectName() == name) return root;
        for (auto* child : root->childItems()) if (auto* found = find(child, name)) return found;
        return nullptr;
    }
    QQuickItem* item(const QString& name) const {
        auto* result = find(window_->contentItem(), name);
        require(result, "workspace_item_missing");
        return result;
    }
    QString text(const QString& name) const { return item(name)->property("text").toString(); }
    void reveal(QQuickItem* target) {
        // Keep actual scrollable controls reachable at the minimum client size.
        for (auto* parent = target->parentItem(); parent; parent = parent->parentItem()) {
            const auto position = parent->property("contentY");
            const auto extent = parent->property("contentHeight");
            if (!position.isValid() || !extent.isValid()) continue;
            const auto point = target->mapToItem(parent, QPointF{});
            const auto maximum = std::max(0.0, extent.toDouble() - parent->height());
            parent->setProperty("contentY", std::clamp(position.toDouble() + point.y() - 12.0, 0.0, maximum));
        }
    }
    void click(const QString& name) {
        auto* target = item(name);
        require(target->isVisible() && target->isEnabled(), "workspace_action_unavailable");
        reveal(target);
        const auto point = target->mapToScene(QPointF(target->width() / 2, target->height() / 2));
        require(window_->contentItem()->contains(point), "workspace_action_outside_client");
        QMouseEvent press(QEvent::MouseButtonPress, point, point, point, Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
        QMouseEvent release(QEvent::MouseButtonRelease, point, point, point, Qt::LeftButton, Qt::NoButton, Qt::NoModifier);
        QCoreApplication::sendEvent(window_, &press); QCoreApplication::sendEvent(window_, &release);
    }
    void key(int code) {
        QKeyEvent press(QEvent::KeyPress, code, Qt::NoModifier), release(QEvent::KeyRelease, code, Qt::NoModifier);
        QCoreApplication::sendEvent(window_, &press); QCoreApplication::sendEvent(window_, &release);
    }
    void dragScrollBar(bool bottom) {
        auto* bar = item("eogWorkspaceScrollBar");
        require(bar->isVisible() && bar->isEnabled() && bar->property("interactive").toBool()
            && bar->property("size").toDouble() > 0 && bar->property("size").toDouble() < 1,
            "workspace_minimum_overflow_scrollbar_unavailable");
        auto* thumb = qvariant_cast<QQuickItem*>(bar->property("contentItem"));
        require(thumb && thumb->height() > 0, "workspace_scrollbar_thumb_unavailable");
        const auto start = thumb->mapToScene(QPointF(thumb->width() / 2, thumb->height() / 2));
        const auto end = bar->mapToScene(QPointF(bar->width() / 2, bottom ? bar->height() - 3 : 3));
        QMouseEvent press(QEvent::MouseButtonPress, start, start, start, Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
        QMouseEvent move(QEvent::MouseMove, end, end, end, Qt::NoButton, Qt::LeftButton, Qt::NoModifier);
        QMouseEvent release(QEvent::MouseButtonRelease, end, end, end, Qt::LeftButton, Qt::NoButton, Qt::NoModifier);
        QCoreApplication::sendEvent(window_, &press); QCoreApplication::sendEvent(window_, &move); QCoreApplication::sendEvent(window_, &release);
    }
    void choose(const QString& name, const QVariantList& rows, const QString& id) {
        qsizetype index = -1;
        for (qsizetype row = 0; row < rows.size(); ++row)
            if (rows[row].toMap().value("id").toString() == id) { index = row; break; }
        require(index >= 0, "workspace_fixture_option_missing");
        auto* control = item(name);
        require(control->isVisible() && control->isEnabled(), "workspace_filter_unavailable");
        reveal(control); control->forceActiveFocus(Qt::TabFocusReason);
        // Closed native ComboBox keyboard navigation emits onActivated, unlike
        // assigning currentIndex in a test, so actual filter wiring is exercised.
        key(Qt::Key_Home);
        for (qsizetype row = 0; row < index; ++row) key(Qt::Key_Down);
        require(control->property("currentIndex").toInt() == index, "workspace_filter_keyboard_selection_failed");
    }
    void numeric(double expected) const {
        bool ok = false;
        const auto value = QLocale().toDouble(text("eogObservationValue"), &ok);
        require(ok && std::isfinite(value) && std::abs(value - expected) < 1e-9, "workspace_numeric_readout_incorrect");
        require(text("eogObservationUnit") == dataset_->unit(), "workspace_reading_unit_incorrect");
    }
    void preserved() const {
        for (const auto& model : models_)
            require(model && model->requestAttempts() == 0, "workspace_model_request_forbidden");
        require(project_->editorDocument() == initial_project_ && project_->networkHash() == initial_hash_
            && project_->runId() == initial_run_ && project_->simulationRevision() == initial_revision_
            && project_->canApprove() == initial_approval_, "workspace_navigation_changed_sandbox_or_approval");
        require(!analysis_->busy() && analysis_->sessionId().isEmpty() && analysis_->events().isEmpty()
            && analysis_->plans().isEmpty() && analysis_->trace().isEmpty() && analysis_->operatorRecords().isEmpty()
            && analysis_->lastOperation().isEmpty() && !analysis_->canReview(), "workspace_navigation_started_analysis_or_review");
        if (!dataset_id_.isEmpty()) require(dataset_->datasetIdentity() == dataset_id_, "workspace_navigation_replaced_dataset");
    }
    void snapshot(const QString& name) {
        if (!artifacts_) return;
        const auto image = window_->grabWindow(); require(!image.isNull(), "workspace_capture_failed");
        QFile file(artifacts_->filePath(name));
        require(file.open(QIODevice::WriteOnly | QIODevice::NewOnly) && image.save(&file, "PNG") && file.flush(), "workspace_capture_write_failed");
    }
    void finish(bool passed, const QString& code) {
        if (finished_) return;
        finished_ = true; timer_.stop();
        QJsonObject result{{"check", "eog_workspace"}, {"passed", passed}, {"code", code}, {"phase", phase_},
            {"checks", checks_}, {"observed", observations_}, {"window_width", window_ ? window_->width() : 0},
            {"window_height", window_ ? window_->height() : 0}, {"requested_width", requested_.width()},
            {"requested_height", requested_.height()}, {"device_pixel_ratio", window_ ? window_->devicePixelRatio() : 0.0},
            {"elapsed_ms", elapsed_.elapsed()}, {"analysis_requested_by_probe", false},
            {"analysis_busy", analysis_ && analysis_->busy()},
            {"analysis_session_id", analysis_ ? analysis_->sessionId() : QString{}},
            {"hosted_model_requests", "blocked_by_verification_transport"}};
        int attempts = 0;
        for (const auto& model : models_) if (model) attempts += model->requestAttempts();
        result["model_request_attempts"] = attempts;
        if (artifacts_) result["artifact_directory"] = artifacts_->path();
        QTextStream(stdout) << QJsonDocument(result).toJson(QJsonDocument::Compact) << '\n';
        app_->exit(passed ? 0 : 8);
    }
    void tick() {
        try {
            require(window_ && project_ && dataset_ && analysis_, "workspace_check_destroyed");
            require(elapsed_.elapsed() < 45000, "workspace_check_timeout");
            preserved();
            switch (phase_) {
            case 0: {
                if (!window_->isVisible() || !window_->isExposed()) return;
                require(window_->size() == requested_, "workspace_actual_client_size_mismatch");
                require(!dataset_->loaded() && !dataset_->busy() && item("eogEmptyState")->isVisible(), "workspace_did_not_start_unloaded");
                for (const auto* name : {"eogOpenExample", "eogStartImport"}) {
                    auto* action = item(name);
                    require(action->isVisible() && action->isEnabled() && !action->property("accessibleName").toString().trimmed().isEmpty(), "workspace_onboarding_action_accessibility_missing");
                }
                checks_.append("unloaded_onboarding_and_accessible_explicit_actions");
                snapshot("01-unloaded-workspace.png"); click("eogOpenExample"); break;
            }
            case 1:
                if (dataset_->busy() || !dataset_->loaded()) {
                    require(dataset_->error().isEmpty(), "workspace_fixture_load_failed"); return;
                }
                dataset_id_ = dataset_->datasetIdentity();
                require(dataset_->systemId() == "CUSTOM-system" && dataset_->facilityId() == "facility-Z"
                    && dataset_->assetId() == "tank-Z" && dataset_->metric() == "level_pct", "workspace_loaded_filter_identity_incorrect");
                for (const auto* name : {"eogSystemChoice", "eogFacilityChoice", "eogAssetChoice", "eogMetricChoice"})
                    require(item(name)->isVisible() && item(name)->isEnabled(), "workspace_loaded_filter_unavailable");
                for (const auto* name : {"eogOpenFolder", "eogDatasetDetails"})
                    require(item(name)->isVisible() && item(name)->isEnabled()
                        && !item(name)->property("accessibleName").toString().trimmed().isEmpty(), "workspace_loaded_action_accessibility_missing");
                reveal(item("eogObservationReadout")); break;
            case 2: {
                if (!window_->property("eogSceneReady").toBool() || !window_->property("eogTrendReady").toBool()) return;
                const auto row = dataset_->selectedValues();
                require(row.value("has_sample").toBool() && row.value("stale").toBool() && row.value("age_s").toDouble() == 1500, "workspace_stale_fixture_incorrect");
                numeric(104); require(text("eogObservationFreshness").contains("stale", Qt::CaseInsensitive)
                    && item("eogObservationStaleNotice")->isVisible()
                    && text("eogObservationTimestamp").contains("2026-10-05"), "workspace_stale_reading_disclosure_missing");
                observations_["stale"] = QJsonObject::fromVariantMap(row);
                checks_.append("loaded_stale_reading_retains_original_value_unit_timestamp_and_age");
                snapshot("02-stale-recorded-reading.png"); dataset_->setCursorUnix(dataset_->firstUnix()); break;
            }
            case 3:
                numeric(-2); require(dataset_->selectedValues().value("value").toDouble() == -2
                    && text("eogObservationFreshness").contains("Historical", Qt::CaseInsensitive), "workspace_negative_historical_reading_clipped_or_unlabeled");
                observations_["negative"] = QJsonObject::fromVariantMap(dataset_->selectedValues());
                checks_.append("historical_negative_reading_is_preserved"); snapshot("03-historical-negative-reading.png");
                dataset_->setCursorUnix(dataset_->firstUnix() + 300); break;
            case 4:
                numeric(104); require(!dataset_->selectedValues().value("stale").toBool()
                    && text("eogObservationFreshness").contains("Historical", Qt::CaseInsensitive), "workspace_fresh_overflowing_history_incorrect");
                checks_.append("finite_above_capacity_reading_is_not_clamped");
                choose("eogAssetChoice", dataset_->assets(), "pump-Z"); break;
            case 5:
                require(dataset_->assetId() == "pump-Z" && dataset_->metric() == "status", "workspace_equipment_filter_not_connected");
                numeric(2); require(item("eogObservationStatus")->isVisible()
                    && text("eogObservationStatus").contains("fault feedback", Qt::CaseInsensitive), "workspace_discrete_status_meaning_missing");
                observations_["status"] = QJsonObject::fromVariantMap(dataset_->selectedValues());
                checks_.append("actual_equipment_filter_and_discrete_status_label"); snapshot("04-discrete-status.png");
                choose("eogMetricChoice", dataset_->metrics(), "flow_bpd"); break;
            case 6:
                require(dataset_->metric() == "flow_bpd" && dataset_->selectedValues().value("has_sample").toBool(), "workspace_metric_filter_not_connected");
                numeric(0); require(!item("eogObservationStatus")->isVisible() && !item("eogObservationMissing")->isVisible(), "workspace_actual_zero_misreported_as_missing");
                checks_.append("actual_metric_filter_preserves_a_legitimate_zero_reading");
                choose("eogAssetChoice", dataset_->assets(), "disposal-Z"); break;
            case 7:
                require(dataset_->assetId() == "disposal-Z" && !dataset_->selectedValues().value("has_sample").toBool(), "workspace_missing_fixture_incorrect");
                require(text("eogObservationValue") == "No reading" && item("eogObservationMissing")->isVisible()
                    && text("eogObservationFreshness").contains("Unavailable", Qt::CaseInsensitive)
                    && !item("eogObservationStatus")->isVisible(), "workspace_missing_channel_rendered_as_zero_or_status");
                checks_.append("missing_channel_is_explicitly_unavailable_and_never_zero");
                reveal(item("eogObservationReadout")); snapshot("05-missing-is-not-zero.png");
                choose("eogAssetChoice", dataset_->assets(), "tank-Z"); click("eogFacility"); break;
            case 8:
                require(dataset_->sceneMode() == "facility", "workspace_facility_action_not_connected");
                if (!window_->property("eogSceneReady").toBool()) return;
                checks_.append("facility_scene_inspection_keeps_analysis_explicit"); snapshot("06-facility-inspection.png");
                click("eogOverview"); break;
            case 9:
                require(dataset_->sceneMode() == "overview", "workspace_overview_action_not_connected");
                click("eogCutoffSlider"); break;
            case 10:
                require(dataset_->cursorUnix() > dataset_->firstUnix() + 300 && dataset_->cursorUnix() < dataset_->nowUnix(), "workspace_cutoff_slider_not_connected");
                require(text("eogObservationFreshness").contains("Historical", Qt::CaseInsensitive), "workspace_cutoff_history_unlabeled");
                checks_.append("actual_cutoff_slider_changes_only_recorded_view"); click("eogLatestCutoff"); break;
            case 11:
                require(dataset_->cursorUnix() == dataset_->nowUnix()
                    && text("eogObservationFreshness").contains("At cutoff", Qt::CaseInsensitive), "workspace_latest_cutoff_action_not_connected");
                checks_.append("latest_cutoff_restores_playback_clock_without_starting_analysis");
                reveal(item("eogObservationReadout")); break;
            case 12:
                preserved(); snapshot("07-latest-cutoff.png");
                checks_.append("all_loading_selection_cutoff_and_scene_actions_preserve_sandbox_controls_and_review_authority");
                if (requested_.height() == 540) { dragScrollBar(false); break; }
                finish(true, "ok"); return;
            case 13:
                require(item("eogWorkspaceScrollBar")->property("position").toDouble() < 0.01,
                    "workspace_scrollbar_drag_did_not_reach_top");
                dragScrollBar(true); break;
            case 14: {
                auto* bar = item("eogWorkspaceScrollBar");
                auto* chart = item("eogTrendChart");
                const auto chartBounds = chart->mapRectToScene(chart->boundingRect());
                const QRectF client(0, 64, window_->width(), window_->height() - 64 - 28);
                const auto visible = chartBounds.intersected(client);
                require(bar->property("position").toDouble() > 0.05 && visible.width() > 0
                    && visible.height() >= std::min(80.0, chart->height() / 2), "workspace_chart_unreachable_through_scrollbar");
                observations_["minimum_scroll"] = QJsonObject{{"position", bar->property("position").toDouble()},
                    {"size", bar->property("size").toDouble()}, {"visible_chart_height", visible.height()}};
                checks_.append("minimum_client_overflow_bar_is_visible_interactive_and_reaches_chart_by_actual_thumb_drag");
                snapshot("08-minimum-scrollbar-chart.png"); finish(true, "ok"); return;
            }
            default: throw std::runtime_error("workspace_invalid_phase");
            }
            ++phase_; window_->requestUpdate();
        } catch (const std::exception& error) { finish(false, QString::fromLatin1(error.what())); }
    }
public:
    Check(QQuickWindow* window, ProjectController* project, eog::EogDatasetController* dataset,
          eog::EogAnalysisController* analysis, QCoreApplication* app,
          const std::array<RejectingModelTransport*, 3>& models)
        : QObject(app), window_(window), project_(project), dataset_(dataset), analysis_(analysis), app_(app),
          initial_project_(project->editorDocument()), initial_hash_(project->networkHash()), initial_run_(project->runId()),
          initial_revision_(project->simulationRevision()), initial_approval_(project->canApprove()) {
        for (std::size_t index = 0; index < models.size(); ++index) models_[index] = models[index];
        requested_ = qEnvironmentVariable("BREAKWATER_UI_MINIMUM") == "1" ? QSize(960, 540)
            : app->arguments().contains("--compact-ui") ? QSize(1100, 700) : QSize(1440, 900);
        window->resize(requested_); elapsed_.start();
        const auto evidence = qEnvironmentVariable("BREAKWATER_UI_EVIDENCE_DIR");
        if (!evidence.isEmpty()) {
            const QFileInfo parent(evidence);
            if (!parent.isAbsolute() || !parent.isDir() || parent.canonicalFilePath().isEmpty()) {
                QTimer::singleShot(0, this, [this] { finish(false, "workspace_evidence_parent_invalid"); }); return;
            }
            artifacts_ = std::make_unique<QTemporaryDir>(QDir(parent.canonicalFilePath()).filePath("breakwater-eog-ui-XXXXXX"));
            if (!artifacts_->isValid()) {
                QTimer::singleShot(0, this, [this] { finish(false, "workspace_evidence_directory_failed"); }); return;
            }
            artifacts_->setAutoRemove(false);
        }
        timer_.setInterval(100);
        connect(&timer_, &QTimer::timeout, this, [this] { tick(); }); timer_.start();
    }
};
} // namespace eog_workspace_check
inline void startEogWorkspaceCheck(QQuickWindow* window, ProjectController* project,
    eog::EogDatasetController* dataset, eog::EogAnalysisController* analysis, QCoreApplication* app,
    const std::array<eog_workspace_check::RejectingModelTransport*, 3>& models) {
    new eog_workspace_check::Check(window, project, dataset, analysis, app, models);
}
} // namespace breakwater::desktop
