#include "EogHistoryRepository.hpp"
#include "EogContracts.hpp"
#include "telemetry/TelemetryContracts.hpp"
#include <QCryptographicHash>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QLockFile>
#include <QSaveFile>
#include <QSet>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QUuid>
#include <algorithm>
#include <cmath>
#include <functional>
#include <stdexcept>

#ifdef Q_OS_WIN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <compressapi.h>
#endif

namespace breakwater::desktop::eog {
namespace {
constexpr qint64 jsonLimit = 4LL * 1024 * 1024, csvLimit = 128LL * 1024 * 1024;
constexpr int applicationId = 0x454f4731;
const QStringList inputNames{"network.json", "ops.json", "telemetry.csv"};
struct HistoryError { QString code, message; };
[[noreturn]] void reject(const QString& code, const QString& message) { throw HistoryError{code, message}; }
void require(bool condition, const QString& message) { if (!condition) reject("invalid_evidence", message); }
QString now() { return QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs); }
QString sqlText(const QString& value) { return value.isNull() ? QStringLiteral("") : value; }
QByteArray json(const QJsonObject& object) { return QJsonDocument(object).toJson(QJsonDocument::Compact); }
QString sha(const QByteArray& bytes) { return QString::fromLatin1(QCryptographicHash::hash(bytes, QCryptographicHash::Sha256).toHex()); }
QJsonObject parse(const QByteArray& bytes, qint64 limit = jsonLimit) {
    try { return telemetry::decode_document(bytes, limit); }
    catch (const std::exception&) { reject("invalid_json", "Expected bounded strict UTF-8 JSON evidence."); }
}
void keys(const QJsonObject& value, const QStringList& expected) {
    const auto actual = value.keys();
    require(QSet<QString>(actual.cbegin(), actual.cend()) ==
            QSet<QString>(expected.cbegin(), expected.cend()), "Archive fields differ from this version.");
}
QString text(const QJsonObject& value, const QString& key, qsizetype maximum = 256, bool empty = false) {
    require(value.value(key).isString(), "Expected an archive text field.");
    const auto result = value.value(key).toString();
    require(result.size() <= maximum && (empty || !result.trimmed().isEmpty()), "Archive text exceeds its bound or is empty.");
    require(QString::fromUtf8(result.toUtf8()) == result, "Archive text contains invalid Unicode.");
    return result;
}
qint64 integer(const QJsonObject& value, const QString& key, qint64 maximum) {
    const auto item = value.value(key); const auto number = item.toDouble(-1);
    require(item.isDouble() && std::isfinite(number) && number >= 0 && number <= maximum && std::floor(number) == number,
            "Expected a bounded exact archive integer.");
    return static_cast<qint64>(number);
}
QDateTime date(const QJsonObject& value, const QString& key) {
    const auto raw = text(value, key, 64);
    const auto result = QDateTime::fromString(raw, Qt::ISODateWithMs);
    require(raw.size() == 24 && raw.endsWith('Z') && result.isValid() &&
            raw == result.toUTC().toString(Qt::ISODateWithMs), "Archive timestamps must use canonical UTC ISO dates with milliseconds.");
    return result;
}
void sessionId(const QString& id) { require(id.size() <= 64 && !QUuid::fromString(id).isNull(), "Session IDs must be UUIDs."); }
QJsonObject metadata(const QJsonObject& value) {
    const QSet<QString> permitted{"model_identity", "http_status", "duration_ms", "retry_count"};
    for (auto it = value.begin(); it != value.end(); ++it)
        if (!permitted.contains(it.key())) reject("invalid_metadata", "Only sanitized model/status/duration/retry metadata is permitted.");
    if (value.contains("model_identity")) (void)text(value, "model_identity");
    if (value.contains("http_status")) {
        const auto status = integer(value, "http_status", 599); require(status >= 100, "Invalid HTTP status.");
    }
    if (value.contains("duration_ms")) {
        const auto amount = value.value("duration_ms");
        require(amount.isDouble() && std::isfinite(amount.toDouble()) && amount.toDouble() >= 0 &&
                amount.toDouble() <= 9007199254740991.0, "Invalid request duration.");
    }
    if (value.contains("retry_count")) (void)integer(value, "retry_count", 1000);
    return value;
}
void inspectEvent(const QJsonValue& value, const QString& runtimeDirectory, int depth = 0) {
    require(depth <= 16, "Agent event nesting exceeds its bound.");
    if (value.isDouble()) require(std::isfinite(value.toDouble()), "Agent event numbers must be finite.");
    if (value.isString()) {
        auto string = value.toString(); require(QString::fromUtf8(string.toUtf8()) == string, "Invalid agent-event Unicode.");
        string.replace('\\', '/'); auto directory = runtimeDirectory; directory.replace('\\', '/');
        require(directory.isEmpty() || !string.contains(directory, Qt::CaseInsensitive), "Agent events must not retain runtime capture paths.");
    }
    if (value.isArray()) for (const auto& item : value.toArray()) inspectEvent(item, runtimeDirectory, depth + 1);
    if (value.isObject()) {
        const QSet<QString> forbidden{"apikey", "nvidiaapikey", "authorization", "headers", "reasoning", "reasoningcontent", "capturedirectory"};
        const auto object = value.toObject();
        for (auto it = object.begin(); it != object.end(); ++it) {
            auto normalized = it.key().toLower(); normalized.remove('_'); normalized.remove('-');
            require(!forbidden.contains(normalized), "Agent events cannot retain credentials, headers, raw reasoning or runtime paths.");
            inspectEvent(it.value(), runtimeDirectory, depth + 1);
        }
    }
}
void checkAgentEvent(const QJsonObject& event, const QMap<QString, QJsonObject>& responses,
                     const QJsonArray& previous, const QString& runtimeDirectory = {}) {
    require(json(event).size() <= EogHistoryRepository::maximumAgentEventBytes, "Agent event exceeds 64 KiB.");
    inspectEvent(event, runtimeDirectory); const auto type = text(event, "type", 32);
    QMap<QString, QString> calls; QSet<QString> results;
    for (const auto& value : previous) {
        const auto item = value.toObject().value("event").toObject();
        if (item.value("type") == "tool_call") calls[item.value("call_id").toString()] = item.value("name").toString();
        if (item.value("type") == "tool_result") results.insert(item.value("call_id").toString());
    }
    if (type == "model_request") {
        auto meta = event; meta.remove("type"); (void)metadata(meta);
        require(meta.contains("model_identity") && meta.contains("duration_ms") && meta.contains("retry_count"), "Model request metadata is incomplete.");
    } else if (type == "tool_call") {
        keys(event, {"type", "call_id", "name", "arguments"}); const auto call = text(event, "call_id"); (void)text(event, "name", 128);
        require(event.value("arguments").isObject() && !calls.contains(call), "Tool call arguments or identity are invalid.");
    } else if (type == "tool_result") {
        const QSet<QString> permitted{"type", "call_id", "name", "ok", "analysis_request_id", "summary", "output_sha256"};
        for (auto it = event.begin(); it != event.end(); ++it) require(permitted.contains(it.key()), "Unknown tool-result event field.");
        const auto call = text(event, "call_id"), name = text(event, "name", 128);
        require(event.value("ok").isBool() && calls.value(call) == name && !results.contains(call), "Tool result does not match one unfinished model-issued call.");
        if (event.contains("analysis_request_id")) require(responses.contains(text(event, "analysis_request_id")), "Tool result references no stored numerical analysis.");
        if (event.contains("summary")) (void)text(event, "summary", 4000, true);
        if (event.contains("output_sha256")) {
            const auto hash = text(event, "output_sha256", 64);
            require(hash.size() == 64 && std::all_of(hash.cbegin(), hash.cend(), [](QChar c) {
                return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f'); }), "Tool-result hash must be lowercase SHA-256.");
        }
    } else if (type == "final") {
        keys(event, {"type", "summary", "selected_plan_id", "validation_id", "event_ids"}); (void)text(event, "summary", 16000, true);
        require(event.value("selected_plan_id").isNull() || event.value("selected_plan_id").isString(), "Invalid final selected-plan reference.");
        require(event.value("validation_id").isNull() || event.value("validation_id").isString(), "Invalid final validation reference.");
        if (!event.value("selected_plan_id").isNull()) (void)text(event, "selected_plan_id");
        if (!event.value("validation_id").isNull()) {
            const auto selected = text(event, "selected_plan_id"), validationId = text(event, "validation_id"); bool found = false;
            for (const auto& response : responses) {
                const auto validation = response.value("result").toObject().value("validation").toObject();
                found |= response.value("status") == "completed" && response.value("operation") == "validate_plan" &&
                    validation.value("validation_id") == validationId && validation.value("plan_id") == selected &&
                    validation.value("independent").toBool() && validation.value("feasible").toBool() && validation.value("coverage") == "available";
            }
            require(found, "Final validation reference is not an available successful exact-plan validation.");
        }
        require(event.value("event_ids").isArray() && event.value("event_ids").toArray().size() <= 256, "Final event references exceed their bound.");
        QSet<QString> known, seen;
        for (const auto& response : responses) if (response.value("operation") == "detect" && response.value("status") == "completed")
            for (const auto& item : response.value("result").toObject().value("events").toArray()) known.insert(item.toObject().value("event_id").toString());
        for (const auto& item : event.value("event_ids").toArray()) {
            require(item.isString() && known.contains(item.toString()) && !seen.contains(item.toString()), "Final event reference is absent or repeated."); seen.insert(item.toString());
        }
    } else {
        require(type == "failure" || type == "cancel", "Unsupported agent event type."); keys(event, {"type", "code"}); (void)text(event, "code", 128);
    }
}
struct PackedFile {
    qint64 rawBytes = 0;
    QString rawHash, compressedHash, codec = "mszip-v1";
    QByteArray compressed;
};
qint64 limitFor(const QString& name) { return name == "telemetry.csv" ? csvLimit : jsonLimit; }
qint64 compressedLimit(qint64 rawLimit) { return rawLimit + 1024 * 1024; }
QByteArray compress(const QByteArray& raw) {
#ifdef Q_OS_WIN
    COMPRESSOR_HANDLE handle = nullptr;
    if (!CreateCompressor(COMPRESS_ALGORITHM_MSZIP, nullptr, &handle))
        reject("compression_unavailable", "Windows MSZIP compression could not be initialized.");
    struct Close { COMPRESSOR_HANDLE handle; ~Close() { CloseCompressor(handle); } } close{handle};
    SIZE_T required = 0;
    if (Compress(handle, raw.constData(), static_cast<SIZE_T>(raw.size()), nullptr, 0, &required) ||
        GetLastError() != ERROR_INSUFFICIENT_BUFFER || required > static_cast<SIZE_T>(compressedLimit(raw.size())))
        reject("compression_failed", "Cannot obtain bounded MSZIP output size.");
    QByteArray result(static_cast<qsizetype>(required), Qt::Uninitialized);
    SIZE_T written = 0;
    if (!Compress(handle, raw.constData(), static_cast<SIZE_T>(raw.size()), result.data(), required, &written) || written > required)
        reject("compression_failed", "Cannot compress EOG input evidence.");
    result.resize(static_cast<qsizetype>(written)); return result;
#else
    Q_UNUSED(raw);
    reject("unsupported_codec", "EOG mszip-v1 archives require the supported Windows compression API.");
#endif
}
QByteArray decompress(const PackedFile& file, qint64 maximum) {
    require(file.codec == "mszip-v1" && file.rawBytes > 0 && file.rawBytes <= maximum &&
            !file.compressed.isEmpty() && file.compressed.size() <= compressedLimit(maximum), "Unsupported or oversized compressed evidence.");
    require(sha(file.compressed) == file.compressedHash, "Compressed evidence hash differs.");
#ifdef Q_OS_WIN
    DECOMPRESSOR_HANDLE handle = nullptr;
    if (!CreateDecompressor(COMPRESS_ALGORITHM_MSZIP, nullptr, &handle))
        reject("compression_unavailable", "Windows MSZIP decompression could not be initialized.");
    struct Close { DECOMPRESSOR_HANDLE handle; ~Close() { CloseDecompressor(handle); } } close{handle};
    SIZE_T required = 0;
    if (Decompress(handle, file.compressed.constData(), static_cast<SIZE_T>(file.compressed.size()), nullptr, 0, &required) ||
        GetLastError() != ERROR_INSUFFICIENT_BUFFER || required != static_cast<SIZE_T>(file.rawBytes))
        reject("invalid_compression", "MSZIP output length differs from its bounded declared raw length.");
    // The untrusted stream size is checked before allocating. Never grow/retry a
    // destination buffer in response to decompressor output.
    QByteArray result(file.rawBytes, Qt::Uninitialized);
    SIZE_T written = 0;
    if (!Decompress(handle, file.compressed.constData(), static_cast<SIZE_T>(file.compressed.size()),
                    result.data(), static_cast<SIZE_T>(result.size()), &written) || written != static_cast<SIZE_T>(file.rawBytes))
        reject("invalid_compression", "Cannot decompress the bounded EOG input.");
    require(sha(result) == file.rawHash, "Raw evidence hash differs."); return result;
#else
    reject("unsupported_codec", "EOG mszip-v1 archives require the supported Windows compression API.");
#endif
}
struct CaptureEvidence {
    QJsonObject manifest;
    QMap<QString, PackedFile> files;
    std::shared_ptr<const Dataset> dataset;
    QJsonObject assets;
};
void writeNewArchive(const QString& destination, const QByteArray& bytes) {
    // QTemporaryFile::close keeps its native handle open, preventing a second
    // QSaveFile from replacing it on Windows. A private same-volume directory
    // provides an unclaimed temporary target without an open reservation handle.
    QTemporaryDir temporary(QDir(QFileInfo(destination).absolutePath()).filePath(".breakwater-eog-export-XXXXXX"));
    if (!temporary.isValid()) reject("archive_write", "Cannot reserve private temporary archive storage.");
    const auto temporaryPath = temporary.filePath("archive.tmp");
    QSaveFile file(temporaryPath);
    if (!file.open(QIODevice::WriteOnly) || file.write(bytes) != bytes.size() || !file.commit())
        reject("archive_write", "Cannot atomically prepare EOG evidence archive.");
#ifdef Q_OS_WIN
    if (!MoveFileExW(reinterpret_cast<LPCWSTR>(temporaryPath.utf16()), reinterpret_cast<LPCWSTR>(destination.utf16()), MOVEFILE_WRITE_THROUGH))
        reject("archive_write", "Archive destination already exists or cannot be published atomically.");
#else
    if (!QFile::rename(temporaryPath, destination)) reject("archive_write", "Archive destination already exists or cannot be published.");
#endif
}
void checkInputs(CaptureEvidence& evidence, const QMap<QString, QByteArray>& raw) {
    validate_document(evidence.manifest, "capture_manifest");
    QTemporaryDir directory; if (!directory.isValid()) reject("temporary_storage", "Cannot verify private EOG input evidence.");
    for (const auto& name : inputNames) {
        const auto info = evidence.manifest.value("captured_files").toObject().value(name).toObject();
        const auto bytes = raw.value(name);
        require(bytes.size() == info.value("size_bytes").toDouble() && sha(bytes) == info.value("sha256").toString(),
                "Captured input does not match the immutable manifest.");
        QFile file(directory.filePath(name));
        if (!file.open(QIODevice::WriteOnly) || file.write(bytes) != bytes.size())
            reject("temporary_storage", "Cannot materialize bounded EOG input verification.");
    }
    evidence.dataset = load_dataset(directory.path());
    const auto first = static_cast<qint64>(evidence.manifest.value("source_first_unix").toDouble());
    const auto sourceNow = static_cast<qint64>(evidence.manifest.value("source_now_unix").toDouble());
    const auto cutoff = static_cast<qint64>(evidence.manifest.value("cutoff_unix").toDouble());
    require(evidence.dataset->first_unix == first && evidence.dataset->now_unix <= cutoff &&
            (cutoff != sourceNow || evidence.dataset->now_unix == sourceNow), "Captured telemetry clock differs from the manifest.");
    for (const auto& asset : evidence.dataset->assets) evidence.assets[asset.id] = asset.kind;
}
CaptureEvidence readCapture(const std::shared_ptr<const Capture>& capture) {
    require(bool(capture), "An immutable capture lease is required.");
    CaptureEvidence evidence; evidence.manifest = capture->manifest();
    validate_document(evidence.manifest, "capture_manifest");
    const auto root = QFileInfo(capture->directory()).canonicalFilePath();
    require(!root.isEmpty(), "The capture directory is unavailable.");
    QMap<QString, QByteArray> raw;
    for (const auto& name : inputNames) {
        const QFileInfo before(QDir(root).filePath(name));
        require(before.isFile() && !before.isSymLink() && before.canonicalPath() == root &&
                before.size() > 0 && before.size() <= limitFor(name), "Capture input is not a bounded local child file.");
        QFile file(before.absoluteFilePath());
        if (!file.open(QIODevice::ReadOnly)) reject("capture_read", "Cannot read immutable EOG capture evidence.");
        const auto bytes = file.read(limitFor(name) + 1); const QFileInfo after(before.absoluteFilePath());
        require(bytes.size() == before.size() && file.atEnd() && after.size() == before.size() &&
                after.lastModified() == before.lastModified(), "Capture input changed during retention.");
        raw[name] = bytes;
    }
    checkInputs(evidence, raw);
    for (const auto& name : inputNames) {
        const auto bytes = raw.value(name); auto& packed = evidence.files[name];
        packed.rawBytes = bytes.size(); packed.rawHash = sha(bytes); packed.compressed = compress(bytes);
        packed.compressedHash = sha(packed.compressed);
    }
    return evidence;
}
QJsonObject restoredRequest(QJsonObject request) {
    require(!request.contains("capture_directory"), "Archived requests must not retain runtime capture paths.");
    // The shortest valid Windows placeholder cannot inflate an originally valid
    // request beyond its byte budget. It is never used for filesystem access.
    request["capture_directory"] = "C:/";
    return request;
}
void checkReferences(const QJsonArray& references, const CaptureEvidence& captured) {
    for (const auto& value : references) {
        const auto reference = value.toObject();
        if (reference.value("value").isNull()) continue;
        const auto found = captured.dataset->series.constFind(series_key(reference.value("asset_id").toString(), reference.value("metric").toString()));
        require(found != captured.dataset->series.cend(), "Evidence references an absent captured channel.");
        const auto time = static_cast<qint64>(reference.value("timestamp_unix").toDouble());
        const auto sample = std::lower_bound(found->samples.cbegin(), found->samples.cend(), time,
            [](const Sample& sample, qint64 timestamp) { return sample.timestamp < timestamp; });
        require(sample != found->samples.cend() && sample->timestamp == time && sample->value == reference.value("value").toDouble(),
                "Evidence value/time does not reference an actual captured observation.");
    }
}
void checkAnalysis(const QJsonObject& archivedRequest, const QJsonObject& response, const QJsonObject& meta,
                   const CaptureEvidence& captured) {
    const auto request = restoredRequest(archivedRequest);
    validate_document(request, "analysis_request");
    require(request.value("capture_manifest") == captured.manifest, "Analysis replaces the session's immutable capture.");
    validate_document(response, "analysis_response", {{"request", request}, {"assets", captured.assets}});
    (void)metadata(meta);
    if (response.value("status") == "completed") {
        const auto result = response.value("result").toObject();
        if (response.value("operation") == "detect") checkReferences(result.value("evidence").toArray(), captured);
        if (response.value("operation") == "validate_plan") checkReferences(result.value("validation").toObject().value("evidence").toArray(), captured);
    }
}
void execute(QSqlQuery& query) { if (!query.exec()) reject("storage_error", "The EOG history database operation failed: " + query.lastError().text()); }
void execute(QSqlDatabase& database, const QString& sql) { QSqlQuery query(database); if (!query.exec(sql)) reject("storage_error", "The EOG history database operation failed: " + query.lastError().text()); }
struct Transaction {
    QSqlDatabase& database; bool committed = false;
    explicit Transaction(QSqlDatabase& db) : database(db) { if (!database.transaction()) reject("storage_error", "Cannot begin the EOG evidence transaction."); }
    ~Transaction() { if (!committed) database.rollback(); }
    void commit() { if (!database.commit()) reject("storage_error", "Cannot commit EOG evidence."); committed = true; }
};
} // namespace

class EogHistoryWorker final : public QObject {
    Q_OBJECT
public:
    explicit EogHistoryWorker(QString path) : path_(std::move(path)) {}
    void initialize() {
        perform("initialize", {}, [&] {
            if (path_.isEmpty()) path_ = QDir(QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation)).filePath("eog-history.sqlite");
            path_ = QFileInfo(path_).absoluteFilePath();
            if (!QDir().mkpath(QFileInfo(path_).absolutePath())) reject("storage_error", "Cannot create the EOG history directory.");
            const QFileInfo databaseFile(path_);
            require(!databaseFile.isSymLink() && (!databaseFile.exists() || databaseFile.isFile()), "EOG history requires a plain database file.");
            const auto parent = QFileInfo(databaseFile.absolutePath()).canonicalFilePath();
            require(!parent.isEmpty(), "Cannot resolve the EOG history directory.");
            path_ = QDir(parent).filePath(databaseFile.fileName());
            lock_ = std::make_unique<QLockFile>(path_ + ".lock"); lock_->setStaleLockTime(0);
            if (!lock_->tryLock(0)) reject("history_in_use", "EOG history is already open in another application or repository.");
            connection_ = "eog-history-" + QUuid::createUuid().toString(QUuid::Id128);
            database_ = QSqlDatabase::addDatabase("QSQLITE", connection_); database_.setDatabaseName(path_);
            if (!database_.open()) reject("storage_error", "Cannot open EOG SQLite history.");
            {
                QSqlQuery query(database_); executeQuery(query, "PRAGMA application_id"); require(query.next(), "Cannot identify history database.");
                const auto appId = query.value(0).toInt();
                if (appId == 0) {
                    executeQuery(query, "SELECT count(*) FROM sqlite_master WHERE type='table' AND name NOT LIKE 'sqlite_%'");
                    require(query.next() && query.value(0).toInt() == 0, "This database belongs to another history format.");
                } else require(appId == applicationId, "This database belongs to another application.");
                executeQuery(query, "PRAGMA user_version"); require(query.next() && query.value(0).toInt() <= 1, "Unsupported EOG database version.");
            }
            execute(database_, "PRAGMA foreign_keys=ON"); execute(database_, "PRAGMA journal_mode=WAL");
            execute(database_, "PRAGMA synchronous=FULL"); execute(database_, "PRAGMA busy_timeout=5000");
            Transaction transaction(database_);
            execute(database_, "CREATE TABLE IF NOT EXISTS eog_captures (capture_id TEXT PRIMARY KEY, manifest BLOB NOT NULL)");
            execute(database_, "CREATE TABLE IF NOT EXISTS eog_files (capture_id TEXT NOT NULL REFERENCES eog_captures(capture_id), name TEXT NOT NULL, codec TEXT NOT NULL, raw_bytes INTEGER NOT NULL, raw_sha256 TEXT NOT NULL, compressed_sha256 TEXT NOT NULL, data BLOB NOT NULL, PRIMARY KEY(capture_id,name))");
            execute(database_, "CREATE TABLE IF NOT EXISTS eog_sessions (session_id TEXT PRIMARY KEY, capture_id TEXT NOT NULL REFERENCES eog_captures(capture_id), objective TEXT NOT NULL, status TEXT NOT NULL, created_at TEXT NOT NULL, updated_at TEXT NOT NULL)");
            execute(database_, "CREATE TABLE IF NOT EXISTS eog_analyses (session_id TEXT NOT NULL REFERENCES eog_sessions(session_id), request_id TEXT NOT NULL, request BLOB NOT NULL, response BLOB NOT NULL, metadata BLOB NOT NULL, saved_at TEXT NOT NULL, ordinal INTEGER NOT NULL, PRIMARY KEY(session_id,request_id), UNIQUE(session_id,ordinal))");
            execute(database_, "CREATE TABLE IF NOT EXISTS eog_operator_records (session_id TEXT NOT NULL REFERENCES eog_sessions(session_id), record_id TEXT NOT NULL, kind TEXT NOT NULL, request_id TEXT NOT NULL, event_id TEXT NOT NULL, note TEXT NOT NULL, created_at TEXT NOT NULL, ordinal INTEGER NOT NULL, PRIMARY KEY(session_id,record_id), UNIQUE(session_id,kind,request_id,event_id), UNIQUE(session_id,ordinal))");
            execute(database_, "CREATE TABLE IF NOT EXISTS eog_agent_events (session_id TEXT NOT NULL REFERENCES eog_sessions(session_id), sequence INTEGER NOT NULL, record BLOB NOT NULL, PRIMARY KEY(session_id,sequence))");
            execute(database_, "PRAGMA application_id=" + QString::number(applicationId)); execute(database_, "PRAGMA user_version=1");
            QSqlQuery recovery(database_); recovery.prepare("UPDATE eog_sessions SET status='interrupted', updated_at=? WHERE status='running'");
            recovery.addBindValue(now()); execute(recovery); transaction.commit(); initialized_ = true; emit ready();
        });
    }
    void close() {
        live_.clear(); database_.close(); database_ = {};
        if (!connection_.isEmpty()) QSqlDatabase::removeDatabase(connection_);
        lock_.reset(); initialized_ = false;
    }
    void begin(const QString& id, const QString& objective, const std::shared_ptr<const Capture>& capture) {
        perform("begin", id, [&] {
            available(); sessionId(id); require(objective.size() <= 4000, "Objective exceeds its bound.");
            auto evidence = readCapture(capture);
            Transaction transaction(database_); auto existing = session(id, false);
            if (!existing.isEmpty()) {
                require(live_.contains(id) && existing.value("capture_id") == evidence.manifest.value("capture_id") &&
                        existing.value("objective") == objective, "An existing historical/session ID cannot be replaced or resumed.");
                transaction.commit(); emit saved(id); return;
            }
            retainCapture(evidence);
            const auto timestamp = now(); QSqlQuery query(database_);
            query.prepare("INSERT INTO eog_sessions VALUES (?,?,?,?,?,?)");
            for (const auto& item : QVariantList{id, evidence.manifest.value("capture_id").toString(), sqlText(objective), "running", timestamp, timestamp}) query.addBindValue(item);
            execute(query); transaction.commit(); live_[id] = capture; emit saved(id);
        });
    }
    void append(const QString& id, QJsonObject request, const QJsonObject& response, const QJsonObject& meta) {
        perform("append", id, [&] {
            available(); require(live_.contains(id), "Historical or retired investigations cannot append live analysis.");
            validate_document(request, "analysis_request");
            require(QFileInfo(request.value("capture_directory").toString()).canonicalFilePath() == QFileInfo(live_.value(id)->directory()).canonicalFilePath(),
                    "Analysis capture path differs from its retained lease.");
            request.remove("capture_directory");
            Transaction transaction(database_); const auto current = session(id);
            auto captured = readStoredCapture(current.value("capture_id").toString());
            checkAnalysis(request, response, meta, captured);
            QSqlQuery query(database_); query.prepare("SELECT request,response,metadata FROM eog_analyses WHERE session_id=? AND request_id=?");
            query.addBindValue(id); query.addBindValue(request.value("request_id").toString()); execute(query);
            if (query.next()) {
                require(parse(query.value(0).toByteArray()) == request && parse(query.value(1).toByteArray()) == response &&
                        parse(query.value(2).toByteArray()) == meta, "A duplicate request cannot replace stored analysis evidence.");
                transaction.commit(); emit saved(id); return;
            }
            require(current.value("status") == "running", "Completed numerical evidence is immutable.");
            const auto count = countRows("eog_analyses", id); require(count < EogHistoryRepository::maximumAnalyses, "EOG history supports at most 12 analyses.");
            query.prepare("INSERT INTO eog_analyses VALUES (?,?,?,?,?,?,?)");
            for (const auto& item : QVariantList{id, request.value("request_id").toString(), json(request), json(response), json(meta), now(), count}) query.addBindValue(item);
            execute(query); touch(id); transaction.commit(); emit saved(id);
        });
    }
    void finish(const QString& id, const QString& status) {
        perform("finish", id, [&] {
            available(); require(live_.contains(id), "Historical or retired investigations cannot be completed as live.");
            require(status == "completed" || status == "failed" || status == "cancelled", "Unknown terminal investigation status.");
            Transaction transaction(database_); const auto current = session(id);
            require(current.value("status") == "running" || current.value("status") == status, "Completed numerical evidence is immutable.");
            if (current.value("status") != status) {
                QSqlQuery query(database_); query.prepare("UPDATE eog_sessions SET status=?,updated_at=? WHERE session_id=?");
                query.addBindValue(status); query.addBindValue(now()); query.addBindValue(id); execute(query);
            }
            transaction.commit(); emit saved(id);
        });
    }
    void appendEvent(const QString& id, const QJsonObject& event) {
        perform("agent_event", id, [&] {
            available(); require(live_.contains(id), "Historical or retired investigations cannot append a live agent trace.");
            Transaction transaction(database_); require(session(id).value("status") == "running", "Completed agent evidence is immutable.");
            const auto previous = agentRecords(id); require(previous.size() < EogHistoryRepository::maximumAgentEvents, "Agent trace exceeds 128 events.");
            const auto responses = responseIndex(id); checkAgentEvent(event, responses, previous, live_.value(id)->directory());
            const auto sequence = previous.size() + 1;
            const QJsonObject record{{"sequence", sequence}, {"recorded_at", now()}, {"event", event}};
            require(json(record).size() <= EogHistoryRepository::maximumAgentEventBytes, "Agent event exceeds 64 KiB including its archive wrapper.");
            auto combined = previous; combined.append(record);
            require(QJsonDocument(combined).toJson(QJsonDocument::Compact).size() <= EogHistoryRepository::maximumAgentTraceBytes,
                    "Complete agent trace exceeds 4 MiB; no event was truncated.");
            QSqlQuery query(database_); query.prepare("INSERT INTO eog_agent_events VALUES (?,?,?)");
            query.addBindValue(id); query.addBindValue(sequence); query.addBindValue(json(record)); execute(query);
            touch(id); transaction.commit(); emit saved(id);
        });
    }
    void retire(const QString& id) { live_.remove(id); }
    void record(const QString& id, const QString& requestId, const QString& eventId, const QString& note, bool review) {
        perform(review ? "review" : "acknowledge", id, [&] {
            available(); require(live_.contains(id), "Imported, reopened or retired evidence cannot regain operator review eligibility.");
            require(note.size() <= 4000, "Operator note exceeds its bound.");
            Transaction transaction(database_); const auto current = session(id);
            require(current.value("status") == "completed" || (!review && current.value("status") == "running"), "Operator review requires a completed current investigation.");
            QSqlQuery query(database_); query.prepare("SELECT request,response,metadata FROM eog_analyses WHERE session_id=? AND request_id=? AND length(request)<=? AND length(response)<=? AND length(metadata)<=16384");
            query.addBindValue(id); query.addBindValue(requestId); query.addBindValue(1024 * 1024); query.addBindValue(jsonLimit);
            execute(query); require(query.next(), "Operator record references no retained analysis request.");
            const auto archivedRequest = parse(query.value(0).toByteArray()), response = parse(query.value(1).toByteArray()), meta = parse(query.value(2).toByteArray());
            const auto captured = readStoredCapture(current.value("capture_id").toString());
            checkAnalysis(archivedRequest, response, meta, captured);
            require(response.value("status") == "completed", "Operator record requires successful retained analysis.");
            const auto result = response.value("result").toObject();
            if (review) {
                const auto validation = result.value("validation").toObject();
                require(response.value("operation") == "validate_plan" && validation.value("coverage") == "available" &&
                        validation.value("independent").toBool() && validation.value("feasible").toBool(),
                        "Operator review requires the successful exact-plan independent validation analysis.");
            } else {
                require(response.value("operation") == "detect", "Acknowledgement must reference detection evidence.");
                bool found = false; for (const auto& value : result.value("events").toArray()) found |= value.toObject().value("event_id") == eventId;
                require(found, "Acknowledgement references no retained detection event.");
            }
            const auto kind = review ? QStringLiteral("operator_review") : QStringLiteral("event_acknowledgement");
            query.prepare("SELECT record_id,note FROM eog_operator_records WHERE session_id=? AND kind=? AND request_id=? AND event_id=?");
            for (const auto& item : QVariantList{id, kind, requestId, sqlText(eventId)}) query.addBindValue(item); execute(query);
            if (query.next()) {
                require(query.value(1).toString() == note, "An existing operator record is immutable.");
                const auto recordId = query.value(0).toString(); transaction.commit(); emit recorded(id, recordId); return;
            }
            const auto count = countRows("eog_operator_records", id);
            require(count < EogHistoryRepository::maximumOperatorRecords, "Operator evidence exceeds 128 records.");
            const auto recordId = QUuid::createUuid().toString(QUuid::WithoutBraces);
            query.prepare("INSERT INTO eog_operator_records VALUES (?,?,?,?,?,?,?,?)");
            for (const auto& item : QVariantList{id, recordId, kind, requestId, sqlText(eventId), sqlText(note), now(), count}) query.addBindValue(item);
            execute(query); touch(id); transaction.commit(); emit recorded(id, recordId); emit saved(id);
        });
    }
    void list(int offset) {
        perform("list", {}, [&] {
            available(); require(offset >= 0 && offset <= 1000000, "History page offset exceeds its bound.");
            QSqlQuery query(database_); query.prepare("SELECT session_id,capture_id,objective,status,created_at,updated_at FROM eog_sessions ORDER BY updated_at DESC,session_id ASC LIMIT 101 OFFSET ?");
            query.addBindValue(offset); execute(query);
            QJsonArray values; while (query.next()) values.append(QJsonObject{{"session_id", query.value(0).toString()}, {"capture_id", query.value(1).toString()},
                {"objective", query.value(2).toString()}, {"status", query.value(3).toString()}, {"created_at", query.value(4).toString()},
                {"updated_at", query.value(5).toString()}, {"historical", true}, {"actionable", false}});
            const bool more = values.size() > 100; if (more) values.removeLast();
            emit listed(values, more ? offset + 100 : -1);
        });
    }
    void load(const QString& id) {
        perform("load", id, [&] { available(); live_.remove(id); Transaction transaction(database_); const auto archive = archiveFor(id); transaction.commit(); emit loaded(archive); });
    }
    void exportArchive(const QString& id, const QUrl& destination) {
        perform("export", id, [&] {
            available(); require(destination.isLocalFile(), "Choose a local archive destination.");
            const QFileInfo info(destination.toLocalFile());
            require(info.fileName().endsWith(".eog-session.json", Qt::CaseInsensitive) && !info.exists() && !info.isSymLink() &&
                    !inputNames.contains(info.fileName()), "Export requires a new .eog-session.json destination.");
            for (const auto& capture : live_) require(!QFileInfo(capture->directory()).canonicalFilePath().isEmpty() &&
                    QFileInfo(info.absolutePath()).canonicalFilePath() != QFileInfo(capture->directory()).canonicalFilePath(), "Do not export into a runtime capture directory.");
            Transaction transaction(database_); const auto archive = archiveFor(id); transaction.commit();
            const auto bytes = json(archive); require(bytes.size() <= EogHistoryRepository::maximumArchiveBytes, "Complete archive exceeds 256 MiB; no evidence was truncated.");
            writeNewArchive(info.absoluteFilePath(), bytes);
            emit exported(id, destination);
        });
    }
    void importArchive(const QUrl& source) {
        perform("import", {}, [&] {
            available(); require(source.isLocalFile(), "Choose a local EOG archive.");
            const QFileInfo info(source.toLocalFile());
            require(info.isFile() && !info.isSymLink() && info.size() > 0 && info.size() <= EogHistoryRepository::maximumArchiveBytes, "Archive is not a bounded local file.");
            QFile file(info.absoluteFilePath()); if (!file.open(QIODevice::ReadOnly)) reject("archive_read", "Cannot open EOG evidence archive.");
            const auto bytes = file.read(EogHistoryRepository::maximumArchiveBytes + 1);
            require(bytes.size() == info.size() && file.atEnd(), "Archive changed or exceeded its bound during reading.");
            auto archive = parse(bytes, EogHistoryRepository::maximumArchiveBytes);
            auto captured = validateArchive(archive); const auto id = archive.value("session_id").toString();
            if (archive.value("status") == "running") { archive["status"] = "interrupted"; archive["updated_at"] = std::max(archive.value("updated_at").toString(), now()); }
            Transaction transaction(database_); const auto previous = session(id, false);
            if (!previous.isEmpty()) {
                require(archiveFor(id) == archive, "Imported evidence conflicts with an existing immutable session.");
                transaction.commit(); live_.remove(id); emit imported(id); return;
            }
            retainCapture(captured);
            QSqlQuery query(database_); query.prepare("INSERT INTO eog_sessions VALUES (?,?,?,?,?,?)");
            for (const auto& item : QVariantList{id, captured.manifest.value("capture_id").toString(), sqlText(archive.value("objective").toString()),
                    archive.value("status").toString(), archive.value("created_at").toString(), archive.value("updated_at").toString()}) query.addBindValue(item);
            execute(query);
            int ordinal = 0;
            for (const auto& value : archive.value("analyses").toArray()) {
                const auto item = value.toObject(), request = item.value("request").toObject();
                query.prepare("INSERT INTO eog_analyses VALUES (?,?,?,?,?,?,?)");
                for (const auto& argument : QVariantList{id, request.value("request_id").toString(), json(request), json(item.value("response").toObject()),
                        json(item.value("metadata").toObject()), item.value("saved_at").toString(), ordinal++}) query.addBindValue(argument);
                execute(query);
            }
            ordinal = 0;
            for (const auto& value : archive.value("operator_records").toArray()) {
                const auto item = value.toObject(); query.prepare("INSERT INTO eog_operator_records VALUES (?,?,?,?,?,?,?,?)");
                for (const auto& argument : QVariantList{id, item.value("record_id").toString(), item.value("kind").toString(),
                        item.value("analysis_request_id").toString(), sqlText(item.value("event_id").toString()), sqlText(item.value("note").toString()),
                        item.value("created_at").toString(), ordinal++}) query.addBindValue(argument);
                execute(query);
            }
            for (const auto& value : archive.value("agent_events").toArray()) {
                const auto item = value.toObject(); query.prepare("INSERT INTO eog_agent_events VALUES (?,?,?)");
                query.addBindValue(id); query.addBindValue(item.value("sequence").toInt()); query.addBindValue(json(item)); execute(query);
            }
            transaction.commit(); live_.remove(id); emit imported(id); emit saved(id);
        });
    }
signals:
    void ready();
    void saved(const QString& id);
    void listed(const QJsonArray& sessions, int nextOffset);
    void loaded(const QJsonObject& archive);
    void exported(const QString& id, const QUrl& destination);
    void imported(const QString& id);
    void recorded(const QString& id, const QString& recordId);
    void failed(const QString& operation, const QString& id, const QString& code, const QString& message);
private:
    QString path_, connection_;
    QSqlDatabase database_;
    std::unique_ptr<QLockFile> lock_;
    QMap<QString, std::shared_ptr<const Capture>> live_;
    bool initialized_ = false;
    void available() { if (!initialized_) reject("history_unavailable", "EOG history is unavailable; numerical forecasting may continue."); }
    void perform(const QString& operation, const QString& id, const std::function<void()>& action) {
        try { action(); }
        catch (const HistoryError& error) { emit failed(operation, id, error.code, error.message.left(1000)); }
        catch (const std::exception& error) { emit failed(operation, id, "invalid_evidence", QString::fromUtf8(error.what()).left(1000)); }
    }
    static void executeQuery(QSqlQuery& query, const QString& sql) { if (!query.exec(sql)) reject("storage_error", "The EOG history database operation failed."); }
    QJsonObject session(const QString& id, bool required = true) {
        sessionId(id); QSqlQuery query(database_);
        query.prepare("SELECT capture_id,objective,status,created_at,updated_at FROM eog_sessions WHERE session_id=? AND length(CAST(objective AS BLOB))<=16000");
        query.addBindValue(id); execute(query);
        if (!query.next()) { require(!required, "EOG history session was not found."); return {}; }
        return {{"session_id", id}, {"capture_id", query.value(0).toString()}, {"objective", query.value(1).toString()},
            {"status", query.value(2).toString()}, {"created_at", query.value(3).toString()}, {"updated_at", query.value(4).toString()}};
    }
    int countRows(const QString& table, const QString& id) {
        require(table == "eog_analyses" || table == "eog_operator_records" || table == "eog_agent_events", "Unknown history table.");
        QSqlQuery query(database_); query.prepare("SELECT count(*) FROM " + table + " WHERE session_id=?");
        query.addBindValue(id); execute(query); require(query.next(), "Cannot count EOG evidence."); return query.value(0).toInt();
    }
    void touch(const QString& id) {
        QSqlQuery query(database_); query.prepare("UPDATE eog_sessions SET updated_at=? WHERE session_id=?");
        query.addBindValue(now()); query.addBindValue(id); execute(query);
    }
    QMap<QString, QJsonObject> responseIndex(const QString& id) {
        QSqlQuery query(database_); query.prepare("SELECT request_id,response FROM eog_analyses WHERE session_id=? AND length(response)<=? ORDER BY ordinal LIMIT 13");
        query.addBindValue(id); query.addBindValue(jsonLimit); execute(query); QMap<QString, QJsonObject> result;
        while (query.next()) result[query.value(0).toString()] = parse(query.value(1).toByteArray());
        require(result.size() <= EogHistoryRepository::maximumAnalyses && result.size() == countRows("eog_analyses", id), "Stored analysis index exceeds its bounds.");
        return result;
    }
    QJsonArray agentRecords(const QString& id) {
        QSqlQuery query(database_); query.prepare("SELECT record FROM eog_agent_events WHERE session_id=? AND length(record)<=? ORDER BY sequence LIMIT 129");
        query.addBindValue(id); query.addBindValue(EogHistoryRepository::maximumAgentEventBytes); execute(query); QJsonArray result;
        while (query.next()) result.append(parse(query.value(0).toByteArray(), EogHistoryRepository::maximumAgentEventBytes));
        require(result.size() <= EogHistoryRepository::maximumAgentEvents && result.size() == countRows("eog_agent_events", id) &&
                QJsonDocument(result).toJson(QJsonDocument::Compact).size() <= EogHistoryRepository::maximumAgentTraceBytes,
                "Stored agent trace exceeds its bounds.");
        return result;
    }
    void retainCapture(const CaptureEvidence& captured) {
        const auto id = captured.manifest.value("capture_id").toString(); QSqlQuery query(database_);
        query.prepare("SELECT manifest FROM eog_captures WHERE capture_id=? AND length(manifest)<=?");
        query.addBindValue(id); query.addBindValue(jsonLimit); execute(query);
        if (query.next()) {
            require(parse(query.value(0).toByteArray()) == captured.manifest, "Stored capture identity conflicts.");
            (void)readStoredCapture(id); return;
        }
        query.prepare("INSERT INTO eog_captures VALUES (?,?)"); query.addBindValue(id); query.addBindValue(json(captured.manifest)); execute(query);
        for (const auto& name : inputNames) {
            const auto packed = captured.files.value(name); query.prepare("INSERT INTO eog_files VALUES (?,?,?,?,?,?,?)");
            for (const auto& item : QVariantList{id, name, packed.codec, packed.rawBytes, packed.rawHash, packed.compressedHash, packed.compressed}) query.addBindValue(item);
            execute(query);
        }
    }
    CaptureEvidence readStoredCapture(const QString& id) {
        CaptureEvidence result; QSqlQuery query(database_);
        query.prepare("SELECT manifest FROM eog_captures WHERE capture_id=? AND length(manifest)<=?");
        query.addBindValue(id); query.addBindValue(jsonLimit); execute(query); require(query.next(), "Stored capture is absent or oversized.");
        result.manifest = parse(query.value(0).toByteArray()); QMap<QString, QByteArray> raw;
        for (const auto& name : inputNames) {
            query.prepare("SELECT codec,raw_bytes,raw_sha256,compressed_sha256,length(data) FROM eog_files WHERE capture_id=? AND name=?");
            query.addBindValue(id); query.addBindValue(name); execute(query); require(query.next(), "Stored input evidence is missing.");
            PackedFile packed; packed.codec = query.value(0).toString(); packed.rawBytes = query.value(1).toLongLong();
            packed.rawHash = query.value(2).toString(); packed.compressedHash = query.value(3).toString();
            require(query.value(4).toLongLong() > 0 && query.value(4).toLongLong() <= compressedLimit(limitFor(name)), "Stored compressed input exceeds its bound.");
            query.prepare("SELECT data FROM eog_files WHERE capture_id=? AND name=?"); query.addBindValue(id); query.addBindValue(name); execute(query); require(query.next(), "Stored input evidence is missing.");
            packed.compressed = query.value(0).toByteArray(); raw[name] = decompress(packed, limitFor(name)); result.files[name] = packed;
        }
        checkInputs(result, raw); return result;
    }
    CaptureEvidence validateArchive(const QJsonObject& archive) {
        keys(archive, {"eog_session_version", "session_id", "objective", "status", "created_at", "updated_at",
                      "capture_manifest", "captured_inputs", "analyses", "operator_records", "agent_events", "historical", "actionable"});
        require(archive.value("eog_session_version") == 1 && archive.value("historical").isBool() && archive.value("historical").toBool() &&
                archive.value("actionable").isBool() && !archive.value("actionable").toBool(), "Imported evidence cannot claim operating authority.");
        sessionId(text(archive, "session_id", 64)); (void)text(archive, "objective", 4000, true);
        const auto created = date(archive, "created_at"), updated = date(archive, "updated_at"); require(created <= updated, "Archive update precedes creation.");
        const QSet<QString> statuses{"running", "completed", "failed", "cancelled", "interrupted"};
        require(statuses.contains(text(archive, "status")), "Unknown investigation status.");
        require(archive.value("capture_manifest").isObject() && archive.value("captured_inputs").isObject() &&
                archive.value("analyses").isArray() && archive.value("operator_records").isArray() && archive.value("agent_events").isArray(), "Archive requires complete typed evidence.");
        CaptureEvidence captured; captured.manifest = archive.value("capture_manifest").toObject();
        validate_document(captured.manifest, "capture_manifest"); const auto inputs = archive.value("captured_inputs").toObject(); keys(inputs, inputNames);
        QMap<QString, QByteArray> raw;
        for (const auto& name : inputNames) {
            require(inputs.value(name).isObject(), "Compressed input must be an object."); const auto value = inputs.value(name).toObject();
            keys(value, {"codec", "raw_bytes", "raw_sha256", "compressed_sha256", "data_base64"});
            PackedFile packed; packed.codec = text(value, "codec", 32); packed.rawBytes = integer(value, "raw_bytes", limitFor(name));
            packed.rawHash = text(value, "raw_sha256", 64); packed.compressedHash = text(value, "compressed_sha256", 64);
            const auto encoded = text(value, "data_base64", 4 * ((compressedLimit(limitFor(name)) + 2) / 3));
            packed.compressed = QByteArray::fromBase64(encoded.toLatin1());
            require(QString::fromLatin1(packed.compressed.toBase64()) == encoded, "Input base64 must be canonical.");
            raw[name] = decompress(packed, limitFor(name)); captured.files[name] = packed;
        }
        checkInputs(captured, raw);
        const auto analyses = archive.value("analyses").toArray(), records = archive.value("operator_records").toArray();
        require(analyses.size() <= EogHistoryRepository::maximumAnalyses && records.size() <= EogHistoryRepository::maximumOperatorRecords, "Archive evidence count exceeds its bound.");
        QMap<QString, QJsonObject> responses;
        for (const auto& value : analyses) {
            require(value.isObject(), "Analysis record must be an object."); const auto item = value.toObject();
            keys(item, {"request", "response", "metadata", "saved_at"});
            require(item.value("request").isObject() && item.value("response").isObject() && item.value("metadata").isObject(), "Analysis envelope is incomplete.");
            const auto request = item.value("request").toObject(), response = item.value("response").toObject();
            checkAnalysis(request, response, item.value("metadata").toObject(), captured);
            const auto requestId = request.value("request_id").toString(); require(!responses.contains(requestId), "Duplicate archived analysis request.");
            const auto timestamp = date(item, "saved_at"); require(created <= timestamp && timestamp <= updated, "Analysis timestamp is outside the session.");
            responses[requestId] = response;
        }
        QSet<QString> recordIds, identities;
        for (const auto& value : records) {
            require(value.isObject(), "Operator record must be an object."); const auto item = value.toObject();
            keys(item, {"record_id", "kind", "analysis_request_id", "event_id", "note", "created_at"});
            const auto id = text(item, "record_id", 64), kind = text(item, "kind", 32), requestId = text(item, "analysis_request_id");
            sessionId(id); const auto eventId = text(item, "event_id", 256, true); (void)text(item, "note", 4000, true);
            require(!recordIds.contains(id) && responses.contains(requestId), "Operator record identity/reference is invalid."); recordIds.insert(id);
            const auto identity = kind + ':' + QString::number(requestId.size()) + ':' + requestId + ':' + eventId;
            require(!identities.contains(identity), "Duplicate operator record."); identities.insert(identity);
            const auto response = responses.value(requestId), result = response.value("result").toObject();
            require(response.value("status") == "completed", "Operator record references unsuccessful analysis.");
            if (kind == "operator_review") {
                const auto validation = result.value("validation").toObject();
                require(eventId.isEmpty() && response.value("operation") == "validate_plan" && validation.value("feasible").toBool() &&
                        validation.value("independent").toBool() && validation.value("coverage") == "available", "Review does not reference successful exact-plan validation.");
            } else {
                require(kind == "event_acknowledgement" && response.value("operation") == "detect", "Unsupported operator record kind.");
                bool found = false; for (const auto& event : result.value("events").toArray()) found |= event.toObject().value("event_id") == eventId;
                require(found, "Acknowledgement references no retained event.");
            }
            const auto timestamp = date(item, "created_at"); require(created <= timestamp && timestamp <= updated, "Operator record timestamp is outside the session.");
        }
        const auto events = archive.value("agent_events").toArray();
        require(events.size() <= EogHistoryRepository::maximumAgentEvents &&
                QJsonDocument(events).toJson(QJsonDocument::Compact).size() <= EogHistoryRepository::maximumAgentTraceBytes, "Agent trace exceeds its archive bounds.");
        QJsonArray previous; auto previousTime = created;
        for (const auto& value : events) {
            require(value.isObject(), "Agent record must be an object."); const auto record = value.toObject();
            keys(record, {"sequence", "recorded_at", "event"});
            require(integer(record, "sequence", EogHistoryRepository::maximumAgentEvents) == previous.size() + 1 && record.value("event").isObject() &&
                    json(record).size() <= EogHistoryRepository::maximumAgentEventBytes, "Agent trace sequence/type/size is invalid.");
            const auto timestamp = date(record, "recorded_at");
            require(previousTime <= timestamp && timestamp <= updated, "Agent trace timestamps are reversed or outside the session.");
            checkAgentEvent(record.value("event").toObject(), responses, previous);
            previous.append(record); previousTime = timestamp;
        }
        return captured;
    }
    QJsonObject archiveFor(const QString& id) {
        const auto current = session(id); auto captured = readStoredCapture(current.value("capture_id").toString());
        QJsonObject inputs;
        for (const auto& name : inputNames) {
            const auto packed = captured.files.value(name);
            inputs[name] = QJsonObject{{"codec", packed.codec}, {"raw_bytes", packed.rawBytes}, {"raw_sha256", packed.rawHash},
                {"compressed_sha256", packed.compressedHash}, {"data_base64", QString::fromLatin1(packed.compressed.toBase64())}};
        }
        QJsonArray analyses, records; QSqlQuery query(database_);
        query.prepare("SELECT request,response,metadata,saved_at FROM eog_analyses WHERE session_id=? AND length(request)<=? AND length(response)<=? AND length(metadata)<=? ORDER BY ordinal");
        for (const auto& item : QVariantList{id, 1024 * 1024, jsonLimit, 16384}) query.addBindValue(item); execute(query);
        while (query.next()) analyses.append(QJsonObject{{"request", parse(query.value(0).toByteArray())}, {"response", parse(query.value(1).toByteArray())},
            {"metadata", parse(query.value(2).toByteArray())}, {"saved_at", query.value(3).toString()}});
        require(analyses.size() == countRows("eog_analyses", id), "Stored analysis exceeds its byte bounds.");
        query.prepare("SELECT record_id,kind,request_id,event_id,note,created_at FROM eog_operator_records WHERE session_id=? AND length(CAST(note AS BLOB))<=16000 ORDER BY ordinal");
        query.addBindValue(id); execute(query);
        while (query.next()) records.append(QJsonObject{{"record_id", query.value(0).toString()}, {"kind", query.value(1).toString()},
            {"analysis_request_id", query.value(2).toString()}, {"event_id", query.value(3).toString()}, {"note", query.value(4).toString()}, {"created_at", query.value(5).toString()}});
        require(records.size() == countRows("eog_operator_records", id), "Stored operator evidence exceeds its bound.");
        QJsonObject archive{{"eog_session_version", 1}, {"session_id", id}, {"objective", current.value("objective")}, {"status", current.value("status")},
            {"created_at", current.value("created_at")}, {"updated_at", current.value("updated_at")}, {"capture_manifest", captured.manifest},
            {"captured_inputs", inputs}, {"analyses", analyses}, {"operator_records", records}, {"agent_events", agentRecords(id)}, {"historical", true}, {"actionable", false}};
        (void)validateArchive(archive);
        require(json(archive).size() <= EogHistoryRepository::maximumArchiveBytes, "Complete archive exceeds 256 MiB; no evidence was truncated.");
        return archive;
    }
};

EogHistoryRepository::EogHistoryRepository(QString path, QObject* parent) : QObject(parent) {
    worker_ = new EogHistoryWorker(std::move(path)); worker_->moveToThread(&thread_);
    connect(&thread_, &QThread::started, worker_, &EogHistoryWorker::initialize);
    connect(&thread_, &QThread::finished, worker_, &QObject::deleteLater);
    connect(worker_, &EogHistoryWorker::ready, this, &EogHistoryRepository::ready);
    connect(worker_, &EogHistoryWorker::saved, this, &EogHistoryRepository::sessionSaved);
    connect(worker_, &EogHistoryWorker::listed, this, &EogHistoryRepository::sessionsListed);
    connect(worker_, &EogHistoryWorker::loaded, this, &EogHistoryRepository::sessionLoaded);
    connect(worker_, &EogHistoryWorker::exported, this, &EogHistoryRepository::archiveExported);
    connect(worker_, &EogHistoryWorker::imported, this, &EogHistoryRepository::archiveImported);
    connect(worker_, &EogHistoryWorker::recorded, this, &EogHistoryRepository::operatorRecordSaved);
    connect(worker_, &EogHistoryWorker::failed, this, &EogHistoryRepository::failed);
    thread_.start();
}
EogHistoryRepository::~EogHistoryRepository() {
    QMetaObject::invokeMethod(worker_, [worker = worker_] { worker->close(); }, Qt::BlockingQueuedConnection);
    thread_.quit(); thread_.wait();
}
void EogHistoryRepository::beginSession(const QString& id, const QString& objective, std::shared_ptr<const Capture> capture) {
    QMetaObject::invokeMethod(worker_, [worker = worker_, id, objective, capture = std::move(capture)] { worker->begin(id, objective, capture); }, Qt::QueuedConnection);
}
void EogHistoryRepository::appendAnalysis(const QString& id, const QJsonObject& request, const QJsonObject& response, const QJsonObject& metadata) {
    QMetaObject::invokeMethod(worker_, [worker = worker_, id, request, response, metadata] { worker->append(id, request, response, metadata); }, Qt::QueuedConnection);
}
void EogHistoryRepository::appendAgentEvent(const QString& id, const QJsonObject& event) {
    QMetaObject::invokeMethod(worker_, [worker = worker_, id, event] { worker->appendEvent(id, event); }, Qt::QueuedConnection);
}
void EogHistoryRepository::recordOperatorReview(const QString& id, const QString& requestId, const QString& note) {
    QMetaObject::invokeMethod(worker_, [worker = worker_, id, requestId, note] { worker->record(id, requestId, {}, note, true); }, Qt::QueuedConnection);
}
void EogHistoryRepository::acknowledgeEvent(const QString& id, const QString& requestId, const QString& eventId, const QString& note) {
    QMetaObject::invokeMethod(worker_, [worker = worker_, id, requestId, eventId, note] { worker->record(id, requestId, eventId, note, false); }, Qt::QueuedConnection);
}
void EogHistoryRepository::finishSession(const QString& id, const QString& status) {
    QMetaObject::invokeMethod(worker_, [worker = worker_, id, status] { worker->finish(id, status); }, Qt::QueuedConnection);
}
void EogHistoryRepository::retireSession(const QString& id) { QMetaObject::invokeMethod(worker_, [worker = worker_, id] { worker->retire(id); }, Qt::QueuedConnection); }
void EogHistoryRepository::listSessions(int offset) { QMetaObject::invokeMethod(worker_, [worker = worker_, offset] { worker->list(offset); }, Qt::QueuedConnection); }
void EogHistoryRepository::loadSession(const QString& id) { QMetaObject::invokeMethod(worker_, [worker = worker_, id] { worker->load(id); }, Qt::QueuedConnection); }
void EogHistoryRepository::exportSession(const QString& id, const QUrl& destination) { QMetaObject::invokeMethod(worker_, [worker = worker_, id, destination] { worker->exportArchive(id, destination); }, Qt::QueuedConnection); }
void EogHistoryRepository::importSession(const QUrl& source) { QMetaObject::invokeMethod(worker_, [worker = worker_, source] { worker->importArchive(source); }, Qt::QueuedConnection); }
} // namespace breakwater::desktop::eog
#include "EogHistoryRepository.moc"
