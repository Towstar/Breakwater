#include "history/HistoryController.hpp"
#include "agent/AgentController.hpp"
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
};
QTEST_GUILESS_MAIN(HistoryTests)
#include "history_controller_tests.moc"
