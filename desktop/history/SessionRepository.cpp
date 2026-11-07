#include "SessionRepository.hpp"
#include "ProjectDocument.hpp"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QMap>
#include <QSaveFile>
#include <QSet>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlRecord>
#include <QStandardPaths>
#include <QUuid>
#include <algorithm>
#include <cmath>
#include <functional>
#include <stdexcept>
#include <vector>

namespace breakwater::desktop::history {
namespace {
constexpr int database_version = 1;
constexpr double maximum_sequence = 9007199254740991.0;
struct RepositoryError { QString code; QString message; };
[[noreturn]] void reject(const QString& code, const QString& message) { throw RepositoryError{code, message}; }
void require(bool condition, const QString& message) { if (!condition) reject("invalid_archive", message); }
QString boundedText(const QJsonObject& object, const QString& field, qsizetype maximum = 256, bool empty = false) {
    const auto value = object.value(field);
    require(value.isString() && value.toString().size() <= maximum && (empty || !value.toString().trimmed().isEmpty()),
        "An archive text field is missing, invalid or over its limit: " + field);
    return value.toString();
}
double finiteNumber(const QJsonObject& object, const QString& field) {
    const auto value = object.value(field);
    require(value.isDouble() && std::isfinite(value.toDouble()), "An archive numeric field is missing or nonfinite: " + field);
    return value.toDouble();
}
QDateTime timestamp(const QJsonObject& object, const QString& field) {
    const auto text = boundedText(object, field, 64);
    auto date = QDateTime::fromString(text, Qt::ISODateWithMs);
    if (!date.isValid()) date = QDateTime::fromString(text, Qt::ISODate);
    require(date.isValid(), "Archive timestamps must use valid ISO dates.");
    return date;
}
void inspectJson(const QJsonValue& value, int depth = 0) {
    require(depth <= 64, "Archive nesting exceeds the supported structure.");
    if (value.isDouble()) require(std::isfinite(value.toDouble()), "Archive numbers must be finite.");
    if (value.isArray()) for (const auto& item : value.toArray()) inspectJson(item, depth + 1);
    if (value.isObject()) {
        const auto object = value.toObject();
        for (auto it = object.begin(); it != object.end(); ++it) {
            auto key = it.key().toLower(); key.remove('_'); key.remove('-');
            require(key != "apikey" && key != "nvidiaapikey" && key != "authorization" &&
                key != "reasoning" && key != "reasoningcontent", "Credentials and raw reasoning fields are prohibited in session archives.");
            inspectJson(it.value(), depth + 1);
        }
    }
}
QJsonObject readJson(const QByteArray& bytes, const QString& code) {
    if (bytes.size() > SessionRepository::maximumArchiveBytes) reject(code, "The session archive exceeds 512 MiB.");
    QJsonParseError error;
    const auto document = QJsonDocument::fromJson(bytes, &error);
    if (error.error != QJsonParseError::NoError || !document.isObject()) reject(code, "The session archive is not a valid JSON object.");
    return document.object();
}
QJsonObject historical(QJsonObject archive) {
    archive["historical"] = true;
    archive["actionable"] = false;
    auto recommendation = archive.value("recommendation").toObject();
    recommendation["actionable"] = false;
    recommendation["approval_status"] = "historical";
    archive["recommendation"] = recommendation;
    return archive;
}
void unchangedEvidence(const QJsonObject& previous, const QJsonObject& next) {
    if (previous.value("snapshot") != next.value("snapshot") || previous.value("created_at") != next.value("created_at") ||
        previous.value("objective") != next.value("objective"))
        reject("checkpoint_conflict", "A session checkpoint cannot replace its immutable snapshot or original objective.");
    QMap<QString, QJsonObject> runs;
    for (const auto& value : next.value("runs").toArray()) {
        const auto details = value.toObject(); runs.insert(details.value("run").toObject().value("run_id").toString(), details);
    }
    for (const auto& value : previous.value("runs").toArray()) {
        const auto details = value.toObject(), summary = details.value("run").toObject();
        const auto id = summary.value("run_id").toString();
        if (!runs.contains(id)) reject("checkpoint_conflict", "A checkpoint cannot discard stored numerical runs.");
        const auto updated = runs.value(id), updated_summary = updated.value("run").toObject();
        for (const auto* field : {"time_h", "tank_ids", "volumes_m3"})
            if (details.value(QLatin1String(field)) != updated.value(QLatin1String(field)))
                reject("checkpoint_conflict", "A checkpoint cannot replace stored numerical trajectories.");
        for (const auto* field : {"run_id", "snapshot_id", "physical_hash", "horizon_h", "cost", "controls", "created_at", "solver", "solver_version", "sample_step_h"})
            if (summary.value(QLatin1String(field)) != updated_summary.value(QLatin1String(field)))
                reject("checkpoint_conflict", "A checkpoint cannot change stored numerical run evidence.");
    }
    for (const auto* field : {"events", "approvals"}) {
        const auto old_values = previous.value(QLatin1String(field)).toArray(), new_values = next.value(QLatin1String(field)).toArray();
        if (old_values.size() > new_values.size()) reject("checkpoint_conflict", "Session audit records cannot be removed.");
        for (qsizetype i = 0; i < old_values.size(); ++i)
            if (old_values[i] != new_values[i]) reject("checkpoint_conflict", "Session audit records are append-only.");
    }
}
void checkArchive(const QJsonObject& archive) {
    inspectJson(archive);
    require(finiteNumber(archive, "archive_version") == 1.0, "Unsupported session archive version.");
    const auto session = boundedText(archive, "session_id");
    require(!QUuid::fromString(session).isNull(), "Session IDs must be UUIDs.");
    const auto sequence = finiteNumber(archive, "checkpoint_sequence");
    require(sequence >= 1.0 && sequence <= maximum_sequence && std::floor(sequence) == sequence,
        "Checkpoint sequence must be a positive, exactly represented integer.");
    require(timestamp(archive, "created_at") <= timestamp(archive, "updated_at"), "Archive update precedes session creation.");
    const QSet<QString> statuses{"running", "completed", "failed", "cancelled", "interrupted"};
    require(statuses.contains(boundedText(archive, "status")), "Unknown session status.");
    (void)boundedText(archive, "objective", 4000, true);
    require(archive.value("snapshot").isObject(), "A captured snapshot is required.");
    const auto snapshot = archive.value("snapshot").toObject();
    const auto snapshot_id = boundedText(snapshot, "snapshot_id"), hash = boundedText(snapshot, "physical_hash");
    const auto horizon = finiteNumber(snapshot, "horizon_h");
    require(horizon > 0.0 && horizon <= 24.0, "Snapshot horizon must be within (0, 24] hours.");
    require(snapshot.value("network").isObject(), "The snapshot needs an actual network.");
    ProjectDocument project;
    try { project = parse_project({{"version", project_version}, {"network", snapshot.value("network")}, {"layout", QJsonObject{}}}); }
    catch (const std::exception&) { reject("invalid_archive", "The captured network is malformed or exceeds the project limits."); }
    require(physics_hash(project) == hash, "The physical hash does not match the captured network.");
    require(archive.value("model").isObject(), "Model metadata must be an object.");
    const auto model = archive.value("model").toObject();
    if (model.contains("configured_model")) (void)boundedText(model, "configured_model", 256, true);
    QStringList tanks;
    for (const auto& asset : project.network.assets) if (asset.kind == AssetKind::tank) tanks.append(QString::fromStdString(asset.id));
    const auto catalog = control_catalog(project.network);
    require(archive.value("runs").isArray() && archive.value("runs").toArray().size() <= 128, "An archive supports at most 128 runs.");
    QSet<QString> run_ids;
    for (const auto& value : archive.value("runs").toArray()) {
        require(value.isObject(), "Run details must be objects.");
        const auto details = value.toObject();
        require(details.value("ok").isBool() && details.value("ok").toBool() && details.value("run").isObject(), "Run details must contain a stored numerical run.");
        const auto run = details.value("run").toObject();
        const auto id = boundedText(run, "run_id");
        require(!run_ids.contains(id), "Run IDs must be unique."); run_ids.insert(id);
        require(boundedText(run, "snapshot_id") == snapshot_id && boundedText(run, "physical_hash") == hash &&
            finiteNumber(run, "horizon_h") == horizon, "Run identity does not match its immutable snapshot and horizon.");
        require(finiteNumber(run, "cost") >= 0.0, "Run costs must be finite and nonnegative.");
        require(details.value("time_h").isArray() && details.value("tank_ids").isArray() && details.value("volumes_m3").isArray(), "Full run trajectories are required.");
        const auto times = details.value("time_h").toArray(), ids = details.value("tank_ids").toArray(), volumes = details.value("volumes_m3").toArray();
        require(times.size() >= 2 && times.size() <= 10001 && ids.size() == tanks.size() && volumes.size() == tanks.size(), "Run trajectory dimensions exceed engine limits or differ from the snapshot.");
        double previous = -1.0;
        for (const auto& time : times) {
            require(time.isDouble() && std::isfinite(time.toDouble()) && time.toDouble() > previous, "Run times must be finite and strictly increasing.");
            previous = time.toDouble();
        }
        require(times.first().toDouble() == 0.0 && times.last().toDouble() == horizon, "The full run must start at zero and cover the fixed horizon.");
        for (qsizetype index = 0; index < tanks.size(); ++index) {
            require(ids[index].isString() && ids[index].toString() == tanks[index], "Run tank IDs/order differ from the captured network.");
            require(volumes[index].isArray() && volumes[index].toArray().size() == times.size(), "Run sample counts do not match the time axis.");
            for (const auto& volume : volumes[index].toArray())
                require(volume.isDouble() && std::isfinite(volume.toDouble()), "Tank trajectories must contain finite numbers.");
        }
        require(run.value("tanks").isArray() && run.value("tanks").toArray().size() == tanks.size(), "Run summaries must identify every captured tank.");
        const auto tank_summaries = run.value("tanks").toArray();
        for (qsizetype index = 0; index < tanks.size(); ++index) {
            require(tank_summaries[index].isObject(), "Tank summaries must be objects.");
            const auto summary = tank_summaries[index].toObject();
            const auto samples = volumes[index].toArray();
            require(boundedText(summary, "asset_id") == tanks[index] && finiteNumber(summary, "initial_m3") == samples.first().toDouble() &&
                finiteNumber(summary, "final_m3") == samples.last().toDouble(), "Tank summaries differ from their stored trajectories.");
        }
        require(run.value("controls").isArray() && run.value("controls").toArray().size() <= 384, "Run controls must be a bounded array.");
        QSet<QString> controls;
        for (const auto& control_value : run.value("controls").toArray()) {
            require(control_value.isObject(), "Run controls must be objects.");
            const auto control = control_value.toObject();
            // Native control IDs contain a prefix/suffix around an asset/route
            // ID, whose independent project-v1 limit remains 256 characters.
            const auto control_id = boundedText(control, "control_id", 512);
            const auto amount = finiteNumber(control, "value");
            require(!controls.contains(control_id), "Run control IDs must be unique."); controls.insert(control_id);
            const auto descriptor = std::find_if(catalog.begin(), catalog.end(), [&](const ControlDescriptor& item) { return QString::fromStdString(item.id) == control_id; });
            require(descriptor != catalog.end() && amount >= descriptor->minimum && amount <= descriptor->maximum, "Run controls differ from the actual catalog or its bounds.");
        }
        const auto validation_status = boundedText(run, "validation_status");
        require(validation_status == "validated" || validation_status == "not_requested", "Unknown run validation status.");
        require(run.value("validation").isObject() && run.value("validation").toObject().value("feasible").isBool(), "A stored validation report is required.");
        const auto validation = run.value("validation").toObject();
        (void)finiteNumber(validation, "minimum_headroom_m3");
        require(finiteNumber(validation, "max_conservation_residual_m3") >= 0.0, "Conservation residuals must be finite and nonnegative.");
        require(validation.value("violations").isArray() && validation.value("violations").toArray().size() <= 1024 &&
            validation.value("limits").isArray() && validation.value("limits").toArray().size() <= 128, "Validation diagnostics exceed native network bounds.");
        for (const auto& diagnostic : validation.value("violations").toArray()) {
            require(diagnostic.isObject(), "Violation diagnostics must be objects.");
            const auto issue = diagnostic.toObject();
            (void)boundedText(issue, "code"); (void)boundedText(issue, "asset_id", 256, true); (void)boundedText(issue, "message", 4096);
            if (issue.contains("time_h")) (void)finiteNumber(issue, "time_h");
        }
        for (const auto& diagnostic : validation.value("limits").toArray()) {
            require(diagnostic.isObject(), "Limit diagnostics must be objects.");
            const auto limit = diagnostic.toObject();
            require(tanks.contains(boundedText(limit, "asset_id")), "Limit diagnostics refer to an unknown tank.");
            const auto kind = boundedText(limit, "limit");
            require(kind == "capacity" || kind == "empty", "Unknown tank limit type.");
            const auto time = finiteNumber(limit, "time_h");
            require(time >= 0.0 && time <= horizon + 1e-9, "A limit diagnostic lies outside the forecast horizon.");
        }
        if (validation_status == "validated") (void)boundedText(run, "validation_id");
    }
    require(archive.value("events").isArray() && archive.value("events").toArray().size() <= 1024, "Structured session events exceed the bounded session limit.");
    double event_sequence = 0.0;
    for (const auto& value : archive.value("events").toArray()) {
        require(value.isObject(), "Events must be structured objects.");
        const auto event = value.toObject();
        (void)boundedText(event, "type", 128);
        require(boundedText(event, "session_id") == session, "Event session identity differs from the archive.");
        const auto next = finiteNumber(event, "sequence");
        require(next > event_sequence && std::floor(next) == next, "Event sequence must increase monotonically."); event_sequence = next;
        (void)timestamp(event, "timestamp");
    }
    require(archive.value("recommendation").isObject(), "Recommendation metadata must be an object.");
    const auto recommendation = archive.value("recommendation").toObject();
    if (recommendation.contains("selected_run_id") && !recommendation.value("selected_run_id").isNull())
        require(recommendation.value("selected_run_id").isString() && run_ids.contains(recommendation.value("selected_run_id").toString()), "Recommendation refers to an unknown archived run.");
    require(archive.value("approvals").isArray() && archive.value("approvals").toArray().size() <= 128, "Approval audit entries exceed the session limit.");
    for (const auto& approval : archive.value("approvals").toArray()) require(approval.isObject(), "Approval audit entries must be objects.");
}
}

bool SessionRepository::validateArchive(const QJsonObject& archive, QString* error) {
    try { checkArchive(archive); if (error) error->clear(); return true; }
    catch (const RepositoryError& failure) { if (error) *error = failure.message; return false; }
    catch (const std::exception&) { if (error) *error = "The session archive could not be validated."; return false; }
}

class SessionRepositoryWorker final : public QObject {
    Q_OBJECT
public:
    explicit SessionRepositoryWorker(QString path) : path_(std::move(path)) {}
    ~SessionRepositoryWorker() override { close(); }
    void initialize() {
        perform([&] {
            const auto parent = QFileInfo(path_).absolutePath();
            if (!QDir().mkpath(parent)) reject("storage_unavailable", "The history directory could not be created.");
            connection_ = "breakwater-history-" + QUuid::createUuid().toString(QUuid::WithoutBraces);
            database_ = QSqlDatabase::addDatabase("QSQLITE", connection_);
            database_.setDatabaseName(path_);
            database_.setConnectOptions("QSQLITE_BUSY_TIMEOUT=5000");
            if (!database_.open()) reject("storage_unavailable", "The session history database could not be opened.");
            QSqlQuery version(database_);
            if (!version.exec("PRAGMA user_version") || !version.next()) reject("storage_corrupt", "The history database header could not be read.");
            const int schema_version = version.value(0).toInt(); version.finish();
            if (schema_version != 0 && schema_version != database_version) reject("unsupported_database_version", "The session history database uses an unsupported schema version.");
            if (schema_version == 0) {
                if (!database_.tables(QSql::Tables).isEmpty()) reject("unsupported_database_version", "An unversioned history database contains an unknown schema.");
                transaction([&] {
                    execute("CREATE TABLE sessions (session_id TEXT PRIMARY KEY NOT NULL, checkpoint_sequence INTEGER NOT NULL CHECK(checkpoint_sequence > 0), created_at TEXT NOT NULL, updated_at TEXT NOT NULL, status TEXT NOT NULL, objective TEXT NOT NULL, snapshot_id TEXT NOT NULL, physical_hash TEXT NOT NULL, archive BLOB NOT NULL)");
                    execute("CREATE INDEX sessions_status_updated ON sessions(status, updated_at DESC)");
                    execute("CREATE INDEX sessions_updated ON sessions(updated_at DESC)");
                    execute("PRAGMA user_version = 1");
                });
            }
            const QStringList expected{"session_id", "checkpoint_sequence", "created_at", "updated_at", "status", "objective", "snapshot_id", "physical_hash", "archive"};
            QSqlQuery columns(database_);
            if (!columns.exec("PRAGMA table_info(sessions)")) reject("storage_corrupt", "The history schema could not be inspected.");
            QStringList actual;
            while (columns.next()) actual.append(columns.value(1).toString());
            columns.finish();
            if (actual != expected) reject("unsupported_database_version", "The history table has an unknown or incomplete schema.");
            execute("PRAGMA synchronous = FULL");
            transaction([&] {
                QSqlQuery query(database_);
                if (!query.exec("SELECT archive FROM sessions WHERE status = 'running'")) reject("storage_failed", "Interrupted sessions could not be inspected.");
                std::vector<QJsonObject> recover;
                while (query.next()) {
                    auto archive = readStored(query.value(0).toByteArray());
                    archive["status"] = "interrupted";
                    const auto now = QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs);
                    if (timestamp(archive, "updated_at") < QDateTime::currentDateTimeUtc()) archive["updated_at"] = now;
                    const auto sequence = archive.value("checkpoint_sequence").toDouble();
                    if (sequence < maximum_sequence) archive["checkpoint_sequence"] = sequence + 1.0;
                    recover.push_back(archive);
                }
                query.finish();
                for (const auto& archive : recover) write(archive);
            });
            initialized_ = true;
            emit ready();
        });
    }
    void close() {
        if (!connection_.isEmpty()) {
            database_.close(); database_ = QSqlDatabase();
            QSqlDatabase::removeDatabase(connection_); connection_.clear();
        }
        initialized_ = false;
    }
    void save(QJsonObject archive) {
        perform([&] {
            available(); checkArchive(archive);
            const auto bytes = serialized(archive);
            const auto id = archive.value("session_id").toString();
            transaction([&] {
                QSqlQuery existing(database_);
                existing.prepare("SELECT checkpoint_sequence, archive FROM sessions WHERE session_id = ?"); existing.addBindValue(id);
                if (!existing.exec()) reject("storage_failed", "A session checkpoint could not be inspected.");
                if (existing.next()) {
                    const auto sequence = existing.value(0).toLongLong();
                    const auto incoming = static_cast<qint64>(archive.value("checkpoint_sequence").toDouble());
                    const auto previous = existing.value(1).toByteArray(); existing.finish();
                    if (incoming < sequence) return;
                    if (incoming == sequence) {
                        if (readStored(previous) != archive) reject("checkpoint_conflict", "Different session evidence has the same checkpoint sequence.");
                        return;
                    }
                    unchangedEvidence(readStored(previous), archive);
                }
                existing.finish(); write(archive, bytes);
            });
            emit sessionSaved(id); emit operationCompleted("save", id);
        });
    }
    void capture(std::function<QJsonObject()> producer) {
        perform([&] {
            available();
            if (!producer) reject("invalid_request", "A checkpoint capture callback is required.");
            save(producer());
        });
    }
    void list() {
        perform([&] {
            available(); QSqlQuery query(database_);
            if (!query.exec("SELECT session_id, checkpoint_sequence, created_at, updated_at, status, objective, snapshot_id, physical_hash FROM sessions ORDER BY updated_at DESC, session_id"))
                reject("storage_failed", "Session history could not be listed.");
            QJsonArray rows;
            while (query.next()) rows.append(QJsonObject{{"session_id", query.value(0).toString()}, {"checkpoint_sequence", query.value(1).toLongLong()},
                {"created_at", query.value(2).toString()}, {"updated_at", query.value(3).toString()}, {"status", query.value(4).toString()},
                {"objective", query.value(5).toString()}, {"snapshot_id", query.value(6).toString()}, {"physical_hash", query.value(7).toString()}, {"historical", true}});
            emit sessionsListed(rows); emit operationCompleted("list", {});
        });
    }
    void load(QString id) {
        perform([&] { available(); const auto archive = read(id); emit sessionLoaded(historical(archive)); emit operationCompleted("load", id); });
    }
    void exportArchive(QString id, QUrl destination) {
        perform([&] {
            available();
            if (!destination.isLocalFile()) reject("file_invalid", "Choose a local file for the session archive.");
            const auto target = QFileInfo(destination.toLocalFile()).absoluteFilePath();
            const auto database_path = QFileInfo(path_).absoluteFilePath();
            for (const auto& suffix : {QString{}, QStringLiteral("-wal"), QStringLiteral("-shm"), QStringLiteral("-journal")})
                if (target.compare(database_path + suffix, Qt::CaseInsensitive) == 0)
                    reject("file_invalid", "An export cannot replace the session history database or its journal.");
            const auto bytes = serialized(read(id));
            QSaveFile file(destination.toLocalFile());
            if (!file.open(QIODevice::WriteOnly) || file.write(bytes) != bytes.size() || !file.commit())
                reject("export_failed", "The session archive could not be saved atomically.");
            emit operationCompleted("export", id);
        });
    }
    void importArchive(QUrl source) {
        perform([&] {
            available();
            if (!source.isLocalFile()) reject("file_invalid", "Choose a local session archive file.");
            QFile file(source.toLocalFile());
            if (!file.open(QIODevice::ReadOnly)) reject("import_failed", "The session archive could not be opened.");
            if (file.size() > SessionRepository::maximumArchiveBytes) reject("invalid_archive", "The session archive exceeds 512 MiB.");
            const auto bytes = file.read(SessionRepository::maximumArchiveBytes + 1);
            if (file.error() != QFileDevice::NoError) reject("import_failed", "The session archive could not be read.");
            const auto archive = readJson(bytes, "invalid_archive"); checkArchive(archive);
            const auto normalized = serialized(archive);
            const auto id = archive.value("session_id").toString();
            transaction([&] {
                QSqlQuery query(database_); query.prepare("SELECT archive FROM sessions WHERE session_id = ?"); query.addBindValue(id);
                if (!query.exec()) reject("storage_failed", "Existing session evidence could not be checked.");
                if (query.next()) {
                    const auto previous = readStored(query.value(0).toByteArray()); query.finish();
                    if (previous != archive) reject("session_collision", "This session ID already identifies different stored evidence.");
                    return;
                }
                query.finish(); write(archive, normalized);
            });
            emit sessionSaved(id); emit sessionLoaded(historical(archive)); emit operationCompleted("import", id);
        });
    }
signals:
    void ready();
    void sessionsListed(const QJsonArray& sessions);
    void sessionLoaded(const QJsonObject& archive);
    void sessionSaved(const QString& sessionId);
    void operationCompleted(const QString& operation, const QString& sessionId);
    void failed(const QString& code, const QString& message);
private:
    QString path_, connection_;
    QSqlDatabase database_;
    bool initialized_ = false;
    void perform(const std::function<void()>& action) {
        try { action(); }
        catch (const RepositoryError& error) { emit failed(error.code, error.message); }
        catch (const std::exception&) { emit failed("storage_failed", "The session history operation failed."); }
    }
    void available() const { if (!initialized_) reject("storage_unavailable", "The session history database is unavailable."); }
    void execute(const QString& sql) {
        QSqlQuery query(database_);
        if (!query.exec(sql)) reject("storage_failed", "A session history database operation failed.");
    }
    void transaction(const std::function<void()>& action) {
        if (!database_.transaction()) reject("storage_failed", "A session history transaction could not begin.");
        try {
            action();
            if (!database_.commit()) reject("storage_failed", "A session history transaction could not commit.");
        } catch (...) { database_.rollback(); throw; }
    }
    QByteArray serialized(const QJsonObject& archive) const {
        const auto bytes = QJsonDocument(archive).toJson(QJsonDocument::Compact);
        if (bytes.size() > SessionRepository::maximumArchiveBytes) reject("invalid_archive", "The session archive exceeds 512 MiB.");
        return bytes;
    }
    QJsonObject readStored(const QByteArray& bytes) const {
        const auto archive = readJson(bytes, "storage_corrupt");
        QString error;
        if (!SessionRepository::validateArchive(archive, &error)) reject("storage_corrupt", "Stored session evidence is malformed; it was not modified.");
        return archive;
    }
    QJsonObject read(const QString& id) {
        if (id.isEmpty() || id.size() > 256) reject("invalid_request", "A bounded session ID is required.");
        QSqlQuery query(database_); query.prepare("SELECT archive FROM sessions WHERE session_id = ?"); query.addBindValue(id);
        if (!query.exec()) reject("storage_failed", "The session evidence could not be loaded.");
        if (!query.next()) reject("unknown_session", "No stored session has that ID.");
        const auto archive = readStored(query.value(0).toByteArray());
        if (archive.value("session_id").toString() != id) reject("storage_corrupt", "Stored session identity does not match its history index.");
        return archive;
    }
    void write(const QJsonObject& archive, QByteArray bytes = {}) {
        if (bytes.isEmpty()) bytes = serialized(archive);
        const auto snapshot = archive.value("snapshot").toObject();
        QSqlQuery query(database_);
        query.prepare("INSERT INTO sessions(session_id, checkpoint_sequence, created_at, updated_at, status, objective, snapshot_id, physical_hash, archive) VALUES(?, ?, ?, ?, ?, ?, ?, ?, ?) ON CONFLICT(session_id) DO UPDATE SET checkpoint_sequence=excluded.checkpoint_sequence, updated_at=excluded.updated_at, status=excluded.status, objective=excluded.objective, snapshot_id=excluded.snapshot_id, physical_hash=excluded.physical_hash, archive=excluded.archive");
        query.addBindValue(archive.value("session_id").toString());
        query.addBindValue(static_cast<qint64>(archive.value("checkpoint_sequence").toDouble()));
        query.addBindValue(archive.value("created_at").toString()); query.addBindValue(archive.value("updated_at").toString());
        query.addBindValue(archive.value("status").toString()); query.addBindValue(archive.value("objective").toString());
        query.addBindValue(snapshot.value("snapshot_id").toString()); query.addBindValue(snapshot.value("physical_hash").toString()); query.addBindValue(bytes);
        if (!query.exec()) reject("storage_failed", "The session checkpoint could not be written.");
    }
};

