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
QJsonObject derivativePayload(const QStringList& assets = {"truck_route", "source_a", "bypass"}, bool opaque = false,
                              double horizon = 3.0, double step = 0.025, double sourceFeedPrice = 0.0) {
    auto input = payload(horizon, step);
    auto project = parse_project({{"version", 1}, {"network", input.value("network")}, {"layout", QJsonObject{}}});
    for (auto& asset : project.network.assets) if (asset.id == "source_a") asset.curtailment_m3h = 3.0;
    for (auto& route : project.network.routes) {
        if (route.id == "feed_a") route.cost_per_m3 = sourceFeedPrice;
        if (route.id == "bypass") route.setpoint_m3h = 5.0;
        if (route.id == "truck_route") route.setpoint_m3h = 3.0;
    }
    const auto suffix = opaque ? QString::fromUtf8(":beta:\xCE\xB2:\xE6\xB0\xB4") : QString{};
    if (opaque) {
        for (auto& asset : project.network.assets) asset.id += suffix.toStdString();
        for (auto& route : project.network.routes) {
            route.id += suffix.toStdString(); route.source_id += suffix.toStdString(); route.target_id += suffix.toStdString();
        }
    }
    const auto catalog = control_catalog(project.network); QJsonArray selected;
    for (const auto& asset : assets) {
        const auto id = (asset + suffix).toStdString();
        const auto found = std::find_if(catalog.begin(), catalog.end(), [&](const ControlDescriptor& control) { return control.asset_id == id; });
        if (found == catalog.end()) qFatal("Invalid derivative test fixture selection");
        selected.append(QJsonObject{{"control_id", QString::fromStdString(found->id)}, {"value", found->value}});
    }
    input["network"] = network_json(project.network); input["controls"] = selected; input["derivatives"] = true;
    auto identity = input.value("identity").toObject(); identity["physical_hash"] = physics_hash(project); input["identity"] = identity;
    return input;
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
void compareNativeDerivatives(const QJsonObject& result, const QJsonObject& input) {
    const auto project = parse_project({{"version", 1}, {"network", input.value("network")}, {"layout", QJsonObject{}}});
    const SimulationConfig config{input.value("horizon_h").toDouble(), input.value("step_h").toDouble()};
    std::vector<ControlValue> controls; QJsonArray ids;
    for (const auto& value : input.value("controls").toArray()) {
        const auto control = value.toObject(); ids.append(control.value("control_id"));
        controls.push_back({control.value("control_id").toString().toStdString(), control.value("value").toDouble()});
    }
    const auto derivative = result.value("derivatives").toObject(); QCOMPARE(derivative.value("control_ids").toArray(), ids);
    const auto matrix = derivative.value("terminal_volume_jacobian_h").toArray(), gradient = derivative.value("cost_gradient").toArray();
    for (std::size_t column = 0; column < controls.size(); ++column) for (const double epsilon : {1e-3, 1e-4, 1e-5}) {
        auto plus = controls, minus = controls; plus[column].value += epsilon; minus[column].value -= epsilon;
        const auto upper = simulate(compile(project.network, plus), config), lower = simulate(compile(project.network, minus), config);
        for (qsizetype tank = 0; tank < matrix.size(); ++tank) {
            const auto finiteDifference = (upper.volumes_m3[static_cast<std::size_t>(tank)].back() - lower.volumes_m3[static_cast<std::size_t>(tank)].back()) / (2 * epsilon);
            const auto actual = matrix[tank].toArray()[static_cast<qsizetype>(column)].toDouble();
            QVERIFY(std::isfinite(actual));
            QVERIFY(std::abs(actual - finiteDifference) <= 1e-6 + 1e-4 * std::max(std::abs(actual), std::abs(finiteDifference)));
        }
        const auto finiteDifference = (upper.cost - lower.cost) / (2 * epsilon), actual = gradient[static_cast<qsizetype>(column)].toDouble();
        QVERIFY(std::isfinite(actual));
        QVERIFY(std::abs(actual - finiteDifference) <= 1e-6 + 1e-4 * std::max(std::abs(actual), std::abs(finiteDifference)));
    }
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
        QCOMPARE(protocol::storagePayload(input), accepted);
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
    void derivativeAdmissionRequiresExplicitInteriorUniqueCatalogControls() {
        auto input = derivativePayload(); QVERIFY(protocol::storagePayload(input));
        auto controls = input.value("controls").toArray(); auto control = controls[0].toObject();
        // Request admission accepts contract overrides; the controller separately
        // requires unchanged captured values before using an existing native ID.
        control["value"] = 4.0; controls[0] = control; input["controls"] = controls; QVERIFY(protocol::storagePayload(input));
        input = derivativePayload({"truck_route"}); controls = input.value("controls").toArray();
        for (const double boundary : {0.0, 22.0, -1.0, 23.0}) {
            control = controls[0].toObject(); control["value"] = boundary; auto changed = controls; changed[0] = control;
            auto rejected = input; rejected["controls"] = changed; QVERIFY(!protocol::storagePayload(rejected));
        }
        input = derivativePayload(); input["controls"] = QJsonArray{}; QVERIFY(!protocol::storagePayload(input));
        input = derivativePayload({"source_b"}); QVERIFY(!protocol::storagePayload(input)); // Fixed source.
        for (const double boundary : {0.0, 15.0}) {
            input = derivativePayload({"source_a"}); control = input.value("controls").toArray()[0].toObject(); control["value"] = boundary;
            input["controls"] = QJsonArray{control}; QVERIFY(!protocol::storagePayload(input));
        }
        input = derivativePayload({"truck_route", "truck_route"}); QVERIFY(!protocol::storagePayload(input));
        input = derivativePayload({"source_a"}); auto network = input.value("network").toObject(); auto routes = network.value("routes").toArray();
        for (qsizetype i = 0; i < routes.size(); ++i) {
            auto route = routes[i].toObject(); if (route.value("id").toString() == "feed_a") route["capacity_m3h"] = 49.0; routes[i] = route;
        }
        network["routes"] = routes; input["network"] = network;
        QVERIFY(!protocol::storagePayload(input)); // Captured feed is valid but exactly at the two-sided derivative boundary.
        input["derivatives"] = false; QVERIFY(protocol::storagePayload(input));
        input = derivativePayload({"main_pump"}); network = input.value("network").toObject(); routes = network.value("routes").toArray();
        for (qsizetype i = 0; i < routes.size(); ++i) {
            auto route = routes[i].toObject(); if (route.value("id").toString() == "main_pump") { route["available"] = false; route["setpoint_m3h"] = 0.0; } routes[i] = route;
        }
        network["routes"] = routes; input["network"] = network; control = input.value("controls").toArray()[0].toObject(); control["value"] = 0.0;
        input["controls"] = QJsonArray{control}; QVERIFY(!protocol::storagePayload(input));
    }
    void explicitDerivativeFixturesRetainOrderZerosSignsAndOpaqueIds() {
        Environment restore("BREAKWATER_STORAGE_FIXTURE_MODE"); mode("valid");
        const auto input = derivativePayload({"truck_route", "source_a", "bypass"}, true, 3.0, 0.025, 300.0);
        PythonWorkerClient client(python(), fixture()); QSignalSpy ready(&client, &PythonWorkerClient::storageReady), failed(&client, &PythonWorkerClient::failed);
        client.runStorage(input); QTRY_COMPARE_WITH_TIMEOUT(ready.size(), 1, 10000); QCOMPARE(failed.size(), 0);
        const auto result = ready.first().first().toJsonObject().value("result").toObject(); QVERIFY(protocol::storage(result, input));
        const auto derivative = result.value("derivatives").toObject();
        QCOMPARE(derivative.value("terminal_volume_jacobian_h").toArray(), QJsonArray({QJsonArray{-3.0, -3.0, -3.0}, QJsonArray{0.0, 0.0, 3.0}}));
        QCOMPARE(derivative.value("cost_gradient").toArray(), QJsonArray({240.0, -150.0, 0.0}));
        compareNative(result, input); compareNativeDerivatives(result, input);
    }
    void rejectsMalformedDerivativeEvidence_data() {
        QTest::addColumn<QByteArray>("fixtureMode"); QTest::addColumn<QString>("expected");
        for (const auto mode : {"missing_derivatives", "derivative_id_order", "derivative_id_foreign", "derivative_id_extra", "derivative_rows",
                 "derivative_columns", "derivative_null", "gradient_columns", "gradient_string", "gradient_null", "derivative_extra", "derivatives", "result_artifact"})
            QTest::newRow(mode) << QByteArray(mode) << QString("worker_storage_invalid");
        QTest::newRow("nonfinite-derivative-json") << QByteArray("derivative_infinity") << QString("worker_protocol_invalid");
    }
    void rejectsMalformedDerivativeEvidence() {
        QFETCH(QByteArray, fixtureMode); QFETCH(QString, expected); Environment restore("BREAKWATER_STORAGE_FIXTURE_MODE"); mode(fixtureMode);
        PythonWorkerClient client(python(), fixture()); QSignalSpy ready(&client, &PythonWorkerClient::storageReady), failed(&client, &PythonWorkerClient::failed);
        client.runStorage(derivativePayload()); QTRY_COMPARE_WITH_TIMEOUT(failed.size(), 1, 15000);
        QCOMPARE(code(failed), expected); QCOMPARE(ready.size(), 0); QVERIFY(!client.readiness().value("storage_execution_verified").toBool());
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
    void actualSelectedDerivativesMatchNativeFiniteDifferences_data() {
        QTest::addColumn<QStringList>("selection"); QTest::addColumn<bool>("opaque"); QTest::addColumn<double>("horizon"); QTest::addColumn<double>("step"); QTest::addColumn<double>("feedPrice");
        QTest::newRow("one-control-subset") << QStringList{"truck_route"} << false << 3.0 << 0.025 << 0.0;
        QTest::newRow("reordered-three-controls") << QStringList({"bypass", "truck_route", "source_a"}) << false << 3.0 << 0.025 << 0.0;
        QTest::newRow("opaque-zero-price-columns") << QStringList({"backup_pump", "main_pump"}) << true << 3.0 << 0.025 << 0.0;
        QTest::newRow("negative-cost-gradient") << QStringList({"source_a", "truck_route"}) << true << 3.0 << 0.025 << 300.0;
        QTest::newRow("one-ulp-terminal-interval") << QStringList({"source_a", "bypass"}) << true << std::nextafter(0.9, std::numeric_limits<double>::infinity()) << 0.1 << 0.0;
    }
    void actualSelectedDerivativesMatchNativeFiniteDifferences() {
        QFETCH(QStringList, selection); QFETCH(bool, opaque); QFETCH(double, horizon); QFETCH(double, step); QFETCH(double, feedPrice);
        PythonWorkerClient client(python(), worker()); QSignalSpy health(&client, &PythonWorkerClient::healthReady), ready(&client, &PythonWorkerClient::storageReady), failed(&client, &PythonWorkerClient::failed);
        client.checkHealth(); QTRY_COMPARE_WITH_TIMEOUT(health.size(), 1, 10000);
        if (!client.readiness().value("torch_detected").toBool()) QSKIP("Configured Python has no Torch runtime; fake protocol tests still execute.");
        const auto input = derivativePayload(selection, opaque, horizon, step, feedPrice); QVERIFY(protocol::storagePayload(input)); client.runStorage(input);
        QTRY_VERIFY_WITH_TIMEOUT(!ready.isEmpty() || !failed.isEmpty(), 60000); QCOMPARE(failed.size(), 0); QCOMPARE(ready.size(), 1);
        const auto result = ready.first().first().toJsonObject().value("result").toObject(); compareNative(result, input); compareNativeDerivatives(result, input);
        if (selection == QStringList({"backup_pump", "main_pump"})) QCOMPARE(result.value("derivatives").toObject().value("cost_gradient").toArray(), QJsonArray({0.0, 0.0}));
        if (feedPrice > 0.0) QVERIFY(result.value("derivatives").toObject().value("cost_gradient").toArray().first().toDouble() < 0.0);
    }
};

QTEST_GUILESS_MAIN(StorageWorkerTests)
#include "storage_worker_tests.moc"
