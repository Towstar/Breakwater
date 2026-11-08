#include "science/StorageScienceController.hpp"
#include "storage_grid_cases.hpp"
#include <QElapsedTimer>
#include <QFileInfo>
#include <QJsonArray>
#include <QProcess>
#include <QSignalSpy>
#include <QTimer>
#include <QtTest>

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
QString workerPath() { return QStringLiteral(BREAKWATER_SOURCE_DIR "/science/worker.py"); }
bool forecast(ProjectController& project, const QString& example = "mixed", SimulationConfig config = {}) {
    project.loadExample(example); project.forecastWithConfig(config);
    QElapsedTimer deadline; deadline.start();
    while (project.busy() && deadline.elapsed() < 10000) QTest::qWait(1);
    return static_cast<bool>(project.capturedForecast());
}
}
class StorageScienceTests final : public QObject {
    Q_OBJECT
private slots:
    void initTestCase() {
        if (!QFileInfo(python()).isFile()) QSKIP("Configure BREAKWATER_PYTHON for real CPU storage-science tests.");
        QProcess process; process.start(python(), {"-c", "import torch; assert torch.ones(1,dtype=torch.float64).device.type=='cpu'"});
        QVERIFY(process.waitForFinished(20000));
        if (process.exitCode() != 0) QSKIP("The configured interpreter cannot execute Torch CPU float64.");
    }
    void captureRetainsOriginalConfigurationAndImmutableReference() {
        ProjectController project; QVERIFY(forecast(project, "mixed", {0.95, 0.3}));
        const auto captured = project.capturedForecast(); QVERIFY(captured);
        QCOMPARE(captured->config.horizon_h, 0.95); QCOMPARE(captured->config.step_h, 0.3);
        QCOMPARE(captured->result.time_h.size(), std::size_t{5}); QCOMPARE(captured->result.time_h.back(), 0.95);
        QCOMPARE(project.capturedRun().value("sample_step_h").toDouble(), 0.3);
        project.moveAsset("tank_a", 300, 100); project.select("truck_route"); project.setCamera(40, -50, 800, 0, 0);
        QCOMPARE(project.capturedForecast(), captured);
        auto parameters = project.pipeParameters("truck_route"); parameters["length_m"] = 120.0;
        QVERIFY(project.setPipeParameters("truck_route", parameters)); QCOMPARE(project.capturedForecast(), captured);
        project.forecast(); QVERIFY(!project.capturedForecast()); QVERIFY(!project.canApprove());
        QTRY_VERIFY_WITH_TIMEOUT(!project.busy(), 10000); QVERIFY(project.capturedForecast() != captured);
        project.reset(); QVERIFY(!project.capturedForecast());
        QCOMPARE(captured->result.time_h.back(), 0.95); QCOMPARE(captured->config.step_h, 0.3);
    }
    void realDesktopWorkerParity_data() {
        QTest::addColumn<QString>("example"); QTest::addColumn<double>("horizon"); QTest::addColumn<double>("step");
        QTest::newRow("mixed") << QString("mixed") << 3.0 << 0.025;
        QTest::newRow("overflowing-baseline") << QString("baseline") << 3.0 << 0.025;
        QTest::newRow("bad-downstream") << QString("bad") << 3.0 << 0.025;
        QTest::newRow("shortened-final-step") << QString("mixed") << 0.95 << 0.3;
        QTest::newRow("decimated-indices") << QString("mixed") << 3.0 << 0.002;
        QTest::newRow("one-ulp-endpoint") << QString("mixed") << 0.00023685942255445778 << 2.7423807173145508e-08;
        for (const auto& item : storage_grid_test::cases())
            QTest::newRow(item.name) << QString("mixed") << item.horizon << item.step;
    }
    void realDesktopWorkerParity() {
        QFETCH(QString, example); QFETCH(double, horizon); QFETCH(double, step);
        ProjectController project; QVERIFY(forecast(project, example, {horizon, step}));
        const auto capture = project.capturedForecast(); const auto document = project.document(); const auto run = project.runId();
        const auto approval = project.canApprove();
        PythonWorkerClient worker(python(), workerPath()); StorageScienceController science(&project, &worker);
        QSignalSpy done(&science, &StorageScienceController::completed), failed(&science, &StorageScienceController::failed);
        QVERIFY(science.canRun()); science.run(); QVERIFY(science.busy());
        bool serviced = false; QTimer::singleShot(0, &project, [&] { serviced = true; }); QTRY_VERIFY(serviced);
        QTRY_VERIFY_WITH_TIMEOUT(!science.busy(), 70000);
        QVERIFY2(failed.isEmpty(), qPrintable(science.status())); QCOMPARE(done.size(), 1);
        const auto result = QJsonObject::fromVariantMap(science.result()); QVERIFY(result.value("within_tolerance").toBool());
        QCOMPARE(result.value("run_id").toString(), run); QCOMPARE(result.value("snapshot_id").toString(), run);
        QCOMPARE(result.value("horizon_h").toDouble(), horizon); QCOMPARE(result.value("step_h").toDouble(), step);
        QCOMPARE(result.value("native_cost").toDouble(), capture->result.cost);
        const auto preview = result.value("preview").toObject(); const auto indices = preview.value("time_indices").toArray();
        const auto times = preview.value("time_h").toArray();
        QCOMPARE(indices.last().toInteger(), static_cast<qint64>(capture->result.time_h.size() - 1));
        QCOMPARE(times.last().toDouble(), horizon);
        for (const auto& item : storage_grid_test::cases()) if (item.horizon == horizon && item.step == step) {
            QCOMPARE(capture->result.time_h, item.times);
            QCOMPARE(indices.last().toInteger(), static_cast<qint64>(item.times.size() - 1));
        }
        for (qsizetype i = 0; i < indices.size(); ++i) QCOMPARE(times[i].toDouble(), capture->result.time_h[static_cast<std::size_t>(indices[i].toInteger())]);
        QCOMPARE(project.document(), document); QCOMPARE(project.runId(), run); QCOMPARE(project.canApprove(), approval);
        QVERIFY(worker.readiness().value("storage_execution_verified").toBool());
        QVERIFY(science.chart().value("available").toBool()); QCOMPARE(science.tanks().size(), 2);
        const auto nativeSeries = science.chart().value("series").toList().first().toMap(); QVERIFY(nativeSeries.value("reference").toBool());
    }
    void renamedCustomNetworkUsesItsCapturedIdsAndTopology() {
        ProjectController project;
        auto document = project.document(), network = document.value("network").toObject();
        auto assets = network.value("assets").toArray(), routes = network.value("routes").toArray();
        for (qsizetype i = 0; i < assets.size(); ++i) {
            auto asset = assets[i].toObject(); asset["id"] = "custom:" + asset.value("id").toString(); assets[i] = asset;
        }
        for (qsizetype i = 0; i < routes.size(); ++i) {
            auto route = routes[i].toObject(); route["id"] = "custom:" + route.value("id").toString();
            route["source_id"] = "custom:" + route.value("source_id").toString(); route["target_id"] = "custom:" + route.value("target_id").toString();
            if (route.value("id").toString() == "custom:bypass") route["target_id"] = "custom:trucking";
            routes[i] = route;
        }
        network["assets"] = assets; network["routes"] = routes; document["network"] = network;
        QVERIFY(project.restore(document)); project.forecast(); QTRY_VERIFY_WITH_TIMEOUT(!project.busy(), 10000); QVERIFY(project.capturedForecast());
        const auto original = project.document(); const auto approval = project.canApprove();
        PythonWorkerClient worker(python(), workerPath()); StorageScienceController science(&project, &worker);
        QSignalSpy done(&science, &StorageScienceController::completed);
        science.run(); QTRY_COMPARE_WITH_TIMEOUT(done.size(), 1, 70000);
        QVERIFY(science.result().value("within_tolerance").toBool());
        for (const auto& tank : science.tanks()) QVERIFY(tank.toMap().value("id").toString().startsWith("custom:"));
        QCOMPARE(project.document(), original); QCOMPARE(project.canApprove(), approval);
    }
    void layoutSelectionAndQualityEditsPreserveCompletedScience() {
        ProjectController project; QVERIFY(forecast(project));
        PythonWorkerClient worker(python(), workerPath()); StorageScienceController science(&project, &worker);
        science.run(); QTRY_VERIFY_WITH_TIMEOUT(!science.busy(), 70000); QVERIFY(!science.result().isEmpty());
        const auto result = science.result(); const auto run = project.runId(); const auto original = project.document();
        science.selectTank("tank_b"); QCOMPARE(science.selectedTankId(), "tank_b");
        QCOMPARE(project.document(), original); QCOMPARE(project.runId(), run); QVERIFY(project.canApprove());
        project.moveAsset("tank_a", 300, 150); project.select("truck_route");
        auto parameters = project.pipeParameters("truck_route"); parameters["length_m"] = 140.0;
        QVERIFY(project.setPipeParameters("truck_route", parameters)); QCOMPARE(science.result(), result);
        project.editSelected("flow", "10"); QVERIFY(science.result().isEmpty()); QVERIFY(!science.canRun()); QVERIFY(!project.canApprove());
    }
    void resetAndNumericalEditsRetireInFlightResultsAndAllowFreshRetry() {
        ProjectController project; QVERIFY(forecast(project));
        PythonWorkerClient worker(python(), workerPath()); StorageScienceController science(&project, &worker);
        QSignalSpy done(&science, &StorageScienceController::completed);
        science.run(); QVERIFY(science.busy()); project.reset();
        QTRY_VERIFY_WITH_TIMEOUT(!worker.busy(), 10000); QVERIFY(!science.busy()); QVERIFY(science.result().isEmpty());
        QVERIFY(!project.canApprove()); QCOMPARE(done.size(), 0);
        QVERIFY(forecast(project)); QTRY_VERIFY_WITH_TIMEOUT(science.canRun(), 10000);
        science.run(); QTRY_COMPARE_WITH_TIMEOUT(done.size(), 1, 70000); QVERIFY(!science.result().isEmpty());
        project.forecast(); QVERIFY(science.result().isEmpty()); QVERIFY(!science.canRun());
        QTRY_VERIFY_WITH_TIMEOUT(!project.busy(), 10000);
    }
    void cancellationDuringComparisonAndDestructionCannotPublishLate() {
        ProjectController project; QVERIFY(forecast(project));
        PythonWorkerClient worker(python(), workerPath()); StorageScienceController science(&project, &worker);
        QSignalSpy done(&science, &StorageScienceController::completed);
        connect(&worker, &PythonWorkerClient::storageReady, &science, [&] { science.cancel(); });
        science.run(); QTRY_VERIFY_WITH_TIMEOUT(!science.busy(), 70000); QVERIFY(science.result().isEmpty()); QCOMPARE(done.size(), 0);
        auto* temporary = new StorageScienceController(&project, &worker); temporary->run(); QVERIFY(temporary->busy());
        QElapsedTimer elapsed; elapsed.start(); delete temporary; QVERIFY(elapsed.elapsed() < 500);
        QTRY_VERIFY_WITH_TIMEOUT(!worker.busy(), 10000);
        QCOMPARE(project.canApprove(), true);
    }
    void synchronousResetDuringPublicationCannotEmitRetiredCompletion() {
        ProjectController project; QVERIFY(forecast(project));
        PythonWorkerClient worker(python(), workerPath()); StorageScienceController science(&project, &worker);
        QSignalSpy done(&science, &StorageScienceController::completed);
        bool retired = false;
        connect(&science, &StorageScienceController::changed, &project, [&] {
            if (!retired && !science.result().isEmpty()) { retired = true; project.reset(); }
        });
        science.run(); QTRY_VERIFY_WITH_TIMEOUT(retired, 70000);
        QVERIFY(!science.busy()); QVERIFY(science.result().isEmpty()); QCOMPARE(done.size(), 0);
        QVERIFY(!project.canApprove());
    }
    void finiteDisagreementIsVisibleWithoutApplyingControlsOrApproval() {
        Environment restore("BREAKWATER_STORAGE_FIXTURE_MODE"); qputenv("BREAKWATER_STORAGE_FIXTURE_MODE", "finite_disagreement");
        ProjectController project; QVERIFY(forecast(project, "bad")); QVERIFY(!project.canApprove());
        const auto document = project.document(); const auto captured = project.capturedForecast();
        PythonWorkerClient worker(python(), QStringLiteral(BREAKWATER_SOURCE_DIR "/desktop/tests/worker_fixtures/storage_worker_fixture.py"));
        StorageScienceController science(&project, &worker); QSignalSpy done(&science, &StorageScienceController::completed);
        science.run(); QTRY_COMPARE_WITH_TIMEOUT(done.size(), 1, 15000);
        QVERIFY(!science.result().value("within_tolerance").toBool());
        QVERIFY(science.status().contains("disagreement")); QVERIFY(science.chart().value("available").toBool());
        QCOMPARE(project.document(), document); QCOMPARE(project.capturedForecast(), captured); QVERIFY(!project.canApprove());
        QCOMPARE(captured->result.volumes_m3[1].back(), 132.0);
    }
};
QTEST_GUILESS_MAIN(StorageScienceTests)
#include "storage_science_tests.moc"
