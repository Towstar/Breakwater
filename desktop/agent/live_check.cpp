#include "AgentController.hpp"
#include "NemotronClient.hpp"
#include "TransportDiagnostics.hpp"
#include "../EvidencePaths.hpp"
#include "../LocalRuntime.hpp"
#include "history/HistoryController.hpp"
#include "history/SessionRepository.hpp"
#include "telemetry/TelemetryContracts.hpp"
#include <QCommandLineParser>
#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QMap>
#include <QRegularExpression>
#include <QSet>
#include <QTextStream>
#include <QTimer>
#include <csignal>
#include <exception>
#include <utility>

using namespace breakwater::desktop;
using namespace breakwater::desktop::agent;
using namespace breakwater::desktop::history;
using namespace breakwater::desktop::evidencepaths;

namespace {
volatile std::sig_atomic_t interrupted = 0;
void interrupt(int) { interrupted = 1; }
enum ExitCode { Passed = 0, InvalidArguments = 2, Incomplete = 3, SetupFailure = 4, Deadline = 5, StorageFailure = 6 };

void print(const QJsonObject& value) {
    QTextStream(stdout) << QString::fromUtf8(QJsonDocument(value).toJson(QJsonDocument::Compact)) << Qt::endl;
}
QString reference(const QString& value) {
    static const QRegularExpression allowed(QStringLiteral("^[A-Za-z0-9][A-Za-z0-9_.:-]{0,255}$"));
    if (value.contains("nvapi-", Qt::CaseInsensitive) || value.contains("bearer", Qt::CaseInsensitive)) return {};
    const auto match = allowed.match(value);
    return match.hasMatch() && match.capturedLength() == value.size() ? value : QString{};
}
QByteArray fileHash(const QString& path) {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly) || file.size() > 2 * 1024 * 1024) return {};
    QCryptographicHash hash(QCryptographicHash::Sha256);
    return hash.addData(&file) ? hash.result().toHex() : QByteArray{};
}
QJsonObject summary(int exitCode, const QString& code, bool live) {
    return {{"status", exitCode == Passed ? "passed" : exitCode == Incomplete ? "incomplete" : "failed"},
        {"code", code}, {"exit_code", exitCode}, {"live_enabled", live}, {"archive_written", false},
        {"controls_applied", false}, {"model_requests", 0}, {"successful_model_replies", 0},
        {"model_issued_tool_calls", 0}, {"started_tool_calls", 0}, {"completed_tool_calls", 0},
        {"failed_tool_calls", 0}, {"executed_tools", 0}, {"reused_validation_calls", 0},
        {"http_attempts", 0}, {"retry_count", 0}, {"request_counts_complete", true}};
}
struct Options { QString project, archive, database, objective; bool local = false; QByteArray inputHash; };

// Only schedules the production controller and its recorder. No tool selection,
// second investigation loop, numerical work, or simulated apply lives here.
class Rehearsal final : public QObject {
public:
    Rehearsal(Options options, ProjectController& project, NemotronConfig configuration, QCoreApplication& app)
        : options_(std::move(options)), app_(app), project_(project), original_(project.editorDocument()),
          original_hash_(project.networkHash()), input_hash_(options_.inputHash), model_(std::move(configuration)),
          repository_(options_.database), history_(&repository_), investigator_(&project_, &model_) {
        investigator_.setHistory(&repository_, &history_);
        connect(&repository_, &SessionRepository::ready, this, [this] {
            if (finished_ || started_) return;
            started_ = true;
            if (options_.local) investigator_.searchLocally();
            else investigator_.investigate(options_.objective);
            advance();
        });
        connect(&history_, &HistoryController::changed, this, [this] { advance(); });
        connect(&investigator_, &AgentController::changed, this, [this] { advance(); });
        connect(&repository_, &SessionRepository::sessionSaved, this, [this](const QString&) { advance(); });
        connect(&repository_, &SessionRepository::sessionInspected, this, [this](const QJsonObject& stored) { inspected(stored); });
        connect(&repository_, &SessionRepository::operationCompleted, this, [this](const QString& operation, const QString& id) {
            if (exporting_ && operation == "export" && id == session_id_) finish(exit_code_, result_code_, true);
        });
        connect(&repository_, &SessionRepository::failed, this, [this](const QString&, const QString&) {
            finish(StorageFailure, "history_failed", false);
        });
        deadline_.setSingleShot(true); archive_deadline_.setSingleShot(true);
        connect(&deadline_, &QTimer::timeout, this, [this] { stopAndArchive(Deadline, "deadline_exceeded"); });
        connect(&archive_deadline_, &QTimer::timeout, this, [this] { finish(StorageFailure, "archive_deadline_exceeded", false); });
        interrupt_poll_.setInterval(100);
        connect(&interrupt_poll_, &QTimer::timeout, this, [this] {
            if (interrupted) stopAndArchive(Incomplete, "cancelled");
        });
        deadline_.start(180000); interrupt_poll_.start();
    }
private:
    Options options_;
    QCoreApplication& app_;
    ProjectController& project_;
    QJsonObject original_;
    QString original_hash_;
    QByteArray input_hash_;
    NemotronClient model_;
    SessionRepository repository_;
    HistoryController history_;
    AgentController investigator_;
    QTimer deadline_, archive_deadline_, interrupt_poll_;
    QJsonObject terminal_, stored_;
    QString session_id_, result_code_ = "investigation_incomplete";
    qint64 desired_sequence_ = 0;
    int exit_code_ = Incomplete;
    bool started_ = false, inspecting_ = false, exporting_ = false, finished_ = false, stopping_ = false;
    bool review_eligible_ = false, grounded_ = false, controls_unchanged_ = false, inputs_unchanged_ = false;

