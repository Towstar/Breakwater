#include "science/PythonWorkerClient.hpp"
#include "science/WorkerProtocol.hpp"
#include "ProjectDocument.hpp"
#include "storage_grid_cases.hpp"

#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QPointer>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QtTest>
#include <algorithm>
#include <cmath>
#include <memory>
#ifdef Q_OS_WIN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

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
    void exactGridAdmissionAndResponseValidation_data() {
        QTest::addColumn<double>("horizon"); QTest::addColumn<double>("step");
        QTest::addColumn<QJsonArray>("fullTimes"); QTest::addColumn<int>("maximum");
        for (const auto& item : storage_grid_test::cases()) {
            QJsonArray times; for (const auto time : item.times) times.append(time);
            for (const int maximum : {2, 256})
                QTest::newRow(qPrintable(QString("%1-preview-%2").arg(item.name).arg(maximum)))
                    << item.horizon << item.step << times << maximum;
        }
    }
    void exactGridAdmissionAndResponseValidation() {
        QFETCH(double, horizon); QFETCH(double, step); QFETCH(QJsonArray, fullTimes); QFETCH(int, maximum);
        auto input = payload(horizon, step); input["preview"] = QJsonObject{{"max_times", maximum}};
        QVERIFY2(protocol::forwardPayload(input), "An independently specified supported grid must be admitted.");
        const auto count = std::min(fullTimes.size(), qsizetype{maximum});
        QJsonArray times, indices, row;
        for (qsizetype i = 0; i < count; ++i) {
            const auto index = i * (fullTimes.size() - 1) / (count - 1);
            times.append(fullTimes[index]); indices.append(index); row.append(80.0);
        }
        QJsonObject result{{"method", "storage_rollout"}, {"method_version", 1}, {"identity", input["identity"]},
            {"provenance", QJsonObject{{"implementation", "torch_storage_float64"}, {"implementation_version", "independent-grid-oracle"},
                {"dtype", "float64"}, {"device", "cpu"}, {"execution_status", "completed"}}},
            {"time_h", times}, {"time_indices", indices}, {"tank_ids", QJsonArray{"tank_a", "tank_b"}},
            {"volumes_m3", QJsonArray{row, row}}, {"cost", 0.0}, {"warnings", QJsonArray{}}};
        QVERIFY2(protocol::storage(result, input), "The exact original horizon and fixed final index must validate.");
        auto corrupt = result;
        auto shortenedTimes = times, shortenedIndices = indices, shortenedRow = row;
        shortenedTimes.removeLast(); shortenedIndices.removeLast(); shortenedRow.removeLast();
        corrupt["time_h"] = shortenedTimes; corrupt["time_indices"] = shortenedIndices;
        corrupt["volumes_m3"] = QJsonArray{shortenedRow, shortenedRow}; QVERIFY(!protocol::storage(corrupt, input));
        corrupt = result; auto badTimes = times; badTimes[badTimes.size() - 1] = times.at(times.size() - 2); corrupt["time_h"] = badTimes;
        QVERIFY(!protocol::storage(corrupt, input));
        corrupt = result; badTimes = times; badTimes[badTimes.size() - 1] = std::nextafter(horizon, 0.0); corrupt["time_h"] = badTimes;
        QVERIFY(!protocol::storage(corrupt, input));
        corrupt = result; auto badIndices = indices; badIndices[badIndices.size() - 1] = fullTimes.size(); corrupt["time_indices"] = badIndices;
        QVERIFY(!protocol::storage(corrupt, input));
        corrupt = result; corrupt["tank_ids"] = QJsonArray{"tank_b", "tank_a"}; QVERIFY(!protocol::storage(corrupt, input));
        corrupt = result; auto identity = input["identity"].toObject(); identity["volume_run_id"] = "forged";
        corrupt["identity"] = identity; QVERIFY(!protocol::storage(corrupt, input));
        auto excessive = payload(std::nextafter(1.0, INFINITY), .0001);
        QVERIFY(!protocol::forwardPayload(excessive)); QVERIFY(!protocol::storage(result, excessive));
    }
    void controlledPeerKeepsOriginalGrid_data() { exactGridAdmissionAndResponseValidation_data(); }
    void controlledPeerKeepsOriginalGrid() {
        QFETCH(double, horizon); QFETCH(double, step); QFETCH(QJsonArray, fullTimes); QFETCH(int, maximum);
        Environment restore("BREAKWATER_STORAGE_FIXTURE_MODE"); mode("valid");
        auto input = payload(horizon, step); input["preview"] = QJsonObject{{"max_times", maximum}};
        PythonWorkerClient client(python(), fixture());
        QSignalSpy ready(&client, &PythonWorkerClient::storageReady), failed(&client, &PythonWorkerClient::failed);
        client.runStorage(input); QTRY_VERIFY_WITH_TIMEOUT(!ready.isEmpty() || !failed.isEmpty(), 10000);
        QCOMPARE(failed.size(), 0); QCOMPARE(ready.size(), 1);
        const auto result = ready.first().first().toJsonObject().value("result").toObject();
        const auto times = result["time_h"].toArray(), indices = result["time_indices"].toArray();
        QCOMPARE(times.last().toDouble(), horizon); QCOMPARE(indices.last().toInteger(), fullTimes.size() - 1);
        QCOMPARE(times.size(), std::min(qsizetype{maximum}, fullTimes.size()));
        for (qsizetype i = 0; i < times.size(); ++i) {
            const auto original = i * (fullTimes.size() - 1) / (times.size() - 1);
            QCOMPARE(indices[i].toInteger(), original); QCOMPARE(times[i], fullTimes[original]);
        }
        compareNative(result, input);
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
        client.runStorage(payload()); QTRY_COMPARE_WITH_TIMEOUT(failed.size(), 1, 15000);
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
    void fiftyCancelRetryCyclesHaveNoOverlapSurvivorsOrLatePublication() {
        Environment modeRestore("BREAKWATER_STORAGE_FIXTURE_MODE"), pidRestore("BREAKWATER_STORAGE_FIXTURE_PIDFILE");
        QTemporaryDir directory; QVERIFY(directory.isValid());
        const auto pidPath = directory.filePath("pid.txt"); qputenv("BREAKWATER_STORAGE_FIXTURE_PIDFILE", pidPath.toUtf8());
        const auto readPid = [&] { QFile file(pidPath); return file.open(QIODevice::ReadOnly) ? file.readAll().toLongLong() : qint64{0}; };
        PythonWorkerClient client(python(), fixture()); client.setTimeoutMs(10000); client.setTerminationGraceMs(25);
        QSignalSpy ready(&client, &PythonWorkerClient::storageReady), failed(&client, &PythonWorkerClient::failed);
        qint64 previous = 0;
#ifdef Q_OS_WIN
        // Retain the original process objects. A bare PID may be reused by an
        // unrelated Python process before the next iteration checks survivors.
        std::vector<std::shared_ptr<void>> children;
        const auto retain = [&](HANDLE handle) { children.emplace_back(handle, [](void* value) { CloseHandle(value); }); };
#endif
        for (int cycle = 0; cycle < 50; ++cycle) {
            mode("hang"); client.runStorage(payload());
            QTRY_VERIFY_WITH_TIMEOUT(readPid() != 0 && readPid() != previous, 10000);
            const auto cancelled = readPid();
#ifdef Q_OS_WIN
            const auto cancelledHandle = OpenProcess(SYNCHRONIZE, FALSE, static_cast<DWORD>(cancelled));
            QVERIFY(cancelledHandle); retain(cancelledHandle);
#endif
            client.cancel(); QCOMPARE(failed.size(), cycle + 1); QCOMPARE(ready.size(), cycle);
            QVERIFY(!client.readiness().value("storage_execution_verified").toBool());
            mode("valid_linger"); client.runStorage(payload());
            QTRY_VERIFY_WITH_TIMEOUT(readPid() != 0 && readPid() != cancelled, 10000);
            previous = readPid();
#ifdef Q_OS_WIN
            const auto completedHandle = OpenProcess(SYNCHRONIZE, FALSE, static_cast<DWORD>(previous));
            QVERIFY(completedHandle); retain(completedHandle);
#endif
            QTRY_COMPARE_WITH_TIMEOUT(ready.size(), cycle + 1, 10000); QCOMPARE(failed.size(), cycle + 1);
            QVERIFY(previous != cancelled);
#ifdef Q_OS_WIN
            for (const auto& child : children)
                QVERIFY2(WaitForSingleObject(child.get(), 0) == WAIT_OBJECT_0, qPrintable(QString("Surviving original fixture process after cycle %1").arg(cycle + 1)));
#endif
            qInfo("Cancel/retry cycle %d: clean exit, no overlap, no late publication; PIDs %lld/%lld", cycle + 1,
                  static_cast<long long>(cancelled), static_cast<long long>(previous));
        }
        QTest::qWait(250); QCOMPARE(ready.size(), 50); QCOMPARE(failed.size(), 50);
    }
    void actualWorkerMatchesNative_data() {
        QTest::addColumn<double>("horizon"); QTest::addColumn<double>("step"); QTest::addColumn<bool>("renamed");
        QTest::addColumn<int>("maximum");
        QTest::newRow("canonical") << 3.0 << 0.025 << false << 256;
        QTest::newRow("renamed-custom") << 3.0 << 0.025 << true << 256;
        QTest::newRow("shortened-final-step") << 1.0 << 0.3 << false << 256;
        QTest::newRow("inherited-one-ULP-endpoint") << 0.225 << 0.075 << true << 256;
        for (const auto& item : storage_grid_test::cases()) for (const int maximum : {2, 256})
            QTest::newRow(qPrintable(QString("%1-preview-%2").arg(item.name).arg(maximum)))
                << item.horizon << item.step << false << maximum;
    }
    void actualWorkerMatchesNative() {
        QFETCH(double, horizon); QFETCH(double, step); QFETCH(bool, renamed); QFETCH(int, maximum);
        PythonWorkerClient client(python(), worker()); QSignalSpy health(&client, &PythonWorkerClient::healthReady), ready(&client, &PythonWorkerClient::storageReady), failed(&client, &PythonWorkerClient::failed);
        client.checkHealth(); QTRY_COMPARE_WITH_TIMEOUT(health.size(), 1, 10000);
        if (!client.readiness().value("torch_detected").toBool()) QSKIP("Configured Python has no Torch runtime; fake protocol tests still execute.");
        auto input = payload(horizon, step, renamed); input["preview"] = QJsonObject{{"max_times", maximum}}; client.runStorage(input);
        QTRY_VERIFY_WITH_TIMEOUT(!ready.isEmpty() || !failed.isEmpty(), 60000);
        QCOMPARE(failed.size(), 0); QCOMPARE(ready.size(), 1); QVERIFY(client.readiness().value("storage_execution_verified").toBool());
        const auto result = ready.first().first().toJsonObject().value("result").toObject(); compareNative(result, input);
        QCOMPARE(input.value("horizon_h").toDouble(), horizon);
        for (const auto& item : storage_grid_test::cases()) if (item.horizon == horizon && item.step == step) {
            const auto times = result["time_h"].toArray(), indices = result["time_indices"].toArray();
            QCOMPARE(times.last().toDouble(), horizon); QCOMPARE(indices.last().toInteger(), static_cast<qint64>(item.times.size() - 1));
            QCOMPARE(times.size(), std::min(qsizetype{maximum}, static_cast<qsizetype>(item.times.size())));
            for (qsizetype i = 0; i < times.size(); ++i) {
                const auto original = static_cast<std::size_t>(i) * (item.times.size() - 1) / static_cast<std::size_t>(times.size() - 1);
                QCOMPARE(indices[i].toInteger(), static_cast<qint64>(original)); QCOMPARE(times[i].toDouble(), item.times[original]);
            }
        }
        if (horizon == 0.225) {
            QCOMPARE(result.value("time_h").toArray().last().toDouble(), horizon);
            QCOMPARE(result.value("time_indices").toArray().last().toInteger(), qint64{4});
        }
    }
};

QTEST_GUILESS_MAIN(StorageWorkerTests)
#include "storage_worker_tests.moc"
