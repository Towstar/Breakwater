#include "TelemetryStream.hpp"
#include "TelemetryContracts.hpp"
#include <QCryptographicHash>
#include <QDateTime>
#include <QJsonDocument>
#include <QMap>
#include <QRegularExpression>
#include <QSet>
#include <algorithm>
#include <cmath>
#include <deque>
#include <stdexcept>

namespace breakwater::desktop::telemetry {
namespace {
namespace dto = breakwater::telemetry;
constexpr qsizetype ledgerLimit = static_cast<qsizetype>(dto::maximum_trace_readings);
bool validClock(double value) { return std::isfinite(value) && value >= 0.0; }
double factor(const QJsonObject& sensor, const QString& originalUnit) {
    if (originalUnit == sensor.value("unit").toString()) return 1.0;
    const auto quantity = sensor.value("quantity").toString();
    if (quantity == "inventory" && originalUnit == "L") return 0.001;
    if (quantity == "level" && originalUnit == "cm") return 0.01;
    if ((quantity == "source_flow" || quantity == "route_flow") && originalUnit == "L/s") return 3.6;
    if ((quantity == "source_flow" || quantity == "route_flow") && originalUnit == "L/h") return 0.001;
    throw std::invalid_argument("Unsupported measurement conversion.");
}
bool validUtc(const QString& value) {
    static const QRegularExpression shape(QStringLiteral("^[0-9]{4}-[0-9]{2}-[0-9]{2}T[0-9]{2}:[0-9]{2}:[0-9]{2}(?:\\.[0-9]{1,3})?Z\\z"));
    return shape.match(value).hasMatch() && value.mid(11, 2).toInt() < 24 &&
        QDateTime::fromString(value, Qt::ISODateWithMs).isValid();
}
}

struct TelemetryStream::State {
    struct Pending { QJsonObject frame; double received = 0.0; QString utc; qsizetype bytes = 0; };
    struct TraceRow { QJsonObject sample; qsizetype bytes = 0; };
    struct Arrival { double monotonic = 0.0; qsizetype bytes = 0, readings = 0; };
    QJsonObject configuration, context;
    dto::SensorConfiguration decoded;
    QMap<QString, QJsonObject> sensors, latest;
    QMap<QString, QString> lastRejection;
    QSet<QString> simultaneousConflicts;
    QMap<quint64, QByteArray> identities;
    QMap<quint64, Pending> reorder;
    std::deque<TraceRow> traces;
    std::deque<Arrival> arrivals;
    QMap<QString, quint64> counters;
    QByteArray recorded;
    QString hash, stream, epoch, mode = "synthetic_live", errorCode, errorMessage;
    double now = 0.0, watermark = 0.0, lastReceive = -1.0, lastValidReceive = -1.0, rate = 1.0;
    qsizetype traceBytes = 0, reorderBytes = 0, ingressBytes = 0, ingressReadings = 0;
    quint64 retiredSequence = 0;
    bool configured = false, started = false, ended = false, failed = false, paused = false;
    bool ledgerRetired = false, recordingTruncated = false;

