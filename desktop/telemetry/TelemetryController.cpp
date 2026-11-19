#include "TelemetryController.hpp"
#include "TelemetryContracts.hpp"
#include "LocalRuntime.hpp"
#include "TelemetryStream.hpp"
#include "ReceiverJournal.hpp"
#include <QDateTime>
#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QHash>
#include <QMap>
#include <QSet>
#include <QCryptographicHash>
#include <QProcess>
#include <QProcessEnvironment>
#include <QSaveFile>
#include <QTemporaryFile>
#include <QUuid>
#include <algorithm>
#include <cmath>
#include <memory>
#include <stdexcept>

namespace breakwater::desktop::telemetry {
namespace {
constexpr qsizetype frameLimit = 256 * 1024;
constexpr qsizetype recordingLimit = 32 * 1024 * 1024;
constexpr qsizetype backlogLimit = 4 * 1024 * 1024;
QString freshId(const QString& prefix) { return prefix + QUuid::createUuid().toString(QUuid::WithoutBraces); }
QByteArray bytes(const QJsonObject& object) { return QJsonDocument(object).toJson(QJsonDocument::Compact) + '\n'; }
}

class TelemetryIoWorker final : public QObject {
    Q_OBJECT
public:
    explicit TelemetryIoWorker(std::shared_ptr<std::atomic<quint64>> sourceSerial) : source_serial_(std::move(sourceSerial)) {
        tick_ = new QTimer(this);
        tick_->setInterval(20);
        connect(tick_, &QTimer::timeout, this, [this] { tick(); });
    }
    void initialize() { monotonic_.start(); tick_->start(); }
    void shutdown() {
        shutting_down_ = true;
        tick_->stop();
        shutdownInput();
        if (retiring_.isEmpty()) emit shutdownReady();
    }
    void setTimeout(int milliseconds) { timeout_ms_ = std::clamp(milliseconds, 50, 30000); }
    void manual() { shutdownInput(); mode_ = "manual"; status_ = configuration_.isEmpty() ? "unconfigured" : "ready"; error_.clear(); }
    void configure(QJsonObject configuration, QJsonObject context) {
        try {
            if (configuration.isEmpty() || configuration.value("sensors").toArray().isEmpty()) {
                shutdownInput(); configuration_ = {}; context_ = {}; configuration_hash_.clear(); stream_ = TelemetryStream{};
                mode_ = "manual"; status_ = "unconfigured"; error_.clear(); return;
            }
            context["configuration"] = configuration;
            TelemetryStream candidate;
            candidate.configure(configuration, context);
            const auto identity = sensor_semantic_hash(configuration);
            if (identity == configuration_hash_) {
                configuration_ = configuration; context_ = context; return;
            }
            shutdownInput();
            configuration_ = configuration; context_ = context; configuration_hash_ = identity;
            stream_ = std::move(candidate); mode_ = "manual"; status_ = "ready"; error_.clear();
        } catch (const std::exception& exception) {
            fail("configuration_invalid", QString::fromUtf8(exception.what()));
        }
    }
    void startProcess(const QString& executable, const QStringList& arguments) {
        if (configuration_.isEmpty()) { reject("configuration_missing", "Place and configure sensors before starting local input."); return; }
        const QFileInfo program(executable);
        if (!program.isAbsolute() || !program.isFile()) { reject("publisher_missing", "Choose an existing absolute local publisher executable."); return; }
        if (arguments.size() > 32 || std::any_of(arguments.cbegin(), arguments.cend(), [](const QString& value) { return value.size() > 4096; })) {
            reject("publisher_arguments", "Publisher arguments exceed the local launch limits."); return;
        }
        if (retiring_.size() >= 4) { reject("publisher_retiring", "Wait for the previous local publisher to finish terminating."); return; }
        shutdownInput();
        error_.clear(); mode_ = "synthetic_live"; status_ = "starting";
        journal_.reset(configuration_hash_,configuration_.value("physical_hash").toString());
        journal_started_ = false; recording_format_ = "receiver_journal_v1";
        stream_.configure(configuration_, context_); stream_.beginEpoch(mode_);
        auto* configurationFile = new QTemporaryFile(this);
        const auto configurationBytes = bytes(configuration_);
        if (!configurationFile->open() || configurationFile->write(configurationBytes) != configurationBytes.size() || !configurationFile->flush()) {
            configurationFile->deleteLater(); fail("configuration_file", "Cannot create the local publisher configuration."); return;
        }
        configuration_file_ = configurationFile;
        auto* contextFile = new QTemporaryFile(this);
        // Only model inputs reach the independent generator, never hidden truth,
        // operating permissions, credentials or mutable editor/layout objects.
        const auto contextBytes = bytes({{"network",context_.value("network")},{"physical_hash",context_.value("physical_hash")}});
        if (!contextFile->open() || contextFile->write(contextBytes) != contextBytes.size() || !contextFile->flush()) {
            contextFile->deleteLater(); fail("configuration_file","Cannot create the local publisher model context."); return;
        }
        context_file_ = contextFile;
        executable_ = executable; arguments_ = arguments;
        process_ = new QProcess(this);
        auto* process = process_;
        const auto generation = generation_;
        // Only runtime essentials and the explicit sensor configuration reach the
        // child. Model/cloud credentials are not part of this acquisition adapter.
        QProcessEnvironment environment;
        for (const auto* name : {"SystemRoot", "WINDIR", "PATH", "PATHEXT", "TEMP", "TMP", "USERPROFILE", "LOCALAPPDATA"}) {
            const auto value = qEnvironmentVariable(name);
            if (!value.isEmpty()) environment.insert(QString::fromLatin1(name), value);
        }
        environment.insert("PYTHONUNBUFFERED", "1");
        environment.insert("PYTHONNOUSERSITE", "1");
        environment.insert("PYTHONDONTWRITEBYTECODE", "1");
        environment.insert("BREAKWATER_TELEMETRY_CONFIGURATION_FILE", configurationFile->fileName());
        environment.insert("BREAKWATER_TELEMETRY_CONTEXT_FILE", contextFile->fileName());
        environment.insert("BREAKWATER_TELEMETRY_CONFIGURATION_HASH", configuration_hash_);
        environment.insert("BREAKWATER_TELEMETRY_PHYSICAL_HASH", configuration_.value("physical_hash").toString());
        process->setProcessEnvironment(environment);
        process->setProgram(executable); process->setArguments(arguments);
        process->setProcessChannelMode(QProcess::SeparateChannels);
        connect(process, &QProcess::readyReadStandardOutput, this, [this, process, generation] {
            if (process == process_ && generation == generation_) drain();
        });
        connect(process, &QProcess::readyReadStandardError, this, [this, process, generation] {
            if (process != process_ || generation != generation_) return;
            stderr_bytes_ += process->readAllStandardError().size();
            if (stderr_bytes_ > 64 * 1024) fail("stderr_limit", "Publisher diagnostic output exceeded its bound.");
        });
        connect(process, &QProcess::errorOccurred, this, [this, process, generation](QProcess::ProcessError error) {
            if (process == process_ && generation == generation_ && error == QProcess::FailedToStart)
                fail("process_start", "The local publisher could not start.");
        });
        connect(process, &QProcess::finished, this, [this, process, generation](int exitCode, QProcess::ExitStatus exitStatus) {
            if (process != process_ || generation != generation_) return;
            drain(true);
            if (process != process_ || generation != generation_) return;
            if (!buffer_.isEmpty()) { fail("truncated_frame", "Publisher exited with an unfinished NDJSON frame."); return; }
            if (exitStatus != QProcess::NormalExit || exitCode != 0 || !received_end_) {
                fail("process_exit", "Publisher exited without a clean, versioned end frame."); return;
            }
            active_ = false; status_ = "ended"; disposeProcess();
        });
        active_ = true; starting_ = true; pending_deadline_ = monotonicS() + timeout_ms_ / 1000.0;
        process->start();
    }
    void startReplay(const QUrl& url, bool historical = false) {
        if (configuration_.isEmpty()) { reject("configuration_missing", "Place and configure sensors before opening a recording."); return; }
        if (!url.isLocalFile()) { reject("recording_path", "Choose a local NDJSON recording."); return; }
        auto candidate = std::make_unique<QFile>(url.toLocalFile());
        if (!candidate->open(QIODevice::ReadOnly) || candidate->size() <= 0 || candidate->size() > recordingLimit) {
            reject("recording_open", "Cannot open recording, or its size exceeds 32 MiB."); return;
        }
        QJsonObject first; bool timed=false; JournalReceipt initialReceipt;
        try {
            const auto line = candidate->readLine(journalLineLimit + 1);
            first = decode_document(line, journalLineLimit);
            timed = first.value("kind") == "receiver_journal";
            if (timed) {
                validate_journal_header(first,configuration_hash_,configuration_.value("physical_hash").toString());
                initialReceipt = parse_journal_receipt(decode_document(candidate->readLine(journalLineLimit+1),journalLineLimit));
                first = decode_document(initialReceipt.frame,frameLimit);
            } else if (line.size()>frameLimit) throw std::invalid_argument("Legacy frame exceeds its bound.");
            validate_document(first, "stream_frame", context_);
            if (first.value("type").toString() != "start" || first.value("configuration_hash").toString() != configuration_hash_ ||
                first.value("physical_hash") != configuration_.value("physical_hash"))
                throw std::invalid_argument("Recording start does not match the registered network and sensors.");
        } catch (const std::exception& exception) { reject("recording_invalid", QString::fromUtf8(exception.what())); return; }
        // The registered start is checked before replacing the current stream.
        shutdownInput();
        replay_ = std::move(candidate); replay_url_ = url; historical_ = historical;
        journal_replay_ = timed; previous_receipt_ = initialReceipt; next_receipt_ = {};
        replay_bytes_ = replay_->size(); replay_footer_ = {};
        first_receipt_monotonic_ = initialReceipt.monotonic_s; base_arrival_ = 0;
        recording_format_ = timed ? "receiver_journal_v1" : "g1_event_only";
        source_stream_ = first.value("stream_id").toString(); source_epoch_ = first.value("epoch_id").toString();
        replay_stream_ = freshId("replay-"); replay_epoch_ = freshId("epoch-");
        mode_ = historical ? "historical" : "replay"; status_ = "replaying"; error_.clear();
        stream_.configure(configuration_, context_); stream_.beginEpoch(mode_,timed);
        base_event_ = timed ? initialReceipt.event_s : first.value("event_time_s").toDouble(); base_monotonic_ = monotonicS();
        rate_ = 1; paused_ = false; active_ = true; started_ = true;
        first = rekey(first);
        if (!acceptCore(first, bytes(first),timed ? &initialReceipt : nullptr)) return;
        readReplayNext();
    }
    void command(const QString& operation, double rate = 1) {
        if (operation == "set_rate" && (!std::isfinite(rate) || rate < 0.1 || rate > 4)) {
            reject("clock_rate", "Playback/source speed must be between 0.1 and 4."); return;
        }
        if (mode_ == "replay" || mode_ == "historical") {
            if (operation == "restart") { startReplay(replay_url_, historical_); return; }
            if (operation == "stop") { shutdownInput(); status_ = "stopped"; return; }
            if (!active_) return;
            freezeClock();
            if (operation == "pause") paused_ = true;
            else if (operation == "resume") paused_ = false;
            else if (operation == "set_rate") rate_ = rate;
            return;
        }
        if (operation == "stop" && (starting_ || !pending_request_.isEmpty())) {
            shutdownInput(); status_ = "stopped"; return;
        }
        if (!active_ || !process_ || !started_) {
            if (operation == "stop") { shutdownInput(); status_ = "stopped"; }
            return;
        }
        if (!pending_request_.isEmpty() || starting_) { reject("command_pending", "Wait for the pending lifecycle acknowledgement."); return; }
        pending_request_ = freshId("request-"); pending_operation_ = operation; pending_rate_ = rate;
        QJsonObject payload;
        if (operation == "set_rate") payload["clock_rate"] = rate;
        const QJsonObject request{{"telemetry_version", 1}, {"kind", "stream_command"}, {"request_id", pending_request_},
            {"stream_id", stream_.streamId()}, {"epoch_id", stream_.epochId()}, {"operation", operation}, {"payload", payload}};
        try { validate_document(request, "stream_command"); }
        catch (const std::exception&) { fail("command_invalid", "Cannot construct the requested lifecycle command."); return; }
        const auto wire = bytes(request);
        if (process_->write(wire) != wire.size()) { fail("command_write", "Cannot queue the lifecycle command."); return; }
        pending_deadline_ = monotonicS() + timeout_ms_ / 1000.0;
    }
    void exportRecording(const QUrl& destination) {
        if (!destination.isLocalFile()) { reject("recording_path", "Choose a local output file."); return; }
        try {
            const auto data = stream_.recording();
            if (data.isEmpty()) throw std::invalid_argument("No valid stream start has been recorded.");
            QSaveFile file(destination.toLocalFile());
            if (!file.open(QIODevice::WriteOnly) || file.write(data) != data.size() || !file.commit())
                throw std::invalid_argument("Cannot save the bounded recording atomically.");
            emit exported(destination);
        } catch (const std::exception& exception) { reject("recording_export", QString::fromUtf8(exception.what())); }
    }
    void exportReceiverJournal(const QUrl& destination) {
        if (!destination.isLocalFile() || mode_!="synthetic_live" || !journal_started_) {
            reject("recording_path","A received local synthetic epoch and local output file are required for a timed journal."); return;
        }
        try {
            const auto data=journal_.exportBytes(journalComplete(),!error_.isEmpty());
            QSaveFile file(destination.toLocalFile());
            if (!file.open(QIODevice::WriteOnly) || file.write(data)!=data.size() || !file.commit())
                throw std::invalid_argument("Cannot atomically save the bounded receiver journal.");
            emit exported(destination);
        } catch (const std::exception& error) { reject("recording_export",QString::fromUtf8(error.what())); }
    }
    void captureObserved(const QString& request, const QJsonObject& registry, const QJsonArray& commands,
                         quint64 revision, const QString& expectedStream, const QString& expectedEpoch, quint64 requestGeneration) {
        try {
            if (!started_ || mode_ == "manual" || (mode_ == "synthetic_live" && !active_) ||
                expectedStream.isEmpty() || expectedEpoch.isEmpty() ||
                stream_.streamId() != expectedStream || stream_.epochId() != expectedEpoch)
                throw std::invalid_argument("The requested acquisition epoch is no longer current.");
            // One receiver-clock instant, on the same thread as accepted input.
            const auto event = nowEvent(), received = receiverMonotonic();
            stream_.advance(event, received);
            const auto serial = source_serial_->load();
            const auto capture = stream_.captureObserved(registry, commands, revision, request);
            emit observedCaptured(request, capture.document, capture.context, capture.diagnostics, capture.effective_network, requestGeneration, serial);
        } catch (const std::exception&) {
            emit observedRejected(request, requestGeneration, "capture_unavailable", "The observed capture context, epoch or bounded evidence is unavailable.");
        }
    }
    void publish() {
        auto report = stream_.report();
        report["mode"] = mode_; report["active"] = active_; report["paused"] = paused_;
        report["clock_rate"] = rate_; report["event_time_s"] = nowEvent();
        report["configuration_hash"] = configuration_hash_;
        report["recording_format"] = recording_format_;
        report["journal_bytes"] = journal_replay_ ? replay_bytes_ : journal_.size();
        report["journal_truncated"] = journal_replay_ ? replay_footer_.value("truncated").toBool() : journal_.truncated();
        report["journal_complete"] = journal_replay_ ? replay_footer_.value("complete").toBool() : journalComplete();
        report["journal_termination"] = journal_replay_ ? replay_footer_.value("termination").toString("prefix") :
            journal_.truncated() ? "size_limit" : !error_.isEmpty() ? "source_fault" : journalComplete() ? "received_end" : "prefix";
        if (!active_) report["source_available"] = false;
        auto rows = report.value("sensors").toArray();
        QHash<QString, QString> labels;
        for (const auto& definition : configuration_.value("sensors").toArray()) {
            const auto sensor = definition.toObject(); labels[sensor.value("sensor_id").toString()] = sensor.value("label").toString();
        }
        for (qsizetype index = 0; index < rows.size(); ++index) {
            auto row = rows[index].toObject(); row["label"] = labels.value(row.value("sensor_id").toString(), row.value("label").toString());
            if (!active_) { row["source_available"] = false; if (row.contains("canonical_value")) row["status"] = "historical"; }
            rows[index] = row;
        }
        report["sensors"] = rows;
        // Local playback pause is not a heartbeat and cannot hide source loss.
        if (active_ && paused_ && report.value("status").toString() == "running") report["status"] = "paused";
        if (active_ && (mode_=="replay" || mode_=="historical")) report["status"] = paused_ ? "paused" : "replaying";
        if (!status_.isEmpty()) report["lifecycle_status"] = status_;
        if (!error_.isEmpty()) { report["error"] = error_; report["status"] = "fault"; }
        else if (!active_ || !started_ || !report.contains("status")) report["status"] = status_;
        report["replay_prefix"] = status_ == "replay_prefix_finished";
        const auto trace = stream_.trace();
        QJsonArray preview;
        for (qsizetype index = std::max<qsizetype>(0, trace.size() - 128); index < trace.size(); ++index) preview.append(trace[index]);
        emit updated(report.toVariantMap(), preview.toVariantList());
    }
signals:
    void updated(const QVariantMap& report, const QVariantList& trace);
    void rejected(const QString& code, const QString& message);
    void exported(const QUrl& destination);
    void observedCaptured(const QString& request, const QJsonObject& document, const QJsonObject& context,
                          const QJsonObject& diagnostics, const QJsonObject& effectiveNetwork, quint64 requestGeneration, quint64 sourceSerial);
    void observedRejected(const QString& request, quint64 requestGeneration, const QString& code, const QString& message);
    void shutdownReady();
private:
    TelemetryStream stream_;
    ReceiverJournal journal_;
    std::shared_ptr<std::atomic<quint64>> source_serial_;
    QJsonObject configuration_, context_;
    QString configuration_hash_, mode_ = "manual", status_ = "unconfigured", error_;
    QElapsedTimer monotonic_;
    QTimer* tick_;
    QProcess* process_ = nullptr;
    QTemporaryFile* configuration_file_ = nullptr;
    QTemporaryFile* context_file_ = nullptr;
    std::unique_ptr<QFile> replay_;
    QJsonObject next_replay_;
    JournalReceipt previous_receipt_,next_receipt_;
    QUrl replay_url_;
    QString executable_, source_stream_, source_epoch_, replay_stream_, replay_epoch_;
    QStringList arguments_;
    QString recording_format_ = "g1_event_only";
    QJsonObject replay_footer_;
    qint64 replay_bytes_ = 0;
    QString pending_request_, pending_operation_;
    double pending_deadline_ = 0, exit_deadline_ = 0, pending_rate_ = 1, base_event_ = 0, base_monotonic_ = 0, rate_ = 1;
    double first_receipt_monotonic_ = 0,base_arrival_ = 0;
    QByteArray buffer_;
    QMap<quint64, QByteArray> acknowledged_;
    QSet<QProcess*> retiring_;
    quint64 generation_ = 0;
    qint64 stderr_bytes_ = 0;
    int timeout_ms_ = 5000;
    bool active_ = false, started_ = false, starting_ = false, paused_ = false, received_end_ = false;
    bool historical_ = false, awaiting_restart_ = false, read_scheduled_ = false, shutting_down_ = false;
    bool journal_replay_ = false,journal_started_ = false;
    double monotonicS() const { return monotonic_.isValid() ? monotonic_.nsecsElapsed() / 1e9 : 0; }
    double elapsedPlayback() const { return active_ && started_ && !paused_ && !historical_ ? std::max(0.0,monotonicS()-base_monotonic_)*rate_ : 0; }
    double arrivalTime() const { return base_arrival_+elapsedPlayback(); }
    double nowEvent() const {
        if (!journal_replay_) return base_event_+elapsedPlayback();
        if (next_receipt_.frame.isEmpty()) return previous_receipt_.event_s;
        const auto span=next_receipt_.monotonic_s-previous_receipt_.monotonic_s;
        const auto fraction=span>0 ? std::clamp((first_receipt_monotonic_+arrivalTime()-previous_receipt_.monotonic_s)/span,0.0,1.0) : 1.0;
        return std::lerp(previous_receipt_.event_s,next_receipt_.event_s,fraction);
    }
    double receiverMonotonic() const {
        if (!journal_replay_) return monotonicS();
        return next_receipt_.frame.isEmpty() ? previous_receipt_.monotonic_s :
            std::clamp(first_receipt_monotonic_+arrivalTime(),previous_receipt_.monotonic_s,next_receipt_.monotonic_s);
    }
    void freezeClock() { const auto event=nowEvent(),arrival=arrivalTime(); base_event_=event; base_arrival_=arrival; base_monotonic_=monotonicS(); }
    bool journalComplete() const { return mode_=="synthetic_live" && journal_started_ && received_end_ && !process_ && error_.isEmpty() && !journal_.truncated(); }
    void retiredFinished(QProcess* process) {
        if (!retiring_.remove(process)) return;
        process->deleteLater();
        if (shutting_down_ && retiring_.isEmpty()) emit shutdownReady();
    }
    void disposeProcess() {
        if (process_) {
            auto* old = process_; process_ = nullptr;
            disconnect(old, nullptr, this, nullptr);
            if (old->state() == QProcess::NotRunning) old->deleteLater();
            else {
                retiring_.insert(old);
                connect(old, &QProcess::finished, this, [this, old] { retiredFinished(old); });
                connect(old, &QProcess::errorOccurred, this, [this, old](QProcess::ProcessError error) {
                    if (error == QProcess::FailedToStart || old->state() == QProcess::NotRunning) retiredFinished(old);
                });
                old->kill();
            }
        }
        if (configuration_file_) { configuration_file_->deleteLater(); configuration_file_ = nullptr; }
        if (context_file_) { context_file_->deleteLater(); context_file_ = nullptr; }
    }
    void shutdownInput() {
        freezeClock(); ++generation_; source_serial_->fetch_add(1); disposeProcess(); replay_.reset(); next_replay_ = {};
        active_ = false; started_ = false; starting_ = false; paused_ = false; received_end_ = false;
        awaiting_restart_ = false; read_scheduled_ = false; buffer_.clear(); pending_request_.clear();
        pending_operation_.clear(); stderr_bytes_ = 0;
        acknowledged_.clear(); exit_deadline_ = 0;
        historical_ = false;
        journal_replay_ = false; previous_receipt_ = {}; next_receipt_ = {}; base_arrival_=0;
        replay_footer_ = {}; replay_bytes_ = 0;
    }
    void reject(const QString& code, const QString& message) { emit rejected(code, message); }
    void fail(const QString& code, const QString& message) {
        shutdownInput(); status_ = "fault"; error_ = code + ": " + message; emit rejected(code, message);
    }
    bool acceptCore(const QJsonObject&, const QByteArray& line,const JournalReceipt* receipt=nullptr) {
        if (!stream_.ingest(line,receipt ? receipt->event_s : nowEvent(),receipt ? receipt->monotonic_s : receiverMonotonic(),
                            receipt ? receipt->utc : QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs))) {
            const auto report = stream_.report();
            const auto reason = report.value("error_code").toString() + ": " + report.value("error_message").toString();
            fail("stream_fault", reason); return false;
        }
        return true;
    }
    bool acceptLive(const QByteArray& line) {
        QJsonObject frame;
        try {
            frame = decode_document(line, frameLimit);
            // Configure validated the immutable network/sensor registry. Check
            // this frame's structure and exact cached identities before touching
            // lifecycle state; the core below still validates enabled sensors,
            // measurement types/units, duplicates, epochs and clock policy.
            (void)parse_stream_frame(frame, {{"physical_hash", configuration_.value("physical_hash")}});
            if (frame.value("configuration_hash") != configuration_hash_)
                throw std::invalid_argument("Configuration identity mismatch.");
        }
        catch (const std::exception& exception) { fail("frame_invalid", QString::fromUtf8(exception.what())); return false; }
        const auto type = frame.value("type").toString();
        const auto payload = frame.value("payload").toObject();
        const bool newStart = type=="start" && (!started_ || (awaiting_restart_ && frame.value("epoch_id").toString()!=stream_.epochId() && frame.value("stream_id").toString()==stream_.streamId()));
        if (newStart && awaiting_restart_) journal_.reset(configuration_hash_,configuration_.value("physical_hash").toString());
        journal_.append({line,monotonicS(),newStart ? frame.value("event_time_s").toDouble() : nowEvent(),QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs)});
        if (newStart) journal_started_=true;
        if (type == "start") {
            // Duplicate starts and old/unauthorized epochs go through the same
            // bounded identity policy without resetting the receiver clock.
            if (started_ && (!awaiting_restart_ || frame.value("epoch_id").toString() == stream_.epochId() ||
                frame.value("stream_id").toString() != stream_.streamId())) return acceptCore(frame, line);
            if (payload.value("mode").toString() != "synthetic_live") { fail("source_mode", "A local publisher must identify itself as synthetic live."); return false; }
            if (awaiting_restart_) { source_serial_->fetch_add(1); stream_.beginEpoch("synthetic_live"); acknowledged_.clear(); }
            started_ = true; starting_ = false; awaiting_restart_ = false; received_end_ = false;
            base_event_ = frame.value("event_time_s").toDouble(); base_monotonic_ = monotonicS();
            rate_ = payload.value("clock_rate").toDouble(); paused_ = payload.value("paused").toBool(); status_ = "running";
            if (rate_ < 0.1 || rate_ > 4) { fail("clock_rate", "Publisher rate is outside the declared adapter range 0.1–4."); return false; }
        } else if (!started_) { fail("missing_start", "Publisher must begin with a registered start frame."); return false; }
        else if (frame.value("stream_id").toString() != stream_.streamId() || frame.value("epoch_id").toString() != stream_.epochId())
            return acceptCore(frame, line);
        if (type != "start" && type != "readings" &&
            (frame.value("event_time_s").toDouble() > nowEvent() + configuration_.value("policy").toObject().value("future_allowance_s").toDouble() ||
             frame.value("event_time_s").toDouble() < stream_.report().value("watermark_event_time_s").toDouble()))
            return acceptCore(frame, line);
        if (type == "heartbeat" && (payload.value("paused").toBool() != paused_ || payload.value("clock_rate").toDouble() != rate_)) {
            fail("heartbeat_clock", "Heartbeat clock state differs from the acknowledged receiver clock."); return false;
        }
        if (type == "ack") {
            const auto sequence = static_cast<quint64>(frame.value("sequence").toDouble());
            if (acknowledged_.contains(sequence)) {
                if (acknowledged_.value(sequence) != QCryptographicHash::hash(bytes(frame), QCryptographicHash::Sha256)) { fail("ack_reused", "A completed lifecycle sequence was acknowledged with different content."); return false; }
                return acceptCore(frame, line);
            }
            if (pending_request_.isEmpty() || payload.value("request_id").toString() != pending_request_ || payload.value("operation").toString() != pending_operation_) {
                fail("ack_mismatch", "Publisher acknowledgement does not match the pending request."); return false;
            }
            const bool expectedPause = pending_operation_ == "pause" ? true : pending_operation_ == "resume" ? false : paused_;
            const double expectedRate = pending_operation_ == "set_rate" ? pending_rate_ : rate_;
            if (payload.value("paused").toBool() != expectedPause || payload.value("clock_rate").toDouble() != expectedRate ||
                payload.value("effective_event_time_s").toDouble() > nowEvent() + configuration_.value("policy").toObject().value("future_allowance_s").toDouble()) {
                fail("ack_clock", "Publisher acknowledgement has inconsistent lifecycle clock state."); return false;
            }
            if (frame.value("event_time_s").toDouble() > nowEvent() + configuration_.value("policy").toObject().value("future_allowance_s").toDouble())
                return acceptCore(frame, line); // future evidence cannot complete a live command
        }
        if (!acceptCore(frame, line)) return false;
        if (type == "ack") {
            acknowledged_.insert(static_cast<quint64>(frame.value("sequence").toDouble()), QCryptographicHash::hash(bytes(frame), QCryptographicHash::Sha256));
            while (acknowledged_.size() > 32768) acknowledged_.erase(acknowledged_.begin());
            freezeClock();
            paused_ = payload.value("paused").toBool(); rate_ = payload.value("clock_rate").toDouble();
            if (pending_operation_ == "restart") { awaiting_restart_ = true; starting_ = true; pending_deadline_ = monotonicS() + timeout_ms_ / 1000.0; }
            pending_request_.clear(); pending_operation_.clear();
        } else if (type == "end") {
            received_end_ = true;
            if (!awaiting_restart_) { freezeClock(); source_serial_->fetch_add(1); active_ = false; status_ = "ended"; exit_deadline_ = monotonicS() + timeout_ms_ / 1000.0; }
        } else if (type == "error") { fail("publisher_error", "Publisher reported a versioned stream error."); return false; }
        return true;
    }
    void drain(bool finishing = false) {
        if (!process_) return;
        if (process_->bytesAvailable() + buffer_.size() > backlogLimit) { fail("backpressure", "Publisher output exceeded the bounded input backlog."); return; }
        buffer_ += process_->read(std::min<qint64>(process_->bytesAvailable(), backlogLimit - buffer_.size()));
        int budget = finishing ? 32768 : 16;
        while (process_ && budget-- > 0) {
            const auto newline = buffer_.indexOf('\n');
            if (newline < 0) {
                if (buffer_.size() >= frameLimit) fail("frame_size", "Publisher frame exceeds 256 KiB.");
                return;
            }
            if (newline + 1 > frameLimit) { fail("frame_size", "Publisher frame exceeds 256 KiB."); return; }
            const auto line = buffer_.left(newline + 1); buffer_.remove(0, newline + 1);
            if (!acceptLive(line)) return;
        }
        if (process_ && buffer_.contains('\n') && !read_scheduled_) {
            read_scheduled_ = true; const auto generation = generation_;
            QTimer::singleShot(0, this, [this, generation] { if (generation != generation_) return; read_scheduled_ = false; drain(); });
        }
    }
    QJsonObject rekey(QJsonObject frame) const {
        const bool primary=frame.value("stream_id").toString()==source_stream_ && frame.value("epoch_id").toString()==source_epoch_;
        if (!primary) return frame;
        frame["stream_id"] = replay_stream_; frame["epoch_id"] = replay_epoch_;
        const auto type = frame.value("type").toString();
        if (type == "start" || type == "heartbeat" || type == "ack") {
            auto payload = frame.value("payload").toObject();
            if (type == "start") payload["mode"] = "replay";
            frame["payload"] = payload;
        }
        return frame;
    }
    void readReplayNext() {
        if (!replay_) return;
        if (replay_->size() > recordingLimit || replay_->pos() > recordingLimit) { fail("recording_size", "The replay file grew beyond its 32 MiB limit."); return; }
        if (replay_->atEnd()) { finishReplay(); return; }
        try {
            const auto limit=journal_replay_ ? journalLineLimit : frameLimit;
            const auto line = replay_->readLine(limit + 1);
            auto document = decode_document(line, limit);
            if (journal_replay_ && document.value("type")=="journal_end") {
                validate_journal_end(document);
                if (!replay_->atEnd() || (document.value("complete").toBool() && !received_end_))
                    throw std::invalid_argument("The journal completion footer conflicts with received evidence.");
                replay_footer_ = document;
                finishReplay(document.value("complete").toBool()); return;
            }
            if (journal_replay_) {
                next_receipt_=parse_journal_receipt(document);
                if (next_receipt_.monotonic_s<previous_receipt_.monotonic_s || next_receipt_.event_s<previous_receipt_.event_s)
                    throw std::invalid_argument("Receiver journal clocks moved backwards.");
                document=decode_document(next_receipt_.frame,frameLimit);
            }
            next_replay_ = document;
            validate_document(next_replay_, "stream_frame", context_);
            if (!journal_replay_ && (next_replay_.value("stream_id").toString() != source_stream_ || next_replay_.value("epoch_id").toString() != source_epoch_ ||
                next_replay_.value("type").toString() == "start")) throw std::invalid_argument("Recording must contain one complete stream epoch.");
        } catch (const std::exception& exception) { fail("recording_invalid", QString::fromUtf8(exception.what())); }
    }
    void finishReplay(bool declaredComplete=false) { freezeClock(); source_serial_->fetch_add(1); active_ = false; replay_.reset(); next_replay_ = {}; next_receipt_={}; status_ = (journal_replay_ ? declaredComplete : received_end_) ? "replay_finished" : "replay_prefix_finished"; }
    void tick() {
        const double mono = monotonicS();
        if ((starting_ || !pending_request_.isEmpty()) && mono > pending_deadline_) { fail("request_timeout", "Publisher start or lifecycle acknowledgement timed out."); return; }
        if (process_ && received_end_ && !active_ && mono > exit_deadline_) { fail("process_end_timeout", "Publisher did not exit after its end frame."); return; }
        if (replay_ && active_ && !paused_) {
            for (int budget = 0; budget < 16 && replay_ && !next_replay_.isEmpty(); ++budget) {
                const double target = journal_replay_ ? next_receipt_.monotonic_s-first_receipt_monotonic_ : next_replay_.value("event_time_s").toDouble();
                if (!historical_ && target > (journal_replay_ ? arrivalTime() : nowEvent())) break;
                if (historical_) { if (journal_replay_) base_arrival_=target; else base_event_ = std::max(base_event_, target); base_monotonic_ = mono; }
                const auto frame = rekey(next_replay_); next_replay_ = {};
                if (!acceptCore(frame, bytes(frame),journal_replay_ ? &next_receipt_ : nullptr)) return;
                if (journal_replay_) { previous_receipt_=next_receipt_; next_receipt_={}; }
                const bool end = frame.value("type").toString() == "end" && stream_.report().value("ended").toBool();
                if (end) received_end_ = true;
                if (!journal_replay_ && end && !replay_->atEnd()) { fail("recording_after_end", "Recording contains data after its end frame."); return; }
                readReplayNext();
            }
        }
        // Replay may ingest several frames during this tick. Use a new receive
        // timestamp after that work, never the older tick-start timestamp.
        if (started_) stream_.advance(nowEvent(), receiverMonotonic());
        if (stream_.faulted() && error_.isEmpty()) fail("stream_fault", stream_.report().value("error_message").toString());
    }
};

