#include "telemetry/TelemetryController.hpp"
#include "telemetry/TelemetryContracts.hpp"
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QSaveFile>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QtTest>
#include <cmath>

using namespace breakwater::desktop::telemetry;
namespace {
QJsonObject sourceJson(const QString& relative) {
    QFile file(QStringLiteral(BREAKWATER_SOURCE_DIR) + "/" + relative);
    if (!file.open(QIODevice::ReadOnly)) throw std::runtime_error("Missing lifecycle fixture");
    return decode_document(file.readAll());
}
QJsonObject configuration() { return sourceJson("contracts/telemetry/examples/canonical-sensors.json"); }
QJsonObject context() {
    const auto project = sourceJson("contracts/telemetry/examples/canonical-project-v1.json");
    return {{"network", project.value("network")}, {"project", project}, {"physical_hash", configuration().value("physical_hash")}};
}
QString python() { return QStringLiteral(BREAKWATER_TEST_PYTHON); }
QString fixture() { return QStringLiteral(BREAKWATER_SOURCE_DIR "/desktop/tests/telemetry_fixtures/publisher_fixture.py"); }
bool writeBytes(const QString& path, const QByteArray& value) {
    QSaveFile file(path);
    return file.open(QIODevice::WriteOnly) && file.write(value) == value.size() && file.commit();
}
QByteArray frame(const QString& type, int sequence, double event, const QJsonObject& payload) {
    return QJsonDocument(QJsonObject{{"telemetry_version", 1}, {"kind", "stream_frame"}, {"type", type},
        {"stream_id", "recorded-fixture"}, {"epoch_id", "recorded-epoch"}, {"sequence", sequence},
        {"physical_hash", configuration().value("physical_hash")}, {"configuration_hash", sensor_semantic_hash(configuration())},
        {"event_time_s", event}, {"payload", payload}}).toJson(QJsonDocument::Compact) + '\n';
}
QByteArray delayedRecording() {
    return frame("start", 0, 0, {{"mode", "synthetic_live"}, {"clock_rate", 1}, {"paused", false}}) +
        frame("readings", 1, 0, {{"readings", QJsonArray{QJsonObject{{"sensor_id", "tank-a-volume"}, {"event_time_s", 0},
            {"acquisition_start_s", 0}, {"acquisition_end_s", 0}, {"value", -2.0}, {"unit", "m3"}, {"quality", "good"}}}}}) +
        frame("heartbeat", 2, 10, {{"clock_rate", 1}, {"paused", false}}) + frame("end", 3, 10, {{"reason", "fixture end"}});
}
}

