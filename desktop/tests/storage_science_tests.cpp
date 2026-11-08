#include "science/StorageScienceController.hpp"
#include "science/StorageScienceEvidence.hpp"
#include "storage_grid_cases.hpp"
#include <QElapsedTimer>
#include <QFileInfo>
#include <QJsonArray>
#include <QProcess>
#include <QPointer>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTimer>
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
QString workerPath() { return QStringLiteral(BREAKWATER_SOURCE_DIR "/science/worker.py"); }
bool forecast(ProjectController& project, const QString& example = "mixed", SimulationConfig config = {}) {
    project.loadExample(example); project.forecastWithConfig(config);
    QElapsedTimer deadline; deadline.start();
    while (project.busy() && deadline.elapsed() < 10000) QTest::qWait(1);
    return static_cast<bool>(project.capturedForecast());
}
QVariantMap controlRow(const StorageScienceController& science, const QString& id) {
    for (const auto& value : science.controls()) if (value.toMap().value("control_id").toString() == id) return value.toMap();
    return {};
}
ProjectDocument sensitivityProject() {
    ProjectDocument project;
    project.network.assets = {{"tank:α", "West", AssetKind::tank, 1000, 100},
        {"tank:β", "East", AssetKind::tank, 1000, 100},
        {"prod:α", "Production", AssetKind::source, 0, 0, 20, 3, 10, 7},
        {"plant:Ω", "Plant", AssetKind::treatment, 50, 0, 0, 0, 0, 5},
        {"truck:υ", "Truck", AssetKind::trucking, 10, 0, 0, 0, 0, 11}};
    project.network.routes = {{"feed:λ", "prod:α", "tank:α", 20, 20, false, true, 2},
        {"transfer:μ", "tank:α", "tank:β", 12, 4, false, true, 1.5},
        {"treat:west", "tank:α", "plant:Ω", 15, 6, false, true, 3},
        {"treat:east", "tank:β", "plant:Ω", 20, 5, false, true, 4},
        {"haul:υ", "tank:α", "truck:υ", 10, 1, false, true, 6},
        {"failed:φ", "tank:α", "plant:Ω", 30, 0, true, false, 0}};
    return project;
}
bool captureProject(ProjectController& project, const ProjectDocument& input, SimulationConfig config = {}) {
    if (!project.restore(project_json(input))) return false;
    project.forecastWithConfig(config);
    QElapsedTimer deadline; deadline.start();
    while (project.busy() && deadline.elapsed() < 10000) QTest::qWait(1);
    return static_cast<bool>(project.capturedForecast());
}
QJsonObject controlledSensitivityResponse(const CapturedForecast& capture, const QStringList& controlIds) {
    QJsonArray ids, values, columns, matrix, gradient;
    for (const auto& id : controlIds) { columns.append(id); gradient.append(0.0); }
    for (std::size_t i = 0; i < capture.result.tank_ids.size(); ++i) {
        ids.append(QString::fromStdString(capture.result.tank_ids[i]));
        values.append(QJsonArray{capture.result.volumes_m3[i].front(), capture.result.volumes_m3[i].back()});
        QJsonArray row; for (qsizetype column = 0; column < controlIds.size(); ++column) row.append(0.0);
        matrix.append(row);
    }
    return {{"status", "completed"}, {"result", QJsonObject{{"method", "storage_rollout"}, {"method_version", 1},
        {"identity", QJsonObject{{"snapshot_id", capture.id}, {"volume_run_id", capture.id}, {"physical_hash", capture.physical_hash},
            {"numerical_revision", static_cast<qint64>(capture.revision)}}},
        {"time_h", QJsonArray{capture.result.time_h.front(), capture.result.time_h.back()}},
        {"time_indices", QJsonArray{0, static_cast<qint64>(capture.result.time_h.size() - 1)}},
        {"tank_ids", ids}, {"volumes_m3", values}, {"cost", capture.result.cost},
        {"provenance", QJsonObject{{"implementation", "torch_storage_float64"}, {"implementation_version", "controlled-test-only"},
            {"dtype", "float64"}, {"device", "cpu"}, {"execution_status", "completed"}}}, {"warnings", QJsonArray{}},
        {"derivatives", QJsonObject{{"control_ids", columns}, {"terminal_volume_jacobian_h", matrix}, {"cost_gradient", gradient}}}}}};
}
}
class StorageScienceTests final : public QObject {
    Q_OBJECT
private slots:
    void splitFeedsCompareWithRealWorkerAndRetireOnAllocationEdit() {
        auto input = sensitivityProject();
        input.network.routes.front().source_share_bps = 7500;
        input.network.routes.push_back({"feed:east", "prod:α", "tank:β", 20, 0, false, true, 6, 2500});
        ProjectController project; QVERIFY(captureProject(project, input, {1.13, 0.07}));
        const auto captured = project.capturedForecast();
        QVERIFY(std::abs(captured->result.volumes_m3[0].back()-101.9775) < 1e-9);
        QVERIFY(std::abs(captured->result.volumes_m3[1].back()-103.6725) < 1e-9);
        PythonWorkerClient worker(python(), workerPath()); StorageScienceController science(&project, &worker);
        QSignalSpy done(&science, &StorageScienceController::completed), failed(&science, &StorageScienceController::failed);
        science.run(); QTRY_COMPARE_WITH_TIMEOUT(done.size(), 1, 70000); QVERIFY2(failed.isEmpty(), qPrintable(science.status()));
        QVERIFY(science.result().value("within_tolerance").toBool()); QCOMPARE(science.result().value("result_version").toInt(), 2);
        QVERIFY(science.selectControl(QStringLiteral("source:prod:α:curtail"), true));
        science.runSensitivities(); QTRY_COMPARE_WITH_TIMEOUT(done.size(), 2, 70000);
        const auto result = QJsonObject::fromVariantMap(science.result());
        QVERIFY2(failed.isEmpty(), qPrintable(science.status())); QVERIFY(result.value("within_tolerance").toBool());
        const auto derivatives = result.value("derivatives").toObject();
        const auto matrix = derivatives.value("terminal_volume_jacobian_h").toArray();
        QVERIFY(std::abs(matrix[0].toArray()[0].toDouble()+.8475)<1e-12);
        QVERIFY(std::abs(matrix[1].toArray()[0].toDouble()+.2825)<1e-12);
        QVERIFY(std::abs(derivatives.value("cost_gradient").toArray()[0].toDouble()-4.52)<1e-12);
        const auto archive = storage_science_archive(*captured, result);
        QString error; QVERIFY2(history::SessionRepository::validateArchive(archive, &error), qPrintable(error));
        QVERIFY(validate_storage_science_evidence(result, archive)); QVERIFY(!archive.value("actionable").toBool(true));
        project.moveAsset(QStringLiteral("tank:β"), 70, 80); QCOMPARE(project.capturedForecast(), captured);
        QVERIFY(!science.result().isEmpty()); QVERIFY(project.canApprove());
        QVERIFY(project.setSourceFeeds(QStringLiteral("prod:α"), {{QStringLiteral("tank:α"), 5000}, {QStringLiteral("tank:β"), 5000}}, project.networkHash()));
        QVERIFY(science.result().isEmpty()); QVERIFY(!project.capturedForecast()); QVERIFY(!project.canApprove());
        QCOMPARE(captured->network.routes.front().source_share_bps, std::uint32_t{7500});
        QVERIFY(validate_storage_science_evidence(result, archive));
    }
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
    void capturedCatalogRejectsBoundariesFixedControlsAndFeedCapacityWithoutEditingValues() {
        ProjectController project; QVERIFY(forecast(project));
        PythonWorkerClient worker(python(), workerPath()); StorageScienceController science(&project, &worker);
        const auto document = project.document(); const auto capture = project.capturedForecast();
        const auto catalog = control_catalog(capture->network);
        QCOMPARE(science.controls().size(), static_cast<qsizetype>(catalog.size()));
        QVERIFY(science.selectedControlIds().isEmpty()); QVERIFY(!science.canRunSensitivities());
        for (qsizetype i = 0; i < science.controls().size(); ++i) {
            const auto row = science.controls()[i].toMap(); const auto& native = catalog[static_cast<std::size_t>(i)];
            QCOMPARE(row.value("control_id").toString(), QString::fromStdString(native.id));
            QCOMPARE(row.value("captured_value").toDouble(), native.value);
            QCOMPARE(row.value("minimum").toDouble(), native.minimum); QCOMPARE(row.value("maximum").toDouble(), native.maximum);
            QCOMPARE(row.value("unit").toString(), QString::fromStdString(native.unit)); QVERIFY(!row.value("selected").toBool());
        }
        QVERIFY(controlRow(science, "source:source_a:curtail").value("eligible").toBool());
        for (const auto id : {"source:source_b:curtail", "route:main_pump:flow", "route:backup_pump:flow", "route:truck_route:flow"}) {
            const auto row = controlRow(science, QLatin1String(id)); QVERIFY(!row.value("eligible").toBool());
            QVERIFY(!row.value("reason").toString().isEmpty()); QVERIFY(!science.selectControl(QLatin1String(id), true));
        }
        QVERIFY(!science.selectControl("fabricated:control", true));
        QVERIFY(science.selectControl("route:bypass:flow", true)); QVERIFY(science.selectControl("source:source_a:curtail", true));
        QCOMPARE(science.selectedControlIds(), QStringList({"source:source_a:curtail", "route:bypass:flow"}));
        QVERIFY(science.canRunSensitivities()); QCOMPARE(project.document(), document); QCOMPARE(project.capturedForecast(), capture); QVERIFY(project.canApprove());
        auto feedBoundary = sensitivityProject(); feedBoundary.network.routes[0].capacity_m3h = 17;
        QVERIFY(captureProject(project, feedBoundary)); QVERIFY(science.selectedControlIds().isEmpty());
        const auto source = QStringLiteral("source:prod:α:curtail"); const auto boundary = controlRow(science, source);
        QCOMPARE(boundary.value("captured_value").toDouble(), 3.0); QVERIFY(!boundary.value("eligible").toBool());
        QVERIFY(boundary.value("reason").toString().contains("feed-capacity")); QVERIFY(!science.selectControl(source, true));
        QVERIFY(science.canRun()); QVERIFY(!science.canRunSensitivities());
        science.runSensitivities(); QVERIFY(!science.busy()); QVERIFY(science.result().isEmpty());
    }
    void realSelectedSensitivitiesRetainCapturedValuesCatalogOrderAndNativeCentralDifferences() {
        ProjectController project; QVERIFY(captureProject(project, sensitivityProject(), {1.13, 0.07}));
        const auto capture = project.capturedForecast(); const auto document = project.document(); const auto approval = project.canApprove();
        PythonWorkerClient worker(python(), workerPath()); StorageScienceController science(&project, &worker);
        const QString source = QStringLiteral("source:prod:α:curtail"), transfer = QStringLiteral("route:transfer:μ:flow"), haul = QStringLiteral("route:haul:υ:flow");
        QVERIFY(science.selectControl(haul, true)); QVERIFY(science.selectControl(source, true)); QVERIFY(science.selectControl(transfer, true));
        const QStringList expectedIds{source, transfer, haul}; QCOMPARE(science.selectedControlIds(), expectedIds);
        QSignalSpy done(&science, &StorageScienceController::completed), failed(&science, &StorageScienceController::failed);
        science.runSensitivities(); QTRY_COMPARE_WITH_TIMEOUT(done.size(), 1, 70000); QVERIFY2(failed.isEmpty(), qPrintable(science.status()));
        const auto result = QJsonObject::fromVariantMap(science.result());
        QCOMPARE(result.value("analysis_mode").toString(), "sensitivities"); QCOMPARE(result.value("native_solver_version").toString(), QString::fromLatin1(solver_version));
        QVERIFY(result.value("within_tolerance").toBool()); const auto selected = result.value("selected_controls").toArray();
        const auto derivatives = result.value("derivatives").toObject();
        const auto ids = derivatives.value("control_ids").toArray(), matrix = derivatives.value("terminal_volume_jacobian_h").toArray();
        const auto gradient = derivatives.value("cost_gradient").toArray(); QCOMPARE(ids.size(), expectedIds.size());
        for (qsizetype column = 0; column < ids.size(); ++column) {
            const auto id = expectedIds[column]; const auto row = controlRow(science, id);
            QCOMPARE(ids[column].toString(), id); QCOMPARE(selected[column].toObject().value("control_id").toString(), id);
            QCOMPARE(selected[column].toObject().value("captured_value").toDouble(), row.value("captured_value").toDouble());
            QCOMPARE(selected[column].toObject().value("unit").toString(), "m3/h");
            const auto capturedValue = row.value("captured_value").toDouble();
            for (const auto relative : {1e-3, 1e-4, 1e-5}) {
                const auto epsilon = relative * std::max(1.0, std::abs(capturedValue));
                const auto plus = simulate(compile(capture->network, std::vector<ControlValue>{{id.toStdString(), capturedValue + epsilon}}), capture->config, Solver::dense_reference);
                const auto minus = simulate(compile(capture->network, std::vector<ControlValue>{{id.toStdString(), capturedValue - epsilon}}), capture->config, Solver::dense_reference);
                for (qsizetype tank = 0; tank < matrix.size(); ++tank) {
                    const auto expected = (plus.volumes_m3[static_cast<std::size_t>(tank)].back() - minus.volumes_m3[static_cast<std::size_t>(tank)].back()) / (2 * epsilon);
                    const auto actual = matrix[tank].toArray()[column].toDouble();
                    QVERIFY(std::abs(actual - expected) <= 1e-6 + 1e-4 * std::abs(expected));
                }
                const auto expected = (plus.cost - minus.cost) / (2 * epsilon);
                QVERIFY(std::abs(gradient[column].toDouble() - expected) <= 1e-6 + 1e-4 * std::abs(expected));
            }
        }
        QCOMPARE(science.sensitivityRows().size(), 3); science.selectTank(QStringLiteral("tank:β"));
        const auto rows = science.sensitivityRows(); QCOMPARE(rows.size(), 3);
        QCOMPARE(rows[0].toMap().value("terminal_volume_jacobian_h").toDouble(), 0.0);
        QCOMPARE(rows[1].toMap().value("terminal_volume_jacobian_h").toDouble(), capture->config.horizon_h);
        QCOMPARE(rows[2].toMap().value("terminal_volume_jacobian_h").toDouble(), 0.0);
        QVERIFY(science.selectControl(transfer, false)); QVERIFY(science.result().isEmpty()); QVERIFY(science.sensitivityRows().isEmpty());
        science.runSensitivities(); QTRY_COMPARE_WITH_TIMEOUT(done.size(), 2, 70000);
        const auto subset = QJsonObject::fromVariantMap(science.result());
        QCOMPARE(subset.value("derivatives").toObject().value("control_ids").toArray(), QJsonArray({source, haul}));
        QCOMPARE(subset.value("worker_cost").toDouble(), result.value("worker_cost").toDouble());
        QCOMPARE(subset.value("preview").toObject().value("worker_volumes_m3"), result.value("preview").toObject().value("worker_volumes_m3"));
        QCOMPARE(project.document(), document); QCOMPARE(project.capturedForecast(), capture); QCOMPARE(project.canApprove(), approval); QVERIFY(approval);
    }
    void selectionChangesKeepIndependentForwardEvidenceAndRetirePendingSensitivities() {
        ProjectController project; QVERIFY(forecast(project));
        const auto document = project.document(); const auto capture = project.capturedForecast();
        PythonWorkerClient worker(python(), workerPath()); StorageScienceController science(&project, &worker);
        QVERIFY(science.selectControl("source:source_a:curtail", true));
        QSignalSpy done(&science, &StorageScienceController::completed);
        science.run(); QVERIFY(science.busy()); QVERIFY(science.selectControl("route:bypass:flow", true));
        QTRY_COMPARE_WITH_TIMEOUT(done.size(), 1, 70000);
        const auto forward = science.result(); QCOMPARE(forward.value("analysis_mode").toString(), "forward");
        QVERIFY(forward.value("selected_controls").toList().isEmpty()); QVERIFY(!forward.contains("derivatives")); QVERIFY(science.sensitivityRows().isEmpty());
        QVERIFY(science.selectControl("route:bypass:flow", false)); QCOMPARE(science.result(), forward);
        science.runSensitivities(); QVERIFY(science.busy());
        QVERIFY(science.selectControl("route:bypass:flow", true));
        QTRY_VERIFY_WITH_TIMEOUT(!science.busy() && !worker.busy(), 10000);
        QVERIFY(science.result().isEmpty()); QCOMPARE(done.size(), 1);
        QCOMPARE(science.selectedControlIds(), QStringList({"source:source_a:curtail", "route:bypass:flow"}));
        science.runSensitivities(); QTRY_COMPARE_WITH_TIMEOUT(done.size(), 2, 70000);
        QCOMPARE(science.result().value("analysis_mode").toString(), "sensitivities");
        const auto sensitivity = science.result();
        QVERIFY(!science.selectControl("route:truck_route:flow", true)); QCOMPARE(science.result(), sensitivity);
        QCOMPARE(project.document(), document); QCOMPARE(project.capturedForecast(), capture); QVERIFY(project.canApprove());
    }
    void layoutQualityAndViewChangesPreserveSelectedSensitivitiesButNewForecastClearsSelection() {
        ProjectController project; QVERIFY(forecast(project));
        PythonWorkerClient worker(python(), workerPath()); StorageScienceController science(&project, &worker);
        QVERIFY(science.selectControl("source:source_a:curtail", true)); QVERIFY(science.selectControl("route:bypass:flow", true));
        const auto capture = project.capturedForecast(); const auto selection = science.selectedControlIds();
        const auto hash = project.networkHash(); QSignalSpy done(&science, &StorageScienceController::completed);
        science.runSensitivities();
        project.moveAsset("tank_a", 320, 140); project.select("truck_route"); project.setCamera(45, -50, 800, 10, 0);
        auto parameters = project.pipeParameters("truck_route"); parameters["length_m"] = 140;
        QVERIFY(project.setPipeParameters("truck_route", parameters)); project.setForecastFraction(0.5);
        QCOMPARE(project.capturedForecast(), capture); QCOMPARE(science.selectedControlIds(), selection);
        QTRY_COMPARE_WITH_TIMEOUT(done.size(), 1, 70000); QVERIFY(science.result().value("within_tolerance").toBool());
        const auto result = science.result(); project.moveAsset("tank_b", 100, 160); science.selectTank("tank_b");
        QCOMPARE(science.result(), result); QCOMPARE(science.selectedControlIds(), selection); QCOMPARE(project.networkHash(), hash); QVERIFY(project.canApprove());
        project.forecast(); QVERIFY(science.selectedControlIds().isEmpty()); QVERIFY(science.controls().isEmpty()); QVERIFY(science.result().isEmpty());
        QTRY_VERIFY_WITH_TIMEOUT(!project.busy(), 10000); QVERIFY(!science.controls().isEmpty()); QVERIFY(!science.canRunSensitivities());
        QVERIFY(science.canRun()); QVERIFY(project.capturedForecast() != capture);
    }
    void numericalEditsAndResetRetireSensitivityResultsWithoutRestoringSelections() {
        ProjectController project; QVERIFY(forecast(project));
        PythonWorkerClient worker(python(), workerPath()); StorageScienceController science(&project, &worker);
        QVERIFY(science.selectControl("source:source_a:curtail", true)); QSignalSpy done(&science, &StorageScienceController::completed);
        science.runSensitivities(); project.select("truck_route"); project.editSelected("flow", "10");
        QTRY_VERIFY_WITH_TIMEOUT(!science.busy() && !worker.busy(), 10000);
        QVERIFY(science.result().isEmpty()); QVERIFY(science.selectedControlIds().isEmpty()); QCOMPARE(done.size(), 0); QVERIFY(!project.canApprove());
        project.forecast(); QTRY_VERIFY_WITH_TIMEOUT(!project.busy(), 10000); QVERIFY(project.capturedForecast());
        QVERIFY(science.selectedControlIds().isEmpty()); QVERIFY(science.selectControl("source:source_a:curtail", true));
        const auto newRun = project.runId(); science.runSensitivities(); QTRY_COMPARE_WITH_TIMEOUT(done.size(), 1, 70000);
        QCOMPARE(science.result().value("run_id").toString(), newRun);
        science.runSensitivities(); project.reset(); QTRY_VERIFY_WITH_TIMEOUT(!science.busy() && !worker.busy(), 10000);
        QVERIFY(science.result().isEmpty()); QVERIFY(science.selectedControlIds().isEmpty()); QCOMPARE(done.size(), 1); QVERIFY(!project.canApprove());
    }
    void sensitivityCancellationLifetimeAndSynchronousSelectionCannotPublishRetiredResults() {
        ProjectController project; QVERIFY(forecast(project));
        PythonWorkerClient worker(python(), workerPath()); StorageScienceController science(&project, &worker);
        QVERIFY(science.selectControl("source:source_a:curtail", true)); const auto document = project.document();
        QSignalSpy done(&science, &StorageScienceController::completed);
        const auto cancellation = connect(&worker, &PythonWorkerClient::storageReady, &science, [&] { science.cancel(); });
        science.runSensitivities(); QTRY_VERIFY_WITH_TIMEOUT(!science.busy(), 70000);
        QVERIFY(science.result().isEmpty()); QCOMPARE(done.size(), 0); QCOMPARE(science.selectedControlIds(), QStringList{"source:source_a:curtail"});
        disconnect(cancellation);
        bool retired = false;
        connect(&science, &StorageScienceController::changed, &project, [&] {
            if (!retired && !science.result().isEmpty()) { retired = true; QVERIFY(science.selectControl("source:source_a:curtail", false)); }
        });
        science.runSensitivities(); QTRY_VERIFY_WITH_TIMEOUT(retired, 70000); QVERIFY(science.result().isEmpty()); QCOMPARE(done.size(), 0);
        auto* temporary = new StorageScienceController(&project, &worker); QVERIFY(temporary->selectControl("source:source_a:curtail", true));
        temporary->runSensitivities(); QVERIFY(temporary->busy()); QElapsedTimer elapsed; elapsed.start(); delete temporary; QVERIFY(elapsed.elapsed() < 500);
        QTRY_VERIFY_WITH_TIMEOUT(!worker.busy(), 10000); QCOMPARE(project.document(), document); QVERIFY(project.canApprove());
    }
    void controllerIndependentlyRejectsMalformedDerivativeOutput_data() {
        QTest::addColumn<QString>("tamper");
        for (const auto name : {"missing", "column-order", "unknown-control", "missing-tank", "missing-column", "null-jacobian", "string-gradient", "extra-field", "foreign-identity"})
            QTest::newRow(name) << QString::fromLatin1(name);
    }
    void selectionRetirementAllowsSynchronousObserverDestruction() {
        ProjectController project; QVERIFY(forecast(project)); const auto document = project.document();
        PythonWorkerClient worker(python(), workerPath());
        QPointer<StorageScienceController> science = new StorageScienceController(&project, &worker);
        QVERIFY(science->selectControl("source:source_a:curtail", true));
        bool deleteOnRetirement = false;
        const auto observer = connect(&worker, &PythonWorkerClient::changed, &project, [&] {
            if (deleteOnRetirement && science) delete science.data();
        });
        science->runSensitivities(); QVERIFY(science->busy()); deleteOnRetirement = true;
        QVERIFY(science->selectControl("route:bypass:flow", true)); QVERIFY(science.isNull());
        disconnect(observer); QTRY_VERIFY_WITH_TIMEOUT(!worker.busy(), 10000);
        QCOMPARE(project.document(), document); QVERIFY(project.canApprove());
    }
    void controllerIndependentlyRejectsMalformedDerivativeOutput() {
        QFETCH(QString, tamper);
        ProjectController project; QVERIFY(forecast(project)); const auto document = project.document();
        const auto capture = project.capturedForecast();
        PythonWorkerClient worker(python(), workerPath()); StorageScienceController science(&project, &worker);
        const QStringList ids{"source:source_a:curtail", "route:bypass:flow"};
        for (const auto& id : ids) QVERIFY(science.selectControl(id, true));
        auto response = controlledSensitivityResponse(*capture, ids), result = response.value("result").toObject();
        auto derivative = result.value("derivatives").toObject();
        if (tamper == "column-order") derivative["control_ids"] = QJsonArray{ids[1], ids[0]};
        else if (tamper == "unknown-control") derivative["control_ids"] = QJsonArray{ids[0], "fabricated:control"};
        else if (tamper == "missing-tank") derivative["terminal_volume_jacobian_h"] = QJsonArray{QJsonArray{0.0, 0.0}};
        else if (tamper == "missing-column") derivative["terminal_volume_jacobian_h"] = QJsonArray{QJsonArray{0.0}, QJsonArray{0.0}};
        else if (tamper == "null-jacobian") derivative["terminal_volume_jacobian_h"] = QJsonArray{QJsonArray{0.0, QJsonValue::Null}, QJsonArray{0.0, 0.0}};
        else if (tamper == "string-gradient") derivative["cost_gradient"] = QJsonArray{0.0, "0"};
        else if (tamper == "extra-field") derivative["native_approval"] = true;
        result["derivatives"] = derivative;
        if (tamper == "missing") result.remove("derivatives");
        if (tamper == "foreign-identity") { auto identity = result.value("identity").toObject(); identity["volume_run_id"] = "foreign"; result["identity"] = identity; }
        response["result"] = result;
        QSignalSpy done(&science, &StorageScienceController::completed), failed(&science, &StorageScienceController::failed);
        science.runSensitivities(); QVERIFY(science.busy());
        // Deliberately bypass transport validation to exercise the controller's
        // independent publication checks; these are not scientific worker traces.
        worker.storageReady(response); worker.cancel();
        QTRY_COMPARE_WITH_TIMEOUT(failed.size(), 1, 10000); QCOMPARE(failed.first().first().toString(), "science_comparison_invalid");
        QCOMPARE(done.size(), 0); QVERIFY(science.result().isEmpty()); QCOMPARE(project.document(), document); QVERIFY(project.canApprove());
    }
    void realDesktopWorkerParity_data() {
        QTest::addColumn<QString>("example"); QTest::addColumn<double>("horizon"); QTest::addColumn<double>("step");
        QTest::newRow("mixed") << QString("mixed") << 3.0 << 0.025;
        QTest::newRow("overflowing-baseline") << QString("baseline") << 3.0 << 0.025;
        QTest::newRow("bad-downstream") << QString("bad") << 3.0 << 0.025;
        QTest::newRow("shortened-final-step") << QString("mixed") << 0.95 << 0.3;
        QTest::newRow("decimated-indices") << QString("mixed") << 3.0 << 0.002;
        QTest::newRow("one-ulp-endpoint") << QString("mixed") << 0.00023685942255445778 << 2.7423807173145508e-08;
        QTest::newRow("ratio-underflow-large-step") << QString("mixed") << std::numeric_limits<double>::denorm_min() << 1e308;
        for (const auto& item : storage_grid_test::cases())
            QTest::newRow(item.name) << QString("mixed") << item.horizon << item.step;
    }
    void realDesktopWorkerParity() {
        QFETCH(QString, example); QFETCH(double, horizon); QFETCH(double, step);
        ProjectController project; QVERIFY(forecast(project, example, {horizon, step}));
        const auto capture = project.capturedForecast(); const auto document = project.document(); const auto run = project.runId();
        QCOMPARE(capture->result.time_h.front(), 0.0); QCOMPARE(capture->result.time_h.back(), horizon);
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
        QCOMPARE(times.first().toDouble(), 0.0); QCOMPARE(times.last().toDouble(), horizon);
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
    void exactRequestedEndpointRemainsInfeasibleDespiteScientificAgreement() {
        const double flow = std::ldexp(1.0, 50), capacity = 0.9 * flow;
        const double horizon = std::nextafter(0.9, std::numeric_limits<double>::infinity());
        ProjectDocument input;
        input.network.assets = {{"tank", "Tank", AssetKind::tank, capacity, 0.0},
            {"source", "Source", AssetKind::source, 0.0, 0.0, flow}};
        input.network.routes = {{"feed", "source", "tank", flow, flow}};
        ProjectController project; QVERIFY(project.restore(project_json(input)));
        const auto document = project.document();
        project.forecastWithConfig({horizon, 0.1}); QTRY_VERIFY_WITH_TIMEOUT(!project.busy(), 10000);
        const auto captured = project.capturedForecast(); QVERIFY(captured);
        QCOMPARE(captured->result.time_h.size(), std::size_t{11}); QCOMPARE(captured->result.time_h.back(), horizon);
        QCOMPARE(captured->result.volumes_m3[0].back(), horizon * flow);
        QCOMPARE(captured->result.volumes_m3[0].back() - capacity, 0.125);
        QVERIFY(!captured->validation.feasible); QVERIFY(!project.canApprove());
        PythonWorkerClient worker(python(), workerPath()); StorageScienceController science(&project, &worker);
        QSignalSpy done(&science, &StorageScienceController::completed), failed(&science, &StorageScienceController::failed);
        science.run(); QTRY_COMPARE_WITH_TIMEOUT(done.size(), 1, 70000);
        QVERIFY(failed.isEmpty()); const auto result = QJsonObject::fromVariantMap(science.result());
        QVERIFY(result.value("within_tolerance").toBool()); QVERIFY(!result.value("native_volume_feasible").toBool());
        const auto preview = result.value("preview").toObject();
        QCOMPARE(preview.value("time_h").toArray().last().toDouble(), horizon);
        const auto nativeValues = preview.value("native_volumes_m3").toArray().first().toArray();
        const auto workerValues = preview.value("worker_volumes_m3").toArray().first().toArray();
        QCOMPARE(nativeValues.last().toDouble(), horizon * flow); QCOMPARE(workerValues.last().toDouble(), horizon * flow);
        QCOMPARE(workerValues.last().toDouble() - capacity, 0.125);
        QCOMPARE(project.document(), document); QCOMPARE(project.capturedForecast(), captured); QVERIFY(!project.canApprove());
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
    void editsDeletionAndReplacementRetireAnExchangedResult_data() {
        QTest::addColumn<QString>("change");
        for (const auto* name : {"edit", "delete", "replacement"}) QTest::newRow(name) << QString::fromLatin1(name);
    }
    void editsDeletionAndReplacementRetireAnExchangedResult() {
        QFETCH(QString, change);
        Environment modeRestore("BREAKWATER_STORAGE_FIXTURE_MODE"), phaseRestore("BREAKWATER_STORAGE_FIXTURE_PHASEFILE");
        QTemporaryDir directory; QVERIFY(directory.isValid());
        qputenv("BREAKWATER_STORAGE_FIXTURE_MODE", "hang_after_exchange");
        qputenv("BREAKWATER_STORAGE_FIXTURE_PHASEFILE", directory.filePath("exchanged.txt").toUtf8());
        ProjectController project; QVERIFY(forecast(project));
        const auto captured = project.capturedForecast();
        const auto network = network_json(captured->network);
        const auto times = captured->result.time_h; const auto volumes = captured->result.volumes_m3;
        PythonWorkerClient worker(python(), QStringLiteral(BREAKWATER_SOURCE_DIR "/desktop/tests/worker_fixtures/storage_worker_fixture.py"));
        worker.setTerminationGraceMs(25); StorageScienceController science(&project, &worker);
        QSignalSpy done(&science, &StorageScienceController::completed), ready(&worker, &PythonWorkerClient::storageReady);
        science.run(); QTRY_VERIFY_WITH_TIMEOUT(QFileInfo(directory.filePath("exchanged.txt")).isFile(), 10000);
        // All three valid frames have been exchanged, but clean exit has not
        // occurred. A numerical mutation must prevent their later publication.
        QVERIFY(science.busy()); QCOMPARE(ready.size(), 0); QCOMPARE(done.size(), 0);
        if (change == "edit") { project.select("bypass"); project.editSelected("flow", "19"); }
        else if (change == "delete") { project.select("tank_b"); project.deleteSelected(); }
        else project.forecast();
        QTRY_VERIFY_WITH_TIMEOUT(!worker.busy() && !project.busy(), 10000);
        QVERIFY(!science.busy()); QVERIFY(science.result().isEmpty());
        QVERIFY(project.capturedForecast() != captured);
        if (change != "replacement") QVERIFY(!project.canApprove());
        QCOMPARE(network_json(captured->network), network);
        QCOMPARE(captured->result.time_h, times); QCOMPARE(captured->result.volumes_m3, volumes);
        QTest::qWait(100); QCOMPARE(ready.size(), 0); QCOMPARE(done.size(), 0);
    }
};
QTEST_GUILESS_MAIN(StorageScienceTests)
#include "storage_science_tests.moc"