TelemetryController::TelemetryController(QObject* parent) : QObject(parent) {
    publisher_executable_ = qEnvironmentVariable("BREAKWATER_TELEMETRY_EXECUTABLE");
    const auto arguments = qEnvironmentVariable("BREAKWATER_TELEMETRY_ARGUMENTS_JSON");
    bool valid = true;
    if (!arguments.isEmpty()) {
        QJsonParseError parseError;
        const auto document = QJsonDocument::fromJson(arguments.toUtf8(), &parseError);
        valid = parseError.error == QJsonParseError::NoError && document.isArray() && document.array().size() <= 32;
        if (valid) for (const auto& value : document.array()) {
            if (!value.isString() || value.toString().size() > 4096) { valid = false; break; }
            publisher_arguments_.append(value.toString());
        }
    }
    publisher_configured_ = valid && QFileInfo(publisher_executable_).isAbsolute() && QFileInfo(publisher_executable_).isFile();
    // Preserve every explicit override, including an invalid one. A deployed
    // bundle never falls back to a developer checkout outside its own bin tree.
    if (publisher_executable_.isEmpty() && arguments.isEmpty()) {
        runtime::configureBundledPython();
        const auto python = qEnvironmentVariable("BREAKWATER_PYTHON").trimmed();
        auto script = QDir(QCoreApplication::applicationDirPath()).absoluteFilePath("../share/breakwater/science/telemetry_publisher.py");
#if defined(BREAKWATER_TELEMETRY_PUBLISHER_PATH) && defined(BREAKWATER_TELEMETRY_DEVELOPMENT_BIN)
        const auto actual = QFileInfo(QCoreApplication::applicationDirPath()).canonicalFilePath();
        const auto development = QFileInfo(QString::fromUtf8(BREAKWATER_TELEMETRY_DEVELOPMENT_BIN)).canonicalFilePath();
        if (!QFileInfo(script).isFile() && !development.isEmpty() && actual.compare(development,Qt::CaseInsensitive)==0)
            script = QString::fromUtf8(BREAKWATER_TELEMETRY_PUBLISHER_PATH);
#endif
        if (QFileInfo(python).isAbsolute() && QFileInfo(python).isFile() && QFileInfo(script).isFile()) {
            publisher_executable_ = python; publisher_arguments_ = {"-B",script};
            publisher_configured_ = bundled_publisher_ = true;
        }
    }
    worker_ = new TelemetryIoWorker(source_serial_);
    worker_->moveToThread(&thread_);
    connect(&thread_, &QThread::started, worker_, &TelemetryIoWorker::initialize);
    connect(&thread_, &QThread::finished, worker_, &QObject::deleteLater);
    connect(worker_, &TelemetryIoWorker::shutdownReady, worker_, [] { QThread::currentThread()->quit(); });
    connect(worker_, &TelemetryIoWorker::updated, this, [this](const QVariantMap& report, const QVariantList& trace) {
        snapshot_ = report; trace_preview_ = trace; poll_pending_ = false; refreshObservedDisposition(); emit changed();
    });
    connect(worker_, &TelemetryIoWorker::rejected, this, [this](const QString& code, const QString& message) {
        emit failed(code, message);
    });
    connect(worker_, &TelemetryIoWorker::exported, this, &TelemetryController::recordingExported);
    connect(worker_, &TelemetryIoWorker::observedCaptured, this,
        [this](const QString& request, const QJsonObject& document, const QJsonObject& context,
               const QJsonObject& diagnostics, const QJsonObject& effectiveNetwork, quint64 generation, quint64 sourceSerial) {
            if (generation != observed_generation_ || request != observed_request_ || !observed_pending_) return;
            observed_pending_ = false; observed_request_.clear();
            if (sourceSerial != source_serial_->load()) {
                observed_disposition_ = "retired"; emit observedCaptureChanged();
                emit observedCaptureFailed(request, "capture_retired", "The source retired before the capture could be published."); return;
            }
            observed_ = std::make_shared<const ObservedEvidence>(ObservedEvidence{document, context, diagnostics, effectiveNetwork});
            observed_source_serial_ = sourceSerial; observed_retired_ = false;
            observed_disposition_ = document.value("mode") == "synthetic_live" ? document.value("availability").toString() : "historical";
            emit observedCaptureChanged(); emit observedCaptureReady(request);
        });
    connect(worker_, &TelemetryIoWorker::observedRejected, this,
        [this](const QString& request, quint64 generation, const QString& code, const QString& message) {
            if (generation != observed_generation_ || request != observed_request_ || !observed_pending_) return;
            observed_pending_ = false; observed_request_.clear();
            if (!observed_) observed_disposition_ = "unavailable";
            emit observedCaptureChanged(); emit observedCaptureFailed(request, code, message);
        });
    poll_.setInterval(100);
    connect(&poll_, &QTimer::timeout, this, [this] {
        if (poll_pending_) return;
        poll_pending_ = true;
        QMetaObject::invokeMethod(worker_, [worker = worker_] { worker->publish(); }, Qt::QueuedConnection);
    });
    thread_.start(); poll_.start();
}
TelemetryController::~TelemetryController() {
    poll_.stop();
    QMetaObject::invokeMethod(worker_, [worker = worker_] { worker->shutdown(); }, Qt::QueuedConnection);
    // The application is shutting down; all ongoing user actions use queued,
    // nonblocking calls. Child termination and parsing run on the worker thread.
    thread_.wait();
}
void TelemetryController::configure(const QJsonObject& configuration, const QJsonObject& context) {
    QString identity;
    try { identity = sensor_semantic_hash(configuration); } catch (const std::exception&) {}
    if (identity != acquisition_hash_) {
        retireObserved("configuration_changed");
        acquisition_hash_ = identity;
        if (identity != observed_context_hash_) observed_context_set_ = false;
    }
    QMetaObject::invokeMethod(worker_, [worker = worker_, configuration, context] { worker->configure(configuration, context); }, Qt::QueuedConnection);
}
void TelemetryController::startProcess(const QString& executable, const QStringList& arguments) {
    retireObserved("source_replaced");
    QMetaObject::invokeMethod(worker_, [worker = worker_, executable, arguments] { worker->startProcess(executable, arguments); }, Qt::QueuedConnection);
}
void TelemetryController::setTimeoutMs(int milliseconds) {
    QMetaObject::invokeMethod(worker_, [worker = worker_, milliseconds] { worker->setTimeout(milliseconds); }, Qt::QueuedConnection);
}
void TelemetryController::startConfiguredPublisher() {
    if (!publisher_configured_) { emit failed("publisher_unavailable", "Configure a local Python executable or a separate G1 synthetic publisher."); return; }
    if (bundled_publisher_) { startSynthetic("healthy",1337); return; }
    startProcess(publisher_executable_, publisher_arguments_);
}
QStringList TelemetryController::syntheticPresets() const {
    return {"healthy","pump_loss","unmetered_loss","drift","stuck","missing","delayed","reordered","mixed"};
}
void TelemetryController::startSynthetic(const QString& preset,quint32 seed) {
    if (!bundled_publisher_ || !publisher_configured_) { emit failed("publisher_unavailable","The bundled local publisher needs a configured Python executable."); return; }
    if (!syntheticPresets().contains(preset)) { emit failed("preset_invalid","Choose a supported local synthetic preset."); return; }
    auto arguments = publisher_arguments_;
    arguments.append({"--preset",preset,"--seed",QString::number(seed)});
    startProcess(publisher_executable_,arguments);
}
void TelemetryController::manual() { retireObserved("manual_mode"); QMetaObject::invokeMethod(worker_, [worker = worker_] { worker->manual(); }, Qt::QueuedConnection); }
void TelemetryController::startReplay(const QUrl& recording) {
    retireObserved("source_replaced");
    QMetaObject::invokeMethod(worker_, [worker = worker_, recording] { worker->startReplay(recording); }, Qt::QueuedConnection);
}
void TelemetryController::showHistorical(const QUrl& recording) {
    retireObserved("source_replaced");
    QMetaObject::invokeMethod(worker_, [worker = worker_, recording] { worker->startReplay(recording, true); }, Qt::QueuedConnection);
}
void TelemetryController::pause() { QMetaObject::invokeMethod(worker_, [worker = worker_] { worker->command("pause"); }, Qt::QueuedConnection); }
void TelemetryController::resume() { QMetaObject::invokeMethod(worker_, [worker = worker_] { worker->command("resume"); }, Qt::QueuedConnection); }
void TelemetryController::setRate(double rate) { QMetaObject::invokeMethod(worker_, [worker = worker_, rate] { worker->command("set_rate", rate); }, Qt::QueuedConnection); }
void TelemetryController::restart() { retireObserved("epoch_restart"); QMetaObject::invokeMethod(worker_, [worker = worker_] { worker->command("restart"); }, Qt::QueuedConnection); }
void TelemetryController::stop() { retireObserved("source_stopped"); QMetaObject::invokeMethod(worker_, [worker = worker_] { worker->command("stop"); }, Qt::QueuedConnection); }
void TelemetryController::exportRecording(const QUrl& destination) {
    QMetaObject::invokeMethod(worker_, [worker = worker_, destination] { worker->exportRecording(destination); }, Qt::QueuedConnection);
}
void TelemetryController::exportReceiverJournal(const QUrl& destination) {
    QMetaObject::invokeMethod(worker_,[worker=worker_,destination] { worker->exportReceiverJournal(destination); },Qt::QueuedConnection);
}
QVariantMap TelemetryController::observedCapture() const { return observed_ ? observed_->document.toVariantMap() : QVariantMap{}; }
QVariantMap TelemetryController::observedCaptureDiagnostics() const { return observed_ ? observed_->diagnostics.toVariantMap() : QVariantMap{}; }
QString TelemetryController::observedCaptureDisposition() const {
    if (observed_ && observed_source_serial_ != source_serial_->load()) return "retired";
    return observed_disposition_;
}
void TelemetryController::retireObserved(const QString& reason) {
    ++observed_generation_; observed_pending_ = false; observed_request_.clear();
    observed_retired_ = static_cast<bool>(observed_);
    observed_disposition_ = observed_ ? QStringLiteral("retired") : observed_context_set_ ? QStringLiteral("ready") : QStringLiteral("not_configured");
    Q_UNUSED(reason);
    emit observedCaptureChanged();
}
bool TelemetryController::setObservedCaptureContext(const QJsonObject& registry, quint64 revision, const QJsonArray& commands) {
    QString identity;
    try {
        if (revision > breakwater::telemetry::maximum_sequence || !registry.value("network").isObject() ||
            !registry.value("configuration").isObject() ||
            QJsonDocument(registry).toJson(QJsonDocument::Compact).size() > maximumDocumentBytes)
            throw std::invalid_argument("Invalid observed context.");
        identity = sensor_semantic_hash(registry.value("configuration").toObject());
    } catch (const std::exception&) {
        emit observedCaptureFailed({}, "capture_context_invalid", "A bounded registered project and sensor context is required."); return false;
    }
    if (!observed_context_set_ || identity != observed_context_hash_ || revision != observed_revision_ ||
        registry.value("physical_hash") != observed_registry_.value("physical_hash") || commands != observed_commands_)
        retireObserved("project_context_changed");
    observed_registry_ = registry; observed_commands_ = commands; observed_revision_ = revision;
    observed_context_hash_ = identity; observed_context_set_ = true;
    if (!observed_ && !observed_pending_) observed_disposition_ = "ready";
    emit observedCaptureChanged(); return true;
}
void TelemetryController::clearObservedCaptureContext() {
    observed_context_set_=false; observed_registry_={}; observed_commands_={}; retireObserved("project_context_unavailable");
}
QString TelemetryController::requestObservedCapture() {
    if (observed_pending_ || !observed_context_set_) {
        emit observedCaptureFailed({}, observed_pending_ ? "capture_busy" : "capture_context_missing", "Supply the current project context and wait for the pending capture."); return {};
    }
    const auto request = freshId("capture-");
    observed_request_ = request; observed_pending_ = true; const auto generation = ++observed_generation_;
    const auto registry = observed_registry_; const auto commands = observed_commands_; const auto revision = observed_revision_;
    const auto stream = snapshot_.value("stream_id").toString(), epoch = snapshot_.value("epoch_id").toString();
    emit observedCaptureChanged();
    QMetaObject::invokeMethod(worker_, [worker = worker_, request, registry, commands, revision, stream, epoch, generation] {
        worker->captureObserved(request, registry, commands, revision, stream, epoch, generation);
    }, Qt::QueuedConnection);
    return request;
}
void TelemetryController::cancelObservedCapture() { retireObserved("capture_cancelled"); }
void TelemetryController::refreshObservedDisposition() {
    if (!observed_ || observed_retired_) return;
    const auto& document = observed_->document;
    if (observed_source_serial_ != source_serial_->load() || snapshot_.value("stream_id") != document.value("stream_id").toVariant() ||
        snapshot_.value("epoch_id") != document.value("epoch_id").toVariant() || configurationHash() != document.value("configuration_hash").toString() ||
        (document.value("mode") == "synthetic_live" && (!active() || !snapshot_.value("source_available").toBool()))) {
        retireObserved("source_retired"); return;
    }
    const auto next = document.value("mode") != "synthetic_live" ? QStringLiteral("historical") :
        eventTime() > document.value("expires_event_time_s").toDouble() ? QStringLiteral("expired") : document.value("availability").toString();
    if (next != observed_disposition_) { observed_disposition_ = next; emit observedCaptureChanged(); }
}
} // namespace breakwater::desktop::telemetry

#include "TelemetryController.moc"
