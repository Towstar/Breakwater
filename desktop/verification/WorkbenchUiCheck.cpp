#include "WorkbenchUiCheck.hpp"
#include "../ProjectController.hpp"
#include "../history/HistoryController.hpp"
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
#include <QMouseEvent>
#include <QPointer>
#include <QQmlContext>
#include <QQmlEngine>
#include <QQmlExpression>
#include <QQuickItem>
#include <QQuickWindow>
#include <QTextStream>
#include <QTemporaryDir>
#include <QTimer>
#include <filesystem>
#include <memory>
#include <stdexcept>

namespace breakwater::desktop {
namespace {
class Check final : public QObject {
    QPointer<QQuickWindow> window_; QPointer<ProjectController> controller_;
    QCoreApplication* app_; QTimer timer_; QElapsedTimer elapsed_;
    int phase_ = 0, changes_ = 0; bool done_ = false; QJsonArray checks_; QJsonObject observed_;
    QElapsedTimer outside_shortcut_setup_;
    bool outside_focus_settled_ = false, outside_escape_sent_ = false;
    QString hash_, run_, feedback_; quint64 revision_ = 0;
    QString pipe_evidence_id_;
    QRectF retained_button_rect_;
    QElapsedTimer retained_viewer_wait_;
    QElapsedTimer stored_source_wait_;
    bool observed_only_=false;
    QJsonObject observed_project_;
    QJsonObject position_; QPointF tank_; double capacity_ = 0;
    std::unique_ptr<QTemporaryDir> artifacts_;
    QVariant eval(const QString& source) {
        QQmlExpression expression(QQmlEngine::contextForObject(window_), window_, source);
        const auto value = expression.evaluate();
        if (expression.hasError()) throw std::runtime_error("qml_expression_failed");
        return value;
    }
    void require(bool value, const char* code) { if (!value) throw std::runtime_error(code); }
    void passed(const char* name) { checks_.append(QString::fromLatin1(name)); }
    void snapshot(const QString& name) {
        if (!artifacts_) return;
        const auto image = window_->grabWindow(); require(!image.isNull(), "evidence_capture_failed");
        QFile file(QDir(artifacts_->path()).filePath(name));
        require(file.open(QIODevice::WriteOnly | QIODevice::NewOnly), "evidence_file_creation_failed");
        require(image.save(&file, "PNG") && file.flush(), "evidence_png_write_failed");
    }
    QQuickItem* find(QQuickItem* root, const QString& value, bool byText = false) {
        if (!root) return nullptr;
        if ((byText ? root->property("text").toString() : root->objectName()) == value) return root;
        for (auto* child : root->childItems()) if (auto* result = find(child, value, byText)) return result;
        return nullptr;
    }
    QQuickItem* item(const QString& name, bool byText = false) {
        auto* result = find(window_->contentItem(), name, byText);
        require(result, "ui_item_missing"); return result;
    }
    void mouse(QEvent::Type type, QPointF point, Qt::MouseButton button, Qt::MouseButtons held, Qt::KeyboardModifiers modifiers = Qt::NoModifier) {
        QMouseEvent event(type, point, point, point, button, held, modifiers);
        QCoreApplication::sendEvent(window_, &event);
    }
    void click(QPointF point) {
        mouse(QEvent::MouseButtonPress, point, Qt::LeftButton, Qt::LeftButton);
        mouse(QEvent::MouseButtonRelease, point, Qt::LeftButton, Qt::NoButton);
    }
    void click(QQuickItem* target) { click(target->mapToScene(QPointF(target->width()/2, target->height()/2))); }
    void controlClick(QQuickItem* target) {
        const auto point = target->mapToScene(QPointF(target->width()/2, target->height()/2));
        mouse(QEvent::MouseButtonPress, point, Qt::LeftButton, Qt::LeftButton, Qt::ControlModifier);
        mouse(QEvent::MouseButtonRelease, point, Qt::LeftButton, Qt::NoButton, Qt::ControlModifier);
    }
    void shiftClick(QQuickItem* target) {
        const auto point = target->mapToScene(QPointF(target->width()/2, target->height()/2));
        mouse(QEvent::MouseButtonPress, point, Qt::LeftButton, Qt::LeftButton, Qt::ShiftModifier);
        mouse(QEvent::MouseButtonRelease, point, Qt::LeftButton, Qt::NoButton, Qt::ShiftModifier);
    }
    void key(int code, Qt::KeyboardModifiers modifiers = Qt::NoModifier, const QString& text = {}) {
        QKeyEvent press(QEvent::KeyPress, code, modifiers, text), release(QEvent::KeyRelease, code, modifiers, text);
        QCoreApplication::sendEvent(window_, &press); QCoreApplication::sendEvent(window_, &release);
    }
    void enter(const QString& value) {
        click(item("numericProperty_capacity"));
        key(Qt::Key_A, Qt::ControlModifier); key(Qt::Key_unknown, Qt::NoModifier, value); key(Qt::Key_Return);
    }
    QJsonObject tankPosition() {
        return controller_->document()["layout"].toObject()["positions"].toObject()["tank_a"].toObject();
    }
    void preserved() {
        require(controller_->networkHash() == hash_ && controller_->runId() == run_
            && controller_->simulationRevision() == revision_ && controller_->canApprove(), "numerical_evidence_changed");
    }
    QJsonObject focusObservation() {
        auto* field = item("numericProperty_capacity");
        auto* focused = window_->activeFocusItem();
        return {{"window_active", window_->isActive()}, {"notice_visible", item("actionFeedbackCard")->isVisible()},
            {"feedback_id", controller_->actionFeedback().value("id").toString()},
            {"feedback_code", controller_->actionFeedback().value("code").toString()},
            {"field_text", field->property("text").toString()},
            {"field_pending_commit", field->property("pendingUserCommit").toBool()},
            {"field_active_focus", field->hasActiveFocus()},
            {"active_focus_item", focused ? focused->objectName() : QString()},
            {"connect_mode", eval("app.connectMode").toBool()}};
    }
    void finish(bool ok, const QString& code) {
        if (done_) return;
        done_ = true; timer_.stop();
        QJsonObject result{{"check", observed_only_ ? "ui_observed_advisory":"ui_overhaul"}, {"passed", ok}, {"code", code},
            {"checks", checks_}, {"phase", phase_}, {"observed", observed_},
            {"device_pixel_ratio", window_ ? window_->devicePixelRatio() : 0.0},
            {"window_width", window_ ? window_->width() : 0}, {"window_height", window_ ? window_->height() : 0},
            {"elapsed_ms", elapsed_.elapsed()}, {"hosted_model_requests", "not_requested"}};
        if (artifacts_ && artifacts_->isValid()) result["artifact_directory"] = artifacts_->path();
        QTextStream stream(stdout); stream << QJsonDocument(result).toJson(QJsonDocument::Compact) << '\n'; stream.flush();
        app_->exit(ok ? 0 : 8);
    }
    void tick() {
        try {
            require(window_ && controller_, "ui_destroyed");
            // Includes actual native pipe execution, SQL retention/reload and
            // the compact History-to-viewer action in addition to editor checks.
            require(elapsed_.elapsed() < 40000, "ui_check_timeout");
            switch (phase_) {
            case 0: {
                if (!window_->isVisible() || !window_->isExposed() || !window_->property("sceneReady").toBool()) return;
                require(!QImage(":/breakwater/assets/breakwater-logo.svg").isNull()
                    && !QImage(":/breakwater/assets/icons/play.svg").isNull(), "bundled_svg_unavailable");
                auto* open = item("Open", true);
                require(open->property("accessibleName").toString() == "Open"
                    && open->property("tooltip").toString() == "Open project", "icon_accessibility_missing");
                require(!item("Undo", true)->isEnabled() && item("Run forecast", true)->isEnabled()
                    && !item("Approve simulated preview", true)->isEnabled(), "initial_action_gates_wrong");
                passed("native_assets_accessibility_action_gates");
                const auto parent = qEnvironmentVariable("BREAKWATER_UI_EVIDENCE_DIR");
                if (!parent.isEmpty()) {
                    const QFileInfo directory(parent);
#ifdef Q_OS_WIN
                    const auto absolute = std::filesystem::path(parent.toStdWString()).is_absolute();
#else
                    const auto absolute = QDir::isAbsolutePath(parent);
#endif
                    require(absolute && directory.isDir() && !directory.canonicalFilePath().isEmpty(), "evidence_parent_invalid");
                    artifacts_ = std::make_unique<QTemporaryDir>(QDir(directory.canonicalFilePath()).filePath("breakwater-ui-XXXXXX"));
                    require(artifacts_->isValid(), "evidence_directory_creation_failed"); artifacts_->setAutoRemove(false);
                }
                snapshot("01-workbench-ready.png");
                if (observed_only_) {
                    controller_->loadExample("monitoring"); observed_project_=controller_->editorDocument();
                    eval("app.openTelemetry()"); phase_=52; break;
                }
                SimulationConfig config; config.horizon_h = 1.25; controller_->forecastWithConfig(config); break;
            }
            case 1:
                if (controller_->busy()) return;
                require(!controller_->runId().isEmpty() && controller_->forecastHorizonH() == 1.25
                    && controller_->canApprove(), "actual_forecast_unavailable");
                hash_ = controller_->networkHash(); run_ = controller_->runId(); revision_ = controller_->simulationRevision();
                position_ = tankPosition(); passed("actual_1_25h_forecast"); break;
            case 2: {
                const auto json = eval(QStringLiteral("JSON.stringify((function(){ let a; for (const row of app.controller.assets) if(row.id==='tank_a') a=row; const p=view.mapFrom3DScene(Qt.vector3d(a.x,40,a.z)); const hit=view.pick(p.x,p.y); let node=hit.objectHit,depth=0; while(node && node.objectName!=='equipment:tank_a'){++depth;node=node.parent} const q=view.mapToItem(null,p.x,p.y);return {name:hit.objectHit?hit.objectHit.objectName:'',depth:depth,ancestor:!!node,x:q.x,y:q.y};})())")).toString();
                const auto data = QJsonDocument::fromJson(json.toUtf8()).object();
                require(data["name"] == "asset:tank_a" && data["ancestor"].toBool() && data["depth"].toInt() >= 2, "nested_tank_pick_failed");
                tank_ = {data["x"].toDouble(), data["y"].toDouble()}; click(tank_); passed("nested_geometry_pick_and_click"); break;
            }
            case 3: {
                require(controller_->selectedId() == "tank_a", "actual_click_did_not_select");
                if (window_->property("compactLayout").toBool() && !window_->property("inspectorVisible").toBool()) {
                    click(item("Inspector", true)); return;
                }
                // Edit two successive fields in the expanded inspector. A
                // short hosted desktop can clip the second field even above
                // the old 620-pixel cutoff; use the real dock action for all sizes.
                if (eval("workspaceSettings.values.dockOpen").toBool()) {
                    click(item("Toggle results", true)); return;
                }
                // Opening a compact pane changes the viewport. Pick the actual
                // current projection before testing the next physical gesture.
                const auto point = eval("(function(){let a;for(const row of app.controller.assets)if(row.id==='tank_a')a=row;const p=view.mapFrom3DScene(Qt.vector3d(a.x,40,a.z));return view.mapToItem(null,p.x,p.y)})()").toPointF();
                tank_ = point;
                capacity_ = controller_->selected().value("capacity").toDouble();
                mouse(QEvent::MouseButtonPress, tank_, Qt::LeftButton, Qt::LeftButton); break;
            }
            case 4:
                mouse(QEvent::MouseMove, tank_ + QPointF(38, 16), Qt::NoButton, Qt::LeftButton); break;
            case 5:
                mouse(QEvent::MouseButtonRelease, tank_ + QPointF(38, 16), Qt::LeftButton, Qt::NoButton); break;
            case 6:
                require(tankPosition() != position_, "actual_drag_did_not_move"); preserved();
                snapshot("04-inspector-before-entry.png");
                passed("layout_drag_preserves_hash_run_approval"); enter("NaN"); break;
            case 7:
                observed_["entry"] = focusObservation();
                require(controller_->actionFeedback().value("code") == "property.invalid_number", "keyboard_invalid_commit_not_rejected");
                require(item("numericRejection_capacity")->isVisible()
                    && item("numericRejection_capacity")->property("text").toString().contains("NaN")
                    && item("numericRejection_capacity")->property("text").toString().contains("Accepted value:"), "inline_rejection_missing");
                require(controller_->selected().value("capacity").toDouble() == capacity_, "rejected_capacity_changed");
                preserved(); feedback_ = controller_->actionFeedback().value("id").toString(); enter("NaN"); break;
            case 8:
                require(controller_->actionFeedback().value("id").toString().toULongLong() == feedback_.toULongLong() + 1, "repeat_rejection_not_exactly_once");
                require(item("actionFeedbackCard")->isVisible(), "repeat_notice_not_visible");
                preserved(); snapshot("02-repeated-invalid-before-dismiss.png");
                feedback_ = controller_->actionFeedback().value("id").toString();
                click(item("dismissActionFeedback")); break;
            case 9:
                observed_ = {{"notice_visible", item("actionFeedbackCard")->isVisible()},
                    {"inline_visible", item("numericRejection_capacity")->isVisible()},
                    {"capacity", controller_->selected().value("capacity").toDouble()},
                    {"selected_id", controller_->selectedId()}, {"feedback_id", controller_->actionFeedback().value("id").toString()},
                    {"before_dismiss_feedback_id", feedback_}, {"field_text", item("numericProperty_capacity")->property("text").toString()}};
                require(!item("actionFeedbackCard")->isVisible(), "dismiss_notice_still_visible");
                require(item("numericRejection_capacity")->isVisible(), "dismiss_inline_evidence_removed");
                require(controller_->actionFeedback().value("id").toString() == feedback_, "dismiss_recommitted_field");
                preserved(); passed("keyboard_rejection_repeat_dismiss_preserves_evidence");
                eval("app.connectMode=true"); enter("NaN"); break;
            case 10:
                observed_["third_retry_before_escape"] = focusObservation();
                require(controller_->actionFeedback().value("id").toString().toULongLong() == feedback_.toULongLong() + 1,
                    "third_keyboard_retry_not_fresh");
                require(item("actionFeedbackCard")->isVisible(), "third_retry_notice_not_visible");
                require(eval("app.connectMode").toBool(), "connection_mode_lost_before_escape");
                item("dismissActionFeedback")->forceActiveFocus();
                observed_["focused_before_escape"] = focusObservation();
                require(item("dismissActionFeedback")->hasActiveFocus(), "notice_keyboard_focus_not_obtained");
                key(Qt::Key_Escape); break;
            case 11:
                observed_["after_focused_escape"] = focusObservation();
                require(!item("actionFeedbackCard")->isVisible(), "focused_escape_notice_not_dismissed");
                require(eval("app.connectMode").toBool(), "focused_escape_cancelled_connection");
                passed("focused_notice_escape_isolated");
                outside_shortcut_setup_.start();
                if (!window_->isActive()) window_->requestActivate();
                item("numericProperty_capacity")->forceActiveFocus();
                observed_["outside_escape_setup_started"] = focusObservation(); break;
            case 12: {
                if (!outside_escape_sent_) {
                    auto* field = item("numericProperty_capacity");
                    observed_["outside_escape_before_key"] = focusObservation();
                    require(outside_shortcut_setup_.elapsed() < 2000, "outside_shortcut_focus_unavailable");
                    if (!window_->isActive() || !field->hasActiveFocus() || window_->activeFocusItem() != field) {
                        outside_focus_settled_ = false;
                        if (!window_->isActive()) window_->requestActivate();
                        field->forceActiveFocus(); return;
                    }
                    // Window shortcuts require an active window. Let the
                    // requested activation/focus settle for an event-loop turn
                    // before exercising the real key/Shortcut delivery path.
                    if (!outside_focus_settled_) { outside_focus_settled_ = true; return; }
                    outside_escape_sent_ = true; key(Qt::Key_Escape); return;
                }
                observed_["after_outside_escape"] = focusObservation();
                require(!eval("app.connectMode").toBool(), "outside_notice_escape_did_not_cancel_connection");
                preserved(); passed("outside_notice_escape_cancels_connection"); enter(QString::number(capacity_ + 1, 'g', 17)); break;
            }
            case 13:
                require(controller_->selected().value("capacity").toDouble() == capacity_ + 1, "keyboard_valid_retry_not_applied");
                require(!item("numericRejection_capacity")->isVisible() && controller_->runId().isEmpty()
                    && !controller_->canApprove() && controller_->networkHash() != hash_, "valid_retry_not_retired");
                require(!item("Approve simulated preview", true)->isEnabled(), "stale_approval_button_enabled");
                snapshot("03-numerical-edit-retired-run.png");
                passed("valid_retry_clears_inline_retires_run_approval");
                changes_ = 0; revision_ = controller_->simulationRevision();
                click(item("numericProperty_capacity")); key(Qt::Key_A, Qt::ControlModifier);
                key(Qt::Key_unknown, Qt::NoModifier, QString::number(capacity_ + 2, 'g', 17));
                click(item("numericProperty_initial")); break;
            case 14:
                require(controller_->selected().value("capacity").toDouble() == capacity_ + 2
                    && controller_->simulationRevision() == revision_ + 1 && changes_ == 1, "focus_loss_commit_not_exactly_once");
                require(!item("numericProperty_capacity")->property("pendingUserCommit").toBool(), "focus_loss_commit_not_consumed");
                click(item("numericProperty_capacity")); break;
            case 15:
                click(item("numericProperty_initial")); break;
            case 16:
                require(changes_ == 1 && controller_->simulationRevision() == revision_ + 1
                    && controller_->selected().value("capacity").toDouble() == capacity_ + 2, "unedited_focus_loss_recommitted");
                passed("edited_focus_loss_once_unedited_focus_loss_no_commit");
                controller_->reset();
                if (window_->property("compactLayout").toBool()) click(item("Navigator", true));
                break;
            case 17:
                click(item("networkAsset:tank_a")); break;
            case 18:
                controlClick(item("networkAsset:tank_b")); break;
            case 19:
                require(controller_->selectedIds().size() == 2, "native_control_click_not_additive");
                hash_ = controller_->networkHash();
                key(Qt::Key_D, Qt::ControlModifier); break;
            case 20:
                require(controller_->assets().size() == 8 && controller_->selectedIds().size() == 2
                    && controller_->networkHash() != hash_, "native_duplicate_shortcut_failed");
                key(Qt::Key_Z, Qt::ControlModifier); break;
            case 21:
                require(controller_->assets().size() == 6 && controller_->networkHash() == hash_, "duplicate_undo_not_atomic");
                passed("native_multiselect_duplicate_atomic_undo");
                click(item("openSensorEditor")); break;
            case 22:
                require(window_->property("sensorEditorVisible").toBool() && item("openSensorEditor")->property("checked").toBool(), "sensor_toggle_highlight_missing");
                key(Qt::Key_D, Qt::ControlModifier);
                require(controller_->assets().size() == 6, "dialog_stole_scene_shortcut");
                snapshot("05-sensor-editor.png"); click(item("sensorPanelClose")); break;
            case 23:
                require(!window_->property("sensorEditorVisible").toBool() && !item("openSensorEditor")->property("checked").toBool(), "sensor_close_did_not_clear_highlight");
                click(item("openTelemetry")); break;
            case 24:
                require(window_->property("telemetryVisible").toBool() && item("openTelemetry")->property("checked").toBool(), "monitoring_toggle_highlight_missing");
                snapshot("06-monitoring.png"); click(item("telemetryPanelClose")); break;
            case 25:
                require(!window_->property("telemetryVisible").toBool(), "monitoring_close_failed");
                click(item("openPipeEvidence")); break;
            case 26:
                require(window_->property("pipeTransportVisible").toBool() && item("openPipeEvidence")->property("checked").toBool(), "pipe_toggle_highlight_missing");
                snapshot("07-pipe-evidence.png"); click(item("pipeTransportClose")); break;
            case 27:
                require(!window_->property("pipeTransportVisible").toBool(), "pipe_close_failed");
                click(item("toggleInvestigator")); break;
            case 28:
                require(window_->property("showAgent").toBool() && item("toggleInvestigator")->property("checked").toBool(), "investigator_toggle_highlight_missing");
                snapshot("08-investigation-dock.png"); click(item("toggleInvestigator")); break;
            case 29:
                require(!window_->property("showAgent").toBool() && !item("toggleInvestigator")->property("checked").toBool(), "repeated_investigator_click_did_not_close");
                passed("native_tool_highlights_close_buttons_shortcut_guard_repeat_toggle");
                eval("app.openRecordedInvestigation()"); break;
            case 30:
                observed_["recording"] = QJsonDocument::fromJson(eval(QStringLiteral(
                    "JSON.stringify({loaded:app.eog.loaded,loading:app.eog.busy,"
                    "folder_matches:app.eog.sourceFolder===String(app.portfolioExamples.operationsPath),"
                    "pending:String(app.pendingRecordedArchive).length>0,"
                    "historical:app.eogAnalysis.historical,status:app.eogAnalysis.status,"
                    "storage:app.eogAnalysis.storageStatus,trace:app.eogAnalysis.trace.length})")).toString().toUtf8()).object();
                if (!eval("app.eogAnalysis.historical").toBool()) return;
                require(eval("app.eog.loaded && app.eogAnalysis.trace.length > 0 && app.eogAnalysis.plans.length > 0 && app.eogAnalysis.summary.length > 0 && !app.eogAnalysis.canReview").toBool(),
                    "recorded_investigation_not_historical_grounded_evidence");
                snapshot("09-recorded-investigation.png");
                passed("bundled_hosted_recording_reopens_historical_without_review_authority");
                eval("app.workspace='workbench'; app.compactPane='navigator'; workspaceSettings.setValue('navigatorOpen',true)");
                controller_->select("source_b");
                { SimulationConfig config; config.horizon_h = 1.25; controller_->forecastWithConfig(config); }
                break;
            case 31:
                if (controller_->busy()) return;
                require(controller_->canApprove() && !controller_->runId().isEmpty(), "filter_forecast_unavailable");
                hash_ = controller_->networkHash(); run_ = controller_->runId(); revision_ = controller_->simulationRevision();
                click(item("networkFilter")); key(Qt::Key_A, Qt::ControlModifier); key(Qt::Key_unknown, Qt::NoModifier, "_A"); break;
            case 32:
                require(eval("JSON.stringify(app.filteredNetworkAssets.map(row=>row.id))").toString() == "[\"tank_a\",\"source_a\"]",
                    "filter_name_id_type_or_case_wrong");
                require(controller_->selectedId() == "source_b", "filter_changed_hidden_selection"); preserved();
                key(Qt::Key_D, Qt::ControlModifier);
                require(controller_->assets().size() == 6, "filter_input_stole_scene_shortcut");
                click(item("networkAsset:tank_a")); break;
            case 33:
                require(controller_->selectedId() == "tank_a", "filtered_asset_click_failed");
                eval("networkAssetList.positionViewAtIndex(1,ListView.Contain)"); break;
            case 34:
                shiftClick(item("networkAsset:source_a")); break;
            case 35:
                require(controller_->selectedIds().size() == 2 && controller_->selectedIds().contains("tank_a")
                    && controller_->selectedIds().contains("source_a"), "filtered_range_selected_hidden_asset");
                preserved(); click(item("clearNetworkFilter")); break;
            case 36:
                require(item("networkFilter")->property("text").toString().isEmpty()
                    && eval("app.filteredNetworkAssets.length").toInt() == 6 && controller_->selectedIds().size() == 2,
                    "clear_filter_changed_selection_or_did_not_restore_list");
                preserved(); passed("native_filter_preserves_evidence_selection_shortcuts_and_visible_ranges");
                eval("app.compactPane='inspector'; workspaceSettings.setValue('inspectorOpen',true)");
                for (const auto& row : controller_->routes()) {
                    const auto route = row.toMap();
                    if (route.value("pump").toBool()) { controller_->select(route.value("id").toString()); break; }
                }
                break;
            case 37: {
                auto* bar = item("propertyInspectorScrollBar"); auto* panel = item("assetInspector");
                const auto origin = bar->mapToItem(panel, QPointF(0, 0));
                const bool mirrored = bar->parentItem() && bar->parentItem()->property("mirrored").toBool();
                observed_["inspector_scrollbar"] = QJsonObject{{"x", origin.x()}, {"height", bar->height()},
                    {"panel_width", panel->width()}, {"panel_height", panel->height()}, {"size", bar->property("size").toDouble()}};
                require((mirrored ? origin.x() < panel->width() * 0.2 : origin.x() > panel->width() * 0.8)
                    && bar->height() > panel->height() * 0.8,
                    "inspector_scrollbar_not_at_viewport_edge");
                require(bar->isVisible() == (bar->property("size").toDouble() < 1.0), "inspector_scrollbar_visibility_wrong");
                require(controller_->selected().value("pump").toBool() && item("inspectorPumpState")->isVisible()
                    && item("inspectorPumpState")->property("text").toString() == (controller_->selected().value("available").toBool() ? "Pump available" : "Pump unavailable"),
                    "inspector_pump_state_missing");
                preserved(); passed("inspector_route_state_preserves_numerical_evidence");
                snapshot("10-route-inspector.png"); controller_->select("tank_a"); break;
            }
            case 38:
                require(item("equipmentLabel")->property("text").toString() == "Tank A"
                    && item("numericProperty_capacity")->isVisible(), "inspector_equipment_fields_missing");
                preserved(); snapshot("11-tank-inspector.png");
                require(eval("siteGround.gridStep >= 20 && siteGround.gridStep % 20 === 0 && siteGround.gridColumns <= 65 && siteGround.gridRows <= 65 && siteGround.gridCenterX % siteGround.gridStep === 0 && siteGround.gridCenterZ % siteGround.gridStep === 0").toBool(),
                    "layout_grid_not_bounded_or_world_aligned");
                click(item("sceneGrid")); break;
            case 39:
                require(!window_->property("siteGridVisible").toBool() && !item("sceneGrid")->property("checked").toBool(), "grid_did_not_hide");
                preserved(); click(item("sceneGrid")); break;
            case 40:
                require(window_->property("siteGridVisible").toBool() && item("sceneGrid")->property("checked").toBool(), "grid_did_not_restore");
                preserved(); passed("world_aligned_bounded_grid_toggle_preserves_numerical_evidence");
                snapshot("12-layout-grid.png"); controller_->loadExample("monitoring");
                eval("app.openTelemetry()"); break;
            case 41: {
                require(window_->property("telemetryVisible").toBool(),"monitoring_example_did_not_open");
                const auto sensors=controller_->sensorConfiguration().value("sensors").toArray();
                require(sensors.size()==10 && controller_->draftIssues().isEmpty(),"monitoring_example_coverage_invalid");
                auto* start=item("startLocalInput"); auto* capture=item("captureObservedState");
                require(start->width()>80 && capture->width()>80 && !capture->isEnabled(),"monitoring_controls_wrong");
                require(!item("saveTimedRecording")->isEnabled(),"manual_mode_enabled_timed_export");
                require(item("observedCaptureSummary")->property("text").toString().contains("Capture an immutable state"),"observed_capture_empty_state_missing");
                passed("instrumented_example_and_monitoring_controls_have_explicit_empty_state");
                snapshot("13-local-monitoring.png"); click(item("telemetryPanelClose")); break;
            }
            case 42:
                require(!window_->property("telemetryVisible").toBool(),"new_monitoring_close_failed");
                controller_->loadExample("healthy"); controller_->forecast(); break;
            case 43:
                if (controller_->busy()) return;
                require(controller_->canApprove(),"retained_pipe_source_forecast_missing");
                hash_=controller_->networkHash(); run_=controller_->runId(); revision_=controller_->simulationRevision();
                controller_->select("main_pump");
                eval("app.pipeTransport.setParameter('cells','4'); app.pipeTransport.setParameter('output_step_h','3'); app.pipeTransport.run()"); break;
            case 44:
                if (eval("app.pipeTransport.busy").toBool() || eval("app.pipeTransport.historyStatus.indexOf('Saving')>=0").toBool()) return;
                require(eval("app.pipeTransport.historyStatus.indexOf('saved')>=0").toBool(),"native_pipe_was_not_retained");
                pipe_evidence_id_=eval("app.pipeTransport.result.result_id").toString(); require(!pipe_evidence_id_.isEmpty(),"retained_pipe_id_missing");
                eval("app.history.loadSession(app.pipeTransport.result.result_id); app.openResults(3)"); break;
            case 45: {
                if (eval("app.history.busy").toBool()) return;
                require(eval("app.history.historical && app.history.retainedEvidence.length===1 && app.history.retainedEvidence[0].kind==='pipe_transport'").toBool(),"retained_pipe_history_did_not_reopen");
                auto* scroll=item("selectedEvidenceScroll"); auto* flick=scroll->property("contentItem").value<QQuickItem*>();
                require(flick,"retained_history_scroll_unavailable");
                auto* button=item("viewRetainedPipeHistory");
                const auto offset=flick->property("contentY").toDouble()+button->mapToItem(flick,QPointF{}).y()-scroll->height()*0.4;
                const auto maximum=std::max(0.0,flick->property("contentHeight").toDouble()-flick->height());
                flick->setProperty("contentY",std::clamp(offset,0.0,maximum)); break;
            }
            case 46: {
                auto* button=item("viewRetainedPipeHistory");
                auto* flick=item("selectedEvidenceScroll")->property("contentItem").value<QQuickItem*>();
                require(flick,"retained_history_scroll_unavailable");
                const auto local=button->mapToItem(flick,QPointF{});
                if (local.y()<0 || local.y()+button->height()>flick->height()) {
                    const auto maximum=std::max(0.0,flick->property("contentHeight").toDouble()-flick->height());
                    flick->setProperty("contentY",std::clamp(flick->property("contentY").toDouble()+local.y()-flick->height()*0.4,0.0,maximum));
                    retained_button_rect_={}; return;
                }
                const QRectF rect(button->mapToScene(QPointF{}),QSizeF(button->width(),button->height()));
                if (rect!=retained_button_rect_) { retained_button_rect_=rect; return; }
                observed_["retained_pipe_button"]=QJsonObject{{"x",rect.x()},{"y",rect.y()},{"width",rect.width()},{"height",rect.height()},
                    {"scroll_y",flick->property("contentY").toDouble()},{"viewport_height",flick->height()}};
                snapshot("14-retained-pipe-history.png"); retained_viewer_wait_.start(); click(button); break;
            }
            case 47:
                if (!window_->property("pipeTransportVisible").toBool() && retained_viewer_wait_.elapsed()<1000) return;
                observed_["retained_pipe_open"]=QJsonObject{{"visible",window_->property("pipeTransportVisible").toBool()},
                    {"imported",eval("app.pipeTransport.viewingImportedEvidence").toBool()},{"status",eval("app.pipeTransport.status").toString()}};
                require(window_->property("pipeTransportVisible").toBool() && eval("app.pipeTransport.viewingImportedEvidence").toBool(),"retained_pipe_action_did_not_open_viewer");
                require(eval("app.pipeTransport.result.result_id").toString()==pipe_evidence_id_ && !eval("app.investigator.canApprove").toBool(),"retained_pipe_identity_or_authority_changed");
                preserved(); snapshot("15-retained-pipe-viewer.png");
                passed("actual_pipe_execution_retention_reload_and_history_viewer_preserve_volume_controls");
                click(item("pipeTransportClose")); break;
            case 48:
                require(!window_->property("pipeTransportVisible").toBool(),"retained_pipe_viewer_did_not_close");
                eval("app.openResults(4)"); break;
            case 49:
                require(item("scienceSourceSelector")->isVisible(),"scientific_source_selector_missing");
                click(item("scienceSourceSelector")); key(Qt::Key_Down); key(Qt::Key_Return); break;
            case 50:
                require(eval("app.storageScience.sourceMode==='candidate' && !app.storageScience.canRun").toBool(),"empty_stored_source_borrowed_current_forecast");
                eval("app.investigator.searchLocally()"); break;
            case 51:
                if (eval("app.investigator.running").toBool()) return;
                if (!stored_source_wait_.isValid()) stored_source_wait_.start();
                if (eval("app.storageScience.busy || !app.storageScience.canRun").toBool() && stored_source_wait_.elapsed()<3000) return;
                require(eval("app.storageScience.sourceMode==='candidate' && app.storageScience.canRun && app.storageScience.sourceIdentity.source_run_id===app.history.candidateRunId").toBool(),"selected_stored_science_source_did_not_reconstruct");
                preserved(); snapshot("16-selected-candidate-science.png");
                click(item("scienceSourceSelector")); key(Qt::Key_Home); key(Qt::Key_Return); break;
            case 52:
                require(eval("app.storageScience.sourceMode==='forecast'").toBool(),"scientific_source_selector_did_not_restore_forecast");
                preserved(); passed("native_source_selector_uses_immutable_stored_candidate_without_applying_controls");
                finish(true,"ok"); return;
            case 53:
                if (eval("app.telemetry.status!=='ready'").toBool()) return;
                require(eval("app.telemetry.bundledPublisher").toBool(),"observed_ui_python_publisher_unavailable");
                eval("app.telemetry.startSynthetic('healthy',73)"); break;
            case 54:
                require(eval("app.telemetry.error.length===0").toBool(),"observed_ui_publisher_failed");
                if (eval("(app.telemetry.counters.readings_received || 0)<10").toBool()) return;
                eval("app.telemetry.pause()"); break;
            case 55:
                if (!eval("app.telemetry.paused").toBool()) return;
                eval("app.telemetry.requestObservedCapture()"); break;
            case 56: {
                if (eval("app.telemetry.observedCaptureBusy || !app.telemetry.observedCapture.snapshot_id").toBool()) return;
                require(eval("app.telemetry.observedCaptureDisposition==='available'").toBool(),"observed_ui_capture_unavailable");
                auto* scroll=item("telemetryScroll"); auto* flick=scroll->property("contentItem").value<QQuickItem*>();
                require(flick,"observed_ui_scroll_unavailable"); auto* button=item("openObservedInvestigation");
                const auto maximum=std::max(0.0,flick->property("contentHeight").toDouble()-flick->height());
                flick->setProperty("contentY",std::clamp(flick->property("contentY").toDouble()+button->mapToItem(flick,QPointF{}).y()-flick->height()*0.4,0.0,maximum));
                break;
            }
            case 57:
                snapshot("17-observed-capture.png"); click(item("openObservedInvestigation")); break;
            case 58:
                if (!window_->property("observedInvestigationVisible").toBool()) return;
                require(item("observedInvestigationOrigin")->isVisible() && !eval("app.sceneKeysEnabled()").toBool(),"observed_dialog_origin_or_shortcut_gate_missing");
                click(item("observedLocalSearch")); break;
            case 59:
                if (eval("app.observedInvestigator.running || !app.observedInvestigator.canAcknowledge").toBool()) return;
                require(controller_->editorDocument()==observed_project_ && controller_->runId().isEmpty() && !controller_->canApprove(),"observed_search_modified_project");
                snapshot("18-observed-native-response.png"); click(item("reviewObservedAdvisory")); break;
            case 60:
                if (!window_->property("observedAdvisoryReviewVisible").toBool()) return;
                require(item("acknowledgeObservedAdvisory")->isEnabled(),"observed_review_acknowledgement_unavailable");
                require(eval("app.observedInvestigator.review.measurements.length>=10 && app.observedInvestigator.review.validation_id.length>0").toBool(),"observed_review_evidence_missing");
                snapshot("19-observed-advisory-review.png"); click(item("acknowledgeObservedAdvisory")); break;
            case 61: {
                auto* observedHistory=qobject_cast<history::HistoryController*>(window_->property("observedHistory").value<QObject*>());
                require(observedHistory,"observed_history_unavailable");
                if (window_->property("observedAdvisoryReviewVisible").toBool() || observedHistory->selectedArchive().value("approvals").toArray().size()!=1) return;
                require(!eval("app.observedInvestigator.canAcknowledge || app.observedInvestigator.canApprove").toBool() &&
                    controller_->editorDocument()==observed_project_ && controller_->runId().isEmpty() && !controller_->canApprove(),"observed_review_restored_operating_authority");
                click(item("observedInvestigationClose")); break;
            }
            case 62:
                require(!window_->property("observedInvestigationVisible").toBool(),"observed_dialog_close_failed");
                eval("app.telemetry.stop()"); break;
            case 63:
                if (eval("app.telemetry.active").toBool()) return;
                require(eval("app.telemetry.error.length===0").toBool(),"observed_ui_source_shutdown_failed");
                passed("actual_python_capture_native_search_advisory_review_and_retention_without_control_application");
                finish(true,"ok"); return;
            default: throw std::runtime_error("invalid_phase");
            }
            ++phase_;
        } catch (const std::exception& error) { finish(false, QString::fromLatin1(error.what())); }
    }
public:
    Check(QQuickWindow* window, ProjectController* controller, QCoreApplication* app)
        : QObject(app), window_(window), controller_(controller), app_(app) {
        observed_only_=qEnvironmentVariable("BREAKWATER_UI_OBSERVED_ONLY")=="1";
        if (qEnvironmentVariable("BREAKWATER_UI_MINIMUM") == "1") window->resize(960, 540);
        else if (qEnvironmentVariable("BREAKWATER_UI_SHORT") == "1") window->resize(1008, 672);
        elapsed_.start(); timer_.setInterval(100);
        connect(controller, &ProjectController::changed, this, [this] { ++changes_; });
        connect(&timer_, &QTimer::timeout, this, [this] { tick(); }); timer_.start();
    }
};
}
void startWorkbenchUiCheck(QQuickWindow* window, ProjectController* controller, QCoreApplication* app) {
    new Check(window, controller, app);
}
}
