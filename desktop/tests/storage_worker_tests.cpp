#include "science/PythonWorkerClient.hpp"
#include "science/WorkerProtocol.hpp"
#include "ProjectDocument.hpp"

#include <QElapsedTimer>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QPointer>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QtTest>
#include <algorithm>
#include <cmath>
#include <limits>

using namespace breakwater;
using namespace breakwater::desktop;
using namespace breakwater::desktop::science;

#ifndef BREAKWATER_TEST_PYTHON
#define BREAKWATER_TEST_PYTHON ""
#endif

namespace {
class Environment final {
public:
    explicit Environment(const char* name) : name_(name), present_(qEnvironmentVariableIsSet(name)), prior_(qgetenv(name)) {}
    ~Environment() { if (present_) qputenv(name_.constData(), prior_); else qunsetenv(name_.constData()); }
private:
    QByteArray name_; bool present_; QByteArray prior_;
};
QString python() { return QString::fromUtf8(BREAKWATER_TEST_PYTHON); }
QString fixture() { return QStringLiteral(BREAKWATER_SOURCE_DIR "/desktop/tests/worker_fixtures/storage_worker_fixture.py"); }
QString worker() { return QStringLiteral(BREAKWATER_SOURCE_DIR "/science/worker.py"); }
void mode(const QByteArray& value) { qputenv("BREAKWATER_STORAGE_FIXTURE_MODE", value); }
QJsonObject payload(double horizon = 3.0, double step = 0.025, bool renamed = false) {
    auto document = example_project();
    if (renamed) {
        QMap<std::string, std::string> ids;
        for (auto& asset : document.network.assets) { const auto before = asset.id; asset.id = "renamed:" + before + ":west"; ids[before] = asset.id; }
        for (auto& route : document.network.routes) {
            route.id = "pipe:" + route.id + ":custom"; route.source_id = ids[route.source_id]; route.target_id = ids[route.target_id];
        }
    }
    return {{"method_version", 1}, {"identity", QJsonObject{{"snapshot_id", "captured:snapshot"}, {"volume_run_id", "actual:native:run"},
        {"numerical_revision", 7}, {"physical_hash", physics_hash(document)}}}, {"network", network_json(document.network)},
        {"controls", QJsonArray{}}, {"horizon_h", horizon}, {"step_h", step}, {"derivatives", false}, {"preview", QJsonObject{{"max_times", 256}}}};
}
QString code(const QSignalSpy& failed) { return failed.isEmpty() ? QString{} : failed.first().first().toString(); }
void compareNative(const QJsonObject& result, const QJsonObject& input) {
    const auto project = parse_project({{"version", 1}, {"network", input.value("network")}, {"layout", QJsonObject{}}});
    const auto native = simulate(compile(project.network), {input.value("horizon_h").toDouble(), input.value("step_h").toDouble()});
    const auto indices = result.value("time_indices").toArray(), times = result.value("time_h").toArray();
    const auto volumes = result.value("volumes_m3").toArray(), ids = result.value("tank_ids").toArray();
    QCOMPARE(native.time_h.front(), 0.0); QCOMPARE(native.time_h.back(), input.value("horizon_h").toDouble());
    QCOMPARE(times.first().toDouble(), 0.0); QCOMPARE(times.last().toDouble(), input.value("horizon_h").toDouble());
    QCOMPARE(indices.first().toInteger(), 0); QCOMPARE(indices.last().toInteger(), static_cast<qint64>(native.time_h.size() - 1));
    QCOMPARE(ids.size(), static_cast<qsizetype>(native.tank_ids.size()));
    for (qsizetype tank = 0; tank < ids.size(); ++tank) {
        QCOMPARE(ids[tank].toString(), QString::fromStdString(native.tank_ids[static_cast<std::size_t>(tank)]));
        const auto row = volumes[tank].toArray();
        for (qsizetype time = 0; time < indices.size(); ++time) {
            const auto index = static_cast<std::size_t>(indices[time].toInteger());
            QCOMPARE(times[time].toDouble(), native.time_h[index]);
            const auto expected = native.volumes_m3[static_cast<std::size_t>(tank)][index], actual = row[time].toDouble();
            QVERIFY(std::abs(actual - expected) <= 1e-9 + 1e-10 * std::max(std::abs(actual), std::abs(expected)));
        }
    }
    const auto actualCost = result.value("cost").toDouble();
    QVERIFY(std::abs(actualCost - native.cost) <= 1e-9 + 1e-10 * std::max(std::abs(actualCost), std::abs(native.cost)));
}
} // namespace