    void stopAndArchive(int code, const QString& reason) {
        if (finished_ || stopping_ || exporting_) return;
        stopping_ = true; exit_code_ = code; result_code_ = reason;
        deadline_.stop(); interrupt_poll_.stop(); archive_deadline_.start(60000);
        if (!started_) { finish(code, reason, false); return; }
        investigator_.cancel();
        advance();
    }
    void advance() {
        if (!started_ || investigator_.running() || inspecting_ || exporting_ || finished_) return;
        const auto archive = history_.selectedArchive();
        const auto status = archive.value("status").toString();
        if (status != "completed" && status != "failed" && status != "cancelled") return;
        const auto events = QJsonArray::fromVariantList(investigator_.events());
        if (events.isEmpty() || archive.value("session_id") != events.first().toObject().value("session_id") ||
            archive.value("events").toArray() != events ||
            archive.value("recommendation").toObject() != QJsonObject::fromVariantMap(investigator_.recommendation())) return;
        session_id_ = archive.value("session_id").toString();
        desired_sequence_ = archive.value("checkpoint_sequence").toInteger();
        terminal_ = archive; review_eligible_ = investigator_.canApprove(); inspecting_ = true;
        deadline_.stop(); interrupt_poll_.stop();
        if (!archive_deadline_.isActive()) archive_deadline_.start(60000);
        // SessionRecorder queues publication before exposing this live archive.
        // FIFO worker delivery reads the durable checkpoint, without selecting
        // historical UI state or changing approval authority.
        repository_.inspectSession(session_id_);
    }
    bool exactValidation(const QJsonObject& archive) const {
        const auto recommendation = archive.value("recommendation").toObject();
        const auto snapshot = archive.value("snapshot").toObject();
        const auto selected = recommendation.value("selected_run_id").toString();
        const auto validation = recommendation.value("validation_id").toString();
        const auto expectedSource = options_.local ? QStringLiteral("local_native") : QStringLiteral("hosted");
        const auto expectedValidation = recommendation.value("validation_source").toString();
        if (expectedValidation != "validate_run" && (!options_.local || expectedValidation != "native_search_dense_reference")) return false;
        if (archive.value("status") != "completed" || recommendation.value("status") != "validated_recommendation" ||
            recommendation.value("source_mode") != expectedSource || !recommendation.value("ok").toBool() ||
            !recommendation.value("verified_feasible").toBool() || recommendation.value("validation_source") != expectedValidation ||
            selected.isEmpty() || validation.isEmpty() || snapshot.value("physical_hash") != original_hash_ ||
            !archive.value("approvals").toArray().isEmpty()) return false;
        QJsonObject selectedRun;
        for (const auto& value : archive.value("runs").toArray()) {
            const auto detail = value.toObject(), run = detail.value("run").toObject();
            if (run.value("run_id") == selected && detail.value("ok").toBool()) {
                if (!selectedRun.isEmpty()) return false;
                selectedRun = run;
            }
        }
        if (selectedRun.isEmpty() || selectedRun.value("validation_status") != "validated" ||
            selectedRun.value("validation_id") != validation || selectedRun.value("validation_source") != expectedValidation ||
            !selectedRun.value("validation").toObject().value("feasible").toBool() ||
            !selectedRun.value("validation").toObject().value("violations").toArray().isEmpty() ||
            selectedRun.value("controls") != recommendation.value("controls") ||
            selectedRun.value("cost") != recommendation.value("verified_cost")) return false;
        for (const auto* key : {"snapshot_id", "physical_hash", "horizon_h"})
            if (selectedRun.value(QLatin1String(key)) != snapshot.value(QLatin1String(key)) ||
                recommendation.value(QLatin1String(key)) != snapshot.value(QLatin1String(key))) return false;
        if (options_.local) return true;
        QSet<QString> requested;
        QMap<QString, QJsonObject> starts;
        for (const auto& value : archive.value("events").toArray()) {
            const auto event = value.toObject();
            const auto id = event.value("tool_call_id").toString(), type = event.value("type").toString();
            if (type == "tool_requested" && event.value("tool_name") == "validate_run") requested.insert(id);
            if (type == "tool_started" && event.value("tool_name") == "validate_run" && requested.contains(id))
                starts.insert(id, event.value("input_summary").toObject());
            if (type == "tool_completed" && event.value("tool_name") == "validate_run" && starts.contains(id)) {
                const auto result = event.value("result").toObject(), args = starts.value(id);
                if (result.value("ok").toBool() && args.value("snapshot_id") == snapshot.value("snapshot_id") &&
                    args.value("run_id") == selected && result.value("run").toObject() == selectedRun) return true;
            }
        }
        return false;
    }
    void inspected(const QJsonObject& archive) {
        if (!inspecting_ || finished_ || archive.value("session_id") != session_id_) return;
        inspecting_ = false;
        if (archive.value("checkpoint_sequence").toInteger() < desired_sequence_ ||
            archive.value("status") != terminal_.value("status") || archive.value("events") != terminal_.value("events") ||
            archive.value("recommendation") != terminal_.value("recommendation") ||
            archive.value("snapshot") != terminal_.value("snapshot")) {
            finish(StorageFailure, "terminal_checkpoint_mismatch", false); return;
        }
        stored_ = archive;
        controls_unchanged_ = project_.networkHash() == original_hash_ && project_.editorDocument() == original_;
        inputs_unchanged_ = !input_hash_.isEmpty() && fileHash(options_.project) == input_hash_;
        bool identities = true; int successes = 0, modelEvents = 0;
        for (const auto& value : archive.value("events").toArray()) {
            const auto event = value.toObject();
            if (event.value("type") == "model_request_started" || event.value("type") == "model_request_completed") ++modelEvents;
            if (event.value("type") != "model_request_completed") continue;
            const auto request = event.value("request").toObject();
            if (request.value("outcome") == "succeeded") {
                ++successes;
                identities &= request.value("configured_model") == model_.configuredModelIdentifier() &&
                    request.value("response_model") == model_.configuredModelIdentifier() && request.value("http_status") == 200;
            }
        }
        const bool modelGate = options_.local ? modelEvents == 0 : identities && successes > 0;
        grounded_ = exactValidation(archive) && modelGate && controls_unchanged_ && inputs_unchanged_;
        if (!stopping_) {
            exit_code_ = grounded_ ? Passed : Incomplete;
            result_code_ = grounded_ ? "verified_rehearsal" : !identities ? "model_identity_mismatch" :
                !controls_unchanged_ || !inputs_unchanged_ ? "input_or_control_state_changed" :
                archive.value("recommendation").toObject().value("status") == "no_validated_plan" ? "no_validated_plan" : "investigation_incomplete";
        }
        if (QFileInfo::exists(options_.archive) || QFileInfo(options_.archive).isSymLink()) {
            finish(StorageFailure, "archive_destination_exists", false); return;
        }
        exporting_ = true;
        repository_.exportSession(session_id_, QUrl::fromLocalFile(options_.archive));
    }
    void finish(int code, const QString& reason, bool archived) {
        if (finished_) return;
        finished_ = true; deadline_.stop(); archive_deadline_.stop(); interrupt_poll_.stop();
        if (investigator_.running()) investigator_.cancel();
        auto output = summary(code, reason, !options_.local);
        output["source_mode"] = options_.local ? "local_native" : "hosted";
        const auto events = stored_.isEmpty() ? QJsonArray::fromVariantList(investigator_.events()) : stored_.value("events").toArray();
        int requests = 0, completedRequests = 0, successes = 0, retries = 0, http = 0;
        int issued = 0, starts = 0, completed = 0, failures = 0, reused = 0;
        bool identities = true;
        QJsonArray order;
        for (const auto& value : events) {
            const auto event = value.toObject(); const auto type = event.value("type").toString();
            if (type == "model_request_started") ++requests;
            if (type == "model_request_completed") {
                ++completedRequests;
                const auto request = event.value("request").toObject();
                const auto retry = request.value("retry_count").toInt(); retries += retry;
                // A logical request can finish while still awaiting admission.
                // Count the posted attempts, not a presumed initial POST.
                if (request.contains("transport_diagnostics"))
                    http += request.value("transport_diagnostics").toObject().value("attempts").toArray().size();
                else if (completedRequests <= requests) http += 1 + retry;
                if (request.value("outcome") == "succeeded") {
                    ++successes;
                    identities &= request.value("configured_model") == model_.configuredModelIdentifier() &&
                        request.value("response_model") == model_.configuredModelIdentifier();
                }
            }
            if (type == "tool_requested") { ++issued; order.append(reference(event.value("tool_name").toString())); }
            if (type == "tool_started") ++starts;
            if (type == "tool_completed") {
                ++completed;
                if (event.value("result").toObject().value("reused_validation").toBool()) ++reused;
            }
            if (type == "tool_failed") ++failures;
        }
        const auto recommendation = stored_.value("recommendation").toObject();
        output["archive_written"] = archived; output["controls_unchanged"] = controls_unchanged_;
        output["inputs_unchanged"] = inputs_unchanged_; output["grounded_validated_final"] = grounded_;
        output["operator_review_eligible_at_completion"] = review_eligible_;
        output["model_identity"] = options_.local ? QStringLiteral("local-native-finite-search") : model_.configuredModelIdentifier();
        output["model_identity_matches"] = !options_.local && identities && successes > 0;
        output["model_requests"] = requests; output["completed_model_requests"] = completedRequests;
        output["successful_model_replies"] = successes; output["retry_count"] = retries; output["http_attempts"] = http;
        output["request_counts_complete"] = completedRequests == requests;
        output["issued_tool_calls"] = issued; output["model_issued_tool_calls"] = options_.local ? 0 : issued;
        output["started_tool_calls"] = starts;
        output["completed_tool_calls"] = completed; output["failed_tool_calls"] = failures;
        output["executed_tools"] = starts - reused; output["reused_validation_calls"] = reused; output["tool_order"] = order;
        output["session_id"] = reference(session_id_); output["session_status"] = reference(stored_.value("status").toString());
        output["selected_run_id"] = reference(recommendation.value("selected_run_id").toString());
        output["validation_id"] = reference(recommendation.value("validation_id").toString());
        output["native_runs"] = stored_.value("runs").toArray().size();
        QString terminalCode;
        for (const auto& value : events) {
            const auto event = value.toObject();
            if (event.value("type") == "agent_failed") terminalCode = reference(event.value("error").toObject().value("code").toString());
        }
        output["terminal_error_code"] = terminalCode;
        output["physical_hash"] = original_hash_;
        print(output); app_.exit(code);
    }
};
}

