#include "EogAnalysisController.hpp"
#include "agent/NemotronClient.hpp"
#include <QCommandLineParser>
#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QMap>
#include <QRegularExpression>
#include <QSet>
#include <QStandardPaths>
#include <QTextStream>
#include <QTimer>
#include <csignal>
#include <memory>
#include <utility>

using namespace breakwater::desktop;
using namespace breakwater::desktop::eog;

namespace {
// Signal handlers only set a flag. Cancellation and persistence use the Qt thread.
volatile std::sig_atomic_t interrupted = 0;
void interrupt(int) { interrupted = 1; }
enum ExitCode { Passed = 0, InvalidArguments = 2, Incomplete = 3, SetupFailure = 4, Deadline = 5, StorageFailure = 6 };

QString safeReference(const QString& value) {
    static const QRegularExpression allowed(QStringLiteral("^[A-Za-z0-9][A-Za-z0-9_.:-]{0,255}$"));
    if (value.contains("nvapi-", Qt::CaseInsensitive) || value.contains("bearer", Qt::CaseInsensitive)) return {};
    const auto match = allowed.match(value);
    return match.hasMatch() && match.capturedLength() == value.size() ? value : QString{};
}
QString pathKey(QString value) {
    value = QDir::cleanPath(QDir::fromNativeSeparators(value));
#ifdef Q_OS_WIN
    value = value.toCaseFolded();
#endif
    return value;
}
QString resolvedOutput(const QString& path) {
    const QFileInfo target(path);
    if (target.exists() || target.isSymLink()) return target.canonicalFilePath();
    QStringList suffix{target.fileName()};
    QFileInfo ancestor(target.absolutePath());
    while (!ancestor.exists()) {
        suffix.prepend(ancestor.fileName());
        const QFileInfo parent(ancestor.absolutePath());
        if (parent.absoluteFilePath() == ancestor.absoluteFilePath()) return {};
        ancestor = parent;
    }
    if (!ancestor.isDir()) return {};
    auto resolved = ancestor.canonicalFilePath();
    if (resolved.isEmpty()) return {};
    for (const auto& part : suffix) resolved = QDir(resolved).filePath(part);
    return QDir::cleanPath(resolved);
}
bool within(const QString& path, const QString& folder) {
    const auto candidate = pathKey(path), root = pathKey(folder);
    return candidate == root || candidate.startsWith(root.endsWith('/') ? root : root + '/');
}
void print(const QJsonObject& summary) {
    QTextStream(stdout) << QString::fromUtf8(QJsonDocument(summary).toJson(QJsonDocument::Compact)) << Qt::endl;
}
struct Options {
    QString dataset, archive, database, objective;
    qint64 cutoff = 0;
    bool hasCutoff = false, live = false;
};

// This schedules the existing production controller; it does not implement an
// investigation loop, choose tools, call a provider directly or apply controls.
class Rehearsal final : public QObject {
public:
    explicit Rehearsal(Options options, QCoreApplication& application)
        : options_(std::move(options)), app_(application), history_(options_.database),
          analysis_(&dataset_, &worker_, &history_, &model_) {
        connect(&history_, &EogHistoryRepository::ready, this, [this] { history_ready_ = true; beginWhenReady(); });
        connect(&dataset_, &EogDatasetController::completed, this, [this] {
            if (finalizing_ || finished_) return;
            if (options_.hasCutoff) {
                if (options_.cutoff < dataset_.firstUnix() || options_.cutoff > dataset_.nowUnix()) {
                    finish(InvalidArguments, "cutoff_out_of_range", false); return;
                }
                dataset_.setCursorUnix(double(options_.cutoff));
                if (qint64(dataset_.cursorUnix()) != options_.cutoff) { finish(SetupFailure, "cutoff_not_selected", false); return; }
            }
            dataset_ready_ = true; beginWhenReady();
        });
        connect(&dataset_, &EogDatasetController::failed, this, [this](const QString&) {
            finish(SetupFailure, "dataset_load_failed", false);
        });
        connect(&history_, &EogHistoryRepository::failed, this,
                [this](const QString& operation, const QString&, const QString&, const QString&) {
            if (finished_) return;
            forced_exit_ = StorageFailure; reason_ = "history_failed";
            if (operation == "export") { finish(StorageFailure, "archive_export_failed", false); return; }
            if (!finalizing_) stopAndArchive(StorageFailure, "history_failed");
        });
        connect(&history_, &EogHistoryRepository::archiveExported, this, [this](const QString& id, const QUrl&) {
            if (!finalizing_ || finished_ || id != session_) return;
            finish(forced_exit_ ? forced_exit_ : gate_ ? Passed : Incomplete,
                   forced_exit_ ? reason_ : gate_ ? QStringLiteral("verified_rehearsal") : reason_, true);
        });
        connect(&analysis_, &EogAnalysisController::changed, this, [this] {
            if (!analysis_.sessionId().isEmpty()) session_ = analysis_.sessionId();
            // busy becomes false before EogInvestigator::finished reaches the
            // controller. A queued turn sees finalResult and its grounding check.
            if (!settle_queued_ && !finalizing_ && !finished_) {
                settle_queued_ = true;
                QTimer::singleShot(0, this, [this] { settle_queued_ = false; settled(); });
            }
        });
        connect(&model_, &agent::ModelTransport::requestStarted, this, [this] { ++model_requests_; });
        connect(&model_, &agent::ModelTransport::replyReady, this, [this](const QJsonObject& message) {
            model_issued_calls_ += message.value("tool_calls").toArray().size();
        });
        connect(&model_, &agent::ModelTransport::requestCompleted, this, [this](const QJsonObject& metadata) {
            const auto status = metadata.value("http_status").toInt();
            if (metadata.value("outcome") != "succeeded" || status < 200 || status >= 300) return;
            ++model_replies_;
            const auto response = model_.lastResponseModel(), configured = model_.configuredModelIdentifier();
            if (response.isEmpty() || configured.isEmpty() || response != configured) model_identity_matches_ = false;
            response_model_ = response;
        });
        connect(&worker_, &EogAnalysisClient::completed, this, [this](const QJsonObject& response, const QJsonObject&) {
            ++numeric_exchanges_;
            if (response.value("status") != "completed") {
                ++unsuccessful_exchanges_;
                if (response.value("status") == "unavailable") {
                    ++unavailable_exchanges_; unavailable_operations_.insert(response.value("operation").toString());
                }
                return;
            }
            ++numeric_completed_;
            if (response.value("operation") == "validate_plan") {
                const auto validation = response.value("result").toObject().value("validation").toObject();
                if (validation.value("independent").toBool() && validation.value("feasible").toBool() && validation.value("coverage") == "available")
                    validations_[validation.value("validation_id").toString()] = validation;
            }
        });
        connect(&deadline_, &QTimer::timeout, this, [this] { stopAndArchive(Deadline, "deadline_exceeded"); });
        connect(&archive_deadline_, &QTimer::timeout, this, [this] { finish(StorageFailure, "archive_deadline_exceeded", false); });
        connect(&interrupt_poll_, &QTimer::timeout, this, [this] {
            if (interrupted && !finalizing_ && !finished_) stopAndArchive(Deadline, "cancelled");
        });
        deadline_.setSingleShot(true); archive_deadline_.setSingleShot(true);
        deadline_.start(300000); interrupt_poll_.start(100);
        QTimer::singleShot(0, this, [this] { dataset_.loadFolder(QUrl::fromLocalFile(options_.dataset)); });
    }
private:
    Options options_;
    QCoreApplication& app_;
    EogDatasetController dataset_;
    EogAnalysisClient worker_;
    EogHistoryRepository history_;
    agent::NemotronClient model_;
    EogAnalysisController analysis_;
    QTimer deadline_, archive_deadline_, interrupt_poll_;
    QMap<QString, QJsonObject> validations_;
    QSet<QString> unavailable_operations_;
    QString session_, reason_, response_model_, selected_, validation_id_, capture_id_;
    QJsonArray tool_order_;
    int model_requests_ = 0, model_replies_ = 0, model_issued_calls_ = 0;
    int numeric_exchanges_ = 0, numeric_completed_ = 0, unavailable_exchanges_ = 0, unsuccessful_exchanges_ = 0;
    int executed_tools_ = 0, accepted_calls_ = 0, forced_exit_ = 0;
    bool history_ready_ = false, dataset_ready_ = false, started_ = false, settle_queued_ = false;
    bool finalizing_ = false, finished_ = false, model_identity_matches_ = true, gate_ = false;

