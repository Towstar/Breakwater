#include "WorkbenchUiCheck.hpp"
#include "../ProjectController.hpp"
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
    void mouse(QEvent::Type type, QPointF point, Qt::MouseButton button, Qt::MouseButtons held) {
        QMouseEvent event(type, point, point, point, button, held, Qt::NoModifier);
        QCoreApplication::sendEvent(window_, &event);
    }
    void click(QPointF point) {
        mouse(QEvent::MouseButtonPress, point, Qt::LeftButton, Qt::LeftButton);
        mouse(QEvent::MouseButtonRelease, point, Qt::LeftButton, Qt::NoButton);
    }
    void click(QQuickItem* target) { click(target->mapToScene(QPointF(target->width()/2, target->height()/2))); }
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
        QJsonObject result{{"check", "ui_overhaul"}, {"passed", ok}, {"code", code},
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
            require(elapsed_.elapsed() < 24000, "ui_check_timeout");
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
            case 3:
                require(controller_->selectedId() == "tank_a", "actual_click_did_not_select");
                capacity_ = controller_->selected().value("capacity").toDouble();
                mouse(QEvent::MouseButtonPress, tank_, Qt::LeftButton, Qt::LeftButton); break;
            case 4:
                mouse(QEvent::MouseMove, tank_ + QPointF(38, 16), Qt::NoButton, Qt::LeftButton); break;
            case 5:
                mouse(QEvent::MouseButtonRelease, tank_ + QPointF(38, 16), Qt::LeftButton, Qt::NoButton); break;
            case 6:
                require(tankPosition() != position_, "actual_drag_did_not_move"); preserved();
                passed("layout_drag_preserves_hash_run_approval"); enter("NaN"); break;
            case 7:
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
                passed("edited_focus_loss_once_unedited_focus_loss_no_commit"); finish(true, "ok"); return;
            default: throw std::runtime_error("invalid_phase");
            }
            ++phase_;
        } catch (const std::exception& error) { finish(false, QString::fromLatin1(error.what())); }
    }
public:
    Check(QQuickWindow* window, ProjectController* controller, QCoreApplication* app)
        : QObject(app), window_(window), controller_(controller), app_(app) {
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
