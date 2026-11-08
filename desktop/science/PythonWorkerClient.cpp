#include "PythonWorkerClient.hpp"
#include "WorkerProtocol.hpp"

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonParseError>
#include <QProcessEnvironment>
#include <QRegularExpression>
#include <QThread>
#include <QUuid>
#include <algorithm>
#include <utility>

namespace breakwater::desktop::science {
namespace {
QString defaultWorkerPath() {
    const auto installed = QDir::cleanPath(QDir(QCoreApplication::applicationDirPath()).filePath("../share/breakwater/science/worker.py"));
    if (QFileInfo(installed).isFile()) return installed;
#if defined(BREAKWATER_WORKER_PATH) && defined(BREAKWATER_DEVELOPMENT_BIN_DIR)
    const auto actual = QFileInfo(QCoreApplication::applicationDirPath()).canonicalFilePath();
    const auto development = QFileInfo(QString::fromUtf8(BREAKWATER_DEVELOPMENT_BIN_DIR)).canonicalFilePath();
#ifdef Q_OS_WIN
    const bool inDevelopment = !development.isEmpty() && actual.compare(development, Qt::CaseInsensitive) == 0;
#else
    const bool inDevelopment = !development.isEmpty() && actual == development;
#endif
    const auto source = QString::fromUtf8(BREAKWATER_WORKER_PATH);
    if (inDevelopment && QFileInfo(source).isFile()) return source;
#endif
    return installed;
}
bool healthResult(const QJsonObject& input, QJsonObject& result) {
    const auto version = input.value("python_version"), methods = input.value("methods");
    static const QRegularExpression pattern(QStringLiteral("^[0-9]+\\.[0-9]+\\.[0-9]+[A-Za-z0-9.\\-]*$"));
    if (!version.isString() || version.toString().size() > 64 || !pattern.match(version.toString()).hasMatch() ||
        !methods.isArray() || methods.toArray() != QJsonArray{QStringLiteral("health")} ||
        !input.value("torch_detected").isBool() || !input.value("physicsnemo_detected").isBool() ||
        input.value("scientific_execution").toString() != "not_implemented") return false;
    result = {{"python_version", version}, {"methods", methods}, {"torch_detected", input.value("torch_detected")},
        {"physicsnemo_detected", input.value("physicsnemo_detected")}, {"scientific_execution", "not_implemented"}};
    return true;
}
bool exactKeys(const QJsonObject& object, const QStringList& keys) {
    if (object.size() != keys.size()) return false;
    for (const auto& key : keys) if (!object.contains(key)) return false;
    return true;
}
} // namespace

PythonWorkerClient::PythonWorkerClient(QString pythonExecutable, QString workerPath, QObject* parent)
    : QObject(parent), python_executable_(pythonExecutable.isEmpty() ? qEnvironmentVariable("BREAKWATER_PYTHON").trimmed() : std::move(pythonExecutable)),
      worker_path_(workerPath.isEmpty() ? defaultWorkerPath() : std::move(workerPath)) {
    worker_path_ = QFileInfo(worker_path_).absoluteFilePath();
    if ((python_executable_.contains('/') || python_executable_.contains('\\')) && QDir::isRelativePath(python_executable_))
        python_executable_ = QFileInfo(python_executable_).absoluteFilePath();
    deadline_.setParent(this); deadline_.setSingleShot(true);
    connect(&deadline_, &QTimer::timeout, this, [this] { fail("worker_timeout", "The scientific worker job exceeded its time limit."); });
}
PythonWorkerClient::~PythonWorkerClient() {
    busy_ = false; ++generation_; deadline_.stop();
    if (process_) retire(process_.data(), true);
    if (retiring_) { disconnect(retiring_, nullptr, this, nullptr); retiring_->kill(); }
}
void PythonWorkerClient::setTimeoutMs(int ms) { timeout_ms_ = std::clamp(ms, 1, 60000); }
void PythonWorkerClient::setTerminationGraceMs(int ms) { termination_grace_ms_ = std::clamp(ms, 0, 1000); }
QString PythonWorkerClient::operation() const {
    if (job_ == Job::Health) return QStringLiteral("health");
    return job_ == Job::Capabilities ? QStringLiteral("capabilities") : QStringLiteral("storage_rollout");
}
bool PythonWorkerClient::current(QProcess* process, quint64 generation) const {
    return busy_ && process_ == process && generation_ == generation;
}
void PythonWorkerClient::checkHealth() { begin(Job::Health); }
void PythonWorkerClient::checkCapabilities() { begin(Job::Capabilities); }
void PythonWorkerClient::runStorage(const QJsonObject& payload) { begin(Job::Storage, payload); }

void PythonWorkerClient::begin(Job job, const QJsonObject& payload) {
    if (busy_) return;
    job_ = job; stage_ = job == Job::Health ? Stage::Health : Stage::Capabilities;
    ++generation_; busy_ = true;
    stdout_buffer_.clear(); pending_response_ = {}; capabilities_ = {}; storage_payload_ = payload;
    stdout_bytes_ = stderr_bytes_ = 0; request_id_.clear();
    readiness_ = {{"worker_available", false}, {"health_status", job == Job::Health ? "checking" : "unchecked"},
        {"capability_status", job == Job::Health ? "unchecked" : "checking"}, {"scientific_execution", "not_implemented"},
        {"storage_implemented", false}, {"storage_execution_verified", false}};
    if (QThread::currentThread() != thread()) { fail("worker_thread_invalid", "Worker jobs must be requested on their owning thread."); return; }
    if (python_executable_.trimmed().isEmpty()) { fail("python_unavailable", "BREAKWATER_PYTHON is not configured; scientific execution is unavailable."); return; }
    if (!QFileInfo(worker_path_).isFile()) { fail("worker_unavailable", "The configured scientific worker file is unavailable."); return; }
    if (job == Job::Storage && !protocol::storagePayload(payload)) { fail("worker_request_invalid", "A valid captured storage request is required; derivatives need an explicit interior selection."); return; }
    status_ = job == Job::Health ? "Checking scientific worker health..." : "Negotiating scientific worker capabilities...";
    deadline_.start(timeout_ms_ ? timeout_ms_ : (job == Job::Health ? 10000 : 60000));
    const auto generation = generation_; const QPointer<PythonWorkerClient> guard(this);
    emit changed();
    if (guard && busy_ && generation_ == generation && !retiring_) launch();
}

void PythonWorkerClient::launch() {
    if (!busy_ || process_ || retiring_) return;
    auto* process = new QProcess(this); process_ = process;
    process->setProcessChannelMode(QProcess::SeparateChannels);
    process->setWorkingDirectory(QFileInfo(worker_path_).absolutePath());
    auto environment = QProcessEnvironment::systemEnvironment(); environment.remove("NVIDIA_API_KEY");
    process->setProcessEnvironment(environment);
    const auto generation = generation_;
    connect(process, &QProcess::started, this, [this, process, generation] {
        if (!current(process, generation)) return;
        send(job_ == Job::Health ? "health" : "capabilities", job_ == Job::Health ? QJsonObject{} : QJsonObject{{"capability_version", 1}});
    });
    connect(process, &QProcess::readyReadStandardOutput, this, [this, process, generation] { readStdout(process, generation); });
    connect(process, &QProcess::readyReadStandardError, this, [this, process, generation] { readStderr(process, generation); });
    connect(process, &QProcess::errorOccurred, this, [this, process, generation](QProcess::ProcessError error) {
        if (!current(process, generation)) return;
        if (error == QProcess::FailedToStart) fail("worker_start_failed", "The configured Python interpreter could not start.");
        else if (error == QProcess::Crashed) fail("worker_crashed", "The scientific worker process crashed.");
        else fail("worker_io_failed", "Communication with the scientific worker failed.");
    });
    connect(process, &QProcess::finished, this, [this, process, generation](int code, QProcess::ExitStatus status) { processFinished(process, generation, code, status); });
    process->start(python_executable_, {QStringLiteral("-u"), worker_path_});
}

void PythonWorkerClient::send(const QString& method, const QJsonObject& payload) {
    if (!busy_ || !process_) return;
    request_id_ = QUuid::createUuid().toString(QUuid::WithoutBraces);
    const auto bytes = QJsonDocument(QJsonObject{{"protocol_version", 1}, {"request_id", request_id_}, {"method", method}, {"payload", payload}}).toJson(QJsonDocument::Compact) + '\n';
    const auto limit = capabilities_.isEmpty() ? maximumLineBytes : capabilities_.value("max_line_bytes").toInteger();
    if (bytes.size() > limit) { fail("worker_request_limit", "The scientific request exceeds the negotiated frame limit."); return; }
    if (process_->state() != QProcess::Running || process_->write(bytes) != bytes.size()) {
        fail("worker_write_failed", "Could not send the scientific worker request."); return;
    }
    if (job_ == Job::Health || job_ == Job::Capabilities || stage_ == Stage::VerifiedCapabilities) process_->closeWriteChannel();
}

void PythonWorkerClient::readStdout(QProcess* process, quint64 generation) {
    if (!current(process, generation)) return;
    process->setReadChannel(QProcess::StandardOutput);
    const qint64 maximum = job_ == Job::Storage ? maximumJobBytes : maximumLineBytes;
    const auto bytes = process->read(maximum - stdout_bytes_ + 1); stdout_bytes_ += bytes.size();
    if (stdout_bytes_ > maximum) { fail("worker_stdout_limit", "The scientific worker exceeded its output byte budget."); return; }
    stdout_buffer_.append(bytes);
    const QPointer<PythonWorkerClient> guard(this);
    while (current(process, generation)) {
        if (stage_ == Stage::Done) {
            if (!(job_ == Job::Health ? stdout_buffer_.trimmed().isEmpty() : stdout_buffer_.isEmpty()))
                fail("worker_protocol_invalid", "The scientific worker returned unexpected extra output.");
            if (guard && current(process, generation)) stdout_buffer_.clear();
            return;
        }
        const auto newline = stdout_buffer_.indexOf('\n');
        if ((newline < 0 && stdout_buffer_.size() >= maximumLineBytes) || newline + 1 > maximumLineBytes) {
            fail("worker_stdout_limit", "The scientific worker exceeded its 1 MiB frame limit."); return;
        }
        if (newline < 0) return;
        auto line = stdout_buffer_.left(newline); stdout_buffer_.remove(0, newline + 1);
        if (line.endsWith('\r')) line.chop(1);
        acceptFrame(line);
        if (!guard) return;
    }
}

void PythonWorkerClient::acceptFrame(const QByteArray& line) {
    QJsonParseError error; const auto document = QJsonDocument::fromJson(line, &error);
    if (QString::fromUtf8(line).toUtf8() != line || error.error != QJsonParseError::NoError || !document.isObject()) {
        fail("worker_protocol_invalid", "The scientific worker returned malformed UTF-8 JSON."); return;
    }
    const auto response = document.object();
    if (!response.value("protocol_version").isDouble() || response.value("protocol_version").toDouble() != 1.0 ||
        !response.value("request_id").isString() || response.value("request_id").toString() != request_id_) {
        fail("worker_response_mismatch", "The scientific worker response does not match this protocol and request."); return;
    }
    if (response.value("status").toString() == "error" && response.value("error").isObject()) {
        const auto problem = response.value("error").toObject(); const auto code = problem.value("code").toString();
        if (job_ != Job::Health && exactKeys(response, {"protocol_version", "request_id", "status", "error"}) &&
            exactKeys(problem, {"code", "message"}) && problem.value("message").isString() &&
            problem.value("message").toString().toUcs4().size() <= 2000 && protocol::publicErrorCode(code))
            fail(code, protocol::publicErrorMessage(code));
        else fail("worker_reported_error", "The scientific worker did not complete its requested operation.");
        return;
    }
    if (response.value("status").toString() != "completed" || !response.value("result").isObject()) {
        fail(job_ == Job::Health ? "worker_health_invalid" : "worker_protocol_invalid", "The scientific worker returned an unsupported response."); return;
    }
    auto result = response.value("result").toObject();
    if (stage_ == Stage::Health) {
        QJsonObject clean;
        if (!healthResult(result, clean)) { fail("worker_health_invalid", "The scientific worker returned an unsupported health response."); return; }
        pending_response_ = {{"protocol_version", 1}, {"request_id", request_id_}, {"status", "completed"}, {"result", clean}};
        stage_ = Stage::Done; return;
    }
    if (!exactKeys(response, {"protocol_version", "request_id", "status", "result"})) { fail("worker_protocol_invalid", "The scientific worker returned unsupported response fields."); return; }
    if (stage_ == Stage::Storage) {
        if (!protocol::storage(result, storage_payload_)) { fail("worker_storage_invalid", "The scientific worker returned inconsistent storage evidence."); return; }
        pending_response_ = response; stage_ = Stage::VerifiedCapabilities;
        send("capabilities", {{"capability_version", 1}}); return;
    }
    if (!protocol::capabilities(result)) { fail("worker_capabilities_invalid", "The scientific worker returned unsupported capabilities."); return; }
    const auto storage = protocol::storageCapability(result);
    if (stage_ == Stage::VerifiedCapabilities) {
        auto prior = capabilities_, after = result; prior.remove("methods"); after.remove("methods");
        if (prior != after || !storage.value("implemented").toBool() || !storage.value("execution_verified").toBool()) {
            fail("worker_capabilities_invalid", "The worker did not verify storage execution in this job process."); return;
        }
        for (const auto& value : capabilities_.value("methods").toArray()) {
            auto expected = value.toObject(); const auto name = expected.value("name").toString();
            if (name == "storage_rollout") expected["execution_verified"] = true;
            bool found = false;
            for (const auto& currentMethod : result.value("methods").toArray())
                if (currentMethod.toObject() == expected) { found = true; break; }
            if (!found) { fail("worker_capabilities_invalid", "The worker changed method versions or support during execution."); return; }
        }
        capabilities_ = result; stage_ = Stage::Done; return;
    }
    if (storage.value("execution_verified").toBool()) { fail("worker_capabilities_invalid", "The fresh worker reported an unexpected execution state."); return; }
    capabilities_ = result;
    if (job_ == Job::Capabilities) { pending_response_ = response; stage_ = Stage::Done; return; }
    if (!storage.value("implemented").toBool()) { fail("runtime_unavailable", "Storage rollout modules are not deployed with this worker."); return; }
    if (storage.value("method_version").toInt() < storage_payload_.value("method_version").toInt()) {
        fail("unsupported_version", "This worker does not support production-feed splits. Select an updated scientific runtime."); return;
    }
    if (storage_payload_.value("preview").toObject().value("max_times").toInt() > result.value("max_preview_times").toInt()) {
        fail("worker_capabilities_invalid", "The worker cannot supply the requested preview budget."); return;
    }
    stage_ = Stage::Storage; send("storage_rollout", storage_payload_);
}

void PythonWorkerClient::readStderr(QProcess* process, quint64 generation) {
    if (!current(process, generation)) return;
    process->setReadChannel(QProcess::StandardError);
    const auto bytes = process->read(maximumStderrBytes - stderr_bytes_ + 1); stderr_bytes_ += bytes.size();
    if (stderr_bytes_ > maximumStderrBytes) fail("worker_stderr_limit", "The scientific worker exceeded its diagnostic output limit.");
}
void PythonWorkerClient::processFinished(QProcess* process, quint64 generation, int exitCode, QProcess::ExitStatus exitStatus) {
    if (!current(process, generation)) return;
    if (exitStatus != QProcess::NormalExit || exitCode != 0) { fail("worker_crashed", "The scientific worker exited unsuccessfully."); return; }
    const QPointer<PythonWorkerClient> guard(this);
    readStdout(process, generation); if (!guard || !current(process, generation)) return;
    readStderr(process, generation); if (!guard || !current(process, generation)) return;
    if (stage_ != Stage::Done || pending_response_.isEmpty() || !stdout_buffer_.isEmpty()) {
        fail("worker_protocol_invalid", "The scientific worker ended without its complete matching response exchange."); return;
    }
    complete();
}

void PythonWorkerClient::retire(QProcess* process, bool immediateKill) {
    if (!process) return;
    if (process_ == process) process_.clear();
    disconnect(process, nullptr, this, nullptr); process->setParent(nullptr);
    process->closeReadChannel(QProcess::StandardOutput); process->closeReadChannel(QProcess::StandardError);
    connect(process, &QProcess::finished, process, &QObject::deleteLater);
    connect(process, &QProcess::errorOccurred, process, [process](QProcess::ProcessError error) { if (error == QProcess::FailedToStart) process->deleteLater(); });
    if (process->state() == QProcess::NotRunning) { process->deleteLater(); return; }
    retiring_ = process;
    connect(process, &QObject::destroyed, this, [this, process] {
        if (retiring_ == process) retiring_.clear();
        if (busy_ && !process_) launch();
    });
    connect(process, &QProcess::finished, this, [this, process] {
        if (retiring_ == process) retiring_.clear();
        if (busy_ && !process_) launch();
    });
    connect(process, &QProcess::started, process, [process] { process->kill(); });
    if (immediateKill) process->kill();
    else {
        process->terminate(); const QPointer<QProcess> guard(process);
        QTimer::singleShot(termination_grace_ms_, process, [guard] { if (guard && guard->state() != QProcess::NotRunning) guard->kill(); });
    }
}
void PythonWorkerClient::updateCapabilities(const QJsonObject& result) {
    readiness_ = result.toVariantMap(); const auto storage = protocol::storageCapability(result);
    readiness_["worker_available"] = true; readiness_["capability_status"] = "completed";
    readiness_["health_status"] = "unchecked";
    readiness_["storage_implemented"] = storage.value("implemented").toBool();
    readiness_["storage_execution_verified"] = storage.value("execution_verified").toBool();
    readiness_["scientific_execution"] = storage.value("execution_verified").toBool() ? "completed_job_process" : "not_executed";
}
void PythonWorkerClient::complete() {
    const auto response = pending_response_, capabilities = capabilities_; const auto job = job_;
    busy_ = false; const auto generation = ++generation_; deadline_.stop();
    if (process_) retire(process_.data());
    pending_response_ = {}; storage_payload_ = {}; stdout_buffer_.clear();
    if (job == Job::Health) {
        readiness_ = response.value("result").toObject().toVariantMap(); readiness_["worker_available"] = true;
        readiness_["health_status"] = "completed"; readiness_["storage_implemented"] = false; readiness_["storage_execution_verified"] = false;
        status_ = "Worker health checked. Scientific execution is not established by health.";
    } else {
        updateCapabilities(capabilities);
        status_ = job == Job::Storage ? "Storage execution checked in this completed worker process. Native approval is separate."
            : "Capabilities checked. Deployed modules do not establish scientific execution.";
    }
    const QPointer<PythonWorkerClient> guard(this); emit changed();
    if (!guard || generation != generation_ || busy_) return;
    if (job == Job::Health) emit healthReady(response);
    else if (job == Job::Capabilities) emit capabilitiesReady(response);
    else emit storageReady(response);
}
void PythonWorkerClient::fail(const QString& code, const QString& message) {
    if (!busy_) return;
    busy_ = false; const auto generation = ++generation_; deadline_.stop();
    if (process_) retire(process_.data());
    pending_response_ = {}; storage_payload_ = {}; stdout_buffer_.clear();
    if (job_ != Job::Health && !capabilities_.isEmpty()) {
        auto clean = capabilities_; auto methods = clean.value("methods").toArray();
        for (qsizetype i = 0; i < methods.size(); ++i) {
            auto method = methods[i].toObject();
            if (method.value("name").toString() == "storage_rollout") { method["execution_verified"] = false; methods[i] = method; }
        }
        clean["methods"] = methods; updateCapabilities(clean);
    }
    else readiness_ = {{"worker_available", false}, {"scientific_execution", "not_implemented"}, {"storage_implemented", false}};
    readiness_["storage_execution_verified"] = false;
    readiness_["health_status"] = code == "cancelled" ? "cancelled" :
        ((code == "python_unavailable" || code == "worker_unavailable") ? "unavailable" : "failed");
    readiness_["error_code"] = code; status_ = message;
    const QPointer<PythonWorkerClient> guard(this); emit changed();
    if (guard && generation == generation_ && !busy_) emit failed(code, message);
}
void PythonWorkerClient::cancel() { if (busy_) fail("cancelled", "The scientific worker job was cancelled."); }

} // namespace breakwater::desktop::science
