#include "EogAnalysisClient.hpp"
#include "EogContracts.hpp"
#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QFutureWatcher>
#include <QJsonArray>
#include <QJsonDocument>
#include <QProcessEnvironment>
#include <QStandardPaths>
#include <QThread>
#include <QtConcurrentRun>
#include <algorithm>
#include <stdexcept>
#include <utility>
#ifdef Q_OS_WIN
#include <qt_windows.h>
#endif

namespace breakwater::desktop::eog {
namespace {
constexpr qint64 maximumStdoutBytes = 4 * 1024 * 1024;
constexpr qint64 maximumStderrBytes = 64 * 1024;
struct TaskResult { QByteArray line; QJsonObject response, assets; QString error; };
struct TaskFailure { QString code; };
bool samePath(const QString& a, const QString& b) {
#ifdef Q_OS_WIN
    return a.compare(b, Qt::CaseInsensitive) == 0;
#else
    return a == b;
#endif
}
bool absoluteLocal(const QString& path) {
    if (path.isEmpty() || path.startsWith("//") || path.startsWith("\\\\") || path.contains(QChar::Null)) return false;
#ifdef Q_OS_WIN
    const auto drive = path[0].unicode();
    return path.size() >= 3 && ((drive >= 'A' && drive <= 'Z') || (drive >= 'a' && drive <= 'z')) &&
        path[1] == ':' && (path[2] == '/' || path[2] == '\\');
#else
    return QDir::isAbsolutePath(path);
#endif
}
void checkCancellation(const std::shared_ptr<std::atomic_bool>& token) {
    if (token->load(std::memory_order_relaxed)) throw TaskFailure{"cancelled"};
}
bool plainPath(const QString& path) {
    if (!absoluteLocal(path)) return false;
    for (auto cursor = QDir::cleanPath(path); !cursor.isEmpty();) {
        const QFileInfo info(cursor);
        if (!info.exists() || info.isSymLink()) return false;
#ifdef Q_OS_WIN
        const auto attrs = GetFileAttributesW(reinterpret_cast<LPCWSTR>(cursor.utf16()));
        if (attrs == INVALID_FILE_ATTRIBUTES || (attrs & FILE_ATTRIBUTE_REPARSE_POINT)) return false;
#endif
        const auto parent = info.absolutePath();
        if (samePath(parent, cursor)) break;
        cursor = parent;
    }
    return true;
}
QString defaultWorker() {
    const auto configured = qEnvironmentVariable("BREAKWATER_EOG_WORKER").trimmed();
    if (!configured.isEmpty()) return configured;
    const auto installed = QDir::cleanPath(QDir(QCoreApplication::applicationDirPath()).filePath("../share/breakwater/science/eog_worker.py"));
    if (QFileInfo(installed).isFile()) return installed;
#if defined(BREAKWATER_EOG_WORKER_PATH) && defined(BREAKWATER_DEVELOPMENT_BIN_DIR)
    const auto actual = QFileInfo(QCoreApplication::applicationDirPath()).canonicalFilePath();
    const auto development = QFileInfo(QString::fromUtf8(BREAKWATER_DEVELOPMENT_BIN_DIR)).canonicalFilePath();
    if (!development.isEmpty() && samePath(actual, development))
        return QString::fromUtf8(BREAKWATER_EOG_WORKER_PATH);
#endif
    return installed;
}
QString interpreter(QString value) {
    if (value.isEmpty()) value = qEnvironmentVariable("BREAKWATER_PYTHON").trimmed();
    if (!value.isEmpty() && !value.contains('/') && !value.contains('\\')) value = QStandardPaths::findExecutable(value);
    return value;
}
QJsonObject verifyCapture(const std::shared_ptr<const Capture>& capture,
                          const std::shared_ptr<std::atomic_bool>& token) {
    if (!capture || !plainPath(capture->directory())) throw TaskFailure{"capture_changed"};
    validate_document(capture->manifest(), "capture_manifest");
    const auto metadata = capture->manifest().value("captured_files").toObject();
    QByteArray network;
    for (const auto* name : {"network.json", "ops.json", "telemetry.csv"}) {
        checkCancellation(token);
        const auto path = QDir(capture->directory()).filePath(name); const QFileInfo before(path);
        const auto expected = metadata.value(QLatin1String(name)).toObject();
        const auto expectedSize = expected.value("size_bytes").toInteger();
        if (!plainPath(path) || !before.isFile() || !samePath(before.canonicalFilePath(), QFileInfo(path).absoluteFilePath()) ||
            expectedSize <= 0 || before.size() != expectedSize) throw TaskFailure{"capture_changed"};
        QFile file(path); if (!file.open(QIODevice::ReadOnly)) throw TaskFailure{"capture_changed"};
        QCryptographicHash hash(QCryptographicHash::Sha256); qint64 total = 0;
        while (!file.atEnd()) {
            checkCancellation(token); const auto chunk = file.read(64 * 1024);
            if (chunk.isEmpty() || file.error() != QFileDevice::NoError) throw TaskFailure{"capture_changed"};
            total += chunk.size(); if (total > expectedSize) throw TaskFailure{"capture_changed"};
            hash.addData(chunk); if (QString::fromLatin1(name) == "network.json") network.append(chunk);
        }
        const QFileInfo after(path);
        if (total != expectedSize || hash.result().toHex() != expected.value("sha256").toString().toLatin1() ||
            after.size() != before.size() || after.lastModified() != before.lastModified() ||
            !plainPath(path)) throw TaskFailure{"capture_changed"};
    }
    const auto root = decode_document(network);
    QJsonObject assets;
    for (const auto& value : root.value("systems").toArray()) {
        const auto system = value.toObject(); const auto disposal = system.value("disposal_site").toObject();
        assets[disposal.value("id").toString()] = "disposal";
        for (const auto& entry : system.value("facilities").toArray()) {
            const auto facility = entry.toObject(); assets[facility.value("id").toString()] = "facility";
            for (const auto& well : facility.value("wells").toArray()) assets[well.toObject().value("id").toString()] = "well";
            for (const auto* kind : {"tank", "pump", "pipeline"})
                assets[facility.value(QLatin1String(kind)).toObject().value("id").toString()] = QString::fromLatin1(kind);
        }
    }
    return assets;
}
TaskResult prepare(const QJsonObject& request, const std::shared_ptr<const Capture>& capture,
                   const std::shared_ptr<std::atomic_bool>& token) {
    TaskResult result;
    try {
        checkCancellation(token);
        validate_document(request, "analysis_request");
        if (!capture || request.value("capture_manifest").toObject() != capture->manifest() ||
            !samePath(QDir::cleanPath(request.value("capture_directory").toString()), QDir::cleanPath(capture->directory())))
            throw TaskFailure{"worker_request_mismatch"};
        result.assets = verifyCapture(capture, token);
        result.line = QJsonDocument(request).toJson(QJsonDocument::Compact) + '\n';
        if (result.line.size() > static_cast<qsizetype>(breakwater::eog::maximum_request_bytes))
            throw TaskFailure{"worker_request_limit"};
    } catch (const TaskFailure& failure) { result.error = failure.code; }
      catch (const std::exception&) { result.error = "worker_request_invalid"; }
      catch (...) { result.error = "worker_request_invalid"; }
    return result;
}
TaskResult validateResponse(QByteArray line, const QJsonObject& request, const QJsonObject& assets,
                            const std::shared_ptr<const Capture>& capture,
                            const std::shared_ptr<std::atomic_bool>& token) {
    TaskResult result;
    try {
        checkCancellation(token);
        if (line.isEmpty() || !line.endsWith('\n') || line.indexOf('\n') != line.size() - 1)
            throw TaskFailure{"worker_protocol_invalid"};
        line.chop(1); if (line.endsWith('\r')) line.chop(1);
        result.response = decode_document(line);
        validate_document(result.response, "analysis_response", {{"request", request}, {"assets", assets}});
        verifyCapture(capture, token);
        if (result.response.value("status") == "completed" &&
            (result.response.value("operation") == "detect" || result.response.value("operation") == "validate_plan")) {
            const auto captured = load_dataset(capture->directory(), token);
            const auto files = capture->manifest().value("captured_files").toObject();
            for (const auto* name : {"network.json", "ops.json", "telemetry.csv"})
                if (captured->source_hashes.value(QLatin1String(name)) != files.value(QLatin1String(name)).toObject().value("sha256"))
                    throw TaskFailure{"capture_changed"};
            validate_source_evidence(result.response, *captured, token);
        }
    } catch (const TaskFailure& failure) { result.error = failure.code; }
      catch (const std::exception&) { result.error = "worker_response_invalid"; }
      catch (...) { result.error = "worker_response_invalid"; }
    return result;
}
QString publicMessage(const QString& code) {
    if (code == "capture_changed") return "The private EOG capture changed or is no longer available.";
    if (code == "cancelled") return "The EOG analysis request was cancelled.";
    if (code == "worker_timeout") return "The EOG analysis request exceeded its deadline.";
    if (code == "worker_request_mismatch") return "The EOG request does not bind its supplied private capture.";
    if (code == "worker_request_limit") return "The EOG request exceeds its byte budget.";
    if (code == "worker_stdout_limit") return "The EOG provider exceeded its response byte budget.";
    if (code == "worker_stderr_limit") return "The EOG provider exceeded its diagnostic byte budget.";
    if (code == "worker_protocol_invalid") return "The EOG provider returned extra or incomplete protocol output.";
    if (code == "worker_response_invalid") return "The EOG response failed contract or request-binding checks.";
    if (code == "worker_crashed" || code == "worker_exit_failed") return "The EOG provider did not exit successfully.";
    if (code == "worker_unavailable") return "Configure a local EOG interpreter and worker before requesting analysis.";
    if (code == "worker_start_failed") return "The configured EOG interpreter could not start.";
    if (code == "worker_io_failed") return "Communication with the EOG provider failed.";
    return "The EOG request failed contract or input checks.";
}
} // namespace

struct EogAnalysisClient::Job {
    QJsonObject request, assets;
    std::shared_ptr<const Capture> capture;
    std::shared_ptr<std::atomic_bool> cancellation = std::make_shared<std::atomic_bool>(false);
    QByteArray line;
    QElapsedTimer elapsed;
};
EogAnalysisClient::EogAnalysisClient(QString pythonExecutable, QString workerPath, QStringList workerArguments, QObject* parent)
    : QObject(parent), python_executable_(interpreter(std::move(pythonExecutable))),
      worker_path_(workerPath.isEmpty() ? defaultWorker() : std::move(workerPath)),
      worker_arguments_(std::move(workerArguments)) {
    deadline_.setParent(this); deadline_.setSingleShot(true);
    connect(&deadline_, &QTimer::timeout, this, [this] { fail("worker_timeout", publicMessage("worker_timeout")); });
}
EogAnalysisClient::~EogAnalysisClient() {
    busy_ = false; ++generation_; deadline_.stop();
    if (job_) job_->cancellation->store(true);
    if (process_) retire(process_);
    if (retiring_) { disconnect(retiring_, nullptr, this, nullptr); retiring_->kill(); }
    job_.reset();
}
bool EogAnalysisClient::configured() const {
    return absoluteLocal(python_executable_) && QFileInfo(python_executable_).isFile() &&
        absoluteLocal(worker_path_) && QFileInfo(worker_path_).isFile();
}
bool EogAnalysisClient::current(QProcess* process, quint64 generation) const {
    return busy_ && process_ == process && generation_ == generation;
}
bool EogAnalysisClient::start(const QJsonObject& request, std::shared_ptr<const Capture> capture, int timeoutMs) {
    if (QThread::currentThread() != thread() || busy_) return false;
    busy_ = true; execution_verified_ = false; prepared_ = false; ++generation_;
    stdout_buffer_.clear(); stdout_bytes_ = stderr_bytes_ = 0;
    job_ = std::make_shared<Job>(); job_->request = request; job_->capture = std::move(capture); job_->elapsed.start();
    if (!configured()) { fail("worker_unavailable", publicMessage("worker_unavailable")); return false; }
    status_ = "Checking the private EOG capture and request...";
    deadline_.start(std::clamp(timeoutMs, 1, 120000));
    const auto generation = generation_; const auto job = job_;
    auto* watcher = new QFutureWatcher<TaskResult>(this);
    connect(watcher, &QFutureWatcher<TaskResult>::finished, this, [this, watcher, generation, job] {
        const auto result = watcher->result(); watcher->deleteLater();
        if (!busy_ || generation != generation_ || job_ != job) return;
        if (!result.error.isEmpty()) { fail(result.error, publicMessage(result.error)); return; }
        job->line = result.line; job->assets = result.assets; prepared_ = true;
        status_ = "Running one captured EOG analysis request...";
        const QPointer<EogAnalysisClient> guard(this); emit changed();
        if (guard && busy_ && generation == generation_ && !retiring_) launch();
    });
    watcher->setFuture(QtConcurrent::run([job] { return prepare(job->request, job->capture, job->cancellation); }));
    emit changed(); return true;
}
void EogAnalysisClient::launch() {
    if (!busy_ || !prepared_ || process_ || retiring_ || !job_) return;
    auto* process = new QProcess(this); process_ = process; const auto generation = generation_;
    // Lifetime follows process destruction too, including cancellation/reaping.
    connect(process, &QObject::destroyed, [capture = job_->capture] {});
    process->setProcessChannelMode(QProcess::SeparateChannels);
    process->setWorkingDirectory(QFileInfo(worker_path_).absolutePath());
    QProcessEnvironment environment;
    for (const auto* name : {"SystemRoot", "WINDIR", "PATH", "PATHEXT", "TEMP", "TMP", "USERPROFILE", "LOCALAPPDATA"}) {
        const auto value = qEnvironmentVariable(name);
        if (!value.isEmpty()) environment.insert(QString::fromLatin1(name), value);
    }
    environment.insert("PYTHONUNBUFFERED", "1"); environment.insert("PYTHONDONTWRITEBYTECODE", "1");
    environment.insert("PYTHONNOUSERSITE", "1");
    process->setProcessEnvironment(environment);
    connect(process, &QProcess::started, this, [this, process, generation] {
        if (!current(process, generation)) return;
        if (process->write(job_->line) != job_->line.size()) { fail("worker_io_failed", publicMessage("worker_io_failed")); return; }
        process->closeWriteChannel();
    });
    connect(process, &QProcess::readyReadStandardOutput, this, [this, process, generation] { readStdout(process, generation); });
    connect(process, &QProcess::readyReadStandardError, this, [this, process, generation] { readStderr(process, generation); });
    connect(process, &QProcess::errorOccurred, this, [this, process, generation](QProcess::ProcessError error) {
        if (!current(process, generation)) return;
        const auto code = error == QProcess::FailedToStart ? "worker_start_failed" :
            error == QProcess::Crashed ? "worker_crashed" : "worker_io_failed";
        fail(code, publicMessage(code));
    });
    connect(process, &QProcess::finished, this, [this, process, generation](int code, QProcess::ExitStatus status) {
        finished(process, generation, code, status);
    });
    auto arguments = QStringList{"-B", "-u", worker_path_}; arguments.append(worker_arguments_);
    process->start(python_executable_, arguments);
}
void EogAnalysisClient::readStdout(QProcess* process, quint64 generation) {
    if (!current(process, generation)) return;
    process->setReadChannel(QProcess::StandardOutput);
    const auto bytes = process->read(maximumStdoutBytes - stdout_bytes_ + 1);
    stdout_bytes_ += bytes.size();
    if (stdout_bytes_ > maximumStdoutBytes) { fail("worker_stdout_limit", publicMessage("worker_stdout_limit")); return; }
    stdout_buffer_.append(bytes);
    const auto newline = stdout_buffer_.indexOf('\n');
    if (newline >= 0 && newline != stdout_buffer_.size() - 1) { fail("worker_protocol_invalid", publicMessage("worker_protocol_invalid")); return; }
    if (newline < 0 && stdout_bytes_ >= maximumStdoutBytes) fail("worker_stdout_limit", publicMessage("worker_stdout_limit"));
}
void EogAnalysisClient::readStderr(QProcess* process, quint64 generation) {
    if (!current(process, generation)) return;
    process->setReadChannel(QProcess::StandardError);
    stderr_bytes_ += process->read(maximumStderrBytes - stderr_bytes_ + 1).size();
    if (stderr_bytes_ > maximumStderrBytes) fail("worker_stderr_limit", publicMessage("worker_stderr_limit"));
}
void EogAnalysisClient::finished(QProcess* process, quint64 generation, int exitCode, QProcess::ExitStatus exitStatus) {
    if (!current(process, generation)) return;
    const QPointer<EogAnalysisClient> guard(this);
    readStdout(process, generation); if (!guard || !current(process, generation)) return;
    readStderr(process, generation); if (!guard || !current(process, generation)) return;
    if (exitStatus != QProcess::NormalExit || exitCode != 0) {
        const auto code = exitStatus == QProcess::CrashExit ? "worker_crashed" : "worker_exit_failed";
        fail(code, publicMessage(code)); return;
    }
    if (stdout_buffer_.isEmpty() || !stdout_buffer_.endsWith('\n')) {
        fail("worker_protocol_invalid", publicMessage("worker_protocol_invalid")); return;
    }
    process_.clear(); disconnect(process, nullptr, this, nullptr); process->deleteLater();
    status_ = "Checking EOG response bindings and unchanged capture...";
    const auto job = job_; auto bytes = std::move(stdout_buffer_);
    auto* watcher = new QFutureWatcher<TaskResult>(this);
    connect(watcher, &QFutureWatcher<TaskResult>::finished, this, [this, watcher, generation, job] {
        const auto result = watcher->result(); watcher->deleteLater();
        if (!busy_ || generation != generation_ || job_ != job) return;
        if (!result.error.isEmpty()) { fail(result.error, publicMessage(result.error)); return; }
        complete(result.response);
    });
    watcher->setFuture(QtConcurrent::run([bytes = std::move(bytes), job] {
        return validateResponse(bytes, job->request, job->assets, job->capture, job->cancellation);
    }));
    emit changed();
}
void EogAnalysisClient::retire(QProcess* process) {
    if (!process) return;
    if (process_ == process) process_.clear();
    disconnect(process, nullptr, this, nullptr); process->setParent(nullptr);
    process->closeReadChannel(QProcess::StandardOutput); process->closeReadChannel(QProcess::StandardError);
    connect(process, &QProcess::finished, process, &QObject::deleteLater);
    connect(process, &QProcess::errorOccurred, process, [process](QProcess::ProcessError error) {
        if (error == QProcess::FailedToStart) process->deleteLater();
    });
    if (process->state() == QProcess::NotRunning) { process->deleteLater(); return; }
    retiring_ = process;
    const auto reaped = [this, process] {
        if (retiring_ == process) retiring_.clear();
        if (busy_ && prepared_ && !process_) launch();
    };
    connect(process, &QProcess::finished, this, reaped);
    connect(process, &QObject::destroyed, this, reaped);
    connect(process, &QProcess::started, process, [process] { process->kill(); });
    process->kill();
}
void EogAnalysisClient::fail(const QString& code, const QString& message) {
    if (!busy_) return;
    busy_ = false; execution_verified_ = false; prepared_ = false; const auto generation = ++generation_;
    deadline_.stop(); if (job_) job_->cancellation->store(true);
    if (process_) retire(process_);
    job_.reset(); stdout_buffer_.clear(); status_ = message;
    const QPointer<EogAnalysisClient> guard(this); emit changed();
    if (guard && generation == generation_ && !busy_) emit failed(code, message);
}
void EogAnalysisClient::complete(const QJsonObject& response) {
    const QJsonObject metadata{{"method", job_->request.value("operation")}, {"status", response.value("status")},
        {"duration_ms", double(job_->elapsed.elapsed())}, {"retry_count", 0}, {"process_exit_code", 0}};
    busy_ = false; prepared_ = false; execution_verified_ = response.value("status") == "completed";
    const auto generation = ++generation_; deadline_.stop(); job_.reset(); stdout_buffer_.clear();
    status_ = execution_verified_ ? "EOG execution passed protocol checks; numerical qualification remains separate."
                                 : "The EOG provider completed an exchange but reported the operation unavailable or unsuccessful.";
    const QPointer<EogAnalysisClient> guard(this); emit changed();
    if (guard && generation == generation_ && !busy_) emit completed(response, metadata);
}
void EogAnalysisClient::cancel() { if (busy_) fail("cancelled", publicMessage("cancelled")); }
} // namespace breakwater::desktop::eog