    void beginWhenReady() {
        if (!history_ready_ || !dataset_ready_ || started_ || finalizing_ || finished_) return;
        if (!options_.live) { finish(Incomplete, "live_not_enabled", false); return; }
        if (!analysis_.providerConfigured()) { finish(SetupFailure, "provider_unavailable", false); return; }
        started_ = true;
        analysis_.investigate(options_.objective);
        settled();
    }
    void settled() {
        if (!started_ || finalizing_ || finished_ || analysis_.busy()) return;
        if (session_.isEmpty()) { finish(SetupFailure, "capture_or_investigation_start_failed", false); return; }
        bool failure = false;
        const auto final = collectTrace(failure);
        // Snapshot eligibility before any cancellation or historical reopening.
        selected_ = analysis_.selectedPlanId();
        const auto checked = QJsonObject::fromVariantMap(analysis_.selectedValidation());
        validation_id_ = checked.value("validation_id").toString();
        capture_id_ = analysis_.evidenceContext().value("capture_id").toString();
        const auto actual = validations_.value(validation_id_);
        gate_ = !failure && !final.isEmpty() && model_requests_ > 0 && model_replies_ > 0 && model_identity_matches_ &&
            model_issued_calls_ > 0 && accepted_calls_ > 0 && executed_tools_ > 0 && numeric_completed_ > 0 && analysis_.canReview() &&
            !selected_.isEmpty() && final.value("selected_plan_id") == selected_ && !validation_id_.isEmpty() &&
            final.value("validation_id") == validation_id_ && actual.value("plan_id") == selected_ &&
            actual.value("plan_hash") == checked.value("plan_hash") && !actual.isEmpty();
        reason_ = !model_identity_matches_ ? "model_identity_mismatch" : unavailable_exchanges_ > 0 ? "provider_operation_unavailable"
            : failure ? "investigation_incomplete" : "grounded_validated_plan_unavailable";
        analysis_.finishSession();
        archive();
    }
    QJsonObject collectTrace(bool& failure) {
        QJsonObject final;
        QMap<QString, QString> calls;
        QSet<QString> results;
        executed_tools_ = 0; tool_order_ = {};
        for (const auto& value : analysis_.trace()) {
            const auto row = QJsonObject::fromVariantMap(value.toMap());
            const auto type = row.value("type").toString();
            if (type == "final") final = row;
            if (type == "failure" || type == "cancel") failure = true;
            if (type == "tool_call") {
                calls[row.value("call_id").toString()] = row.value("name").toString();
                tool_order_.append(safeReference(row.value("name").toString()));
            }
            if (type == "tool_result" && calls.value(row.value("call_id").toString()) == row.value("name").toString() &&
                    !results.contains(row.value("call_id").toString())) {
                results.insert(row.value("call_id").toString());
                if (row.value("ok").toBool()) ++executed_tools_;
            }
        }
        accepted_calls_ = calls.size();
        return final;
    }
    void stopAndArchive(int code, const QString& reason) {
        if (finalizing_ || finished_) return;
        forced_exit_ = code; reason_ = reason; gate_ = false;
        if (!analysis_.sessionId().isEmpty()) session_ = analysis_.sessionId();
        capture_id_ = analysis_.evidenceContext().value("capture_id").toString();
        bool failure = false; collectTrace(failure);
        // cancel queues the running session's terminal record before retiring it;
        // the repository still owns its immutable stored input and ordered trace.
        analysis_.cancel(); dataset_.cancel();
        archive();
    }
    void archive() {
        if (finalizing_ || finished_) return;
        finalizing_ = true; deadline_.stop(); interrupt_poll_.stop();
        if (session_.isEmpty() || !history_ready_) {
            finish(forced_exit_ ? forced_exit_ : Incomplete, reason_.isEmpty() ? "no_session_to_archive" : reason_, false); return;
        }
        archive_deadline_.start(120000);
        // Same repository queue: begin/analyses/agent events/finish all precede
        // export. Export revalidates and writes a new full archive atomically.
        history_.exportSession(session_, QUrl::fromLocalFile(options_.archive));
    }
    void finish(int code, const QString& reason, bool archived) {
        if (finished_) return;
        finished_ = true; deadline_.stop(); archive_deadline_.stop(); interrupt_poll_.stop();
        if (analysis_.busy()) analysis_.cancel();
        if (dataset_.busy()) dataset_.cancel();
        QStringList unavailable = unavailable_operations_.values(); unavailable.sort();
        QJsonArray unavailableNames;
        for (const auto& operation : unavailable) unavailableNames.append(safeReference(operation));
        print({{"status", code == Passed ? "passed" : code == Incomplete ? "incomplete" : "failed"},
            {"code", reason}, {"exit_code", code}, {"archive_written", archived}, {"live_enabled", options_.live},
            {"model_identity", response_model_}, {"model_requests", model_requests_}, {"successful_model_replies", model_replies_},
            {"model_issued_tool_calls", model_issued_calls_}, {"executed_tools", executed_tools_}, {"tool_order", tool_order_},
            {"numeric_exchanges", numeric_exchanges_}, {"numeric_completed", numeric_completed_}, {"unavailable_exchanges", unavailable_exchanges_},
            {"unsuccessful_exchanges", unsuccessful_exchanges_}, {"unavailable_operations", unavailableNames},
            {"grounded_review_eligible_final", gate_}, {"session_id", safeReference(session_)},
            {"dataset_identity", dataset_.datasetIdentity()}, {"capture_id", safeReference(capture_id_)},
            {"selected_plan_id", safeReference(selected_)}, {"validation_id", safeReference(validation_id_)},
            {"summary", code == Passed ? "Archived hosted tool use and an exact independently validated advisory plan. No controls were applied."
                : archived ? "Archived incomplete investigation evidence. Unavailable methods or ungrounded advice do not pass the rehearsal."
                : started_ ? "Investigation evidence could not be exported; the local history database may retain completed records."
                : "No live investigation was run. Hosted calls require the explicit --live flag and local readiness."}});
        app_.exit(code);
    }
};
}

