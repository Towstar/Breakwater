#include "history/HistoryController.hpp"
#include "agent/AgentController.hpp"
#include "science/StorageScienceController.hpp"
#include "science/StorageScienceEvidence.hpp"
#include <QDateTime>
#include <QFile>
#include <QDir>
#include <QJsonDocument>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QSaveFile>
#include <QUuid>
#include <QtTest>

using namespace breakwater;
using namespace breakwater::desktop;
using namespace breakwater::desktop::agent;
using namespace breakwater::desktop::history;
namespace {
QJsonObject tool(const QString& id, const QString& name, const QJsonObject& args = {}) {
    return {{"id", id}, {"type", "function"}, {"function", QJsonObject{{"name", name}, {"arguments", QString::fromUtf8(QJsonDocument(args).toJson(QJsonDocument::Compact))}}}};
}
QJsonObject toolResponse(const QJsonArray& history, const QString& id) {
    for (const auto& value : history) {
        const auto object = value.toObject();
        if (object.value("role") == "tool" && object.value("tool_call_id") == id) return QJsonDocument::fromJson(object.value("content").toString().toUtf8()).object();
    }
    return {};
}
class EvidenceModel final : public ModelTransport {
public:
    int step = 0, epoch = 0;
    QString snapshot, run;
    void cancel() override { ++epoch; }
    void request(const QJsonArray& history, const QJsonArray&) override {
        emit requestStarted();
        QJsonArray calls;
        switch (step++) {
        case 0: calls = {tool("state", "get_network_state"), tool("catalog", "get_control_catalog")}; break;
        case 1: {
            snapshot = toolResponse(history, "state").value("snapshot_id").toString();
            QJsonArray controls;
            for (const auto& value : incident_controls("mixed")) controls.append(QJsonObject{{"control_id", QString::fromStdString(value.id)}, {"value", value.value}});
            calls = {tool("baseline", "forecast_baseline", {{"snapshot_id", snapshot}}), tool("candidate", "simulate_candidates", {{"snapshot_id", snapshot}, {"candidates", QJsonArray{QJsonObject{{"candidate_id", "mixed"}, {"controls", controls}}}}})};
            break;
        }
        case 2:
            run = toolResponse(history, "candidate").value("runs").toArray().first().toObject().value("run_id").toString();
            calls = {tool("validate", "validate_run", {{"snapshot_id", snapshot}, {"run_id", run}}), tool("stage", "request_approval", {{"snapshot_id", snapshot}, {"run_id", run}})};
            break;
        default: break;
        }
        QJsonObject message{{"role", "assistant"}, {"content", QJsonValue::Null}};
        if (!calls.isEmpty()) message["tool_calls"] = calls;
        else message["content"] = QString::fromUtf8(QJsonDocument(QJsonObject{{"selected_run_id", run}, {"explanation", "Use the independently validated stored mixed plan."}}).toJson(QJsonDocument::Compact));
        const auto current = epoch;
        QTimer::singleShot(0, this, [this, current, message] { if (current == epoch) emit replyReady(message); });
    }
};
QJsonObject actualArchive() {
    auto project = example_project(); project.network = main_pump_failure(project.network);
    SimulationGateway gateway(project.network, physics_hash(project));
    gateway.execute("get_network_state", {}); gateway.execute("get_control_catalog", {});
    const auto baseline = gateway.execute("forecast_baseline", {{"snapshot_id", gateway.snapshotId()}}).value("run").toObject();
    QJsonArray controls;
    for (const auto& value : incident_controls("mixed")) controls.append(QJsonObject{{"control_id", QString::fromStdString(value.id)}, {"value", value.value}});
    const auto candidate = gateway.execute("simulate_candidates", {{"snapshot_id", gateway.snapshotId()}, {"candidates", QJsonArray{QJsonObject{{"candidate_id", "mixed"}, {"controls", controls}}}}}).value("runs").toArray().first().toObject();
    const auto id = candidate.value("run_id").toString();
    gateway.execute("validate_run", {{"snapshot_id", gateway.snapshotId()}, {"run_id", id}});
    const auto stamp = QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs);
    return {{"archive_version", 1}, {"session_id", QUuid::createUuid().toString(QUuid::WithoutBraces)}, {"checkpoint_sequence", 1},
        {"created_at", stamp}, {"updated_at", stamp}, {"status", "completed"}, {"objective", "Compare actual forecasts."},
        {"snapshot", gateway.snapshot()}, {"model", QJsonObject{}}, {"events", QJsonArray{}}, {"approvals", QJsonArray{}},
        {"runs", QJsonArray{gateway.runDetails(baseline.value("run_id").toString()), gateway.runDetails(id)}}, {"recommendation", gateway.recommendation(id)}};
}
// A display/archive fixture built from an actual native capture. The copied
// worker endpoints and declared derivatives are synthetic test data, not a worker run.
QJsonObject scientificFixture(const CapturedForecast& capture, bool sensitivities = false) {
    QJsonArray descriptors, errors, values;
    for (std::size_t i = 0; i < capture.result.tank_ids.size(); ++i) {
        const auto& id = capture.result.tank_ids[i];
        for (const auto& asset : capture.network.assets) if (asset.id == id)
            descriptors.append(QJsonObject{{"id", QString::fromStdString(id)}, {"label", QString::fromStdString(asset.label)}, {"capacity_m3", asset.capacity_m3}});
        errors.append(QJsonObject{{"tank_id", QString::fromStdString(id)}, {"max_absolute_error_m3", 0.0}, {"max_scaled_error", 0.0}, {"within_tolerance", true}});
        values.append(QJsonArray{capture.result.volumes_m3[i].front(), capture.result.volumes_m3[i].back()});
    }
    QJsonObject result{{"result_version", 1}, {"result_id", QUuid::createUuid().toString(QUuid::WithoutBraces)},
        {"created_at", QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs)}, {"method", "storage_rollout"},
        {"run_id", capture.id}, {"snapshot_id", capture.id}, {"physical_hash", capture.physical_hash},
        {"numerical_revision", static_cast<qint64>(capture.revision)}, {"horizon_h", capture.config.horizon_h}, {"step_h", capture.config.step_h},
        {"analysis_mode", sensitivities ? "sensitivities" : "forward"}, {"selected_controls", QJsonArray{}},
        {"native_solver_version", QString::fromLatin1(solver_version)}, {"native_cost", capture.result.cost}, {"worker_cost", capture.result.cost},
        {"cost_difference", 0.0}, {"cost_absolute_error", 0.0}, {"cost_scaled_error", 0.0}, {"within_tolerance", true},
        {"comparison_scope", "Returned worker preview samples versus their original native indices; not a feasibility verdict"},
        {"absolute_tolerance", 1e-9}, {"relative_tolerance", 1e-10}, {"native_volume_feasible", capture.validation.feasible},
        {"tanks", descriptors}, {"tank_errors", errors},
        {"provenance", QJsonObject{{"implementation", "torch_storage_float64"}, {"implementation_version", "history-test-fixture"},
            {"dtype", "float64"}, {"device", "cpu"}, {"execution_status", "completed"}}},
        {"warnings", QJsonArray{"Synthetic history test fixture; no worker execution."}},
        {"preview", QJsonObject{{"time_h", QJsonArray{capture.result.time_h.front(), capture.result.time_h.back()}},
            {"time_indices", QJsonArray{0, static_cast<qint64>(capture.result.time_h.size() - 1)}},
            {"native_volumes_m3", values}, {"worker_volumes_m3", values}, {"source_frames", static_cast<qint64>(capture.result.time_h.size())}}}};
    if (sensitivities) {
        result["selected_controls"] = QJsonArray{QJsonObject{{"control_id", "source:source_a:curtail"}, {"captured_value", 10.0}, {"unit", "m3/h"}},
            QJsonObject{{"control_id", "route:bypass:flow"}, {"captured_value", 20.0}, {"unit", "m3/h"}}};
        result["derivatives"] = QJsonObject{{"control_ids", QJsonArray{"source:source_a:curtail", "route:bypass:flow"}},
            {"terminal_volume_jacobian_h", QJsonArray{QJsonArray{-3.0, -3.0}, QJsonArray{0.0, 3.0}}}, {"cost_gradient", QJsonArray{750.0, 0.0}}};
    }
    return result;
}
}