    void count(const QString& name, quint64 amount = 1) {
        counters[name] = std::min<quint64>(dto::maximum_sequence, counters.value(name) + amount);
    }
    void appendTrace(QJsonObject sample) {
        const auto bytes = QJsonDocument(sample).toJson(QJsonDocument::Compact).size() + 1;
        while (!traces.empty() && (traces.size() >= dto::maximum_trace_readings ||
                traceBytes + bytes > static_cast<qsizetype>(dto::maximum_trace_bytes))) {
            traceBytes -= traces.front().bytes; traces.pop_front(); count("trace_dropped_readings");
        }
        traces.push_back({std::move(sample), bytes}); traceBytes += bytes;
    }
    void readings(const Pending& pending, bool mayUpdate, const QString& disposition) {
        const auto frame = pending.frame;
        for (const auto& value : frame.value("payload").toObject().value("readings").toArray()) {
            const auto reading = value.toObject(), sensor = sensors.value(reading.value("sensor_id").toString());
            const auto id = reading.value("sensor_id").toString();
            QJsonValue canonical = reading.value("value");
            if (canonical.isDouble()) canonical = canonical.toDouble() * factor(sensor, reading.value("unit").toString());
            QJsonObject sample{{"sensor_id", id}, {"frame_sequence", frame.value("sequence")},
                {"stream_id", frame.value("stream_id")}, {"epoch_id", frame.value("epoch_id")},
                {"event_time_s", reading.value("event_time_s")}, {"acquisition_start_s", reading.value("acquisition_start_s")},
                {"acquisition_end_s", reading.value("acquisition_end_s")}, {"received_monotonic_s", pending.received},
                {"received_utc", pending.utc}, {"original_value", reading.value("value")}, {"original_unit", reading.value("unit")},
                {"canonical_value", canonical}, {"canonical_unit", sensor.value("unit")},
                {"reported_quality", reading.value("quality")}, {"disposition", disposition}};
            const auto previous = latest.value(id);
            if (mayUpdate && (previous.isEmpty() || sample.value("event_time_s").toDouble() >= previous.value("event_time_s").toDouble())) {
                if (!previous.isEmpty() && sample.value("event_time_s") == previous.value("event_time_s") &&
                    sample.value("canonical_value") != previous.value("canonical_value")) {
                    simultaneousConflicts.insert(id);
                    sample["disposition"] = "inconsistent";
                } else {
                    if (previous.isEmpty() || sample.value("event_time_s").toDouble() > previous.value("event_time_s").toDouble())
                        simultaneousConflicts.remove(id);
                    latest[id] = sample; lastRejection.remove(id);
                }
            } else if (mayUpdate) sample["disposition"] = "history_only";
            if (!mayUpdate && (disposition == "future" || disposition == "rejected")) lastRejection[id] = disposition;
            appendTrace(std::move(sample));
        }
    }
    void flush(bool force = false) {
        QList<quint64> ready;
        const auto cutoff = std::max(now, watermark) - decoded.policy.reorder_window_s;
        for (auto it = reorder.cbegin(); it != reorder.cend(); ++it)
            if (force || it.value().frame.value("event_time_s").toDouble() <= cutoff) ready.push_back(it.key());
        std::sort(ready.begin(), ready.end(), [this](quint64 left, quint64 right) {
            const auto a = reorder.value(left).frame.value("event_time_s").toDouble();
            const auto b = reorder.value(right).frame.value("event_time_s").toDouble();
            return a == b ? left < right : a < b;
        });
        for (auto sequence : ready) {
            const auto pending = reorder.take(sequence); reorderBytes -= pending.bytes;
            readings(pending, true, "reordered"); count("reorder_flushed");
        }
    }
    void fault(const QString& code, const QString& message) {
        if (failed) return;
        failed = true; errorCode = code; errorMessage = message;
        for (const auto& pending : reorder) readings(pending, false, "history_only");
        count("reorder_dropped_frames", static_cast<quint64>(reorder.size())); reorder.clear(); reorderBytes = 0;
    }
    bool clocks(double event, double monotonic) {
        if (!validClock(event) || !validClock(monotonic) || (lastReceive >= 0.0 && monotonic < lastReceive) ||
            (started && event + 1e-9 < now)) {
            fault("receiver_clock_invalid", "Receiver event and monotonic clocks must be finite, nonnegative and nondecreasing."); return false;
        }
        now = event; lastReceive = monotonic;
        while (!arrivals.empty() && arrivals.front().monotonic <= monotonic - 1.0) {
            ingressBytes -= arrivals.front().bytes; ingressReadings -= arrivals.front().readings; arrivals.pop_front();
        }
        flush(); return true;
    }
    void record(const QByteArray& bytes) {
        if (recordingTruncated) { count("recording_dropped_frames"); return; }
        if (recorded.size() + bytes.size() > static_cast<qsizetype>(dto::maximum_trace_bytes)) {
            recordingTruncated = true; count("recording_dropped_frames"); return;
        }
        recorded.append(bytes);
    }
    QSet<QString> disagreements() const {
        auto result = simultaneousConflicts;
        QMap<QString, QList<QString>> groups;
        for (auto it = sensors.cbegin(); it != sensors.cend(); ++it)
            if (it.value().value("enabled").toBool()) groups[it.value().value("group_id").toString()].push_back(it.key());
        for (const auto& ids : groups) for (qsizetype a = 0; a < ids.size(); ++a) for (qsizetype b = a + 1; b < ids.size(); ++b) {
            const auto left = latest.value(ids[a]), right = latest.value(ids[b]);
            if (left.isEmpty() || right.isEmpty() || left.value("reported_quality") != "good" || right.value("reported_quality") != "good") continue;
            const auto leftTime = left.value("event_time_s").toDouble(), rightTime = right.value("event_time_s").toDouble();
            if (std::abs(leftTime - rightTime) > decoded.policy.max_skew_s ||
                now - leftTime > sensors.value(ids[a]).value("max_age_s").toDouble() ||
                now - rightTime > sensors.value(ids[b]).value("max_age_s").toDouble()) continue;
            const auto l = left.value("canonical_value"), r = right.value("canonical_value");
            const auto bound = sensors.value(ids[a]).value("uncertainty_bound").toDouble() + sensors.value(ids[b]).value("uncertainty_bound").toDouble();
            const bool conflict = l.isString() ? l != r : std::abs(l.toDouble() - r.toDouble()) > bound;
            if (conflict) { result.insert(ids[a]); result.insert(ids[b]); }
        }
        return result;
    }
};

TelemetryStream::TelemetryStream() : state_(std::make_unique<State>()) {}
TelemetryStream::~TelemetryStream() = default;
TelemetryStream::TelemetryStream(TelemetryStream&&) noexcept = default;
TelemetryStream& TelemetryStream::operator=(TelemetryStream&&) noexcept = default;
void TelemetryStream::configure(const QJsonObject& configuration, const QJsonObject& context) {
    if (!context.value("network").isObject() || !context.value("physical_hash").isString())
        throw std::invalid_argument("Monitoring requires registered network and physical-hash context.");
    auto replacement = std::make_unique<State>();
    replacement->context = context;
    for (const auto* name : {"clock", "previous_frame", "command"}) replacement->context.remove(name);
    replacement->context["configuration"] = configuration;
    replacement->decoded = parse_sensor_configuration(configuration, replacement->context);
    replacement->configuration = configuration; replacement->hash = sensor_semantic_hash(configuration);
    for (const auto& value : configuration.value("sensors").toArray()) {
        const auto sensor = value.toObject(); replacement->sensors.insert(sensor.value("sensor_id").toString(), sensor);
    }
    replacement->configured = true; state_ = std::move(replacement);
}
void TelemetryStream::beginEpoch(const QString& mode) {
    if (!state_->configured) throw std::invalid_argument("Configure sensors before beginning a telemetry epoch.");
    if (mode != "synthetic_live" && mode != "replay" && mode != "historical") throw std::invalid_argument("Unsupported receiver mode.");
    const auto configuration = state_->configuration, context = state_->context;
    configure(configuration, context); state_->mode = mode;
}
bool TelemetryStream::ingest(const QByteArray& raw, double nowEventS, double receivedMonotonicS, const QString& receivedUtc) {
    auto& s = *state_;
    if (s.failed) return false;
    s.count("frames_received");
    if (!s.configured) { s.fault("configuration_unavailable", "No registered sensor configuration is available."); return false; }
    if (!validUtc(receivedUtc)) { s.fault("receiver_time_invalid", "A receiver-owned UTC timestamp is required."); return false; }
    if (!s.clocks(nowEventS, receivedMonotonicS)) return false;
    auto bytes = raw; if (!bytes.endsWith('\n')) bytes.append('\n');
    if (bytes.size() > static_cast<qsizetype>(dto::maximum_frame_bytes)) {
        s.count("malformed_frames"); s.fault("frame_size_limit", "Telemetry frame exceeds 256 KiB including its newline."); return false;
    }
    if (s.mode == "synthetic_live" && s.ingressBytes + bytes.size() > static_cast<qsizetype>(dto::maximum_ingress_bytes_per_s)) {
        s.count("rate_faults"); s.fault("ingress_byte_limit", "Telemetry exceeds the 1 MiB/s receiver ingress limit."); return false;
    }
    try {
        const auto frame = decode_document(bytes, static_cast<qsizetype>(dto::maximum_frame_bytes));
        // Configuration/network semantics were validated atomically at configure.
        // Reuse the immutable registry instead of reparsing the full network and
        // configuration several times per incoming heartbeat or reading batch.
        const auto decoded = parse_stream_frame(frame, {{"physical_hash", s.configuration.value("physical_hash")}});
        if (frame.value("configuration_hash") != s.hash) throw std::invalid_argument("Configuration identity mismatch.");
        for (const auto& reading : decoded.readings) {
            const auto sensor = s.sensors.value(QString::fromStdString(reading.sensor_id));
            if (sensor.isEmpty() || !sensor.value("enabled").toBool()) throw std::invalid_argument("Unavailable sensor reference.");
            const bool pump = sensor.value("quantity") == "pump_state";
            if (pump != std::holds_alternative<std::string>(reading.value)) throw std::invalid_argument("Wrong reading value type.");
            const auto multiplier = factor(sensor, QString::fromStdString(reading.unit));
            if (!pump && !std::isfinite(std::get<double>(reading.value) * multiplier)) throw std::invalid_argument("Conversion overflow.");
        }
        const auto readingCount = static_cast<qsizetype>(decoded.readings.size());
        if (s.mode == "synthetic_live" && s.ingressReadings + readingCount > static_cast<qsizetype>(dto::maximum_ingress_readings_per_s)) {
            s.count("rate_faults"); s.fault("ingress_reading_limit", "Telemetry exceeds the 1,024 readings/s receiver ingress limit."); return false;
        }
        // Acquisition quotas constrain an external live process, not accelerated
        // playback of already bounded local evidence. Replay/file scheduling has
        // its own bounded worker slices; all frame/trace/reorder caps still apply.
        if (s.mode == "synthetic_live") {
            s.arrivals.push_back({receivedMonotonicS, bytes.size(), readingCount});
            s.ingressBytes += bytes.size(); s.ingressReadings += readingCount;
        }
        s.count("readings_received", static_cast<quint64>(readingCount));
        const auto stream = QString::fromStdString(decoded.stream_id), epoch = QString::fromStdString(decoded.epoch_id);
        if (!s.started) {
            if (decoded.type != "start") { s.fault("start_required", "A new epoch must begin with its sequence-zero start frame."); return false; }
            if (s.mode != "historical" && QString::fromStdString(decoded.mode) != s.mode) {
                s.fault("mode_mismatch", "Publisher mode differs from the receiver-owned mode."); return false;
            }
            s.started = true; s.stream = stream; s.epoch = epoch; s.now = std::max(nowEventS, decoded.event_time_s);
            s.watermark = decoded.event_time_s; s.paused = decoded.clock.paused; s.rate = decoded.clock.clock_rate;
        } else if (stream != s.stream || epoch != s.epoch) {
            s.count("old_epoch_frames");
            if (decoded.type == "readings") s.readings({frame, receivedMonotonicS, receivedUtc, bytes.size()}, false, "old_epoch");
            return true;
        }
        const auto digest = QCryptographicHash::hash(QJsonDocument(frame).toJson(QJsonDocument::Compact), QCryptographicHash::Sha256);
        const auto identity = s.identities.constFind(decoded.sequence);
        if (identity != s.identities.cend()) {
            if (*identity == digest) { s.count("duplicate_frames"); return true; }
            s.count("conflicting_duplicate_frames"); s.fault("sequence_conflict", "The same stream/epoch/sequence has conflicting frame content."); return false;
        }
        if (s.ledgerRetired && decoded.sequence <= s.retiredSequence) {
            s.count("sequence_identity_expired"); s.fault("sequence_identity_expired", "This old sequence is outside the bounded identity ledger; restart/resynchronize before accepting it."); return false;
        }
        if (s.ended) { s.fault("frame_after_end", "The epoch already ended; start a separately authorized new epoch."); return false; }
        s.identities.insert(decoded.sequence, digest);
        while (s.identities.size() > ledgerLimit) {
            s.retiredSequence = std::max(s.retiredSequence, s.identities.firstKey()); s.identities.erase(s.identities.begin()); s.ledgerRetired = true;
        }
        s.lastValidReceive = receivedMonotonicS;
        s.record(bytes);
        State::Pending pending{frame, receivedMonotonicS, receivedUtc, bytes.size()};
        if (decoded.event_time_s > s.now + s.decoded.policy.future_allowance_s) {
            s.count("future_frames"); if (decoded.type == "readings") s.readings(pending, false, "future"); return true;
        }
        if (decoded.event_time_s < s.watermark - s.decoded.policy.reorder_window_s) {
            s.count("late_frames"); if (decoded.type == "readings") s.readings(pending, false, "history_only"); return true;
        }
        if (decoded.event_time_s < s.watermark) {
            s.count("buffered_frames");
            if (decoded.type == "readings") {
                if (s.reorder.size() >= static_cast<qsizetype>(dto::maximum_reorder_batches) ||
                    s.reorderBytes + bytes.size() > static_cast<qsizetype>(dto::maximum_reorder_bytes)) {
                    s.count("reorder_limit_frames"); s.readings(pending, false, "rejected");
                    s.fault("reorder_limit", "The bounded telemetry reorder queue is full; resynchronization is required."); return false;
                }
                s.reorder.insert(decoded.sequence, pending); s.reorderBytes += bytes.size();
            }
            return true;
        }
        s.watermark = std::max(s.watermark, decoded.event_time_s); s.count("accepted_frames");
        if (decoded.type == "readings") s.readings(pending, true, s.mode == "synthetic_live" ? "accepted" : "history_only");
        else if (decoded.type == "heartbeat" || decoded.type == "start") { s.paused = decoded.clock.paused; s.rate = decoded.clock.clock_rate; }
        else if (decoded.type == "ack" && decoded.acknowledgement) { s.paused = decoded.acknowledgement->clock.paused; s.rate = decoded.acknowledgement->clock.clock_rate; }
        else if (decoded.type == "end") { s.flush(true); s.ended = true; }
        else if (decoded.type == "error") { s.fault("source_reported_error", "The source reported an error; current monitoring is unavailable."); return false; }
        s.flush(); return true;
    } catch (const std::exception&) {
        s.count("malformed_frames"); s.fault("invalid_frame", "The source returned invalid or incompatible G1 telemetry."); return false;
    }
}
void TelemetryStream::advance(double nowEventS, double receivedMonotonicS) {
    if (!state_->failed) state_->clocks(nowEventS, receivedMonotonicS);
}
QJsonObject TelemetryStream::report() const {
    const auto& s = *state_;
    const bool sourceUnavailable = !s.started || s.failed || (s.mode == "synthetic_live" &&
        (s.ended || s.lastValidReceive < 0.0 ||
         (s.lastReceive >= 0.0 && s.lastReceive - s.lastValidReceive > s.decoded.policy.source_timeout_s)));
    const auto conflicts = s.disagreements(); QJsonArray sensors;
    for (auto it = s.sensors.cbegin(); it != s.sensors.cend(); ++it) {
        const auto definition = it.value(), sample = s.latest.value(it.key());
        QString receiver = "missing", status = s.lastRejection.value(it.key(), "missing");
        if (!definition.value("enabled").toBool()) status = "disabled";
        else if (!sample.isEmpty()) {
            receiver = s.now - sample.value("event_time_s").toDouble() > definition.value("max_age_s").toDouble() ? "stale" : "good";
            if (conflicts.contains(it.key())) receiver = "inconsistent";
            if (sample.value("reported_quality") != "good" && receiver == "good") receiver = "rejected";
            status = receiver;
            if (sourceUnavailable && receiver == "good") status = "source_unavailable";
        }
        auto row = sample; row["sensor_id"] = it.key(); row["label"] = definition.value("label");
        row["quantity"] = definition.value("quantity"); row["unit"] = definition.value("unit");
        row["attachment"] = definition.value("attachment"); row["group_id"] = definition.value("group_id"); row["role"] = definition.value("role");
        row["status"] = status; row["receiver_status"] = receiver; row["source_available"] = !sourceUnavailable;
        row["age_s"] = sample.isEmpty() ? QJsonValue() : QJsonValue(std::max(0.0, s.now - sample.value("event_time_s").toDouble()));
        row["mapping_available"] = false; row["mapping_status"] = "unavailable";
        row["mapping_unavailable_reason"] = "Observation-to-forecast mapping is not implemented by G2 monitoring.";
        row["actionable"] = false;
        if (!sample.isEmpty() && sample.value("canonical_value").isDouble()) {
            const auto value = sample.value("canonical_value").toDouble();
            row["outside_instrument_range"] = (definition.contains("instrument_min") && value < definition.value("instrument_min").toDouble()) ||
                (definition.contains("instrument_max") && value > definition.value("instrument_max").toDouble());
            const auto calibration = definition.value("calibration").toObject();
            if (calibration.value("kind") == "level_volume_piecewise_linear") {
                const auto knots = calibration.value("knots").toArray();
                const bool outside = value < knots.first().toObject().value("level_m").toDouble() || value > knots.last().toObject().value("level_m").toDouble();
                row["outside_calibration_domain"] = outside;
                if (outside) {
                    row["mapping_unavailable_reason"] = "The level lies outside its declared calibration domain.";
                    if (receiver == "good" && !sourceUnavailable) row["status"] = "outside_calibration_domain";
                }
            }
        }
        sensors.append(row);
    }
    QJsonObject counters; for (auto it = s.counters.cbegin(); it != s.counters.cend(); ++it) counters[it.key()] = static_cast<qint64>(it.value());
    QJsonObject limits{{"frame_bytes", static_cast<qint64>(dto::maximum_frame_bytes)}, {"reorder_batches", static_cast<qint64>(dto::maximum_reorder_batches)},
        {"reorder_bytes", static_cast<qint64>(dto::maximum_reorder_bytes)}, {"trace_readings", static_cast<qint64>(dto::maximum_trace_readings)},
        {"trace_bytes", static_cast<qint64>(dto::maximum_trace_bytes)}, {"identity_frames", ledgerLimit},
        {"recording_bytes", static_cast<qint64>(dto::maximum_trace_bytes)},
        {"ingress_readings_per_s", static_cast<qint64>(dto::maximum_ingress_readings_per_s)},
        {"ingress_bytes_per_s", static_cast<qint64>(dto::maximum_ingress_bytes_per_s)}};
    const auto status = s.failed ? QStringLiteral("fault") : s.ended ? QStringLiteral("ended") : !s.started ? QStringLiteral("awaiting_start") :
        sourceUnavailable ? QStringLiteral("source_unavailable") : s.paused ? QStringLiteral("paused") : QStringLiteral("running");
    return {{"status", status}, {"configured", s.configured}, {"active", s.started && !s.ended && !s.failed}, {"ended", s.ended},
        {"stream_id", s.stream}, {"epoch_id", s.epoch}, {"mode", s.mode}, {"configuration_hash", s.hash},
        {"event_time_s", s.now}, {"watermark_event_time_s", s.watermark}, {"paused", s.paused}, {"clock_rate", s.rate},
        {"source_available", !sourceUnavailable}, {"sensors", sensors}, {"counters", counters}, {"limits", limits},
        {"ingress_scope", "external_synthetic_live_receive_monotonic"}, {"ingress_enforced", s.mode == "synthetic_live"},
        {"trace_readings", static_cast<qint64>(s.traces.size())}, {"trace_bytes", s.traceBytes},
        {"reorder_batches", static_cast<qint64>(s.reorder.size())}, {"reorder_bytes", s.reorderBytes},
        {"identity_frames", static_cast<qint64>(s.identities.size())}, {"recording_bytes", s.recorded.size()},
        {"recording_truncated", s.recordingTruncated},
        {"recording_complete", s.started && s.ended && !s.failed && !s.recordingTruncated},
        {"recording_prefix", s.started && !s.ended && !s.recordingTruncated},
        {"error_code", s.errorCode}, {"error_message", s.errorMessage}, {"actionable", false}, {"forecast_integration", "not_implemented"}};
}
QJsonArray TelemetryStream::trace(int maximum) const {
    QJsonArray result; const auto& rows = state_->traces;
    const auto count = maximum < 0 ? rows.size() : std::min(rows.size(), static_cast<std::size_t>(maximum));
    for (auto it = rows.end() - static_cast<std::ptrdiff_t>(count); it != rows.end(); ++it) result.append(it->sample);
    return result;
}
QByteArray TelemetryStream::recording() const {
    if (state_->recordingTruncated) throw std::runtime_error("The bounded recording was truncated; restart a shorter recording before export.");
    return state_->recorded;
}
bool TelemetryStream::faulted() const { return state_->failed; }
QString TelemetryStream::streamId() const { return state_->stream; }
QString TelemetryStream::epochId() const { return state_->epoch; }
QString TelemetryStream::mode() const { return state_->mode; }
double TelemetryStream::eventTime() const { return state_->now; }
bool TelemetryStream::paused() const { return state_->paused; }
double TelemetryStream::clockRate() const { return state_->rate; }

} // namespace breakwater::desktop::telemetry