int main(int argc, char** argv) {
    QCoreApplication app(argc, argv);
    QCoreApplication::setApplicationName("BREAKWATER");
    QCoreApplication::setOrganizationName("Breakwater");
    QCommandLineParser parser;
    parser.setApplicationDescription("Verification-only EOG production investigation and full evidence archive. Never applies controls. Hosted calls require --live.");
    parser.addHelpOption();
    parser.addOptions({{QStringLiteral("dataset"), "Existing folder containing only the three allowed input names.", "folder"},
        {QStringLiteral("archive"), "NEW full .eog-session.json archive destination.", "file"},
        {QStringLiteral("cutoff"), "Optional historical Unix cutoff (exact integer).", "unix"},
        {QStringLiteral("db"), "Optional local SQLite evidence database, outside the dataset.", "file"},
        {QStringLiteral("objective"), "Operator objective (1–4,000 characters); no fixed tool order.", "text"},
        {QStringLiteral("live"), "Explicitly invoke the real hosted Nemotron production loop."}});
    if (!parser.parse(app.arguments())) { print({{"status", "failed"}, {"code", "invalid_arguments"}, {"exit_code", InvalidArguments}}); return InvalidArguments; }
    if (parser.isSet("help")) { QTextStream(stdout) << parser.helpText(); return InvalidArguments; }
    Options options;
    options.dataset = QFileInfo(parser.value("dataset")).absoluteFilePath();
    options.archive = QFileInfo(parser.value("archive")).absoluteFilePath();
    options.database = parser.isSet("db") ? QFileInfo(parser.value("db")).absoluteFilePath()
        : QDir(QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation)).filePath("eog-live-check.sqlite");
    options.objective = parser.isSet("objective") ? parser.value("objective") : QStringLiteral(
        "Investigate the captured EOG network for the operator. Use actual evidence to explain current risks and compare useful responses. "
        "Select advice only when its exact plan has available independent validation; otherwise explain missing capabilities and finish without a selected plan. Choose tools adaptively.");
    options.live = parser.isSet("live"); options.hasCutoff = parser.isSet("cutoff");
    if (options.hasCutoff) {
        const auto text = parser.value("cutoff");
        static const QRegularExpression integer(QStringLiteral("^(0|[1-9][0-9]{0,11})$"));
        bool ok = false; options.cutoff = text.toLongLong(&ok);
        if (!ok || !integer.match(text).hasMatch() || options.cutoff > 253402300799LL) {
            print({{"status", "failed"}, {"code", "invalid_cutoff"}, {"exit_code", InvalidArguments}}); return InvalidArguments;
        }
    }
    const QFileInfo archive(options.archive), database(options.database);
    const auto archivePath = resolvedOutput(options.archive), databasePath = resolvedOutput(options.database);
    const auto sourceFolder = QFileInfo(options.dataset).canonicalFilePath();
    if (!parser.isSet("dataset") || !parser.isSet("archive") || parser.value("dataset").isEmpty() || parser.value("archive").isEmpty() ||
            !parser.positionalArguments().isEmpty() || !QFileInfo(options.dataset).isDir() || sourceFolder.isEmpty() ||
            options.objective.trimmed().isEmpty() || options.objective.size() > 4000 ||
            !archive.fileName().endsWith(".eog-session.json", Qt::CaseInsensitive) || archive.exists() || archive.isSymLink() || database.isSymLink() ||
            (database.exists() && !database.isFile()) || (parser.isSet("db") && parser.value("db").isEmpty()) ||
            archivePath.isEmpty() || databasePath.isEmpty() || pathKey(archivePath) == pathKey(databasePath) ||
            within(archivePath, sourceFolder) || within(databasePath, sourceFolder)) {
        print({{"status", "failed"}, {"code", "invalid_input_or_output_paths"}, {"exit_code", InvalidArguments}}); return InvalidArguments;
    }
    // A database journal/lock cannot occupy the archive destination either.
    for (const auto* suffix : {"-wal", "-shm", "-journal", ".lock"}) if (pathKey(archivePath) == pathKey(databasePath + QLatin1String(suffix))) {
        print({{"status", "failed"}, {"code", "output_path_collision"}, {"exit_code", InvalidArguments}}); return InvalidArguments;
    }
    if (!QDir().mkpath(archive.absolutePath())) {
        print({{"status", "failed"}, {"code", "archive_directory_unavailable"}, {"exit_code", SetupFailure}}); return SetupFailure;
    }
    std::signal(SIGINT, interrupt); std::signal(SIGTERM, interrupt);
#ifdef SIGBREAK
    std::signal(SIGBREAK, interrupt);
#endif
    Rehearsal rehearsal(std::move(options), app);
    return app.exec();
}
