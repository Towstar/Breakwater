#include "science/AgentSensitivityClient.hpp"
#include "science/StorageScienceAnalysis.hpp"
#include "history/SessionRepository.hpp"
#include <QElapsedTimer>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QPointer>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTimer>
#include <QUuid>
#include <QtTest>
#include <algorithm>
#include <cmath>

using namespace breakwater;
using namespace breakwater::desktop;
using namespace breakwater::desktop::science;
#ifndef BREAKWATER_TEST_PYTHON
#define BREAKWATER_TEST_PYTHON ""
#endif
namespace {
class FixtureMode final {
public:
    explicit FixtureMode(const QByteArray& mode)
        : present_(qEnvironmentVariableIsSet("BREAKWATER_STORAGE_FIXTURE_MODE")), previous_(qgetenv("BREAKWATER_STORAGE_FIXTURE_MODE")) {
        qputenv("BREAKWATER_STORAGE_FIXTURE_MODE", mode);
    }
    ~FixtureMode() {
        if (present_) qputenv("BREAKWATER_STORAGE_FIXTURE_MODE", previous_); else qunsetenv("BREAKWATER_STORAGE_FIXTURE_MODE");
    }
private:
    bool present_; QByteArray previous_;
};
QString python() { return QString::fromUtf8(BREAKWATER_TEST_PYTHON); }
QString fixture() { return QStringLiteral(BREAKWATER_SOURCE_DIR "/desktop/tests/worker_fixtures/storage_worker_fixture.py"); }
QString worker() { return QStringLiteral(BREAKWATER_SOURCE_DIR "/science/worker.py"); }
QString uuid() { return QUuid::createUuid().toString(QUuid::WithoutBraces); }
std::shared_ptr<const CapturedForecast> captured(bool renamed = false, bool edited = false) {
    auto project = example_project(); project.network = compile(main_pump_failure(project.network), incident_controls("mixed")).network;
    if (edited) {
        project.network.assets.push_back({"buffer", "Buffer", AssetKind::tank, 100, 10});
        project.network.routes.push_back({"third_transfer", "tank_b", "buffer", 12, 5, false, true, 2});
    }
    if (renamed) {
        for (auto& asset : project.network.assets) asset.id = "edited:" + asset.id + ":\xCE\xB2";
        for (auto& route : project.network.routes) {
            route.id = "pipe:" + route.id + ":\xE6\xB0\xB4";
            route.source_id = "edited:" + route.source_id + ":\xCE\xB2";
            route.target_id = "edited:" + route.target_id + ":\xCE\xB2";
        }
    }
    auto capture = std::make_shared<CapturedForecast>(); capture->network = project.network; capture->config = {0.95, 0.3};
    const auto native = compile(capture->network); capture->result = simulate(native, capture->config, Solver::dense_reference);
    capture->validation = validate(native, capture->result, capture->config);
    capture->id = uuid(); capture->physical_hash = physics_hash(project); capture->revision = 7;
    return capture;
}
QJsonArray selected(const CapturedForecast& capture) {
    QJsonArray controls;
    for (const auto& control : control_catalog(capture.network)) {
        if (control.id.find("source_a") != std::string::npos || control.id.find("bypass") != std::string::npos)
            controls.append(QJsonObject{{"control_id", QString::fromStdString(control.id)}, {"value", control.value}});
    }
    return controls;
}
QJsonObject origin(const CapturedForecast& capture) {
    return {{"incident_snapshot_id", uuid()}, {"incident_physical_hash", QString(64, 'a')}, {"run_id", capture.id},
        {"effective_physical_hash", capture.physical_hash}, {"agent_session_id", uuid()}, {"tool_call_id", "model-selected-call:1"}};
}
QJsonObject resultOf(const QSignalSpy& completed) { return completed.last().at(0).toJsonObject(); }
QJsonObject archiveOf(const QSignalSpy& completed) { return completed.last().at(1).toJsonObject(); }
}

