#include "PythonWorkerClient.hpp"

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
    const auto installedPath = QDir::cleanPath(QDir(QCoreApplication::applicationDirPath()).filePath("../share/breakwater/science/worker.py"));
    if (QFileInfo(installedPath).isFile()) return installedPath;
#ifdef BREAKWATER_WORKER_PATH
    const auto developmentPath = QString::fromUtf8(BREAKWATER_WORKER_PATH);
    if (QFileInfo(developmentPath).isFile()) return developmentPath;
#endif
    return installedPath;
}

bool healthResult(const QJsonObject& input, QJsonObject& result) {
    const auto version = input.value("python_version");
    const auto methods = input.value("methods");
    static const QRegularExpression versionPattern(QStringLiteral("^[0-9]+\\.[0-9]+\\.[0-9]+[A-Za-z0-9.\\-]*$"));
    if (!version.isString() || version.toString().size() > 64 || !versionPattern.match(version.toString()).hasMatch() ||
        !methods.isArray() || methods.toArray() != QJsonArray{QStringLiteral("health")} ||
        !input.value("torch_detected").isBool() || !input.value("physicsnemo_detected").isBool() ||
        input.value("scientific_execution").toString() != "not_implemented") return false;
    result = {{"python_version", version}, {"methods", methods}, {"torch_detected", input.value("torch_detected")},
              {"physicsnemo_detected", input.value("physicsnemo_detected")}, {"scientific_execution", "not_implemented"}};
    return true;
}
} // namespace

PythonWorkerClient::PythonWorkerClient(QString pythonExecutable, QString workerPath, QObject* parent)
    : QObject(parent), python_executable_(pythonExecutable.isEmpty() ? qEnvironmentVariable("BREAKWATER_PYTHON").trimmed() : std::move(pythonExecutable)),
      worker_path_(workerPath.isEmpty() ? defaultWorkerPath() : std::move(workerPath)) {
    worker_path_ = QFileInfo(worker_path_).absoluteFilePath();
    if ((python_executable_.contains('/') || python_executable_.contains('\\')) && QDir::isRelativePath(python_executable_))
        python_executable_ = QFileInfo(python_executable_).absoluteFilePath();
    deadline_.setParent(this);
    deadline_.setSingleShot(true);
    connect(&deadline_, &QTimer::timeout, this, [this] {
        fail("worker_timeout", "The scientific worker health check exceeded its time limit.");
    });
}

PythonWorkerClient::~PythonWorkerClient() {
    busy_ = false;
    ++generation_;
    deadline_.stop();
    // QProcess's destructor may wait. Hand the killed process its own asynchronous
    // cleanup instead of blocking the UI while a child process is still running.
    if (process_) retire(process_.data(), true);
}

void PythonWorkerClient::setTimeoutMs(int milliseconds) { timeout_ms_ = std::clamp(milliseconds, 1, 60000); }
void PythonWorkerClient::setTerminationGraceMs(int milliseconds) { termination_grace_ms_ = std::clamp(milliseconds, 0, 1000); }

bool PythonWorkerClient::current(QProcess* process, quint64 generation) const {
    return busy_ && process_ == process && generation_ == generation;
}

