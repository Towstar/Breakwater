#include "telemetry/TelemetryStream.hpp"
#include "telemetry/TelemetryContracts.hpp"
#include "breakwater/telemetry.hpp"
#include <QFile>
#include <QJsonDocument>
#include <QtTest>
#include <limits>
#include <stdexcept>

using namespace breakwater::desktop::telemetry;
namespace {
const QString utc = QStringLiteral("2026-11-08T00:00:00.000Z");
QJsonObject jsonFile(const QString& relative) {
    QFile file(QStringLiteral(BREAKWATER_SOURCE_DIR "/contracts/telemetry/") + relative);
    if (!file.open(QIODevice::ReadOnly)) throw std::runtime_error("Missing telemetry fixture.");
    return decode_document(file.readAll());
}
struct Fixture {
    QJsonObject configuration = jsonFile("examples/canonical-sensors.json");
    QJsonObject context = jsonFile("fixtures/validation-cases.json").value("contexts").toObject().value("canonical").toObject();
    QString stream = "plant-1", epoch = "epoch-1";
    mutable QJsonObject hashedConfiguration;
    mutable QString configurationHash;
    QString hash() const {
        // Stress tests exercise the actual stream bounds, not repeated schema
        // validation/hashing of the same immutable fixture on the sending side.
        if (configurationHash.isEmpty() || configuration != hashedConfiguration) {
            configurationHash = sensor_semantic_hash(configuration);
            hashedConfiguration = configuration;
        }
        return configurationHash;
    }
    QJsonObject frame(const QString& type, quint64 sequence, double event, QJsonObject payload = {}) const {
        if ((type == "start" || type == "heartbeat") && payload.isEmpty()) payload = {{"paused", false}, {"clock_rate", 1.0}};
        if (type == "start" && !payload.contains("mode")) payload["mode"] = "synthetic_live";
        return {{"telemetry_version", 1}, {"kind", "stream_frame"}, {"type", type}, {"stream_id", stream}, {"epoch_id", epoch},
            {"sequence", static_cast<qint64>(sequence)}, {"configuration_hash", hash()},
            {"physical_hash", configuration.value("physical_hash")}, {"event_time_s", event}, {"payload", payload}};
    }
    void configure(TelemetryStream& target, const QString& mode = "synthetic_live") const {
        target.configure(configuration, context); target.beginEpoch(mode);
    }
    void inventorySensors(int count) {
        const auto prototype = configuration.value("sensors").toArray().first().toObject(); QJsonArray sensors;
        for (int i = 0; i < count; ++i) {
            auto sensor = prototype; const auto id = QStringLiteral("inventory-%1").arg(i);
            sensor["sensor_id"] = id; sensor["channel_id"] = id; sensor["group_id"] = id; sensor["label"] = id;
            sensors.append(sensor);
        }
        configuration["sensors"] = sensors;
    }
};
QJsonObject reading(const QString& id, double event, QJsonValue value, const QString& unit = "m3", const QString& quality = "good") {
    return {{"sensor_id", id}, {"event_time_s", event}, {"acquisition_start_s", event}, {"acquisition_end_s", event},
        {"value", value}, {"unit", unit}, {"quality", quality}};
}
QJsonObject batch(const Fixture& f, quint64 sequence, double event, const QJsonArray& readings) {
    return f.frame("readings", sequence, event, {{"readings", readings}});
}
QByteArray bytes(const QJsonObject& document) { return QJsonDocument(document).toJson(QJsonDocument::Compact) + '\n'; }
bool ingest(TelemetryStream& target, const QJsonObject& frame, double now, double receive = -1.0) {
    return target.ingest(bytes(frame), now, receive < 0.0 ? now : receive, utc);
}
QJsonObject sensor(const TelemetryStream& target, const QString& id = "tank-a-volume") {
    for (const auto& row : target.report().value("sensors").toArray()) if (row.toObject().value("sensor_id") == id) return row.toObject();
    return {};
}
qint64 counter(const TelemetryStream& target, const QString& id) { return target.report().value("counters").toObject().value(id).toInteger(); }
}

