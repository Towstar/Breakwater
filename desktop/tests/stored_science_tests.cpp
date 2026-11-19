#include "science/StoredCandidateScience.hpp"
#include "science/StorageScienceController.hpp"
#include "agent/SimulationGateway.hpp"
#include <QDateTime>
#include <QFileInfo>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QUuid>
#include <QtTest>
#include <cmath>
#include <stdexcept>
using namespace breakwater;
using namespace breakwater::desktop;
using namespace breakwater::desktop::science;
using namespace breakwater::desktop::history;
using namespace breakwater::desktop::agent;
namespace {
struct Fixture {
    ProjectDocument project;
    QJsonObject archive;
    QString baseline,candidate;
    Fixture() {
        project.network={{{"tank","Tank",AssetKind::tank,200,80},{"well","Well",AssetKind::source,0,0,36,0,36,7},
            {"outlet","Outlet",AssetKind::treatment,100}},{{"feed","well","tank",100,36,false,true},{"pump","tank","outlet",100,12,true,true,2}}};
        SimulationGateway gateway(project.network,physics_hash(project));
        gateway.execute("get_network_state",{}); gateway.execute("get_control_catalog",{});
        baseline=gateway.execute("forecast_baseline",{{"snapshot_id",gateway.snapshotId()}}).value("run").toObject().value("run_id").toString();
        candidate=gateway.execute("simulate_candidates",{{"snapshot_id",gateway.snapshotId()},{"candidates",QJsonArray{QJsonObject{
            {"candidate_id","stored-response"},{"controls",QJsonArray{QJsonObject{{"control_id","source:well:curtail"},{"value",8}},
                QJsonObject{{"control_id","route:pump:flow"},{"value",20}}}}}}}}).value("runs").toArray().first().toObject().value("run_id").toString();
        gateway.execute("validate_run",{{"snapshot_id",gateway.snapshotId()},{"run_id",candidate}});
        auto recommendation=gateway.recommendation(candidate); recommendation["actionable"]=false;
        const auto now=QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs);
        archive={{"archive_version",1},{"session_id",QUuid::createUuid().toString(QUuid::WithoutBraces)},{"checkpoint_sequence",1},
            {"created_at",now},{"updated_at",now},{"status","completed"},{"objective","Stored candidate science fixture"},
            {"snapshot",gateway.snapshot()},{"model",QJsonObject{}},{"runs",QJsonArray{gateway.runDetails(baseline),gateway.runDetails(candidate)}},
            {"events",QJsonArray{}},{"recommendation",recommendation},{"approvals",QJsonArray{}},{"current",true}};
    }
};
}
class StoredScienceTests final:public QObject {
    Q_OBJECT
private slots:
    void selectedRunUsesActualStoredOverridesAndOriginalTrajectory() {
        Fixture f; const auto original=project_json(f.project); const auto source=capture_stored_science(f.archive,f.candidate);
        QVERIFY(source.capture); QCOMPARE(source.capture->network.routes[1].setpoint_m3h,20.0); QCOMPARE(source.capture->network.assets[1].curtailment_m3h,8.0);
        QVERIFY(std::abs(source.capture->result.volumes_m3[0].back()-104.0)<1e-9);
        QCOMPARE(source.origin.value("source_run_id"),QJsonValue(f.candidate)); QCOMPARE(source.origin.value("source_physical_hash"),QJsonValue(physics_hash(f.project)));
        QVERIFY(source.origin.value("effective_physical_hash")!=source.origin.value("source_physical_hash"));
        QCOMPARE(project_json(f.project),original); QCOMPARE(source.capture->revision,quint64(0));
        const auto baseline=capture_stored_science(f.archive,f.baseline); QCOMPARE(baseline.capture->network.routes[1].setpoint_m3h,12.0);
    }
    void inconsistentStoredNumericsAndUnknownRunsCannotBecomeScienceInputs() {
        Fixture f; auto archive=f.archive; auto runs=archive.value("runs").toArray(); auto details=runs[1].toObject();
        auto volumes=details.value("volumes_m3").toArray(); auto values=volumes[0].toArray(); values[1]=values[1].toDouble()+1;
        volumes[0]=values; details["volumes_m3"]=volumes; runs[1]=details; archive["runs"]=runs;
        QVERIFY(SessionRepository::validateArchive(archive));
        QVERIFY_THROWS_EXCEPTION(std::invalid_argument,capture_stored_science(archive,f.candidate));
        QVERIFY_THROWS_EXCEPTION(std::invalid_argument,capture_stored_science(f.archive,"unknown"));
    }
    void selectionReplacementAndReconstructionCancellationCannotPublishOldSource() {
        Fixture f; ProjectController project; QVERIFY(project.restore(project_json(f.project)));
        HistoryController history(nullptr); history.setLiveSession(f.archive);
        PythonWorkerClient worker; StorageScienceController science(&project,&worker); science.setHistoryController(&history);
        science.setSourceMode("candidate"); history.selectRuns(f.baseline,f.baseline); history.selectRuns(f.baseline,f.candidate);
        QTRY_VERIFY_WITH_TIMEOUT(!science.busy(),15000); QCOMPARE(science.sourceIdentity().value("source_run_id").toString(),f.candidate);
        QVERIFY(science.canRun()); QVERIFY(project.capturedRun().isEmpty()); QVERIFY(!project.canApprove());
        science.setSourceMode("forecast"); QVERIFY(!science.canRun());
        science.setSourceMode("candidate"); science.cancel(); QTRY_VERIFY_WITH_TIMEOUT(!science.busy(),15000);
        QVERIFY(science.sourceIdentity().isEmpty()); QVERIFY(!science.canRun());
    }
    void realCpuWorkerAndHistoryUseTheHistoricalCandidateWithoutApplyingIt() {
        if (!QFileInfo::exists(QStringLiteral(BREAKWATER_TEST_PYTHON))) QSKIP("Configured CPU Python is unavailable.");
        Fixture f; QTemporaryDir directory; SessionRepository repository(directory.filePath("history.sqlite"));
        QSignalSpy ready(&repository,&SessionRepository::ready),saved(&repository,&SessionRepository::sessionSaved),inspected(&repository,&SessionRepository::sessionInspected);
        QTRY_COMPARE(ready.size(),1); repository.saveSession(f.archive); QTRY_COMPARE(saved.size(),1);
        HistoryController history(&repository); history.loadSession(f.archive.value("session_id").toString());
        QTRY_VERIFY(history.historical() && !history.busy()); history.selectRuns(f.baseline,f.candidate);
        ProjectController project; QVERIFY(project.restore(project_json(f.project))); const auto original=project.document();
        PythonWorkerClient worker(QStringLiteral(BREAKWATER_TEST_PYTHON),QStringLiteral(BREAKWATER_SOURCE_DIR "/science/worker.py"));
        StorageScienceController science(&project,&worker); science.setHistoryRepository(&repository); science.setHistoryController(&history); science.setSourceMode("candidate");
        QTRY_VERIFY_WITH_TIMEOUT(science.canRun(),20000); QCOMPARE(science.sourceIdentity().value("source_mode").toString(),QString("historical_candidate"));
        QSignalSpy completed(&science,&StorageScienceController::completed),failed(&science,&StorageScienceController::failed);
        science.run(); QTRY_COMPARE_WITH_TIMEOUT(completed.size(),1,60000); QVERIFY2(failed.isEmpty(),qPrintable(science.status()));
        QVERIFY(science.result().value("within_tolerance").toBool()); QCOMPARE(science.result().value("run_id").toString(),f.candidate);
        QTRY_VERIFY_WITH_TIMEOUT(!science.historyPending() && !science.historySavedResultId().isEmpty(),15000);
        repository.inspectSession(science.historySavedResultId()); QTRY_COMPARE(inspected.size(),1);
        const auto archive=inspected.first().first().value<QJsonObject>(); QString problem;
        QVERIFY2(SessionRepository::validateArchive(archive,&problem),qPrintable(problem)); QCOMPARE(archive.value("archive_version"),QJsonValue(2));
        QCOMPARE(archive.value("evidence_envelopes").toArray().first().toObject().value("kind"),QJsonValue("manual_science_origin"));
        QVERIFY(!archive.contains("agent_origin"));
        auto altered=archive; auto envelopes=altered.value("evidence_envelopes").toArray(); auto item=envelopes.first().toObject();
        auto origin=item.value("payload").toObject(); origin["sample_step_h"]=0.123; item["payload"]=origin;
        envelopes[0]=item; altered["evidence_envelopes"]=envelopes; QVERIFY(!SessionRepository::validateArchive(altered));
        QVERIFY(science.selectControl("source:well:curtail",true)); science.runSensitivities();
        QTRY_COMPARE_WITH_TIMEOUT(completed.size(),2,60000); QVERIFY(science.result().value("within_tolerance").toBool());
        QCOMPARE(science.result().value("analysis_mode").toString(),QString("sensitivities"));
        QCOMPARE(project.document(),original); QVERIFY(project.capturedRun().isEmpty()); QVERIFY(!project.canApprove());
    }
};
QTEST_GUILESS_MAIN(StoredScienceTests)
#include "stored_science_tests.moc"
