#include "ObservedFixture.hpp"
#include "agent/ObservedInvestigationController.hpp"
#include "history/SessionRecorder.hpp"
#include "history/EvidenceEnvelope.hpp"
#include "telemetry/ObservationMonitor.hpp"
#include <QSignalSpy>
#include <QFileInfo>
#include <QTemporaryDir>
#include <QtTest>
#include <functional>
#include <atomic>
using namespace breakwater;
using namespace breakwater::desktop;
using namespace breakwater::desktop::agent;
using namespace breakwater::desktop::history;
using namespace breakwater::desktop::test_observation;
namespace {
QJsonObject tool(const QString& id,const QString& name,const QJsonObject& args={}) {
    return {{"id",id},{"type","function"},{"function",QJsonObject{{"name",name},{"arguments",QString::fromUtf8(QJsonDocument(args).toJson(QJsonDocument::Compact))}}}};
}
QJsonObject calls(const QJsonArray& c) { return {{"tool_calls",c},{"content",QJsonValue::Null}}; }
QJsonObject final(const QJsonValue& id=QJsonValue::Null) {
    return {{"content",QString::fromUtf8(QJsonDocument(QJsonObject{{"selected_run_id",id},{"explanation","Nominal advisory evidence only; no controls applied."}}).toJson(QJsonDocument::Compact))}};
}
QJsonObject result(const QJsonArray& messages,const QString& id) {
    for (const auto& row:messages) { const auto m=row.toObject(); if (m.value("role")=="tool" && m.value("tool_call_id")==id) return QJsonDocument::fromJson(m.value("content").toString().toUtf8()).object(); }
    return {};
}
class Model final:public ModelTransport {
public:
    std::function<QJsonObject(const QJsonArray&)> respond;
    QJsonArray history; int requests=0,generation=0;
    void request(const QJsonArray& messages,const QJsonArray&) override {
        history=messages; ++requests; if (!respond) return;
        const auto reply=respond(messages); const auto epoch=generation;
        QTimer::singleShot(0,this,[this,reply,epoch] { if (epoch==generation) emit replyReady(reply); });
    }
    void cancel() override { ++generation; }
};
QJsonArray commandsFor(const ProjectController& project) {
    QJsonArray commands; const auto parsed=parse_project(project.document());
    for (const auto& c:control_catalog(parsed.network)) commands.append(QJsonObject{{"control_id",QString::fromStdString(c.id)},{"value",c.value},{"unit",QString::fromStdString(c.unit)}});
    return commands;
}
}
class ObservedForecastTests final:public QObject {
    Q_OBJECT
private slots:
    void frozenMeasurementsInitializeRenamedChangedTopologyWithoutEditingDesign() {
        try {
        Fixture f(true); const auto original=project_json(f.project);
        auto row=f.readings[0].toObject(); row["value"]=91.25; f.readings[0]=row; f.begin();
        const auto capture=f.stream.captureObserved(f.context,f.commands,7,"renamed-capture");
        const auto g=SimulationGateway::fromObservation(capture,0.1); QVERIFY(g->advisoryOnly()); QVERIFY(g->observationsAvailable());
        const auto observations=g->execute("get_observations",{}); QCOMPARE(observations.value("observed_snapshot").toObject(),capture.document);
        QCOMPARE(g->execute("get_detection_events",{}).value("detector").toObject().value("balance_status"),QJsonValue("not_captured"));
        const auto run=g->execute("forecast_baseline",{{"snapshot_id",g->snapshotId()}}).value("run").toObject();
        QVERIFY(!run.isEmpty()); QCOMPARE(run.value("tanks").toArray().first().toObject().value("initial_m3"),QJsonValue(91.25));
        QVERIFY(g->physicalHash()!=physics_hash(f.project)); QCOMPARE(project_json(f.project),original);
        const auto network=g->execute("get_network_state",{}).value("network").toObject();
        QCOMPARE(network.value("routes").toArray()[0].toObject().value("target_id"),QJsonValue("custom_tank_b"));
        } catch (const std::exception& e) { QFAIL(e.what()); }
    }
    void failedPumpCannotBeReenabledByAnObservedCandidate() {
        Fixture f;
        for (qsizetype i=0;i<f.readings.size();++i) { auto r=f.readings[i].toObject();
            if (r.value("sensor_id")=="pump_state-backup_pump") r["value"]="delivery_loss";
            if (r.value("sensor_id")=="route_flow-backup_pump") r["value"]=0;
            f.readings[i]=r;
        }
        f.begin(); const auto capture=f.stream.captureObserved(f.context,f.commands,0,"pump-capture");
        const auto g=SimulationGateway::fromObservation(capture); QVERIFY(g->observationsAvailable());
        const auto state=g->execute("get_network_state",{}).value("network").toObject();
        bool found=false; for (const auto& v:state.value("routes").toArray()) if (v.toObject().value("id")=="backup_pump") {
            found=true; QCOMPARE(v.toObject().value("available"),QJsonValue(false)); QCOMPARE(v.toObject().value("setpoint_m3h"),QJsonValue(0));
        }
        QVERIFY(found);
        const auto rejected=g->execute("simulate_candidates",{{"snapshot_id",g->snapshotId()},{"candidates",QJsonArray{QJsonObject{
            {"candidate_id","invented-repair"},{"controls",QJsonArray{QJsonObject{{"control_id","route:backup_pump:flow"},{"value",16}}}}}}}});
        QVERIFY(!rejected.value("ok").toBool()); QVERIFY(g->runs().isEmpty());
        QCOMPARE(f.project.network.routes[3].setpoint_m3h,16.0); QVERIFY(f.project.network.routes[3].available);
    }
    void missingCoverageBlocksEveryNumericalAndScientificTool() {
        Fixture f; f.readings.removeFirst(); f.begin(); const auto capture=f.stream.captureObserved(f.context,f.commands,0,"missing-capture");
        const auto g=SimulationGateway::fromObservation(capture); QVERIFY(!g->observationsAvailable());
        for (const auto* name:{"forecast_baseline","simulate_candidates","search_interventions","validate_run","compare_runs","request_approval","get_sensitivity_catalog","analyze_sensitivities"}) {
            const auto r=g->execute(QLatin1String(name),{{"snapshot_id",g->snapshotId()}});
            QCOMPARE(r.value("error").toObject().value("code"),QJsonValue("coverage_unavailable"));
        }
        QCOMPARE(g->prepareSensitivity({}).error.value("error").toObject().value("code"),QJsonValue("coverage_unavailable"));
        QTemporaryDir directory; SessionRepository repository(directory.filePath("missing.sqlite")); HistoryController history(&repository);
        QSignalSpy ready(&repository,&SessionRepository::ready); QTRY_COMPARE(ready.size(),1);
        Model model; AgentOrchestrator session(&model); SessionRecorder recorder(&session,&repository,&history,"local-native-finite-search");
        session.start(g,"Explain missing coverage.",{},true);
        QTRY_VERIFY_WITH_TIMEOUT(!session.running(),15000); QCOMPARE(model.requests,0); QVERIFY(g->runs().isEmpty());
        QCOMPARE(session.result().value("status"),QJsonValue("no_validated_plan")); QVERIFY(!session.result().value("actionable").toBool());
        QTRY_COMPARE(history.selectedArchive().value("status"),QJsonValue("completed")); QString error;
        QVERIFY2(SessionRepository::validateArchive(history.selectedArchive(),&error),qPrintable(error));
        QCOMPARE(history.selectedArchive().value("archive_version"),QJsonValue(2)); QVERIFY(history.selectedArchive().value("runs").toArray().isEmpty());
    }
    void finiteOutOfRangeMeasurementsStayRawAndUnavailable() {
        for (const double value:{-4.0,150.0}) {
            Fixture f; auto row=f.readings[0].toObject(); row["value"]=value; f.readings[0]=row; f.begin();
            const auto capture=f.stream.captureObserved(f.context,f.commands,0,"out-of-range");
            const auto g=SimulationGateway::fromObservation(capture); QVERIFY(!g->observationsAvailable());
            const auto raw=g->execute("get_observations",{}).value("observed_snapshot").toObject().value("samples").toArray().first().toObject();
            QCOMPARE(raw.value("original_value"),QJsonValue(value)); QCOMPARE(raw.value("canonical_value"),QJsonValue(value));
            QVERIFY(!g->execute("forecast_baseline",{{"snapshot_id",g->snapshotId()}}).value("ok").toBool());
        }
    }
    void mappingBoundsCannotBeSubstituted() {
        Fixture f; f.begin(); auto capture=f.stream.captureObserved(f.context,f.commands,0,"bounded-capture");
        auto bounds=capture.diagnostics.value("bounds").toArray(); auto row=bounds[0].toObject(); row["absolute_bound"]=900;
        bounds[0]=row; capture.diagnostics["bounds"]=bounds;
        QVERIFY_THROWS_EXCEPTION(std::invalid_argument,SimulationGateway::fromObservation(capture));
    }
    void detectorCoverageIsFrozenAndRejectsFutureForeignOrUnsupportedReports() {
        Fixture f; f.begin(); auto capture=f.stream.captureObserved(f.context,f.commands,0,"coverage-report");
        breakwater::desktop::telemetry::ObservationMonitor monitor; monitor.observe(capture);
        auto report=monitor.report(); report.remove("events"); capture.diagnostics["detector"]=report;
        const auto gateway=SimulationGateway::fromObservation(capture);
        QCOMPARE(gateway->execute("get_detection_events",{}).value("detector").toObject(),report);
        QCOMPARE(report.value("balance_status"),QJsonValue("temporal_model_unavailable"));
        for (const auto& change:QList<QPair<QString,QJsonValue>>{{"as_of_s",101},{"stream_id","foreign"},{"window_scalars",32769},
                {"window_points",4097},{"detector_version","unregistered"},{"mode","replay"}}) {
            auto altered=capture; auto bad=report; bad[change.first]=change.second; altered.diagnostics["detector"]=bad;
            QVERIFY_THROWS_EXCEPTION(std::invalid_argument,SimulationGateway::fromObservation(altered));
            auto origin=gateway->observedOrigin(); auto mapping=origin.value("mapping").toObject(); mapping["detector"]=bad; origin["mapping"]=mapping;
            const auto archive=evidence_archive("observed_origin",origin,gateway->snapshot(),"Invalid frozen coverage fixture");
            QVERIFY(!SessionRepository::validateArchive(archive));
        }
        capture.diagnostics["detector"]="invalid"; QVERIFY_THROWS_EXCEPTION(std::invalid_argument,SimulationGateway::fromObservation(capture));
    }
    void slowHistoryCoalescesIntermediateCheckpointsAndRetainsCompleteEvidence() {
        Fixture f; f.begin(); const auto capture=f.stream.captureObserved(f.context,f.commands,0,"slow-history");
        const auto g=SimulationGateway::fromObservation(capture);
        QTemporaryDir directory; SessionRepository repository(directory.filePath("slow.sqlite")); HistoryController history(&repository);
        QSignalSpy ready(&repository,&SessionRepository::ready),finished(&repository,&SessionRepository::checkpointFinished),inspected(&repository,&SessionRepository::sessionInspected);
        QTRY_COMPARE(ready.size(),1);
        const auto barrier=evidence_archive("observed_origin",g->observedOrigin(),g->snapshot(),"Slow SQL worker fixture");
        auto entered=std::make_shared<std::atomic<bool>>(false),released=std::make_shared<std::atomic<bool>>(false);
        repository.enqueueCheckpoint([barrier,entered,released] {
            entered->store(true); QElapsedTimer timeout; timeout.start();
            while (!released->load() && timeout.elapsed()<10000) QThread::msleep(1);
            return barrier;
        },barrier.value("session_id").toString());
        QTRY_VERIFY(entered->load());
        Model unused; AgentOrchestrator session(&unused); SessionRecorder recorder(&session,&repository,&history,"local-native-finite-search");
        session.start(g,"Search while SQL is busy.",{},true); QTRY_VERIFY_WITH_TIMEOUT(!session.running(),15000);
        QVERIFY(recorder.pending()); QVERIFY(session.events().size()>20); released->store(true);
        QTRY_VERIFY_WITH_TIMEOUT(!recorder.pending(),30000);
        int writes=0; for (const auto& row:finished) if (row[0].toString()==session.sessionId()) { ++writes; QVERIFY(row[2].toBool()); }
        QVERIFY(writes>=1 && writes<=2);
        repository.inspectSession(session.sessionId()); QTRY_COMPARE_WITH_TIMEOUT(inspected.size(),1,15000);
        const auto archive=inspected.first().first().value<QJsonObject>(); QString error;
        QVERIFY2(SessionRepository::validateArchive(archive,&error),qPrintable(error));
        QCOMPARE(archive.value("events").toArray(),QJsonArray::fromVariantList(session.events()));
        QCOMPARE(archive.value("recommendation").toObject().value("selected_run_id"),session.result().value("selected_run_id"));
    }
    void modelMustInspectObservationsAndKeepsItsSelectedToolOrder() {
        Fixture f; f.begin(); const auto capture=f.stream.captureObserved(f.context,f.commands,0,"model-capture");
        Model model; int step=0; QString snapshot,selected;
        model.respond=[&](const QJsonArray& h)->QJsonObject {
            switch (step++) {
            case 0:return calls({tool("observations","get_observations"),tool("network","get_network_state"),tool("catalog","get_control_catalog"),tool("detection","get_detection_events")});
            case 1:snapshot=result(h,"network").value("snapshot_id").toString(); return calls({tool("baseline","forecast_baseline",{{"snapshot_id",snapshot}})});
            case 2:selected=result(h,"baseline").value("run").toObject().value("run_id").toString(); return calls({tool("validate","validate_run",{{"snapshot_id",snapshot},{"run_id",selected}})});
            case 3:return calls({tool("stage","request_approval",{{"snapshot_id",snapshot},{"run_id",selected}})});
            default:return final(selected);
            }
        };
        AgentOrchestrator session(&model); const auto g=SimulationGateway::fromObservation(capture,0.1); session.start(g,"Inspect this observed incident.");
        QTRY_VERIFY_WITH_TIMEOUT(!session.running(),15000); QVERIFY(session.result().value("verified_feasible").toBool());
        QVERIFY(!session.result().value("actionable").toBool()); QCOMPARE(session.result().value("approval_status"),QJsonValue("pending_advisory_review"));
        QCOMPARE(result(model.history,"observations").value("observed_snapshot").toObject(),capture.document);
        QStringList order; for (const auto& row:session.events()) if (row.toMap().value("type")=="tool_completed") order.append(row.toMap().value("tool_name").toString());
        QCOMPARE(order,(QStringList{"get_observations","get_network_state","get_control_catalog","get_detection_events","forecast_baseline","validate_run","request_approval"}));
        Model skipping; int skipped=0; skipping.respond=[&](const QJsonArray&)->QJsonObject { return skipped++==0 ? calls({tool("n","get_network_state"),tool("c","get_control_catalog")}):final(); };
        AgentOrchestrator denied(&skipping); denied.start(SimulationGateway::fromObservation(capture),"Skip raw evidence.");
        QTRY_VERIFY(!denied.running()); QVERIFY(denied.result().isEmpty());
        QCOMPARE(denied.events().last().toMap().value("error").toMap().value("code").toString(),QString("inspection_required"));
    }
    void actualPublisherCaptureSearchReviewAndHistoryNeverApplyControls() {
        if (!QFileInfo::exists(QStringLiteral(BREAKWATER_TEST_PYTHON))) QSKIP("Configured CPU Python is unavailable.");
        ProjectController project; project.loadExample("monitoring"); const auto document=project.editorDocument();
        const auto revision=project.simulationRevision(); const auto commands=commandsFor(project);
        breakwater::desktop::telemetry::TelemetryController monitor; monitor.setTimeoutMs(20000); const auto context=project.sensorContext();
        monitor.configure(project.sensorConfiguration(),context); QVERIFY(monitor.setObservedCaptureContext(context,revision,commands)); QTRY_COMPARE(monitor.status(),QString("ready"));
        monitor.startProcess(QStringLiteral(BREAKWATER_TEST_PYTHON),{"-B",QStringLiteral(BREAKWATER_SOURCE_DIR "/science/telemetry_publisher.py"),"--preset","healthy","--seed","71"});
        QTRY_VERIFY_WITH_TIMEOUT(monitor.counters().value("readings_received").toInt()>=10,15000);
        monitor.pause(); QTRY_VERIFY(monitor.paused()); QSignalSpy captured(&monitor,&breakwater::desktop::telemetry::TelemetryController::observedCaptureReady);
        monitor.requestObservedCapture(); QTRY_COMPARE(captured.size(),1); QCOMPARE(monitor.observedCaptureDisposition(),QString("available"));
        const auto coverage=monitor.capturedObservation()->diagnostics.value("detector").toObject();
        QCOMPARE(coverage.value("balance_status"),QJsonValue("temporal_model_unavailable"));
        QVERIFY(coverage.value("as_of_s").toDouble()<=monitor.capturedObservation()->document.value("captured_event_time_s").toDouble());
        QTemporaryDir directory; SessionRepository repository(directory.filePath("observed.sqlite")); HistoryController history(&repository);
        Model unused; ObservedInvestigationController investigation(&project,&monitor,&unused); investigation.setHistory(&repository,&history);
        investigation.searchLocally(); QTRY_VERIFY_WITH_TIMEOUT(!investigation.running(),20000); QTRY_VERIFY_WITH_TIMEOUT(investigation.canAcknowledge(),15000);
        QVERIFY(!investigation.canApprove()); QCOMPARE(unused.requests,0); QCOMPARE(project.editorDocument(),document);
        QCOMPARE(project.simulationRevision(),revision); QVERIFY(project.capturedRun().isEmpty());
        const auto live=history.selectedArchive(); QString error; QVERIFY2(SessionRepository::validateArchive(live,&error),qPrintable(error));
        QCOMPARE(live.value("archive_version"),QJsonValue(2)); QCOMPARE(live.value("session_kind"),QJsonValue("observed_origin"));
        QVERIFY(!live.value("actionable").toBool()); QVERIFY(!live.value("recommendation").toObject().value("actionable").toBool());
        const auto candidate=history.candidateRunId(),baseline=history.baselineRunId();
        history.selectRuns(baseline,baseline); QVERIFY(!investigation.canAcknowledge()); QVERIFY(!investigation.acknowledgePlan());
        history.selectRuns(baseline,candidate); QTRY_VERIFY(investigation.canAcknowledge());
        project.moveAsset("tank_a",123,456); QVERIFY(investigation.canAcknowledge()); QCOMPARE(project.simulationRevision(),revision);
        QVERIFY(investigation.acknowledgePlan()); QVERIFY(!investigation.acknowledgePlan());
        QTRY_COMPARE(history.selectedArchive().value("approvals").toArray().size(),1);
        QCOMPARE(history.selectedArchive().value("approvals").toArray().first().toObject().value("status"),QJsonValue("operator_acknowledged_advisory"));
        QVERIFY(!project.canApprove()); QVERIFY(project.capturedRun().isEmpty());
        const auto session=history.selectedSessionId(); history.loadSession(session); QTRY_VERIFY_WITH_TIMEOUT(history.historical() && !history.busy(),20000);
        QVERIFY(!investigation.canAcknowledge()); QVERIFY(!investigation.acknowledgePlan());
        // A cancelled replacement cannot restore the preceding acknowledgement.
        history.showLive(); investigation.searchLocally(); investigation.cancel(); QTRY_VERIFY_WITH_TIMEOUT(!investigation.running(),15000);
        QVERIFY(!investigation.canAcknowledge());
        monitor.stop(); QTRY_VERIFY(!monitor.active());
    }
    void detectorEventsFreezeAtCaptureAndExpiryRetiresAdvisoryReview() {
        if (!QFileInfo::exists(QStringLiteral(BREAKWATER_TEST_PYTHON))) QSKIP("Configured CPU Python is unavailable.");
        ProjectController project; project.loadExample("monitoring"); const auto document=project.editorDocument();
        breakwater::desktop::telemetry::TelemetryController monitor; monitor.setTimeoutMs(20000);
        monitor.configure(project.sensorConfiguration(),project.sensorContext());
        QVERIFY(monitor.setObservedCaptureContext(project.sensorContext(),project.simulationRevision(),commandsFor(project)));
        QTRY_COMPARE(monitor.status(),QString("ready"));
        monitor.startProcess(QStringLiteral(BREAKWATER_TEST_PYTHON),{"-B",QStringLiteral(BREAKWATER_SOURCE_DIR "/science/telemetry_publisher.py"),
            "--preset","pump_loss","--seed","72","--onset-s","0.1","--recovery-s","100"});
        QTRY_VERIFY_WITH_TIMEOUT(!monitor.detection().value("events").toList().isEmpty(),15000);
        // This fixture configures a 20-second lifecycle deadline. A checked
        // worker can drain slower than QTest's unrelated five-second default.
        monitor.pause(); QTRY_VERIFY_WITH_TIMEOUT(monitor.paused(),20000); QSignalSpy captured(&monitor,&breakwater::desktop::telemetry::TelemetryController::observedCaptureReady);
        monitor.requestObservedCapture(); QTRY_COMPARE_WITH_TIMEOUT(captured.size(),1,20000); const auto source=monitor.capturedObservation(); QVERIFY(source);
        QVERIFY(!source->detection_events.isEmpty()); const auto events=source->detection_events;
        for (const auto& row:events) QVERIFY(row.toObject().value("updated_event_time_s").toDouble()<=source->document.value("captured_event_time_s").toDouble());
        auto future=*source; auto event=events.first().toObject(); event["updated_event_time_s"]=source->document.value("captured_event_time_s").toDouble()+1;
        future.detection_events=QJsonArray{event}; QVERIFY_THROWS_EXCEPTION(std::invalid_argument,SimulationGateway::fromObservation(future));
        Model unused; ObservedInvestigationController investigation(&project,&monitor,&unused);
        investigation.searchLocally(); QTRY_VERIFY_WITH_TIMEOUT(!investigation.running(),20000); QVERIFY2(investigation.canAcknowledge(),qPrintable(investigation.status()));
        QVERIFY(investigation.review().value("measurements").toList().size()>=10); QVERIFY(!investigation.canApprove());
        const auto recommendation=investigation.recommendation(); const auto runs=investigation.runs();
        // Exercise expiry at the acquisition's real-time rate. Lifecycle rate
        // changes have their own tests; accelerated input can outrun an ASan
        // receiver and correctly leave the subsequent capture unavailable.
        monitor.resume(); QTRY_VERIFY_WITH_TIMEOUT(!monitor.paused(),20000);
        QTRY_COMPARE_WITH_TIMEOUT(monitor.observedCaptureDisposition(),QString("expired"),10000);
        QVERIFY(!investigation.canAcknowledge()); QVERIFY(!investigation.acknowledgePlan());
        QCOMPARE(investigation.recommendation(),recommendation); QCOMPARE(investigation.runs(),runs); QCOMPARE(source->detection_events,events);
        QCOMPARE(project.editorDocument(),document); QVERIFY(project.capturedRun().isEmpty());
        monitor.pause(); QTRY_VERIFY_WITH_TIMEOUT(monitor.paused(),20000); monitor.requestObservedCapture(); QTRY_COMPARE_WITH_TIMEOUT(captured.size(),2,20000);
        QCOMPARE(monitor.observedCaptureDisposition(),QString("available"));
        QVERIFY(!investigation.canAcknowledge());
        investigation.searchLocally(); QTRY_VERIFY_WITH_TIMEOUT(!investigation.running(),20000);
        QVERIFY2(investigation.canAcknowledge(),qPrintable(QString::fromUtf8(QJsonDocument(QJsonObject{
            {"status",investigation.status()},{"capture_disposition",monitor.observedCaptureDisposition()},
            {"capture",QJsonObject::fromVariantMap(monitor.observedCaptureDiagnostics())},
            {"events",QJsonArray::fromVariantList(investigation.events())}}).toJson(QJsonDocument::Compact))));
        project.select("tank_a"); project.editSelected("capacity","121");
        QVERIFY(!investigation.canAcknowledge()); QVERIFY(!investigation.acknowledgePlan());
        monitor.stop(); QTRY_VERIFY_WITH_TIMEOUT(!monitor.active(),20000); QCOMPARE(unused.requests,0);
    }
};
QTEST_GUILESS_MAIN(ObservedForecastTests)
#include "observed_forecast_tests.moc"