class TelemetryStreamTests final : public QObject {
    Q_OBJECT
private slots:
    void configurationIsBoundAndAtomic() {
        Fixture f; TelemetryStream stream;
        QVERIFY_THROWS_EXCEPTION(std::invalid_argument, stream.configure(f.configuration, {}));
        f.configure(stream); QVERIFY(ingest(stream, f.frame("start", 0, 0), 0));
        auto invalid = f.configuration; invalid["physical_hash"] = QString(64, 'a');
        QVERIFY_THROWS_EXCEPTION(std::invalid_argument, stream.configure(invalid, f.context));
        QCOMPARE(stream.streamId(), QStringLiteral("plant-1"));
        QVERIFY(ingest(stream, batch(f, 1, 1, {reading("tank-a-volume", 1, 80)}), 1));
        QCOMPARE(sensor(stream).value("canonical_value").toDouble(), 80.0);
    }
    void renamedNetworkUsesItsRegisteredAttachmentIdentities() {
        Fixture f; f.configuration = jsonFile("examples/custom-sensors.json");
        f.context = jsonFile("fixtures/validation-cases.json").value("contexts").toObject().value("custom").toObject();
        TelemetryStream stream; f.configure(stream); QVERIFY(ingest(stream, f.frame("start", 0, 0), 0));
        QVERIFY(ingest(stream, batch(f, 1, 1, {reading("tank-a-volume", 1, 91)}), 1));
        QCOMPARE(sensor(stream).value("attachment").toObject().value("id"), QJsonValue("north_plant_tank_a"));
        QCOMPARE(sensor(stream).value("canonical_value").toDouble(), 91.0);
    }
    void startIsRequiredAndAnchorsRelativeTime() {
        Fixture f; TelemetryStream stream; f.configure(stream);
        QVERIFY(!ingest(stream, f.frame("heartbeat", 1, 0), 0));
        QCOMPARE(stream.report().value("error_code"), QJsonValue("start_required"));
        stream.beginEpoch("synthetic_live"); QVERIFY(ingest(stream, f.frame("start", 0, 100), 0));
        QCOMPARE(stream.eventTime(), 100.0); QVERIFY(!stream.report().value("actionable").toBool());
        QVERIFY(!ingest(stream, f.frame("heartbeat", 1, 100), 99, 1));
        QCOMPARE(stream.report().value("error_code"), QJsonValue("receiver_clock_invalid"));
    }
    void originalAndCanonicalValuesKeepNegativeAndOverflowEvidence() {
        Fixture f; auto sensors = f.configuration.value("sensors").toArray(); auto inventory = sensors[0].toObject();
        inventory["instrument_min"] = 0; inventory["instrument_max"] = 130; sensors[0] = inventory; f.configuration["sensors"] = sensors;
        TelemetryStream stream; f.configure(stream); QVERIFY(ingest(stream, f.frame("start", 0, 0), 0));
        QVERIFY(ingest(stream, batch(f, 1, 1, {reading("tank-a-volume", 1, 80000, "L")}), 1));
        QCOMPARE(sensor(stream).value("original_value").toDouble(), 80000.0); QCOMPARE(sensor(stream).value("canonical_value").toDouble(), 80.0);
        QVERIFY(ingest(stream, batch(f, 2, 2, {reading("tank-a-volume", 2, -1000, "L")}), 2));
        QCOMPARE(sensor(stream).value("canonical_value").toDouble(), -1.0); QVERIFY(sensor(stream).value("outside_instrument_range").toBool());
        QVERIFY(ingest(stream, batch(f, 3, 3, {reading("tank-a-volume", 3, 132)}), 3));
        QCOMPARE(sensor(stream).value("canonical_value").toDouble(), 132.0); QVERIFY(sensor(stream).value("outside_instrument_range").toBool());
        QVERIFY(!sensor(stream).value("mapping_available").toBool());
    }
    void pumpFeedbackIsTypedAndNotACommand() {
        Fixture f; TelemetryStream stream; f.configure(stream); QVERIFY(ingest(stream, f.frame("start", 0, 0), 0));
        QVERIFY(ingest(stream, batch(f, 1, 1, {reading("main-pump-feedback", 1, "unavailable", "state")}), 1));
        QCOMPARE(sensor(stream, "main-pump-feedback").value("canonical_value"), QJsonValue("unavailable"));
        QVERIFY(!stream.report().contains("commands"));
        QVERIFY(!ingest(stream, batch(f, 2, 2, {reading("main-pump-feedback", 2, 1.0, "state")}), 2));
        QCOMPARE(stream.report().value("error_code"), QJsonValue("invalid_frame"));
    }
    void acceptedLevelCalibrationDomainIsVisibleWithoutApplyingAMapper() {
        Fixture f; f.configuration = jsonFile("examples/all-quantities-sensors.json");
        f.context = jsonFile("fixtures/validation-cases.json").value("contexts").toObject().value("all_quantities").toObject();
        TelemetryStream stream; f.configure(stream); QVERIFY(ingest(stream, f.frame("start", 0, 0), 0));
        QVERIFY(ingest(stream, batch(f, 1, 1, {reading("tank-a-level", 1, 250, "cm")}), 1));
        const auto level = sensor(stream, "tank-a-level");
        QCOMPARE(level.value("original_value").toDouble(), 250.0); QCOMPARE(level.value("canonical_value").toDouble(), 2.5);
        QVERIFY(level.value("outside_calibration_domain").toBool()); QVERIFY(!level.value("mapping_available").toBool());
        QCOMPARE(level.value("mapping_status"), QJsonValue("unavailable"));
        QCOMPARE(level.value("status"), QJsonValue("outside_calibration_domain"));
        QVERIFY(!level.contains("volume_m3"));
    }
    void freshnessAndSourceLossAreSeparateFromReportedQuality() {
        Fixture f; TelemetryStream stream; f.configure(stream); QVERIFY(ingest(stream, f.frame("start", 0, 0), 0));
        QVERIFY(ingest(stream, batch(f, 1, 1, {reading("tank-a-volume", 1, 80)}), 1));
        stream.advance(4, 4); QCOMPARE(sensor(stream).value("receiver_status"), QJsonValue("good"));
        stream.advance(6.1, 6.1); QCOMPARE(sensor(stream).value("receiver_status"), QJsonValue("stale"));
        QCOMPARE(sensor(stream).value("reported_quality"), QJsonValue("good"));
        QVERIFY(ingest(stream, f.frame("heartbeat", 2, 6.1), 6.1));
        QVERIFY(stream.report().value("source_available").toBool()); QCOMPARE(sensor(stream).value("receiver_status"), QJsonValue("stale"));
        QVERIFY(ingest(stream, batch(f, 3, 7, {reading("tank-a-volume", 7, 81, "m3", "suspect")}), 7));
        QCOMPARE(sensor(stream).value("reported_quality"), QJsonValue("suspect")); QCOMPARE(sensor(stream).value("receiver_status"), QJsonValue("rejected"));
    }
    void pausedReplayUsesPlaybackClockWithoutHeartbeatTimeout() {
        Fixture f; TelemetryStream stream; f.configure(stream, "replay");
        QVERIFY(ingest(stream, f.frame("start", 0, 0, {{"mode", "replay"}, {"paused", true}, {"clock_rate", 2.0}}), 0));
        QVERIFY(ingest(stream, batch(f, 1, 1, {reading("tank-a-volume", 1, 80)}), 1, 1));
        stream.advance(1, 10000); QCOMPARE(sensor(stream).value("receiver_status"), QJsonValue("good"));
        QVERIFY(stream.report().value("source_available").toBool()); QCOMPARE(stream.report().value("status"), QJsonValue("paused"));
        QVERIFY(!stream.report().value("actionable").toBool());
        stream.advance(7, 10001); QCOMPARE(sensor(stream).value("receiver_status"), QJsonValue("stale"));
        stream.beginEpoch("historical"); QVERIFY(ingest(stream, f.frame("start", 0, 0), 0, 0));
        QVERIFY(ingest(stream, batch(f, 1, 1, {reading("tank-a-volume", 1, 80)}), 1, 1));
        stream.advance(1, 20000); QVERIFY(stream.report().value("source_available").toBool());
    }
    void livePausedSourceCanDisappearWithoutAgingItsMeasurements() {
        Fixture f; TelemetryStream stream; f.configure(stream);
        QVERIFY(ingest(stream, f.frame("start", 0, 0, {{"mode", "synthetic_live"}, {"paused", true}, {"clock_rate", 1.0}}), 0));
        QVERIFY(ingest(stream, batch(f, 1, 1, {reading("tank-a-volume", 1, 80)}), 1));
        stream.advance(1, 10); QCOMPARE(sensor(stream).value("receiver_status"), QJsonValue("good"));
        QCOMPARE(sensor(stream).value("status"), QJsonValue("source_unavailable"));
    }
    void exactDuplicatesAreIdempotentAndConflictsFault() {
        Fixture f; TelemetryStream stream; f.configure(stream); QVERIFY(ingest(stream, f.frame("start", 0, 0), 0));
        auto frame = batch(f, 1, 1, {reading("tank-a-volume", 1, 80)});
        QVERIFY(ingest(stream, frame, 1)); const auto recording = stream.recording();
        QVERIFY(ingest(stream, frame, 1, 1.1)); QCOMPARE(stream.trace().size(), 1); QCOMPARE(stream.recording(), recording);
        QCOMPARE(counter(stream, "duplicate_frames"), 1);
        frame["payload"] = QJsonObject{{"readings", QJsonArray{reading("tank-a-volume", 1, 81)}}};
        QVERIFY(!ingest(stream, frame, 1, 1.2)); QCOMPARE(stream.report().value("error_code"), QJsonValue("sequence_conflict"));
        QCOMPARE(sensor(stream).value("canonical_value").toDouble(), 80.0);
    }
    void oldEpochAndLateSamplesRemainHistorical() {
        Fixture f; TelemetryStream stream; f.configure(stream); QVERIFY(ingest(stream, f.frame("start", 0, 0), 0));
        QVERIFY(ingest(stream, batch(f, 5, 10, {reading("tank-a-volume", 10, 80)}), 10));
        QVERIFY(ingest(stream, batch(f, 1, 1, {reading("tank-a-volume", 1, 20)}), 10, 10.1));
        auto old = f; old.epoch = "old-epoch";
        QVERIFY(ingest(stream, batch(old, 1, 10, {reading("tank-a-volume", 10, 99)}), 10, 10.2));
        QCOMPARE(sensor(stream).value("canonical_value").toDouble(), 80.0); QCOMPARE(stream.trace().size(), 3);
        QCOMPARE(counter(stream, "late_frames"), 1); QCOMPARE(counter(stream, "old_epoch_frames"), 1);
    }
    void reorderWindowFlushesWithoutOverwritingNewerSamples() {
        Fixture f; TelemetryStream stream; f.configure(stream); QVERIFY(ingest(stream, f.frame("start", 0, 0), 0));
        QVERIFY(ingest(stream, batch(f, 2, 2, {reading("tank-a-volume", 2, 80)}), 2));
        QVERIFY(ingest(stream, batch(f, 1, 1, {reading("tank-a-volume", 1, 20), reading("main-pump-feedback", 1, "running", "state")}), 2, 2.1));
        QCOMPARE(stream.report().value("reorder_batches").toInteger(), 1);
        stream.advance(3, 3); QCOMPARE(stream.report().value("reorder_batches").toInteger(), 0);
        QCOMPARE(sensor(stream).value("canonical_value").toDouble(), 80.0);
        QCOMPARE(sensor(stream, "main-pump-feedback").value("canonical_value"), QJsonValue("running"));
        QCOMPARE(counter(stream, "reorder_flushed"), 1);
    }
    void futureEvidenceNeverBecomesCurrentOnRetransmission() {
        Fixture f; TelemetryStream stream; f.configure(stream); QVERIFY(ingest(stream, f.frame("start", 0, 0), 0));
        const auto frame = batch(f, 1, 10, {reading("tank-a-volume", 10, 80)});
        QVERIFY(ingest(stream, frame, 1)); QCOMPARE(sensor(stream).value("status"), QJsonValue("future"));
        QVERIFY(!sensor(stream).contains("canonical_value")); QCOMPARE(counter(stream, "future_frames"), 1);
        QVERIFY(ingest(stream, frame, 10, 10)); QVERIFY(!sensor(stream).contains("canonical_value"));
        QCOMPARE(stream.trace().size(), 1); QCOMPARE(stream.trace().first().toObject().value("disposition"), QJsonValue("future"));
    }
    void malformedAndOversizedFramesFailClosed_data() {
        QTest::addColumn<QByteArray>("raw");
        QTest::newRow("duplicate-key") << QByteArray("{\"kind\":\"stream_frame\",\"kind\":\"stream_frame\"}\n");
        QTest::newRow("nonfinite") << QByteArray("{\"value\":NaN}\n");
        QTest::newRow("invalid-utf8") << QByteArray("{\"value\":\"\xff\"}\n");
        QTest::newRow("oversized") << QByteArray(256 * 1024 + 1, ' ');
    }
    void malformedAndOversizedFramesFailClosed() {
        QFETCH(QByteArray, raw); Fixture f; TelemetryStream stream; f.configure(stream);
        QVERIFY(!stream.ingest(raw, 0, 0, utc)); QVERIFY(stream.faulted()); QVERIFY(stream.trace().isEmpty());
    }
    void ingressBoundsCountBytesAndReadings() {
        Fixture f; TelemetryStream stream; f.configure(stream); QVERIFY(ingest(stream, f.frame("start", 0, 0), 0));
        auto padded = bytes(f.frame("heartbeat", 1, 0)); padded.prepend(QByteArray(256 * 1024 - padded.size(), ' '));
        for (int i = 0; i < 3; ++i) QVERIFY(stream.ingest(padded, 0, 0.01 * (i + 1), utc));
        QVERIFY(!stream.ingest(padded, 0, 0.04, utc)); QCOMPARE(stream.report().value("error_code"), QJsonValue("ingress_byte_limit"));
        f.inventorySensors(256); f.configure(stream); QVERIFY(ingest(stream, f.frame("start", 0, 0), 0)); QJsonArray readings;
        for (int i = 0; i < 256; ++i) readings.append(reading(QStringLiteral("inventory-%1").arg(i), 0, 80));
        for (int i = 1; i <= 4; ++i) QVERIFY(ingest(stream, batch(f, static_cast<quint64>(i), 0, readings), 0, i * 0.01));
        QVERIFY(!ingest(stream, batch(f, 5, 0, readings), 0, 0.05));
        QCOMPARE(stream.report().value("error_code"), QJsonValue("ingress_reading_limit"));
    }
    void acceleratedLocalEvidenceDoesNotUseExternalIngressQuota_data() {
        QTest::addColumn<QString>("mode");
        QTest::newRow("replay") << QStringLiteral("replay");
        QTest::newRow("historical") << QStringLiteral("historical");
    }
    void acceleratedLocalEvidenceDoesNotUseExternalIngressQuota() {
        QFETCH(QString, mode);
        Fixture f; f.inventorySensors(256); TelemetryStream stream; f.configure(stream, mode);
        QVERIFY(ingest(stream, f.frame("start", 0, 0, {{"mode", "replay"}, {"paused", false}, {"clock_rate", 4.0}}), 0, 1));
        for (quint64 frame = 0; frame < 8; ++frame) {
            QJsonArray readings;
            for (int i = 0; i < 256; ++i)
                readings.append(reading(QStringLiteral("inventory-%1").arg(i), static_cast<double>(frame), static_cast<double>(frame)));
            QVERIFY(ingest(stream, batch(f, frame + 1, static_cast<double>(frame), readings), static_cast<double>(frame), 1));
        }
        QCOMPARE(counter(stream, "readings_received"), 2048);
        for (quint64 i = 9; i <= 13; ++i) {
            auto padded = bytes(f.frame("heartbeat", i, static_cast<double>(i)));
            padded.prepend(QByteArray(256 * 1024 - padded.size(), ' '));
            QVERIFY(stream.ingest(padded, static_cast<double>(i), 1, utc));
        }
        QVERIFY(!stream.faulted()); QCOMPARE(counter(stream, "rate_faults"), 0);
        QVERIFY(!stream.report().value("ingress_enforced").toBool());
        QCOMPARE(stream.report().value("ingress_scope"), QJsonValue("external_synthetic_live_receive_monotonic"));
        QVERIFY(stream.report().value("trace_readings").toInteger() <= 32768);
        QVERIFY(stream.report().value("recording_bytes").toInteger() <= 32 * 1024 * 1024);
    }
    void reorderQueueIsBoundedAndOverflowRequiresResynchronization() {
        Fixture f; TelemetryStream stream; f.configure(stream); QVERIFY(ingest(stream, f.frame("start", 0, 0), 0));
        QVERIFY(ingest(stream, batch(f, 100, 11, {reading("tank-a-volume", 11, 80)}), 11));
        for (quint64 i = 1; i <= 16; ++i) QVERIFY(ingest(stream, batch(f, i, 10.5, {reading("tank-a-volume", 10.5, 70)}), 11, 11 + i * 0.001));
        QCOMPARE(stream.report().value("reorder_batches").toInteger(), 16);
        QVERIFY(!ingest(stream, batch(f, 17, 10.5, {reading("tank-a-volume", 10.5, 70)}), 11, 11.1));
        QCOMPARE(stream.report().value("error_code"), QJsonValue("reorder_limit"));
        QCOMPARE(stream.report().value("reorder_batches").toInteger(), 0); QCOMPARE(sensor(stream).value("canonical_value").toDouble(), 80.0);
        QCOMPARE(counter(stream, "reorder_dropped_frames"), 16);
    }
    void redundantDisagreementIsVisibleAndNeverAveraged() {
        Fixture f; auto sensors = f.configuration.value("sensors").toArray(); auto secondary = sensors.first().toObject();
        secondary["sensor_id"] = "secondary-volume"; secondary["source_id"] = "independent-source";
        secondary["channel_id"] = "secondary-volume"; secondary["role"] = "redundant"; sensors.append(secondary); f.configuration["sensors"] = sensors;
        TelemetryStream stream; f.configure(stream); QVERIFY(ingest(stream, f.frame("start", 0, 0), 0));
        QVERIFY(ingest(stream, batch(f, 1, 1, {reading("tank-a-volume", 1, 80), reading("secondary-volume", 1, 90)}), 1));
        QCOMPARE(sensor(stream).value("receiver_status"), QJsonValue("inconsistent"));
        QCOMPARE(sensor(stream, "secondary-volume").value("receiver_status"), QJsonValue("inconsistent"));
        QCOMPARE(sensor(stream).value("canonical_value").toDouble(), 80.0); QCOMPARE(sensor(stream, "secondary-volume").value("canonical_value").toDouble(), 90.0);
        QVERIFY(ingest(stream, batch(f, 2, 2, {reading("tank-a-volume", 2, 81), reading("secondary-volume", 2, 81.1)}), 2));
        QCOMPARE(sensor(stream).value("receiver_status"), QJsonValue("good"));
    }
    void disabledPrimaryDoesNotPromoteRedundantSensor() {
        Fixture f; auto sensors = f.configuration.value("sensors").toArray(); auto primary = sensors.first().toObject();
        auto secondary = primary; secondary["sensor_id"] = "secondary-volume"; secondary["source_id"] = "independent-source";
        secondary["channel_id"] = "secondary-volume"; secondary["role"] = "redundant"; sensors.append(secondary);
        primary["enabled"] = false; sensors[0] = primary; f.configuration["sensors"] = sensors;
        TelemetryStream stream; f.configure(stream); QVERIFY(ingest(stream, f.frame("start", 0, 0), 0));
        QVERIFY(ingest(stream, batch(f, 1, 1, {reading("secondary-volume", 1, 90)}), 1));
        QCOMPARE(sensor(stream).value("status"), QJsonValue("disabled"));
        QCOMPARE(sensor(stream, "secondary-volume").value("role"), QJsonValue("redundant"));
        QVERIFY(!ingest(stream, batch(f, 2, 2, {reading("tank-a-volume", 2, 80)}), 2));
    }
    void recordingPreservesOriginalValidatedFramesAndRefusesTruncation() {
        Fixture f; TelemetryStream stream; f.configure(stream); const auto start = bytes(f.frame("start", 0, 0));
        QVERIFY(stream.ingest(start, 0, 0, utc)); QCOMPARE(stream.recording(), start);
        for (quint64 i = 1; i <= 129; ++i) {
            auto padded = bytes(f.frame("heartbeat", i, static_cast<double>(i)));
            padded.prepend(QByteArray(256 * 1024 - padded.size(), ' '));
            QVERIFY(stream.ingest(padded, static_cast<double>(i), static_cast<double>(i), utc));
        }
        QVERIFY(stream.report().value("recording_truncated").toBool());
        QVERIFY(stream.report().value("recording_bytes").toInteger() <= 32 * 1024 * 1024);
        QVERIFY_THROWS_EXCEPTION(std::runtime_error, stream.recording());
    }
    void onlyAnUnfaultedEndMakesRecordingACompleteEpoch() {
        Fixture f; TelemetryStream stream; f.configure(stream);
        QVERIFY(ingest(stream, f.frame("start", 0, 0), 0));
        QVERIFY(!stream.report().value("recording_complete").toBool()); QVERIFY(stream.report().value("recording_prefix").toBool());
        QVERIFY(!stream.recording().isEmpty());
        QVERIFY(!stream.ingest("{malformed}\n", 1, 1, utc));
        QVERIFY(!stream.report().value("recording_complete").toBool());
        stream.beginEpoch("synthetic_live"); QVERIFY(ingest(stream, f.frame("start", 0, 0), 0));
        QVERIFY(ingest(stream, f.frame("end", 1, 1, {{"reason", "complete"}}), 1));
        QVERIFY(stream.report().value("recording_complete").toBool()); QVERIFY(!stream.report().value("recording_prefix").toBool());
    }
    void traceRingHasActualCountAndByteBounds() {
        Fixture f; f.inventorySensors(256); TelemetryStream stream; f.configure(stream); QVERIFY(ingest(stream, f.frame("start", 0, 0), 0));
        for (quint64 frame = 0; frame < 129; ++frame) {
            QJsonArray readings; for (int i = 0; i < 256; ++i) readings.append(reading(QStringLiteral("inventory-%1").arg(i), static_cast<double>(frame), static_cast<double>(frame)));
            QVERIFY(ingest(stream, batch(f, frame + 1, static_cast<double>(frame), readings), static_cast<double>(frame)));
        }
        QCOMPARE(stream.report().value("trace_readings").toInteger(), 32768); QCOMPARE(stream.trace().size(), 128);
        QCOMPARE(stream.trace(-1).size(), 32768); QCOMPARE(counter(stream, "trace_dropped_readings"), 256);
        QVERIFY(stream.report().value("trace_bytes").toInteger() <= 32 * 1024 * 1024);
        QCOMPARE(stream.trace(-1).first().toObject().value("event_time_s").toDouble(), 1.0);
    }
    void identityLedgerIsBoundedAndExpiredIdentityFailsClosed() {
        Fixture f; f.inventorySensors(1); TelemetryStream stream; f.configure(stream); const auto start = f.frame("start", 0, 0);
        QVERIFY(ingest(stream, start, 0));
        auto heartbeat = f.frame("heartbeat", 1, 1);
        for (quint64 i = 1; i <= 32768; ++i) {
            heartbeat["sequence"] = static_cast<qint64>(i); heartbeat["event_time_s"] = static_cast<double>(i);
            QVERIFY(ingest(stream, heartbeat, static_cast<double>(i)));
        }
        QCOMPARE(stream.report().value("identity_frames").toInteger(), 32768);
        QVERIFY(!ingest(stream, start, 32769)); QCOMPARE(stream.report().value("error_code"), QJsonValue("sequence_identity_expired"));
        QCOMPARE(counter(stream, "conflicting_duplicate_frames"), 0); QCOMPARE(counter(stream, "sequence_identity_expired"), 1);
    }
    void endFlushesPendingRowsAndRestartRetiresCurrentReadings() {
        Fixture f; TelemetryStream stream; f.configure(stream); QVERIFY(ingest(stream, f.frame("start", 0, 0), 0));
        QVERIFY(ingest(stream, batch(f, 2, 2, {reading("tank-a-volume", 2, 80)}), 2));
        QVERIFY(ingest(stream, batch(f, 1, 1.5, {reading("main-pump-feedback", 1.5, "running", "state")}), 2, 2.1));
        QVERIFY(ingest(stream, f.frame("end", 3, 2, {{"reason", "complete"}}), 2, 2.2));
        QVERIFY(stream.report().value("ended").toBool()); QCOMPARE(stream.report().value("reorder_batches").toInteger(), 0);
        QCOMPARE(sensor(stream, "main-pump-feedback").value("canonical_value"), QJsonValue("running"));
        QVERIFY(!ingest(stream, f.frame("heartbeat", 4, 3), 3, 3));
        stream.beginEpoch("synthetic_live"); f.epoch = "epoch-2";
        QVERIFY(ingest(stream, f.frame("start", 0, 0), 0, 0)); QCOMPARE(sensor(stream).value("status"), QJsonValue("missing"));
        QCOMPARE(stream.epochId(), QStringLiteral("epoch-2")); QCOMPARE(stream.trace().size(), 0);
    }
    void changedIdentityAndInvalidReceiverClocksCannotPublish() {
        Fixture f; TelemetryStream stream; f.configure(stream); auto start = f.frame("start", 0, 0); start["configuration_hash"] = QString(64, 'a');
        QVERIFY(!ingest(stream, start, 0)); QCOMPARE(stream.report().value("error_code"), QJsonValue("invalid_frame"));
        f.configure(stream); QVERIFY(ingest(stream, f.frame("start", 0, 0), 0));
        stream.advance(std::numeric_limits<double>::quiet_NaN(), 1); QVERIFY(stream.faulted());
        QCOMPARE(stream.eventTime(), 0.0);
    }
};
QTEST_GUILESS_MAIN(TelemetryStreamTests)
#include "telemetry_stream_tests.moc"