class StorageWorkerTests final : public QObject {
    Q_OBJECT
private slots:
    void initTestCase() {
        if (python().isEmpty() || !QFileInfo(python()).isFile()) QSKIP("A configured Python interpreter is unavailable.");
    }
    void capabilitiesAreSeparateFromActualExecutionAndRestartResetsVerification() {
        Environment restore("BREAKWATER_STORAGE_FIXTURE_MODE"); mode("valid");
        PythonWorkerClient client(python(), fixture());
        QSignalSpy caps(&client, &PythonWorkerClient::capabilitiesReady), ready(&client, &PythonWorkerClient::storageReady), failed(&client, &PythonWorkerClient::failed);
        client.checkCapabilities(); QCOMPARE(client.operation(), "capabilities"); QTRY_COMPARE_WITH_TIMEOUT(caps.size(), 1, 10000);
        QVERIFY(client.readiness().value("storage_implemented").toBool()); QVERIFY(!client.readiness().value("storage_execution_verified").toBool());
        client.runStorage(payload()); QCOMPARE(client.operation(), "storage_rollout"); QVERIFY(client.busy()); client.runStorage(payload());
        QTRY_COMPARE_WITH_TIMEOUT(ready.size(), 1, 10000); QCOMPARE(failed.size(), 0);
        QVERIFY(client.readiness().value("storage_execution_verified").toBool());
        const auto response = ready.first().first().toJsonObject();
        QVERIFY(protocol::storage(response.value("result").toObject(), payload())); compareNative(response.value("result").toObject(), payload());
        client.checkCapabilities(); QTRY_COMPARE_WITH_TIMEOUT(caps.size(), 2, 10000);
        QVERIFY(!client.readiness().value("storage_execution_verified").toBool());
        QVERIFY(caps.first().first().toJsonObject().value("request_id") != caps.last().first().toJsonObject().value("request_id"));
    }
    void roundedGridAdmissionMatchesTheNativeWorkPolicy_data() {
        QTest::addColumn<double>("horizon"); QTest::addColumn<double>("step");
        QTest::addColumn<bool>("accepted"); QTest::addColumn<int>("frames");
        QTest::newRow("one-ULP-terminal-interval") << std::nextafter(0.9, std::numeric_limits<double>::infinity()) << 0.1 << true << 11;
        QTest::newRow("rounded-final-multiple-once") << 0.30000000000000004 << 0.1 << true << 4;
        QTest::newRow("ratio-underflow") << std::numeric_limits<double>::denorm_min() << 1e308 << true << 2;
        QTest::newRow("larger-step") << 1.0 << 24.0 << true << 2;
        QTest::newRow("exact-update-limit") << 1.0 << 0.0001 << true << 10001;
        QTest::newRow("division-rounds-above-limit") << 0.0082 << 8.2e-7 << true << 10001;
        QTest::newRow("terminal-interval-exceeds-limit") << std::nextafter(1.0, std::numeric_limits<double>::infinity()) << 0.0001 << false << 0;
        QTest::newRow("unrepresentable-work-count") << 1.0 << std::numeric_limits<double>::denorm_min() << false << 0;
    }
    void roundedGridAdmissionMatchesTheNativeWorkPolicy() {
        QFETCH(double, horizon); QFETCH(double, step); QFETCH(bool, accepted); QFETCH(int, frames);
        const auto input = payload(horizon, step);
        QCOMPARE(protocol::forwardPayload(input), accepted);
        if (accepted) {
            const auto network = parse_project({{"version", 1}, {"network", input.value("network")}, {"layout", QJsonObject{}}}).network;
            const auto native = simulate(compile(network), {horizon, step});
            QCOMPARE(native.time_h.size(), static_cast<std::size_t>(frames));
            QCOMPARE(native.time_h.front(), 0.0); QCOMPARE(native.time_h.back(), horizon);
            for (std::size_t i = 1; i < native.time_h.size(); ++i) QVERIFY(native.time_h[i] > native.time_h[i - 1]);
            Environment restore("BREAKWATER_STORAGE_FIXTURE_MODE"); mode("valid");
            PythonWorkerClient client(python(), fixture());
            QSignalSpy ready(&client, &PythonWorkerClient::storageReady), failed(&client, &PythonWorkerClient::failed);
            client.runStorage(input); QTRY_COMPARE_WITH_TIMEOUT(ready.size(), 1, 10000); QCOMPARE(failed.size(), 0);
            compareNative(ready.first().first().toJsonObject().value("result").toObject(), input);
        }
    }
    void partialAndLargeValidFramesRemainResponsiveAndUsePerFrameLimits_data() {
        QTest::addColumn<QByteArray>("fixtureMode"); QTest::newRow("fragmented") << QByteArray("partial");
        QTest::newRow("three-one-MiB-frames") << QByteArray("large_frames");
        QTest::newRow("unicode-warning-contract-limit") << QByteArray("unicode_warning");
    }
    void partialAndLargeValidFramesRemainResponsiveAndUsePerFrameLimits() {
        QFETCH(QByteArray, fixtureMode); Environment restore("BREAKWATER_STORAGE_FIXTURE_MODE"); mode(fixtureMode);
        PythonWorkerClient client(python(), fixture()); QSignalSpy ready(&client, &PythonWorkerClient::storageReady), failed(&client, &PythonWorkerClient::failed);
        int beats = 0; QTimer heartbeat; heartbeat.setInterval(2); connect(&heartbeat, &QTimer::timeout, this, [&] { ++beats; }); heartbeat.start();
        client.runStorage(payload()); QTRY_COMPARE_WITH_TIMEOUT(ready.size(), 1, 15000); QCOMPARE(failed.size(), 0); QVERIFY(beats > 0);
        QVERIFY(client.readiness().value("storage_execution_verified").toBool());
    }
    void rejectsPeerFaultsBeforePublishing_data() {
        QTest::addColumn<QByteArray>("fixtureMode"); QTest::addColumn<QString>("expected");
        const std::pair<const char*, const char*> cases[] = {
            {"not_deployed", "runtime_unavailable"}, {"runtime_error", "runtime_unavailable"},
            {"bad_capability_version", "worker_capabilities_invalid"}, {"duplicate_methods", "worker_capabilities_invalid"},
            {"bad_limits", "worker_capabilities_invalid"}, {"artifacts", "worker_capabilities_invalid"},
            {"preverified", "worker_capabilities_invalid"}, {"post_unverified", "worker_capabilities_invalid"},
            {"wrong_identity", "worker_storage_invalid"}, {"wrong_tank", "worker_storage_invalid"}, {"wrong_index", "worker_storage_invalid"},
            {"wrong_time", "worker_storage_invalid"}, {"null_sample", "worker_storage_invalid"}, {"wrong_shape", "worker_storage_invalid"},
            {"shortened_endpoint", "worker_storage_invalid"}, {"extended_endpoint", "worker_storage_invalid"},
            {"missing_terminal_interval", "worker_storage_invalid"}, {"duplicate_endpoint", "worker_storage_invalid"},
            {"wrong_provenance", "worker_storage_invalid"}, {"derivatives", "worker_storage_invalid"}, {"result_artifact", "worker_storage_invalid"},
            {"private_result", "worker_storage_invalid"}, {"oversized_warning", "worker_storage_invalid"},
            {"wrong_id", "worker_response_mismatch"}, {"wrong_protocol", "worker_response_mismatch"},
            {"invalid_utf8", "worker_protocol_invalid"}, {"oversized_frame", "worker_stdout_limit"}, {"oversized_stderr", "worker_stderr_limit"},
            {"crash_after_storage", "worker_crashed"}, {"crash_after_exchange", "worker_crashed"}, {"trailing_noise", "worker_protocol_invalid"},
            {"aggregate_limit", "worker_stdout_limit"}};
        for (const auto& [fixtureMode, expected] : cases) QTest::newRow(fixtureMode) << QByteArray(fixtureMode) << QString::fromLatin1(expected);
    }
    void rejectsPeerFaultsBeforePublishing() {
        QFETCH(QByteArray, fixtureMode); QFETCH(QString, expected); Environment restore("BREAKWATER_STORAGE_FIXTURE_MODE"); mode(fixtureMode);
        PythonWorkerClient client(python(), fixture()); QSignalSpy ready(&client, &PythonWorkerClient::storageReady), failed(&client, &PythonWorkerClient::failed);
        const auto input = fixtureMode == "missing_terminal_interval" ?
            payload(std::nextafter(0.9, std::numeric_limits<double>::infinity()), 0.1) : payload();
        client.runStorage(input); QTRY_COMPARE_WITH_TIMEOUT(failed.size(), 1, 15000);
        if (fixtureMode == "crash_after_storage")
            QVERIFY(code(failed) == expected || code(failed) == "worker_io_failed" || code(failed) == "worker_write_failed");
        else QCOMPARE(code(failed), expected);
        QCOMPARE(ready.size(), 0); QVERIFY(!client.busy());
        QVERIFY(!client.readiness().value("storage_execution_verified").toBool());
        QVERIFY(!QJsonDocument::fromVariant(client.readiness()).toJson().contains("private-secret-marker"));
        QVERIFY(!failed.first().at(1).toString().contains("private-secret-marker"));
    }
    void duplicateCoalescedResponseCannotAdvanceAnotherRequest() {
        Environment restore("BREAKWATER_STORAGE_FIXTURE_MODE"); mode("duplicate_response");
        PythonWorkerClient client(python(), fixture()); QSignalSpy ready(&client, &PythonWorkerClient::storageReady), failed(&client, &PythonWorkerClient::failed);
        client.runStorage(payload()); QTRY_COMPARE_WITH_TIMEOUT(failed.size(), 1, 10000);
        QVERIFY(code(failed) == "worker_response_mismatch" || code(failed) == "worker_protocol_invalid"); QCOMPARE(ready.size(), 0);
    }
    void invalidPayloadsAreRejectedWithoutStartingScience() {
        PythonWorkerClient client(python(), fixture()); QSignalSpy failed(&client, &PythonWorkerClient::failed), ready(&client, &PythonWorkerClient::storageReady);
        auto input = payload(); input["derivatives"] = true; client.runStorage(input); QCOMPARE(code(failed), "worker_request_invalid");
        input = payload(); input["step_h"] = 0; client.runStorage(input); QCOMPARE(failed.size(), 2);
        input = payload(); input["controls"] = QJsonArray{QJsonObject{{"control_id", "invented:control"}, {"value", 1}}}; client.runStorage(input); QCOMPARE(failed.size(), 3);
        input = payload(); auto identity = input.value("identity").toObject(); identity["numerical_revision"] = 0.5; input["identity"] = identity;
        client.runStorage(input); QCOMPARE(failed.size(), 4); QCOMPARE(ready.size(), 0); QVERIFY(!client.busy());
    }
    void terminalUlpBeyondTheWorkBudgetCannotStartTheWorker() {
        Environment modeRestore("BREAKWATER_STORAGE_FIXTURE_MODE"), pidRestore("BREAKWATER_STORAGE_FIXTURE_PIDFILE");
        QTemporaryDir directory; QVERIFY(directory.isValid());
        const auto pidFile = directory.filePath("must-not-start.txt"); qputenv("BREAKWATER_STORAGE_FIXTURE_PIDFILE", pidFile.toUtf8());
        mode("hang"); PythonWorkerClient client(python(), fixture());
        QSignalSpy ready(&client, &PythonWorkerClient::storageReady), failed(&client, &PythonWorkerClient::failed);
        client.runStorage(payload(std::nextafter(1.0, std::numeric_limits<double>::infinity()), 0.0001));
        QCOMPARE(failed.size(), 1); QCOMPARE(code(failed), "worker_request_invalid");
        QVERIFY(!client.busy()); QCOMPARE(ready.size(), 0); QVERIFY(!QFileInfo::exists(pidFile));
    }
    void deadlinesRetireWithoutOverlappingOrPublishingLateAndDestructionDoesNotWait() {
        Environment modeRestore("BREAKWATER_STORAGE_FIXTURE_MODE"), pidRestore("BREAKWATER_STORAGE_FIXTURE_PIDFILE");
        QTemporaryDir directory; QVERIFY(directory.isValid()); qputenv("BREAKWATER_STORAGE_FIXTURE_PIDFILE", directory.filePath("pid.txt").toUtf8());
        mode("hang"); PythonWorkerClient client(python(), fixture()); client.setTimeoutMs(750); client.setTerminationGraceMs(250);
        QSignalSpy ready(&client, &PythonWorkerClient::storageReady), failed(&client, &PythonWorkerClient::failed);
        client.runStorage(payload()); QTRY_VERIFY_WITH_TIMEOUT(QFileInfo(directory.filePath("pid.txt")).isFile(), 10000);
        QTRY_COMPARE_WITH_TIMEOUT(failed.size(), 1, 10000); QCOMPARE(code(failed), "worker_timeout");
        mode("valid"); client.setTimeoutMs(10000); client.runStorage(payload());
        QTRY_COMPARE_WITH_TIMEOUT(ready.size(), 1, 10000); QCOMPARE(failed.size(), 1);
        mode("hang_after_exchange"); client.setTimeoutMs(750); client.runStorage(payload());
        QTRY_COMPARE_WITH_TIMEOUT(failed.size(), 2, 10000); QCOMPARE(ready.size(), 1); QVERIFY(!client.readiness().value("storage_execution_verified").toBool());
        mode("hang"); client.setTimeoutMs(10000); client.runStorage(payload()); client.cancel(); QCOMPARE(failed.size(), 3);
        mode("valid"); client.runStorage(payload()); QTRY_COMPARE_WITH_TIMEOUT(ready.size(), 2, 10000); QCOMPARE(failed.size(), 3);
        QTest::qWait(100); QCOMPARE(ready.size(), 2);
        auto* disposable = new PythonWorkerClient(python(), fixture()); disposable->runStorage(payload());
        QElapsedTimer timer; timer.start(); delete disposable; QVERIFY(timer.elapsed() < 500);
        QTest::qWait(100);
    }
    void storageNeverInheritsModelCredentials() {
        Environment modeRestore("BREAKWATER_STORAGE_FIXTURE_MODE"), keyRestore("NVIDIA_API_KEY");
        mode("credential_environment"); qputenv("NVIDIA_API_KEY", "private-secret-marker");
        PythonWorkerClient client(python(), fixture()); QSignalSpy ready(&client, &PythonWorkerClient::storageReady), failed(&client, &PythonWorkerClient::failed);
        client.runStorage(payload()); QTRY_COMPARE_WITH_TIMEOUT(ready.size(), 1, 10000); QCOMPARE(failed.size(), 0);
        QVERIFY(!QJsonDocument(ready.first().first().toJsonObject()).toJson().contains("private-secret-marker"));
    }
    void actualWorkerMatchesNative_data() {
        QTest::addColumn<double>("horizon"); QTest::addColumn<double>("step"); QTest::addColumn<bool>("renamed");
        QTest::newRow("canonical") << 3.0 << 0.025 << false;
        QTest::newRow("renamed-custom") << 3.0 << 0.025 << true;
        QTest::newRow("shortened-final-step") << 1.0 << 0.3 << false;
        QTest::newRow("corrected-one-ULP-endpoint") << 0.225 << 0.075 << true;
        QTest::newRow("one-ULP-terminal-interval") << std::nextafter(0.9, std::numeric_limits<double>::infinity()) << 0.1 << false;
        QTest::newRow("rounded-final-multiple-once") << 0.30000000000000004 << 0.1 << false;
        QTest::newRow("ratio-underflow") << std::numeric_limits<double>::denorm_min() << 1e308 << false;
        QTest::newRow("division-rounds-above-work-limit") << 0.0082 << 8.2e-7 << true;
    }
    void actualWorkerMatchesNative() {
        QFETCH(double, horizon); QFETCH(double, step); QFETCH(bool, renamed);
        PythonWorkerClient client(python(), worker()); QSignalSpy health(&client, &PythonWorkerClient::healthReady), ready(&client, &PythonWorkerClient::storageReady), failed(&client, &PythonWorkerClient::failed);
        client.checkHealth(); QTRY_COMPARE_WITH_TIMEOUT(health.size(), 1, 10000);
        if (!client.readiness().value("torch_detected").toBool()) QSKIP("Configured Python has no Torch runtime; fake protocol tests still execute.");
        const auto input = payload(horizon, step, renamed); client.runStorage(input);
        QTRY_VERIFY_WITH_TIMEOUT(!ready.isEmpty() || !failed.isEmpty(), 60000);
        QCOMPARE(failed.size(), 0); QCOMPARE(ready.size(), 1); QVERIFY(client.readiness().value("storage_execution_verified").toBool());
        const auto result = ready.first().first().toJsonObject().value("result").toObject(); compareNative(result, input);
        QCOMPARE(input.value("horizon_h").toDouble(), horizon);
        QCOMPARE(result.value("time_h").toArray().last().toDouble(), horizon);
    }
};

QTEST_GUILESS_MAIN(StorageWorkerTests)
#include "storage_worker_tests.moc"