SessionRepository::SessionRepository(QString databasePath, QObject* parent) : QObject(parent) {
    if (databasePath.isEmpty()) databasePath = QDir(QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation)).filePath("history.sqlite");
    worker_ = new SessionRepositoryWorker(std::move(databasePath));
    worker_->moveToThread(&worker_thread_);
    connect(&worker_thread_, &QThread::finished, worker_, &QObject::deleteLater);
    connect(worker_, &SessionRepositoryWorker::ready, this, &SessionRepository::ready, Qt::QueuedConnection);
    connect(worker_, &SessionRepositoryWorker::sessionsListed, this, &SessionRepository::sessionsListed, Qt::QueuedConnection);
    connect(worker_, &SessionRepositoryWorker::sessionLoaded, this, &SessionRepository::sessionLoaded, Qt::QueuedConnection);
    connect(worker_, &SessionRepositoryWorker::sessionSaved, this, &SessionRepository::sessionSaved, Qt::QueuedConnection);
    connect(worker_, &SessionRepositoryWorker::operationCompleted, this, &SessionRepository::operationCompleted, Qt::QueuedConnection);
    connect(worker_, &SessionRepositoryWorker::failed, this, &SessionRepository::failed, Qt::QueuedConnection);
    QMetaObject::invokeMethod(worker_, [worker = worker_] { worker->initialize(); }, Qt::QueuedConnection);
    worker_thread_.setObjectName("BreakwaterSessionHistory");
    worker_thread_.start();
}