void PythonWorkerClient::checkHealth() {
    if (busy_) return;
    ++generation_;
    busy_ = true;
    stdout_buffer_.clear(); pending_health_ = {};
    stdout_bytes_ = stderr_bytes_ = 0;
    request_id_ = QUuid::createUuid().toString(QUuid::WithoutBraces);
    readiness_ = {{"worker_available", false}, {"health_status", "checking"}, {"scientific_execution", "not_implemented"}};
    if (QThread::currentThread() != thread()) { fail("worker_thread_invalid", "Worker health must be requested on its owning thread."); return; }
    if (python_executable_.trimmed().isEmpty()) { fail("python_unavailable", "BREAKWATER_PYTHON is not configured; scientific execution is unavailable."); return; }
    if (!QFileInfo(worker_path_).isFile()) { fail("worker_unavailable", "The configured scientific worker file is unavailable."); return; }
    auto* process = new QProcess(this);
    process_ = process;
    process->setProcessChannelMode(QProcess::SeparateChannels);
    process->setWorkingDirectory(QFileInfo(worker_path_).absolutePath());
    auto environment = QProcessEnvironment::systemEnvironment();
    environment.remove("NVIDIA_API_KEY"); // Health/package discovery has no use for model credentials.
    process->setProcessEnvironment(environment);
    const auto generation = generation_;
    connect(process, &QProcess::started, this, [this, process, generation] {
        if (!current(process, generation)) return;
        const auto request = QJsonDocument(QJsonObject{{"protocol_version", 1}, {"request_id", request_id_},
            {"method", "health"}, {"payload", QJsonObject{}}}).toJson(QJsonDocument::Compact) + '\n';
        if (process->write(request) != request.size()) { fail("worker_write_failed", "Could not send the scientific worker health request."); return; }
        // This health-only adapter makes each interpreter disposable and catches a
        // worker that crashes or writes protocol noise after its response.
        process->closeWriteChannel();
    });
    connect(process, &QProcess::readyReadStandardOutput, this, [this, process, generation] { readStdout(process, generation); });
    connect(process, &QProcess::readyReadStandardError, this, [this, process, generation] { readStderr(process, generation); });
    connect(process, &QProcess::errorOccurred, this, [this, process, generation](QProcess::ProcessError error) {
        if (!current(process, generation)) return;
        if (error == QProcess::FailedToStart) fail("worker_start_failed", "The configured Python interpreter could not start.");
        else if (error == QProcess::Crashed) fail("worker_crashed", "The scientific worker process crashed.");
        else fail("worker_io_failed", "Communication with the scientific worker failed.");
    });
    connect(process, &QProcess::finished, this, [this, process, generation](int code, QProcess::ExitStatus status) {
        processFinished(process, generation, code, status);
    });
    status_ = "Checking scientific worker health...";
    deadline_.start(timeout_ms_);
    const QPointer<PythonWorkerClient> guard(this);
    emit changed();
    if (!guard || !current(process, generation)) return;
    process->start(python_executable_, {QStringLiteral("-u"), worker_path_});
}

void PythonWorkerClient::readStdout(QProcess* process, quint64 generation) {
    if (!current(process, generation)) return;
    process->setReadChannel(QProcess::StandardOutput);
    const auto bytes = process->read(maximumLineBytes - stdout_bytes_ + 1);
    stdout_bytes_ += bytes.size();
    if (stdout_bytes_ > maximumLineBytes) { fail("worker_stdout_limit", "The scientific worker exceeded its 1 MiB response limit."); return; }
    stdout_buffer_.append(bytes);
    if (!pending_health_.isEmpty()) {
        if (!stdout_buffer_.trimmed().isEmpty()) { fail("worker_protocol_invalid", "The scientific worker returned unexpected extra output."); return; }
        stdout_buffer_.clear();
        return;
    }
    const auto newline = stdout_buffer_.indexOf('\n');
    if (newline < 0) return;
    auto line = stdout_buffer_.left(newline);
    stdout_buffer_.remove(0, newline + 1);
    if (line.endsWith('\r')) line.chop(1);
    QJsonParseError error;
    const auto document = QJsonDocument::fromJson(line, &error);
    if (error.error != QJsonParseError::NoError || !document.isObject()) { fail("worker_protocol_invalid", "The scientific worker returned malformed JSON."); return; }
    const auto response = document.object();
    if (!response.value("protocol_version").isDouble() || response.value("protocol_version").toDouble() != 1.0 ||
        !response.value("request_id").isString() || response.value("request_id").toString() != request_id_) {
        fail("worker_response_mismatch", "The scientific worker response does not match this protocol and request."); return;
    }
    if (response.value("status").toString() == "error" && response.value("error").isObject()) {
        fail("worker_reported_error", "The scientific worker did not complete its health check."); return;
    }
    QJsonObject result;
    if (response.value("status").toString() != "completed" || !response.value("result").isObject() ||
        !healthResult(response.value("result").toObject(), result)) {
        fail("worker_health_invalid", "The scientific worker returned an unsupported health response."); return;
    }
    pending_health_ = {{"protocol_version", 1}, {"request_id", request_id_}, {"status", "completed"}, {"result", result}};
    if (!stdout_buffer_.trimmed().isEmpty()) { fail("worker_protocol_invalid", "The scientific worker returned unexpected extra output."); return; }
    stdout_buffer_.clear();
}

