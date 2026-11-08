#include "AgentSensitivityClient.hpp"
#include "StorageScienceAnalysis.hpp"
#include "StorageScienceEvidence.hpp"
#include "WorkerProtocol.hpp"

#include <QFutureWatcher>
#include <QJsonDocument>
#include <QPointer>
#include <QRegularExpression>
#include <QSet>
#include <QThread>
#include <QUuid>
#include <QtConcurrentRun>
#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace breakwater::desktop::science {
namespace {
constexpr qsizetype maximumAgentSensitivityBytes = 256 * 1024;
void require(bool condition) {
    if (!condition) throw std::invalid_argument("A captured run, matching origin and one to eight unchanged interior controls are required.");
}
bool boundedText(const QJsonValue& value, qsizetype maximum = 256) {
    if (!value.isString() || value.toString().trimmed().isEmpty() || value.toString().size() > maximum) return false;
    for (const auto character : value.toString()) if (character.category() == QChar::Other_Control) return false;
    return true;
}
bool validOrigin(const CapturedForecast& capture, const QJsonObject& origin) {
    const QSet<QString> fields{"incident_snapshot_id", "incident_physical_hash", "run_id", "effective_physical_hash", "agent_session_id", "tool_call_id"};
    if (origin.size() != fields.size()) return false;
    for (const auto& field : fields) if (!boundedText(origin.value(field))) return false;
    for (const auto field : {"incident_snapshot_id", "run_id", "agent_session_id"})
        if (QUuid::fromString(origin.value(QLatin1String(field)).toString()).isNull()) return false;
    static const QRegularExpression hash(QStringLiteral("^[0-9a-f]{64}$"));
    return hash.match(origin.value("incident_physical_hash").toString()).hasMatch() &&
        hash.match(origin.value("effective_physical_hash").toString()).hasMatch() &&
        origin.value("run_id") == capture.id && origin.value("effective_physical_hash") == capture.physical_hash;
}
bool capturedControls(const CapturedForecast& capture, const QJsonArray& controls) {
    if (controls.isEmpty() || controls.size() > 8) return false;
    const auto catalog = control_catalog(capture.network);
    std::ptrdiff_t previous = -1;
    for (const auto& value : controls) {
        if (!value.isObject()) return false;
        const auto item = value.toObject();
        if (item.size() != 2 || !boundedText(item.value("control_id"), 512) || !item.value("value").isDouble() ||
            !std::isfinite(item.value("value").toDouble())) return false;
        const auto found = std::find_if(catalog.begin(), catalog.end(), [&](const ControlDescriptor& control) {
            return QString::fromStdString(control.id) == item.value("control_id").toString();
        });
        if (found == catalog.end() || std::distance(catalog.begin(), found) <= previous ||
            found->value != item.value("value").toDouble() || !sensitivity_ineligible_reason(capture.network, *found).isEmpty()) return false;
        previous = std::distance(catalog.begin(), found);
    }
    return true;
}
struct AnalysisResult { QJsonObject outcome, archive; QString error; };
AnalysisResult analyzeResponse(const std::shared_ptr<const CapturedForecast>& capture, const QJsonObject& response,
                              const QJsonArray& controls, const QJsonObject& origin) {
    try {
        const auto evidence = compare_storage_science(capture, response, true, controls);
        auto archive = storage_science_archive(*capture, evidence);
        archive["agent_origin"] = origin;
        QJsonObject analysis;
        for (const auto field : {"analysis_mode", "snapshot_id", "horizon_h", "step_h", "native_cost", "worker_cost", "within_tolerance",
                 "tanks", "selected_controls", "derivatives", "provenance", "comparison_scope", "native_solver_version"})
            analysis[QLatin1String(field)] = evidence.value(QLatin1String(field));
        analysis["effective_physical_hash"] = capture->physical_hash;
        auto warnings = evidence.value("warnings").toArray();
        warnings.append("Sensitivities apply only to the selected continuous interior controls at their captured values; bounds and approval are not differentiated.");
        warnings.append("This analysis does not validate a plan. Use native validate_run and explicit operator review before applying simulated controls.");
        analysis["warnings"] = warnings;
        analysis["units"] = QJsonObject{{"captured_controls", "m3/h"}, {"terminal_volume_jacobian_h", "h"},
            {"cost_gradient", "synthetic units / (m3/h)"}, {"cost", "synthetic units"}};
        QJsonObject outcome{{"ok", true}, {"result_id", evidence.value("result_id")}, {"run_id", capture->id},
            {"origin", origin}, {"analysis", analysis}, {"advisory", true}, {"validation_status", "not_performed"}};
        if (QJsonDocument(outcome).toJson(QJsonDocument::Compact).size() > maximumAgentSensitivityBytes)
            return {{}, {}, "The scientific summary exceeds its bounded model-tool response limit."};
        return {outcome, archive, {}};
    } catch (const std::exception&) {
        return {{}, {}, "The scientific response could not be compared and archived against its immutable native capture."};
    }
}
} // namespace