SessionRepository::~SessionRepository() {
    if (worker_thread_.isRunning()) {
        // FIFO delivery drains earlier checkpoints before closing the connection.
        QMetaObject::invokeMethod(worker_, [worker = worker_] { worker->close(); }, Qt::BlockingQueuedConnection);
        worker_thread_.quit(); worker_thread_.wait();
    }
}
void SessionRepository::saveSession(const QJsonObject& archive) { QMetaObject::invokeMethod(worker_, [worker = worker_, archive] { worker->save(archive); }, Qt::QueuedConnection); }
void SessionRepository::enqueueCheckpoint(std::function<QJsonObject()> capture) {
    QMetaObject::invokeMethod(worker_, [worker = worker_, capture = std::move(capture)]() mutable { worker->capture(std::move(capture)); }, Qt::QueuedConnection);
}
void SessionRepository::listSessions() { QMetaObject::invokeMethod(worker_, [worker = worker_] { worker->list(); }, Qt::QueuedConnection); }
void SessionRepository::loadSession(const QString& id) { QMetaObject::invokeMethod(worker_, [worker = worker_, id] { worker->load(id); }, Qt::QueuedConnection); }
void SessionRepository::exportSession(const QString& id, const QUrl& url) { QMetaObject::invokeMethod(worker_, [worker = worker_, id, url] { worker->exportArchive(id, url); }, Qt::QueuedConnection); }
void SessionRepository::importSession(const QUrl& url) { QMetaObject::invokeMethod(worker_, [worker = worker_, url] { worker->importArchive(url); }, Qt::QueuedConnection); }

} // namespace breakwater::desktop::history

#include "SessionRepository.moc"