void PythonWorkerClient::readStderr(QProcess* process, quint64 generation) {
    if (!current(process, generation)) return;
    process->setReadChannel(QProcess::StandardError);
    // Drain a bounded amount and retain only its count. Diagnostics never contain
    // child-provided text, which could include credentials or private environment data.
    const auto bytes = process->read(maximumStderrBytes - stderr_bytes_ + 1);
    stderr_bytes_ += bytes.size();
    if (stderr_bytes_ > maximumStderrBytes)
        fail("worker_stderr_limit", "The scientific worker exceeded its diagnostic output limit.");
}

void PythonWorkerClient::processFinished(QProcess* process, quint64 generation, int exitCode, QProcess::ExitStatus exitStatus) {
    if (!current(process, generation)) return;
    const QPointer<PythonWorkerClient> guard(this);
    readStdout(process, generation);
    if (!guard || !current(process, generation)) return;
    readStderr(process, generation);
    if (!guard || !current(process, generation)) return;
    if (exitStatus != QProcess::NormalExit || exitCode != 0) { fail("worker_crashed", "The scientific worker exited unsuccessfully."); return; }
    if (pending_health_.isEmpty() || !stdout_buffer_.trimmed().isEmpty()) {
        fail("worker_protocol_invalid", "The scientific worker ended without a complete health response line."); return;
    }
    complete();
}

void PythonWorkerClient::retire(QProcess* process, bool immediateKill) {
    if (!process) return;
    if (process_ == process) process_.clear();
    disconnect(process, nullptr, this, nullptr);
    process->setParent(nullptr);
    connect(process, &QProcess::finished, process, &QObject::deleteLater);
    connect(process, &QProcess::errorOccurred, process, [process](QProcess::ProcessError error) {
        if (error == QProcess::FailedToStart) process->deleteLater();
    });
    if (process->state() == QProcess::NotRunning) { process->deleteLater(); return; }
    connect(process, &QProcess::started, process, [process] { process->kill(); });
    if (immediateKill) process->kill();
    else {
        process->terminate();
        const QPointer<QProcess> guard(process);
        QTimer::singleShot(termination_grace_ms_, process, [guard] {
            if (guard && guard->state() != QProcess::NotRunning) guard->kill();
        });
    }
}

void PythonWorkerClient::complete() {
    const auto response = pending_health_;
    busy_ = false;
    const auto generation = ++generation_;
    deadline_.stop();
    if (process_) retire(process_.data());
    pending_health_ = {}; stdout_buffer_.clear();
    readiness_ = response.value("result").toObject().toVariantMap();
    readiness_["worker_available"] = true;
    readiness_["health_status"] = "completed";
    status_ = "Worker health checked. Scientific execution is not implemented.";
    const QPointer<PythonWorkerClient> guard(this);
    emit changed();
    if (guard && generation == generation_ && !busy_) emit healthReady(response);
}

void PythonWorkerClient::fail(const QString& code, const QString& message) {
    if (!busy_) return;
    busy_ = false;
    const auto generation = ++generation_;
    deadline_.stop();
    if (process_) retire(process_.data());
    pending_health_ = {}; stdout_buffer_.clear();
    readiness_ = {{"worker_available", false}, {"health_status", code == "cancelled" ? "cancelled" :
        ((code == "python_unavailable" || code == "worker_unavailable") ? "unavailable" : "failed")},
        {"scientific_execution", "not_implemented"}, {"error_code", code}};
    status_ = message;
    const QPointer<PythonWorkerClient> guard(this);
    emit changed();
    if (guard && generation == generation_ && !busy_) emit failed(code, message);
}

void PythonWorkerClient::cancel() {
    if (busy_) fail("cancelled", "The scientific worker health check was cancelled.");
}

} // namespace breakwater::desktop::science