AgentSensitivityClient::AgentSensitivityClient(QString pythonExecutable, QString workerPath, QObject* parent)
    : QObject(parent), worker_(new PythonWorkerClient(std::move(pythonExecutable), std::move(workerPath), this)) {
    connect(worker_, &PythonWorkerClient::storageReady, this, &AgentSensitivityClient::accept);
    connect(worker_, &PythonWorkerClient::failed, this, [this](const QString& code, const QString& message) {
        if (waiting_) reject(code, message);
    });
}
AgentSensitivityClient::~AgentSensitivityClient() {
    ++generation_; waiting_ = false;
    disconnect(worker_, nullptr, this, nullptr);
    // QObject owns the worker; its destructor terminates both active/retiring
    // children without emitting cancellation callbacks from this destructor.
}
void AgentSensitivityClient::setTimeoutMs(int milliseconds) { worker_->setTimeoutMs(milliseconds); }
void AgentSensitivityClient::setTerminationGraceMs(int milliseconds) { worker_->setTerminationGraceMs(milliseconds); }
void AgentSensitivityClient::analyze(std::shared_ptr<const CapturedForecast> capture, QJsonArray controls, QJsonObject origin) {
    if (QThread::currentThread() != thread()) {
        emit failed("sensitivity_thread_invalid", "Scientific analyses must be requested on their owning thread."); return;
    }
    if (busy()) { emit failed("sensitivity_busy", "A scientific analysis or its cancelled comparison is still finishing."); return; }
    QJsonObject request;
    try {
        require(capture && validOrigin(*capture, origin) && capturedControls(*capture, controls));
        request = storage_science_payload(*capture, true, controls);
        require(protocol::storagePayload(request));
    } catch (const std::exception&) {
        emit failed("invalid_sensitivity_request", "A captured run, matching origin and one to eight unchanged interior controls are required."); return;
    }
    ++generation_; capture_ = std::move(capture); controls_ = std::move(controls); origin_ = std::move(origin); waiting_ = true;
    // runStorage may synchronously emit an error or trigger owner destruction.
    worker_->runStorage(request);
}
void AgentSensitivityClient::cancel() {
    ++generation_; waiting_ = false; capture_.reset(); controls_ = {}; origin_ = {};
    // Keep comparing_ true until the bounded immutable task actually finishes,
    // preventing repeated cancel/restart from accumulating comparison tasks.
    worker_->cancel();
}
void AgentSensitivityClient::reject(const QString& code, const QString& message) {
    ++generation_; waiting_ = false; capture_.reset(); controls_ = {}; origin_ = {};
    emit failed(code, message);
}
void AgentSensitivityClient::accept(const QJsonObject& response) {
    if (!waiting_ || !capture_) return;
    waiting_ = false; comparing_ = true;
    const auto epoch = generation_; const auto capture = capture_; const auto controls = controls_; const auto origin = origin_;
    auto* watcher = new QFutureWatcher<AnalysisResult>(this);
    connect(watcher, &QFutureWatcher<AnalysisResult>::finished, this, [this, watcher, epoch] {
        const auto result = watcher->result(); watcher->deleteLater(); comparing_ = false;
        if (epoch != generation_) return;
        capture_.reset(); controls_ = {}; origin_ = {};
        if (!result.error.isEmpty()) { reject("sensitivity_comparison_invalid", result.error); return; }
        emit completed(result.outcome, result.archive);
    });
    watcher->setFuture(QtConcurrent::run([capture, response, controls, origin] { return analyzeResponse(capture, response, controls, origin); }));
}

} // namespace breakwater::desktop::science