int main(int argc, char** argv) {
    QCoreApplication app(argc, argv);
    QCoreApplication::setApplicationName("BREAKWATER"); QCoreApplication::setOrganizationName("Breakwater");
    QCommandLineParser parser;
    parser.setApplicationDescription("Verification-only Sandbox production-controller investigation and unchanged evidence export. Never applies controls. Hosted calls require --live.");
    parser.addHelpOption();
    parser.addOptions({{QStringLiteral("project"), "Existing local project-v1/v2 JSON file.", "file"},
        {QStringLiteral("archive"), "NEW .session.json evidence archive outside the input folder.", "file"},
        {QStringLiteral("db"), "NEW local SQLite evidence database outside the input folder.", "file"},
        {QStringLiteral("objective"), "Operator objective (1..4000 characters); no fixed tool order.", "text"},
        {QStringLiteral("live"), "Explicitly invoke the real hosted Nemotron production controller."},
        {QStringLiteral("local"), "Explicitly run production local finite search; zero hosted model requests."}});
    if (!parser.parse(app.arguments())) { print(summary(InvalidArguments, "invalid_arguments", false)); return InvalidArguments; }
    if (parser.isSet("help")) { QTextStream(stdout) << parser.helpText(); return InvalidArguments; }
    const bool live = parser.isSet("live"), local = parser.isSet("local");
    if (live && local) { print(summary(InvalidArguments, "conflicting_modes", false)); return InvalidArguments; }
    Options options{QFileInfo(parser.value("project")).absoluteFilePath(), QFileInfo(parser.value("archive")).absoluteFilePath(),
        QFileInfo(parser.value("db")).absoluteFilePath(), parser.isSet("objective") ? parser.value("objective") : QStringLiteral(
        "Investigate the captured synthetic network. Explain supported risks and compare useful responses using actual evidence. "
        "Select a response only with independent validation of that exact run, otherwise explain why no supported choice is available. "
        "Keep action for human review and use returned evidence references."), local};
    const QFileInfo source(options.project), archive(options.archive), database(options.database);
    const auto archivePath = resolvedOutput(options.archive), databasePath = resolvedOutput(options.database);
    const auto inputFolder = canonicalExisting(source.absolutePath());
    if (!parser.isSet("project") || !parser.isSet("archive") || !parser.isSet("db") ||
        parser.value("project").isEmpty() || parser.value("archive").isEmpty() || parser.value("db").isEmpty() ||
        !parser.positionalArguments().isEmpty() || !source.isFile() || source.isSymLink() || source.size() > 2 * 1024 * 1024 ||
        options.objective.trimmed().isEmpty() || options.objective.size() > 4000 ||
        !archive.fileName().endsWith(".session.json", Qt::CaseInsensitive) || archive.exists() || archive.isSymLink() ||
        database.exists() || database.isSymLink() || archivePath.isEmpty() || databasePath.isEmpty() || inputFolder.isEmpty() ||
        pathKey(archivePath) == pathKey(databasePath) || within(archivePath, inputFolder) || within(databasePath, inputFolder)) {
        print(summary(InvalidArguments, "invalid_input_or_output_paths", live)); return InvalidArguments;
    }
    for (const auto* suffix : {"-wal", "-shm", "-journal", ".lock"}) {
        const auto sidecar = databasePath + QLatin1String(suffix);
        if (pathKey(archivePath) == pathKey(sidecar) || QFileInfo::exists(sidecar) || QFileInfo(sidecar).isSymLink()) {
            print(summary(InvalidArguments, "output_path_collision", live)); return InvalidArguments;
        }
    }
    options.archive = archivePath; options.database = databasePath;
    ProjectController project;
    bool projectValid = false;
    QFile input(options.project);
    if (input.open(QIODevice::ReadOnly)) {
        const auto bytes = input.read(2 * 1024 * 1024 + 1);
        if (input.error() == QFileDevice::NoError && bytes.size() <= 2 * 1024 * 1024) {
            try {
                projectValid = project.restore(telemetry::decode_document(bytes, 2 * 1024 * 1024)) && project.draftIssues().isEmpty();
                options.inputHash = QCryptographicHash::hash(bytes, QCryptographicHash::Sha256).toHex();
            } catch (const std::exception&) { /* Only a fixed sanitized error is public. */ }
        }
    }
    input.close();
    if (!projectValid) {
        print(summary(InvalidArguments, "project_invalid", live)); return InvalidArguments;
    }
    if (!live && !local) {
        auto result = summary(Incomplete, "live_not_enabled", false);
        result["inputs_unchanged"] = fileHash(options.project) == options.inputHash; result["controls_unchanged"] = true;
        result["physical_hash"] = project.networkHash(); print(result); return Incomplete;
    }
    runtime::configureBundledPython();
    auto configuration = local ? NemotronConfig{} : NemotronConfig::fromEnvironment();
    if (live && configuration.apiKey.isEmpty()) { print(summary(SetupFailure, "credentials_missing", true)); return SetupFailure; }
    if (!QDir().mkpath(QFileInfo(options.archive).absolutePath()) || !QDir().mkpath(QFileInfo(options.database).absolutePath())) {
        print(summary(SetupFailure, "output_directory_unavailable", live)); return SetupFailure;
    }
    std::signal(SIGINT, interrupt); std::signal(SIGTERM, interrupt);
#ifdef SIGBREAK
    std::signal(SIGBREAK, interrupt);
#endif
    Rehearsal rehearsal(std::move(options), project, std::move(configuration), app);
    return app.exec();
}