class AgentScienceTests final : public QObject {
    Q_OBJECT
private slots:
    void invalidRequestsNeverLaunchAWorker_data() {
        QTest::addColumn<QString>("variation");
        for (const auto* value : {"null", "empty", "too-many", "changed-value", "boundary", "duplicate", "reordered", "foreign-origin",
                 "bad-hash", "missing-origin", "extra-origin", "bad-session", "bad-call"}) QTest::newRow(value) << QString::fromLatin1(value);
    }
    void invalidRequestsNeverLaunchAWorker() {
        QFETCH(QString, variation);
        auto capture = captured(); auto controls = selected(*capture); auto parent = origin(*capture);
        if (variation == "null") capture.reset();
        else if (variation == "empty") controls = {};
        else if (variation == "too-many") while (controls.size() < 9) controls.append(controls.first());
        else if (variation == "changed-value") { auto item = controls.first().toObject(); item["value"] = 11; controls[0] = item; }
        else if (variation == "boundary") controls = QJsonArray{QJsonObject{{"control_id", "route:truck_route:flow"}, {"value", 22}}};
        else if (variation == "duplicate") controls.append(controls.first());
        else if (variation == "reordered") { const QJsonValue first = controls[0]; controls[0] = controls[1]; controls[1] = first; }
        else if (variation == "foreign-origin") parent["run_id"] = uuid();
        else if (variation == "bad-hash") parent["effective_physical_hash"] = QString(64, 'b');
        else if (variation == "missing-origin") parent.remove("tool_call_id");
        else if (variation == "extra-origin") parent["extra"] = true;
        else if (variation == "bad-session") parent["agent_session_id"] = "not-a-session";
        else if (variation == "bad-call") parent["tool_call_id"] = QString(257, 'x');
        AgentSensitivityClient client("missing-python-for-rejected-input", fixture());
        QSignalSpy done(&client, &AgentSensitivityClient::completed), errors(&client, &AgentSensitivityClient::failed);
        client.analyze(capture, controls, parent);
        QCOMPARE(errors.size(), 1); QCOMPARE(errors.first().at(0).toString(), "invalid_sensitivity_request");
        QVERIFY(done.isEmpty()); QVERIFY(!client.busy());
    }
    void partialWorkerProducesCompactAdvisoryAndExactArchive() {
        if (!QFileInfo(python()).isFile()) QSKIP("Configure Python for the controlled peer.");
        FixtureMode mode("partial"); const auto capture = captured(); const auto parent = origin(*capture);
        const auto network = network_json(capture->network);
        AgentSensitivityClient client(python(), fixture());
        QSignalSpy done(&client, &AgentSensitivityClient::completed), errors(&client, &AgentSensitivityClient::failed);
        client.analyze(capture, selected(*capture), parent); QVERIFY(client.busy());
        QTRY_COMPARE_WITH_TIMEOUT(done.size(), 1, 10000); QVERIFY(errors.isEmpty()); QVERIFY(!client.busy());
        const auto result = resultOf(done), archive = archiveOf(done), analysis = result.value("analysis").toObject();
        QVERIFY(result.value("ok").toBool()); QVERIFY(result.value("advisory").toBool());
        QCOMPARE(result.value("validation_status").toString(), "not_performed"); QCOMPARE(result.value("origin").toObject(), parent);
        QVERIFY(!analysis.contains("preview")); QVERIFY(!analysis.contains("native_volumes_m3")); QVERIFY(!analysis.contains("time_h"));
        QVERIFY(QJsonDocument(result).toJson(QJsonDocument::Compact).size() <= 256 * 1024);
        QCOMPARE(analysis.value("units").toObject().value("terminal_volume_jacobian_h").toString(), "h");
        QCOMPARE(analysis.value("selected_controls").toArray().size(), 2); QVERIFY(analysis.value("within_tolerance").toBool());
        QCOMPARE(archive.value("agent_origin").toObject(), parent); QVERIFY(!archive.value("actionable").toBool(true));
        QVERIFY(archive.value("approvals").toArray().isEmpty()); QVERIFY(archive.value("recommendation").toObject().isEmpty());
        QString error; QVERIFY2(history::SessionRepository::validateArchive(archive, &error), qPrintable(error));
        QCOMPARE(archive.value("runs").toArray().first().toObject().value("time_h").toArray().size(), 5);
        QCOMPARE(network_json(capture->network), network);
    }
    void invalidWorkersNeverPublishScientificArchives_data() {
        QTest::addColumn<QByteArray>("fixtureMode");
        for (const auto* mode : {"wrong_id", "wrong_identity", "gradient_columns", "crash_after_storage", "crash_after_exchange", "trailing_noise", "oversized_frame", "runtime_error"})
            QTest::newRow(mode) << QByteArray(mode);
    }
    void invalidWorkersNeverPublishScientificArchives() {
        if (!QFileInfo(python()).isFile()) QSKIP("Configure Python for the controlled peer.");
        QFETCH(QByteArray, fixtureMode); FixtureMode mode(fixtureMode); const auto capture = captured();
        AgentSensitivityClient client(python(), fixture()); client.setTimeoutMs(3000);
        QSignalSpy done(&client, &AgentSensitivityClient::completed), errors(&client, &AgentSensitivityClient::failed);
        client.analyze(capture, selected(*capture), origin(*capture)); QTRY_COMPARE_WITH_TIMEOUT(errors.size(), 1, 6000);
        QVERIFY(!client.busy()); QVERIFY(done.isEmpty());
        QVERIFY(!errors.first().at(1).toString().contains("private-secret-marker"));
    }
    void timeoutIsNonblockingAndMissingPythonIsExplicit() {
        if (!QFileInfo(python()).isFile()) QSKIP("Configure Python for the controlled peer.");
        FixtureMode mode("hang"); const auto capture = captured();
        AgentSensitivityClient client(python(), fixture()); client.setTimeoutMs(250); client.setTerminationGraceMs(0);
        QSignalSpy errors(&client, &AgentSensitivityClient::failed), done(&client, &AgentSensitivityClient::completed);
        int ticks = 0; QTimer heartbeat; connect(&heartbeat, &QTimer::timeout, &heartbeat, [&ticks] { ++ticks; }); heartbeat.start(1);
        client.analyze(capture, selected(*capture), origin(*capture));
        QTRY_COMPARE_WITH_TIMEOUT(errors.size(), 1, 3000); QVERIFY(ticks > 5); QVERIFY(done.isEmpty()); QVERIFY(!client.busy());
        QCOMPARE(errors.first().at(0).toString(), "worker_timeout");
        AgentSensitivityClient missing("missing-python-for-controlled-unavailability", fixture());
        QSignalSpy unavailable(&missing, &AgentSensitivityClient::failed);
        missing.analyze(capture, selected(*capture), origin(*capture)); QTRY_COMPARE_WITH_TIMEOUT(unavailable.size(), 1, 3000);
        QCOMPARE(unavailable.first().at(0).toString(), "worker_start_failed"); QVERIFY(!missing.busy());
    }
    void cancellationSuppressesOldResultsAndAllowsFreshAnalysis() {
        if (!QFileInfo(python()).isFile()) QSKIP("Configure Python for the controlled peer.");
        FixtureMode mode("hang"); const auto capture = captured();
        AgentSensitivityClient client(python(), fixture()); client.setTerminationGraceMs(0);
        QSignalSpy errors(&client, &AgentSensitivityClient::failed), done(&client, &AgentSensitivityClient::completed);
        client.analyze(capture, selected(*capture), origin(*capture)); QVERIFY(client.busy()); QTest::qWait(30);
        client.cancel(); QVERIFY(!client.busy()); qputenv("BREAKWATER_STORAGE_FIXTURE_MODE", "valid");
        const auto next = captured(true); client.analyze(next, selected(*next), origin(*next));
        QTRY_COMPARE_WITH_TIMEOUT(done.size(), 1, 10000); QVERIFY(errors.isEmpty());
        QCOMPARE(resultOf(done).value("run_id").toString(), next->id);
        QTest::qWait(40); QCOMPARE(done.size(), 1);
    }
    void cancellationDuringComparisonSuppressesPublication() {
        if (!QFileInfo(python()).isFile()) QSKIP("Configure Python for the controlled peer.");
        FixtureMode mode("valid"); const auto capture = captured();
        AgentSensitivityClient client(python(), fixture());
        QSignalSpy errors(&client, &AgentSensitivityClient::failed), done(&client, &AgentSensitivityClient::completed);
        auto* peer = client.findChild<PythonWorkerClient*>(); QVERIFY(peer);
        connect(peer, &PythonWorkerClient::storageReady, &client, [&client] { client.cancel(); });
        client.analyze(capture, selected(*capture), origin(*capture));
        QTRY_VERIFY_WITH_TIMEOUT(!client.busy(), 10000); QVERIFY(done.isEmpty()); QVERIFY(errors.isEmpty());
    }
    void destructionFromWorkerStartAndCompletionIsSafe() {
        if (!QFileInfo(python()).isFile()) QSKIP("Configure Python for the controlled peer.");
        FixtureMode mode("valid"); const auto capture = captured();
        auto* starting = new AgentSensitivityClient(python(), fixture()); QPointer<AgentSensitivityClient> startingGuard(starting);
        auto* peer = starting->findChild<PythonWorkerClient*>(); QVERIFY(peer);
        connect(peer, &PythonWorkerClient::changed, starting, [starting] { delete starting; });
        starting->analyze(capture, selected(*capture), origin(*capture)); QVERIFY(!startingGuard);
        auto* finishing = new AgentSensitivityClient(python(), fixture()); QPointer<AgentSensitivityClient> finishingGuard(finishing);
        connect(finishing, &AgentSensitivityClient::completed, finishing, [finishing] { delete finishing; });
        finishing->analyze(capture, selected(*capture), origin(*capture));
        QTRY_VERIFY_WITH_TIMEOUT(!finishingGuard, 10000);
    }
    void realWorkerMatchesDenseFiniteDifferencesAcrossEditedNetworks_data() {
        QTest::addColumn<bool>("renamed"); QTest::addColumn<bool>("edited");
        QTest::newRow("canonical") << false << false;
        QTest::newRow("renamed-unicode") << true << false;
        QTest::newRow("changed-topology") << true << true;
    }
    void realWorkerMatchesDenseFiniteDifferencesAcrossEditedNetworks() {
        if (!QFileInfo(python()).isFile()) QSKIP("Configure the actual CPU PyTorch worker.");
        QFETCH(bool, renamed); QFETCH(bool, edited); const auto capture = captured(renamed, edited); const auto controls = selected(*capture);
        AgentSensitivityClient client(python(), worker()); QSignalSpy done(&client, &AgentSensitivityClient::completed), errors(&client, &AgentSensitivityClient::failed);
        client.analyze(capture, controls, origin(*capture)); QTRY_VERIFY_WITH_TIMEOUT(!client.busy(), 70000);
        QCOMPARE(errors.size(), 0); QCOMPARE(done.size(), 1);
        const auto result = resultOf(done), analysis = result.value("analysis").toObject(), derivatives = analysis.value("derivatives").toObject();
        QVERIFY(analysis.value("within_tolerance").toBool()); const auto matrix = derivatives.value("terminal_volume_jacobian_h").toArray();
        const auto gradient = derivatives.value("cost_gradient").toArray(); QCOMPARE(matrix.size(), static_cast<qsizetype>(capture->result.tank_ids.size()));
        for (qsizetype control = 0; control < controls.size(); ++control) {
            const auto item = controls[control].toObject(); const auto id = item.value("control_id").toString().toStdString(); const auto value = item.value("value").toDouble();
            for (const double epsilon : {1e-3, 1e-4, 1e-5}) {
                const auto plus = simulate(compile(capture->network, std::vector<ControlValue>{{id, value + epsilon}}), capture->config, Solver::dense_reference);
                const auto minus = simulate(compile(capture->network, std::vector<ControlValue>{{id, value - epsilon}}), capture->config, Solver::dense_reference);
                QVERIFY(std::abs(gradient[control].toDouble() - (plus.cost - minus.cost) / (2 * epsilon)) < 1e-6);
                for (qsizetype tank = 0; tank < matrix.size(); ++tank)
                    QVERIFY(std::abs(matrix[tank].toArray()[control].toDouble() -
                        (plus.volumes_m3[static_cast<std::size_t>(tank)].back() - minus.volumes_m3[static_cast<std::size_t>(tank)].back()) / (2 * epsilon)) < 1e-6);
            }
        }
        QString error; QVERIFY2(history::SessionRepository::validateArchive(archiveOf(done), &error), qPrintable(error));
    }
};

QTEST_GUILESS_MAIN(AgentScienceTests)
#include "agent_science_tests.moc"