class TelemetryLifecycleTests : public QObject {
    Q_OBJECT
private slots:
    void initTestCase() { QVERIFY2(QFileInfo::exists(python()), "The configured real CPU Python interpreter is required."); }
    void noSensorsCannotStartInput() {
        TelemetryController controller;
        auto empty = configuration(); empty["sensors"] = QJsonArray{};
        controller.configure(empty, context());
        QSignalSpy rejected(&controller, &TelemetryController::failed);
        controller.startProcess(python(), {fixture(), "normal"});
        QTRY_COMPARE(rejected.size(), 1);
        QCOMPARE(controller.status(), "unconfigured"); QVERIFY(!controller.active()); QVERIFY(controller.configurationHash().isEmpty());
    }
    void fragmentedProcessAndResponsiveUi() {
        TelemetryController controller;
        controller.configure(configuration(), context());
        QTRY_COMPARE(controller.status(), "ready");
        int uiTicks = 0; QTimer uiTimer; uiTimer.setInterval(5);
        connect(&uiTimer, &QTimer::timeout, this, [&] { ++uiTicks; }); uiTimer.start();
        controller.startProcess(python(), {fixture(), "fragmented"});
        QTRY_VERIFY_WITH_TIMEOUT(controller.tracePreview().size() >= 2, 10000);
        QVERIFY(controller.active()); QVERIFY(controller.error().isEmpty()); QVERIFY(uiTicks > 3);
        QCOMPARE(controller.mode(), "synthetic_live");
        controller.stop(); QTRY_VERIFY_WITH_TIMEOUT(!controller.active(), 10000);
        QVERIFY(controller.error().isEmpty());
    }
    void pausedPublisherLossRemainsUnavailableAndCanRecover() {
        TelemetryController controller; controller.configure(configuration(), context());
        QTRY_COMPARE(controller.status(), "ready");
        controller.startProcess(python(), {fixture(), "pause_silent"});
        QTRY_VERIFY_WITH_TIMEOUT(controller.tracePreview().size() >= 2, 10000);
        controller.pause(); QTRY_VERIFY(controller.paused());
        const auto frozenTime = controller.eventTime();
        QTRY_COMPARE_WITH_TIMEOUT(controller.status(), "source_unavailable", 10000);
        QVERIFY(controller.active()); QVERIFY(controller.error().isEmpty());
        QVERIFY(!controller.snapshot().value("source_available").toBool());
        QVERIFY(std::abs(controller.eventTime() - frozenTime) < 0.03);
        controller.resume(); QTRY_VERIFY(!controller.paused());
        QTRY_COMPARE(controller.status(), "running");
        QVERIFY(controller.snapshot().value("source_available").toBool());
        controller.stop(); QTRY_VERIFY(!controller.active()); QVERIFY(controller.error().isEmpty());
    }
    void actualRecordingReplayCommandsAndRenderArtifacts() {
        QTemporaryDir directory; QVERIFY(directory.isValid());
        TelemetryController controller; controller.setTimeoutMs(15000); controller.configure(configuration(), context());
        QTRY_COMPARE(controller.status(), "ready");
        controller.startProcess(python(), {fixture(), "normal"});
        QTRY_VERIFY_WITH_TIMEOUT(controller.tracePreview().size() >= 2, 10000);
        controller.pause(); QTRY_VERIFY(controller.paused());
        const auto pausedTime = controller.eventTime(); QTest::qWait(220);
        QVERIFY(std::abs(controller.eventTime() - pausedTime) < 0.03);
        controller.setRate(2); QTRY_COMPARE(controller.rate(), 2.0);
        controller.resume(); QTRY_VERIFY(!controller.paused());
        QTest::qWait(200); QVERIFY(controller.eventTime() > pausedTime + 0.1);
        const auto oldEpoch = controller.snapshot().value("epoch_id").toString();
        controller.restart();
        QTRY_VERIFY_WITH_TIMEOUT(controller.snapshot().value("epoch_id").toString() != oldEpoch, 10000);
        QTRY_VERIFY(controller.tracePreview().size() >= 2);
        controller.stop(); QTRY_VERIFY_WITH_TIMEOUT(!controller.active(), 10000);
        QVERIFY2(controller.error().isEmpty(), qPrintable(controller.error()));
        const auto recorded = QUrl::fromLocalFile(directory.filePath("actual.ndjson"));
        QSignalSpy saved(&controller, &TelemetryController::recordingExported);
        controller.exportRecording(recorded); QTRY_COMPARE(saved.size(), 1);
        QFile file(recorded.toLocalFile()); QVERIFY(file.open(QIODevice::ReadOnly));
        const auto data = file.readAll(); QVERIFY(data.startsWith('{')); QVERIFY(data.contains("\"value\":132"));
        controller.startReplay(recorded);
        QTRY_COMPARE(controller.mode(), "replay");
        QTRY_VERIFY_WITH_TIMEOUT(!controller.active() && !controller.tracePreview().isEmpty(), 10000);
        QVERIFY2(controller.error().isEmpty(), qPrintable(controller.error()));
        QCOMPARE(controller.status(), "replay_finished");
        const auto replayEpoch = controller.snapshot().value("epoch_id").toString();
        QVERIFY(replayEpoch != oldEpoch);
        const auto firstTrace = controller.tracePreview();
        controller.restart();
        QTRY_VERIFY(controller.snapshot().value("epoch_id").toString() != replayEpoch);
        QTRY_VERIFY_WITH_TIMEOUT(!controller.active() && !controller.tracePreview().isEmpty(), 10000);
        QCOMPARE(controller.tracePreview().size(), firstTrace.size());
        const auto artifactDirectory = qEnvironmentVariable("BREAKWATER_TEST_ARTIFACT_DIR");
        if (!artifactDirectory.isEmpty()) {
            QVERIFY(QDir().mkpath(artifactDirectory));
            const auto destination = QUrl::fromLocalFile(QDir(artifactDirectory).filePath("g2-recording.ndjson"));
            // Export the actual emitted recording, not a hand-authored trace.
            TelemetryController original; original.configure(configuration(), context());
            QTRY_COMPARE(original.status(), "ready");
            original.startProcess(python(), {fixture(), "normal"});
            QTRY_VERIFY_WITH_TIMEOUT(original.tracePreview().size() >= 2, 10000);
            original.stop(); QTRY_VERIFY_WITH_TIMEOUT(!original.active(), 10000);
            QSignalSpy artifactSaved(&original, &TelemetryController::recordingExported);
            original.exportRecording(destination); QTRY_COMPARE(artifactSaved.size(), 1);
            const QJsonObject project{{"version", 2}, {"project", sourceJson("contracts/telemetry/examples/canonical-project-v1.json")},
                {"sensor_configuration", configuration()}, {"sensor_layout", QJsonObject{{"tank-a-volume", QJsonObject{{"x", -80}, {"y", 35}, {"z", 30}}},
                    {"main-pump-feedback", QJsonObject{{"x", 0}, {"y", 35}, {"z", -20}}}}}};
            QVERIFY(writeBytes(QDir(artifactDirectory).filePath("g2-monitoring-project.json"), QJsonDocument(project).toJson()));
        }
    }
    void replayClockPauseRestartAndHistorical() {
        QTemporaryDir directory; QVERIFY(directory.isValid());
        const auto path = directory.filePath("delayed.ndjson"); QVERIFY(writeBytes(path, delayedRecording()));
        TelemetryController controller; controller.configure(configuration(), context());
        QTRY_COMPARE(controller.status(), "ready"); controller.startReplay(QUrl::fromLocalFile(path));
        QTRY_VERIFY2(controller.active() && controller.tracePreview().size() == 1, qPrintable(controller.error()));
        controller.pause(); QTRY_VERIFY(controller.paused()); const auto time = controller.eventTime();
        QTest::qWait(220); QVERIFY(std::abs(controller.eventTime() - time) < 0.03);
        controller.setRate(4); QTRY_COMPARE(controller.rate(), 4.0);
        controller.resume(); QTRY_VERIFY(!controller.paused()); QTest::qWait(220);
        QVERIFY(controller.eventTime() > time + 0.3);
        const auto epoch = controller.snapshot().value("epoch_id").toString();
        controller.restart(); QTRY_VERIFY(controller.snapshot().value("epoch_id").toString() != epoch);
        controller.stop(); QTRY_VERIFY(!controller.active());
        controller.showHistorical(QUrl::fromLocalFile(path));
        QTRY_COMPARE(controller.mode(), "historical"); QTRY_VERIFY(!controller.active());
        QCOMPARE(controller.status(), "replay_finished"); QVERIFY(controller.error().isEmpty());
    }
    void invalidReplayDoesNotReplaceRunningStream() {
        QTemporaryDir directory; QVERIFY(directory.isValid());
        const auto path = directory.filePath("bad.ndjson"); QVERIFY(writeBytes(path, "{\"broken\":true}\n"));
        TelemetryController controller; controller.configure(configuration(), context()); QTRY_COMPARE(controller.status(), "ready");
        controller.startProcess(python(), {fixture(), "normal"}); QTRY_VERIFY(controller.tracePreview().size() >= 2);
        const auto epoch = controller.snapshot().value("epoch_id").toString();
        QSignalSpy failed(&controller, &TelemetryController::failed);
        controller.startReplay(QUrl::fromLocalFile(path)); QTRY_COMPARE(failed.size(), 1);
        QVERIFY(controller.active()); QCOMPARE(controller.snapshot().value("epoch_id").toString(), epoch);
        controller.stop(); QTRY_VERIFY(!controller.active());
    }
    void staleLifecycleFramesCannotChangeCurrentClockOrEndTheStream() {
        QTemporaryDir directory; QVERIFY(directory.isValid());
        const auto acknowledgementGate = directory.filePath("allow-correct-ack");
        TelemetryController controller; controller.setTimeoutMs(15000);
        controller.configure(configuration(), context()); QTRY_COMPARE(controller.status(), "ready");
        controller.startProcess(python(), {fixture(), "stale_lifecycle", acknowledgementGate}); QTRY_VERIFY(controller.tracePreview().size() >= 2);
        controller.pause();
        QTRY_VERIFY(controller.counters().value("old_epoch_frames").toInt() >= 3);
        QVERIFY2(controller.active(), qPrintable(controller.status() + ": " + controller.error()));
        QVERIFY2(!controller.paused(), "Stale acknowledgements must not complete the pending pause.");
        QVERIFY(controller.counters().value("future_frames").toInt() >= 1);
        QVERIFY(writeBytes(acknowledgementGate, "allow correct acknowledgement"));
        QTRY_VERIFY(controller.paused());
        QVERIFY2(controller.active(), qPrintable(controller.status() + ": " + controller.error()));
        QVERIFY2(controller.error().isEmpty(), qPrintable(controller.error()));
        controller.stop(); QTRY_VERIFY(!controller.active()); QVERIFY2(controller.error().isEmpty(), qPrintable(controller.error()));
    }
    void lifecycleDuplicatesAreIdempotent_data() {
        QTest::addColumn<QString>("fixtureMode");
        QTest::newRow("start") << "duplicate_start";
        QTest::newRow("acknowledgement") << "duplicate_ack";
    }
    void lifecycleDuplicatesAreIdempotent() {
        QFETCH(QString, fixtureMode);
        TelemetryController controller; controller.configure(configuration(), context()); QTRY_COMPARE(controller.status(), "ready");
        controller.startProcess(python(), {fixture(), fixtureMode}); QTRY_VERIFY(controller.tracePreview().size() >= 2);
        if (fixtureMode == "duplicate_ack") { controller.pause(); QTRY_VERIFY(controller.paused()); }
        QTRY_VERIFY(controller.counters().value("duplicate_frames").toInt() >= 1);
        QVERIFY(controller.active()); QVERIFY(controller.error().isEmpty());
        controller.stop(); QTRY_VERIFY(!controller.active()); QVERIFY(controller.error().isEmpty());
    }
    void prefixReplayNeverClaimsACompleteEpoch() {
        QTemporaryDir directory; QVERIFY(directory.isValid());
        const auto path = directory.filePath("prefix.ndjson");
        const auto parts = delayedRecording().split('\n');
        QVERIFY(writeBytes(path, parts[0] + '\n' + parts[1] + '\n'));
        TelemetryController controller; controller.configure(configuration(), context()); QTRY_COMPARE(controller.status(), "ready");
        controller.startReplay(QUrl::fromLocalFile(path)); QTRY_COMPARE(controller.status(), "replay_prefix_finished");
        QVERIFY(!controller.active()); QVERIFY(!controller.snapshot().value("ended").toBool());
        QVERIFY(controller.snapshot().value("replay_prefix").toBool()); QVERIFY(controller.error().isEmpty());
        QCOMPARE(controller.tracePreview().size(), 1);
        const auto epoch = controller.snapshot().value("epoch_id").toString();
        QVERIFY(writeBytes(path, parts[0] + '\n'));
        controller.startReplay(QUrl::fromLocalFile(path));
        QTRY_VERIFY(controller.snapshot().value("epoch_id").toString() != epoch);
        QTRY_COMPARE(controller.status(), "replay_prefix_finished");
        QVERIFY(!controller.snapshot().value("ended").toBool()); QVERIFY(controller.tracePreview().isEmpty());
    }
    void cachedRegistryStillRejectsInvalidLiveFrames_data() {
        QTest::addColumn<QString>("fixtureMode"); QTest::addColumn<QString>("expectedCode");
        QTest::newRow("configuration-binding") << "wrong_configuration" << "frame_invalid";
        QTest::newRow("physical-binding") << "wrong_physical" << "frame_invalid";
        QTest::newRow("unknown-sensor") << "unknown_sensor" << "stream_fault";
        QTest::newRow("disabled-sensor") << "disabled_sensor" << "stream_fault";
        QTest::newRow("reading-type") << "wrong_reading_type" << "stream_fault";
        QTest::newRow("reading-unit") << "wrong_reading_unit" << "stream_fault";
    }
    void cachedRegistryStillRejectsInvalidLiveFrames() {
        QFETCH(QString, fixtureMode); QFETCH(QString, expectedCode);
        auto config = configuration();
        if (fixtureMode == "disabled_sensor") {
            auto sensors = config.value("sensors").toArray();
            auto disabled = sensors.last().toObject(); disabled["enabled"] = false;
            sensors[sensors.size() - 1] = disabled; config["sensors"] = sensors;
        }
        TelemetryController controller;
        controller.configure(config, context()); QTRY_COMPARE(controller.status(), "ready");
        controller.startProcess(python(), {fixture(), fixtureMode});
        QTRY_COMPARE_WITH_TIMEOUT(controller.status(), "fault", 10000);
        QVERIFY2(controller.error().contains(expectedCode), qPrintable(controller.error()));
        QVERIFY(!controller.active());
        QVERIFY2(controller.tracePreview().isEmpty(), "An invalid batch cannot publish a partial measurement update.");
    }
    void processFaults_data() {
        QTest::addColumn<QString>("fixtureMode"); QTest::addColumn<QString>("errorCode");
        QTest::newRow("missing start") << "silent" << "request_timeout";
        QTest::newRow("oversized partial output") << "oversized" << "frame_size";
        QTest::newRow("process crash") << "crash" << "process_exit";
        QTest::newRow("wrong acknowledgement") << "wrong_ack" << "ack_mismatch";
        QTest::newRow("acknowledgement timeout") << "no_ack" << "request_timeout";
        QTest::newRow("unacknowledged heartbeat clock") << "heartbeat_clock" << "heartbeat_clock";
        QTest::newRow("end without process exit") << "no_exit" << "process_end_timeout";
        QTest::newRow("bounded diagnostic output") << "stderr_flood" << "stderr_limit";
    }
    void processFaults() {
        QFETCH(QString, fixtureMode); QFETCH(QString, errorCode);
        TelemetryController controller; controller.configure(configuration(), context()); controller.setTimeoutMs(1000);
        QTRY_COMPARE(controller.status(), "ready"); controller.startProcess(python(), {fixture(), fixtureMode});
        if (fixtureMode == "wrong_ack" || fixtureMode == "no_ack") {
            QTRY_VERIFY_WITH_TIMEOUT(controller.tracePreview().size() >= 2, 10000); controller.pause();
        }
        if (fixtureMode == "no_exit") { QTRY_VERIFY(controller.tracePreview().size() >= 2); controller.stop(); }
        QTRY_COMPARE_WITH_TIMEOUT(controller.status(), "fault", 10000);
        QVERIFY2(controller.error().contains(errorCode), qPrintable(controller.error())); QVERIFY(!controller.active());
        controller.startProcess(python(), {fixture(), "normal"});
        QTRY_VERIFY_WITH_TIMEOUT(controller.active() && controller.error().isEmpty() && controller.tracePreview().size() >= 2, 10000);
        controller.stop(); QTRY_VERIFY(!controller.active());
    }
    void stopCancelsPendingStartAndConfigurationRetiresInput() {
        TelemetryController controller; controller.configure(configuration(), context()); QTRY_COMPARE(controller.status(), "ready");
        controller.startProcess(python(), {fixture(), "silent"}); QTRY_VERIFY(controller.active());
        controller.stop(); QTRY_COMPARE(controller.status(), "stopped"); QVERIFY(!controller.active());
        controller.startProcess(python(), {fixture(), "normal"}); QTRY_VERIFY(controller.tracePreview().size() >= 2);
        const auto epoch = controller.snapshot().value("epoch_id").toString();
        auto renamed = configuration(); auto sensors = renamed.value("sensors").toArray();
        auto sensor = sensors[0].toObject(); sensor["label"] = "Display-only rename"; sensors[0] = sensor; renamed["sensors"] = sensors;
        controller.configure(renamed, context()); QTest::qWait(220);
        QVERIFY(controller.active()); QCOMPARE(controller.snapshot().value("epoch_id").toString(), epoch);
        sensor["uncertainty_bound"] = 0.5; sensors[0] = sensor; renamed["sensors"] = sensors;
        controller.configure(renamed, context()); QTRY_VERIFY(!controller.active()); QCOMPARE(controller.status(), "ready");
        QVERIFY(controller.tracePreview().isEmpty());
    }
};
QTEST_GUILESS_MAIN(TelemetryLifecycleTests)
#include "telemetry_lifecycle_tests.moc"