class HistoryTests final : public QObject {
    Q_OBJECT
private slots:
    void chartsAndComparisonUseActualUnclippedEvidence() {
        const auto archive = actualArchive();
        HistoryController history(nullptr); history.setLiveSession(archive);
        QCOMPARE(history.runs().size(), 2); QVERIFY(history.comparison().value("compatible").toBool());
        QCOMPARE(history.comparison().value("candidate").toMap().value("cost").toDouble(), 12780.0);
        QVERIFY(!history.comparison().value("control_changes").toList().isEmpty());
        history.selectTank("tank_a"); const auto chart = history.chart();
        QVERIFY(chart.value("available").toBool()); QCOMPARE(chart.value("series").toList().size(), 2);
        QVERIFY(chart.value("y_max").toDouble() > 236.0);
        const auto baseline = chart.value("series").toList().first().toMap().value("points").toList();
        QVERIFY(std::abs(baseline.last().toMap().value("volume_m3").toDouble() - 236.0) < 1e-8);
        QCOMPARE(chart.value("capacity_m3").toDouble(), 120.0);
        const auto original = history.selectedArchive(); history.selectTank("tank_b"); history.selectRuns(history.candidateRunId(), history.baselineRunId());
        QCOMPARE(history.selectedArchive(), original);
        const auto artifactDirectory = qEnvironmentVariable("BREAKWATER_TEST_ARTIFACT_DIR");
        if (!artifactDirectory.isEmpty()) {
            QVERIFY(QDir().mkpath(artifactDirectory));
            QSaveFile artifact(QDir(artifactDirectory).filePath("offline-verified-session.json"));
            QVERIFY(artifact.open(QIODevice::WriteOnly));
            auto evidence = archive; evidence["verification_note"] = "Offline native verification. No live NVIDIA model execution occurred.";
            const auto bytes = QJsonDocument(evidence).toJson(QJsonDocument::Indented);
            QCOMPARE(artifact.write(bytes), bytes.size()); QVERIFY(artifact.commit());
        }
    }
    void incompatibleRunsRemainIndividuallyViewable() {
        auto archive = actualArchive(); auto runs = archive.value("runs").toArray();
        auto detail = runs.last().toObject(), summary = detail.value("run").toObject(); summary["snapshot_id"] = "different"; detail["run"] = summary; runs[1] = detail; archive["runs"] = runs;
        HistoryController history(nullptr); history.setLiveSession(archive);
        QVERIFY(!history.comparison().value("compatible").toBool());
        QVERIFY(history.chart().value("available").toBool()); QCOMPARE(history.chart().value("series").toList().size(), 1);
        QVERIFY(!history.comparison().contains("cost_delta"));
    }
    void automaticHistoryRestartAndHistoricalApprovalRejection() {
        QTemporaryDir directory; QVERIFY(directory.isValid());
        QString sessionId; QJsonObject retained;
        {
            SessionRepository repository(directory.filePath("history.sqlite"));
            QSignalSpy ready(&repository, &SessionRepository::ready); QTRY_COMPARE(ready.size(), 1);
            HistoryController history(&repository);
            ProjectController project; project.loadExample("baseline");
            EvidenceModel model; AgentController agent(&project, &model); agent.setHistory(&repository, &history);
            agent.investigate("Keep production running for three hours.");
            QTRY_VERIFY_WITH_TIMEOUT(!agent.running(), 10000);
            QTRY_VERIFY_WITH_TIMEOUT(agent.canApprove(), 10000);
            QCOMPARE(agent.review().value("cost").toDouble(), 12780.0);
            QVERIFY(!agent.review().value("controls").toList().isEmpty());
            const auto selected = history.candidateRunId(), baseline = history.baselineRunId(); sessionId = history.selectedSessionId();
            history.selectRuns(baseline, baseline); QVERIFY(!agent.canApprove()); QVERIFY(!agent.approvePlan());
            history.selectRuns(baseline, selected); QVERIFY(agent.canApprove());
            project.moveAsset("tank_a", 180, 100); QVERIFY(agent.canApprove());
            QVERIFY(agent.approvePlan()); QVERIFY(!agent.approvePlan());
            QTRY_VERIFY_WITH_TIMEOUT(history.selectedArchive().value("approvals").toArray().size() == 1, 10000);
            retained = history.selectedArchive();
            QSignalSpy loaded(&repository, &SessionRepository::sessionLoaded);
            QTRY_VERIFY_WITH_TIMEOUT(!history.sessions().isEmpty(), 10000);
            history.loadSession(sessionId); QTRY_COMPARE(loaded.size(), 1);
            QVERIFY(history.historical()); QVERIFY(!agent.canApprove()); QVERIFY(!agent.approvePlan());
            QCOMPARE(history.selectedArchive().value("recommendation").toObject().value("approval_status").toString(), "historical");
            QTRY_VERIFY_WITH_TIMEOUT(!project.busy(), 10000);
        }
        SessionRepository reopened(directory.filePath("history.sqlite"));
        QSignalSpy ready(&reopened, &SessionRepository::ready); QTRY_COMPARE(ready.size(), 1);
        QSignalSpy loaded(&reopened, &SessionRepository::sessionLoaded); reopened.loadSession(sessionId); QTRY_COMPARE(loaded.size(), 1);
        const auto restored = loaded.first().first().toJsonObject();
        QVERIFY(restored.value("historical").toBool()); QCOMPARE(restored.value("runs"), retained.value("runs"));
        QCOMPARE(restored.value("approvals").toArray().size(), 1);
    }
    void storageFailureDoesNotDisableNativeForecasts() {
        QTemporaryDir directory; QFile file(directory.filePath("not-a-directory")); QVERIFY(file.open(QIODevice::WriteOnly)); file.write("x"); file.close();
        SessionRepository repository(file.fileName() + "/history.sqlite");
        HistoryController history(&repository); ProjectController project; EvidenceModel model; AgentController agent(&project, &model);
        agent.setHistory(&repository, &history);
        QTRY_VERIFY(agent.storageStatus().contains("error", Qt::CaseInsensitive));
        project.loadExample("mixed"); project.forecast(); QTRY_VERIFY_WITH_TIMEOUT(!project.busy(), 10000);
        QVERIFY(project.summary().contains("PASS"));
    }
    void scientificSelectionIsReadOnlyAndLegacyArchivesClearIt() {
        ProjectController project; project.loadExample("mixed"); project.forecast();
        QTRY_VERIFY_WITH_TIMEOUT(!project.busy(), 10000); const auto capture = project.capturedForecast(); QVERIFY(capture);
        const auto forward = scientificFixture(*capture), sensitivities = scientificFixture(*capture, true);
        auto archive = science::storage_science_archive(*capture, forward);
        archive["scientific_evidence"] = QJsonArray{forward, sensitivities};
        HistoryController history(nullptr); history.setLiveSession(archive);
        science::PythonWorkerClient worker; science::StorageScienceController scienceController(&project, &worker);
        QVERIFY(scienceController.selectControl("source:source_a:curtail", true));
        const auto original = history.selectedArchive(), document = project.document();
        const auto selected = scienceController.selectedControlIds();
        const bool canApprove = project.canApprove();
        QCOMPARE(history.scientificEvidence().size(), 2); QCOMPARE(history.scientificResult(), forward.toVariantMap());
        QVERIFY(history.scientificSensitivityRows().isEmpty());
        history.selectScientificResult(sensitivities.value("result_id").toString());
        QCOMPARE(history.scientificSensitivityRows().size(), 2);
        QCOMPARE(history.scientificSensitivityRows().first().toMap().value("terminal_volume_jacobian_h").toDouble(), -3.0);
        history.selectScientificTank("tank_b");
        QCOMPARE(history.scientificSensitivityRows().first().toMap().value("terminal_volume_jacobian_h").toDouble(), 0.0);
        QCOMPARE(history.scientificSensitivityRows().last().toMap().value("terminal_volume_jacobian_h").toDouble(), 3.0);
        const auto id = history.selectedScientificResultId(); history.selectScientificResult("unknown"); history.selectScientificTank("unknown");
        QCOMPARE(history.selectedScientificResultId(), id); QCOMPARE(history.selectedScientificTankId(), "tank_b");
        QCOMPARE(history.selectedArchive(), original); QCOMPARE(project.document(), document); QCOMPARE(project.capturedForecast(), capture);
        QCOMPARE(scienceController.selectedControlIds(), selected); QCOMPARE(project.canApprove(), canApprove); QVERIFY(scienceController.result().isEmpty());
        history.setLiveSession(actualArchive());
        QVERIFY(history.scientificEvidence().isEmpty()); QVERIFY(history.scientificResult().isEmpty());
        QVERIFY(history.selectedScientificResultId().isEmpty()); QVERIFY(history.selectedScientificTankId().isEmpty());
        QVERIFY(!history.scientificChart().value("available").toBool());
    }
    void scientificChartsPreserveOverflowingPredictions() {
        ProjectController project; project.loadExample("baseline"); project.forecast();
        QTRY_VERIFY_WITH_TIMEOUT(!project.busy(), 10000); const auto capture = project.capturedForecast(); QVERIFY(capture);
        HistoryController history(nullptr); history.setLiveSession(science::storage_science_archive(*capture, scientificFixture(*capture)));
        history.selectScientificTank("tank_a");
        const auto chart = history.scientificChart(); QVERIFY(chart.value("available").toBool());
        QCOMPARE(chart.value("capacity_m3").toDouble(), 120.0);
        const auto nativeTerminal = capture->result.volumes_m3.front().back();
        QVERIFY(nativeTerminal > 120.0); QVERIFY(chart.value("y_max").toDouble() >= nativeTerminal);
        const auto points = chart.value("series").toList().last().toMap().value("points").toList();
        QVERIFY(std::abs(points.last().toMap().value("volume_m3").toDouble() - 236.0) < 1e-8);
    }
    void scientificRestartReopenAndExportPreserveLiveCapture() {
        QTemporaryDir directory; QVERIFY(directory.isValid());
        ProjectController project; project.loadExample("mixed"); project.forecast();
        QTRY_VERIFY_WITH_TIMEOUT(!project.busy(), 10000); const auto capture = project.capturedForecast(); QVERIFY(capture);
        const auto result = scientificFixture(*capture, true), archive = science::storage_science_archive(*capture, result);
        QVERIFY(SessionRepository::validateArchive(archive));
        const auto path = directory.filePath("history.sqlite");
        { SessionRepository repository(path); repository.saveSession(archive); }
        SessionRepository reopened(path); HistoryController history(&reopened);
        QSignalSpy ready(&reopened, &SessionRepository::ready), loaded(&reopened, &SessionRepository::sessionLoaded), failed(&reopened, &SessionRepository::failed);
        QTRY_COMPARE(ready.size(), 1);
        const auto live = actualArchive(); history.setLiveSession(live);
        EvidenceModel model; AgentController agent(&project, &model); agent.setHistory(&reopened, &history);
        const auto document = project.document(); history.loadSession(archive.value("session_id").toString());
        QTRY_COMPARE(loaded.size(), 1); QVERIFY(history.historical()); QCOMPARE(history.scientificResult(), result.toVariantMap());
        QCOMPARE(history.scientificSensitivityRows().size(), 2); history.selectScientificTank("tank_b");
        QCOMPARE(history.scientificChart().value("series").toList().size(), 2); QVERIFY(!agent.canApprove()); QVERIFY(!agent.approvePlan());
        const auto exportPath = directory.filePath("scientific.json");
        QSignalSpy completed(&reopened, &SessionRepository::operationCompleted); history.exportSession(QUrl::fromLocalFile(exportPath));
        QTRY_VERIFY(completed.size() >= 1); QFile file(exportPath); QVERIFY(file.open(QIODevice::ReadOnly));
        const auto exported = QJsonDocument::fromJson(file.readAll()).object();
        QCOMPARE(exported.value("scientific_evidence"), archive.value("scientific_evidence"));
        history.showLive(); QVERIFY(!history.historical()); QCOMPARE(history.selectedSessionId(), live.value("session_id").toString());
        QVERIFY(history.scientificResult().isEmpty()); QCOMPARE(project.document(), document); QCOMPARE(project.capturedForecast(), capture);
        QVERIFY(failed.isEmpty());
    }
};
QTEST_GUILESS_MAIN(HistoryTests)
#include "history_controller_tests.moc"
