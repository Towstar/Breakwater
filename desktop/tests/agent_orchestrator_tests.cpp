#include "agent/AgentController.hpp"
#include "science/AgentSensitivityClient.hpp"
#include "history/SessionRepository.hpp"
#include "history/HistoryController.hpp"
#include "history/RehearsalEvidenceGuard.hpp"
#include "history/RehearsalArchiveCoordinator.hpp"
#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QMap>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QtTest>
#include <algorithm>
#include <functional>

#ifndef BREAKWATER_TEST_PYTHON
#define BREAKWATER_TEST_PYTHON ""
#endif

using namespace breakwater;
using namespace breakwater::desktop;
using namespace breakwater::desktop::agent;
namespace {
QJsonObject call(const QString& id, const QString& name, const QJsonObject& args = {}) {
    return {{"id", id}, {"type", "function"}, {"function", QJsonObject{{"name", name},
        {"arguments", QString::fromUtf8(QJsonDocument(args).toJson(QJsonDocument::Compact))}}}};
}
QJsonObject calls(const QJsonArray& values) { return {{"role", "assistant"}, {"content", QJsonValue::Null}, {"tool_calls", values}}; }
QJsonObject conclusion(const QJsonValue& run = QJsonValue::Null) {
    return {{"role", "assistant"}, {"content", QString::fromUtf8(QJsonDocument(QJsonObject{{"selected_run_id", run}, {"explanation", "Review the stored numerical evidence. Quality is not evaluated."}}).toJson(QJsonDocument::Compact))}};
}
QJsonArray controlJson(const std::vector<ControlValue>& values) {
    QJsonArray out;
    for (const auto& v : values) out.append(QJsonObject{{"control_id", QString::fromStdString(v.id)}, {"value", v.value}});
    return out;
}
QJsonObject candidateArguments(const QString& snapshot, const QString& id, const QJsonArray& controls) {
    return {{"snapshot_id", snapshot}, {"candidates", QJsonArray{QJsonObject{{"candidate_id", id}, {"controls", controls}}}}};
}
QJsonObject resultFor(const QJsonArray& history, const QString& id) {
    for (const auto& value : history) {
        const auto message = value.toObject();
        if (message["role"] == "tool" && message["tool_call_id"] == id)
            return QJsonDocument::fromJson(message["content"].toString().toUtf8()).object();
    }
    return {};
}
QStringList toolNames(const QJsonArray& definitions) {
    QStringList names;
    for (const auto& definition : definitions)
        names.append(definition.toObject()["function"].toObject()["name"].toString());
    return names;
}
QJsonObject completionLedger(const QJsonArray& messages) {
    const auto content=messages.first().toObject().value("content").toString();
    const QString marker="Current retained completion references (data, not a new tool result): ";
    const auto start=content.indexOf(marker);
    if(start<0) return {};
    return QJsonDocument::fromJson(content.mid(start+marker.size()).section('\n',0,0).toUtf8()).object();
}

// This is an explicit test model. Every tool execution still uses the real engine.
class FakeModel final : public ModelTransport {
public:
    using ModelTransport::ModelTransport;
    std::function<QJsonObject(const QJsonArray&)> respond;
    QJsonArray history;
    QJsonArray tools;
    QList<QJsonArray> toolsPerRequest;
    int requests = 0, cancellations = 0, generation = 0, finalJsonRequests = 0;
    bool silent = false;
    bool metadataOnCancel = false;
    QMap<int,QString> failures;
    void request(const QJsonArray& messages, const QJsonArray& definitions) override {
        history = messages; tools = definitions; toolsPerRequest.append(definitions); ++requests; emit requestStarted();
        if (silent) return;
        if (failures.contains(requests)) {
            const auto code=failures.value(requests); const auto epoch=generation;
            QTimer::singleShot(0,this,[this,code,epoch] { if(epoch==generation) emit requestFailed(code,"Private response must not be retained"); });
            return;
        }
        const auto reply = respond(messages);
        const auto epoch = generation;
        QTimer::singleShot(0, this, [this, reply, epoch] { if (epoch == generation) emit replyReady(reply); });
    }
    void cancel() override {
        ++generation; ++cancellations;
        if (metadataOnCancel) emit requestCompleted(QJsonObject{{"request_id", "cancelled-test-request"}, {"outcome", "cancelled"}, {"error_code", "cancelled"}, {"http_status", 0}});
    }
    void requestFinalJson(const QJsonArray& messages) override { ++finalJsonRequests; request(messages,{}); }
};

void adaptiveModel(FakeModel& model, bool skipValidation = false) {
    model.respond = [step = 0, snapshot = QString{}, bad = QString{}, good = QString{}, skipValidation](const QJsonArray& history) mutable -> QJsonObject {
        switch (step++) {
        case 0: return calls({call("state", "get_network_state"), call("catalog", "get_control_catalog")});
        case 1: {
            snapshot = resultFor(history, "state")["snapshot_id"].toString();
            return calls({call("baseline", "forecast_baseline", {{"snapshot_id", snapshot}}),
                call("bad", "simulate_candidates", candidateArguments(snapshot, "attempt-1", controlJson(incident_controls("bad"))))});
        }
        case 2: {
            bad = resultFor(history, "bad")["runs"].toArray().first().toObject()["run_id"].toString();
            return calls({call("bad-validation", "validate_run", {{"snapshot_id", snapshot}, {"run_id", bad}})});
        }
        case 3: {
            const auto report = resultFor(history, "bad-validation")["run"].toObject()["validation"].toObject();
            if (report["feasible"].toBool(true)) return conclusion("fabricated-success");
            return calls({call("revised", "simulate_candidates", candidateArguments(snapshot, "attempt-2", controlJson(incident_controls("mixed"))))});
        }
        case 4:
            if (resultFor(history, "revised")["runs"].toArray().isEmpty()) return conclusion();
            good = resultFor(history, "revised")["runs"].toArray().first().toObject()["run_id"].toString();
            if (skipValidation) return conclusion(good);
            return calls({call("good-validation", "validate_run", {{"snapshot_id", snapshot}, {"run_id", good}}),
                call("comparison", "compare_runs", {{"snapshot_id", snapshot}, {"run_ids", QJsonArray{bad, good}}})});
        case 5: return calls({call("stage", "request_approval", {{"snapshot_id", snapshot}, {"run_id", good}})});
        default: return conclusion(good);
        }
    };
}
}

class OrchestratorTests : public QObject {
    Q_OBJECT
private slots:
    void recoversExactStoredRunOnlyAfterModelValidation_data() {
        QTest::addColumn<bool>("budget");
        QTest::addColumn<bool>("malformed");
        QTest::newRow("one-validation-recovery") << true << false;
        QTest::newRow("no-extra-tool-budget") << false << false;
        QTest::newRow("no-second-format-repair") << true << true;
    }
    void recoversExactStoredRunOnlyAfterModelValidation() {
        QFETCH(bool, budget); QFETCH(bool, malformed);
        Network network; network.assets = {{"store", "Store", AssetKind::tank, 90.0, 45.0}};
        const auto gateway = std::make_shared<SimulationGateway>(network, "fixed-input");
        const auto original = gateway->snapshot();
        FakeModel model; QString selected; QStringList recoveryTools; int step = 0;
        model.respond = [&](const QJsonArray& history) {
            if (step++ == 0) return calls({call("state", "get_network_state"), call("catalog", "get_control_catalog"),
                call("baseline", "forecast_baseline", {{"snapshot_id", gateway->snapshotId()}})});
            selected = resultFor(history, "baseline").value("run").toObject().value("run_id").toString();
            if (step == 3) {
                recoveryTools = toolNames(model.tools);
                return calls({call("own-proof", "validate_run", {{"snapshot_id", gateway->snapshotId()}, {"run_id", selected}})});
            }
            if (malformed && step == 4) return QJsonObject{{"role", "assistant"}, {"content", "not JSON"}};
            return conclusion(selected);
        };
        AgentOrchestrator investigator(&model);
        investigator.start(gateway, "Keep controls fixed and validate the selected stored run.", {budget ? 4 : 3, 1, 5000});
        QTRY_VERIFY_WITH_TIMEOUT(!investigator.running(), 6000);
        QCOMPARE(gateway->snapshot(), original); QVERIFY(!gateway->stagedPlan().value("ok").toBool());
        if (budget && !malformed) {
            QCOMPARE(recoveryTools, QStringList({"validate_run"}));
            QVERIFY(investigator.result().value("verified_feasible").toBool());
            const auto text = investigator.result().value("explanation").toString();
            QVERIFY(text.contains("initial 45 m3; terminal 45 m3; capacity 90 m3"));
            QVERIFY(text.contains("No operating-control changes"));
            QVERIFY(!text.contains("full capacity"));
        } else QVERIFY(investigator.result().isEmpty());
        if (malformed) {
            QCOMPARE(model.requests, 4);
            for (const auto& row : investigator.events())
                QVERIFY(row.toMap().value("type") != "final_format_correction_requested");
        }
    }

    void completedInfeasibleProofDoesNotStartReferenceRecovery() {
        const auto network = main_pump_failure(canonical_scenario());
        const auto gateway = std::make_shared<SimulationGateway>(network, "infeasible-fixed-input");
        FakeModel model; QString selected; int step = 0;
        model.respond = [&](const QJsonArray& history) {
            if (step++ == 0) return calls({call("state", "get_network_state"), call("catalog", "get_control_catalog"),
                call("baseline", "forecast_baseline", {{"snapshot_id", gateway->snapshotId()}})});
            selected = resultFor(history, "baseline").value("run").toObject().value("run_id").toString();
            if (step == 2) return calls({call("own-proof", "validate_run", {{"snapshot_id", gateway->snapshotId()}, {"run_id", selected}})});
            return conclusion(selected);
        };
        AgentOrchestrator investigator(&model);
        investigator.start(gateway, "Retain the actual failed independent validation.", {8, 1, 5000});
        QTRY_VERIFY_WITH_TIMEOUT(!investigator.running(), 6000);
        QCOMPARE(model.requests, 3); QVERIFY(investigator.result().isEmpty());
        QCOMPARE(investigator.events().last().toMap().value("error").toMap().value("code").toString(), QString("ungrounded_recommendation"));
        for (const auto& row : investigator.events()) QVERIFY(row.toMap().value("type") != "final_reference_rejected");
        const auto run = gateway->runDetails(selected).value("run").toObject();
        QVERIFY(!run.value("validation").toObject().value("feasible").toBool());
        QVERIFY(!gateway->stagedPlan().value("ok").toBool());
    }
    void repeatedEligibleValidationClosesToolsWithoutInventingASelection() {
        FakeModel model; QString snapshot, selected; int step = 0;
        model.respond = [&](const QJsonArray& history) -> QJsonObject {
            switch (step++) {
            case 0: return calls({call("n", "get_network_state"), call("c", "get_control_catalog")});
            case 1:
                snapshot = resultFor(history,"n").value("snapshot_id").toString();
                return calls({call("candidate", "simulate_candidates", candidateArguments(snapshot,"mixed",controlJson(incident_controls("mixed"))))});
            case 2:
                selected = resultFor(history,"candidate").value("runs").toArray().first().toObject().value("run_id").toString();
                return calls({call("v1","validate_run",{{"snapshot_id",snapshot},{"run_id",selected}})});
            case 3: return calls({call("v2","validate_run",{{"run_id",selected},{"snapshot_id",snapshot}}), call("read","get_control_catalog")});
            default: return conclusion(); // Model remains free to select null.
            }
        };
        AgentOrchestrator session(&model);
        auto gateway=std::make_shared<SimulationGateway>(main_pump_failure(canonical_scenario()),"completion-repeat");
        session.start(gateway,"Inspect and investigate."); QTRY_VERIFY_WITH_TIMEOUT(!session.running(),10000);
        QCOMPARE(model.requests,5); QCOMPARE(model.finalJsonRequests,0); QCOMPARE(toolNames(model.tools),QStringList{"request_approval"});
        QVERIFY(resultFor(model.history,"v2").value("reused_validation").toBool());
        QCOMPARE(resultFor(model.history,"v1").value("run"),resultFor(model.history,"v2").value("run"));
        QVERIFY(resultFor(model.history,"read").value("ok").toBool()); // Entire batch retained in order.
        QVERIFY(session.result().value("selected_run_id").isNull()); QVERIFY(!gateway->stagedPlan().value("ok").toBool());
        QCOMPARE(gateway->runs().size(),1);
        QVERIFY(session.result().value("explanation").toString().contains("does not establish"));
    }
    void repeatedValidationStillAllowsExplicitStagingAndFreshSessionsResetCompletion() {
        FakeModel model;AgentOrchestrator session(&model);
        for(int attempt=0;attempt<2;++attempt) {
            QString snapshot,selected;int step=0;model.requests=model.finalJsonRequests=0;
            model.respond=[&](const QJsonArray& h)->QJsonObject {
                switch(step++) {
                case 0:return calls({call("n","get_network_state"),call("c","get_control_catalog")});
                case 1:snapshot=resultFor(h,"n").value("snapshot_id").toString();return calls({call("a","simulate_candidates",candidateArguments(snapshot,"mixed",controlJson(incident_controls("mixed"))))});
                case 2:selected=resultFor(h,"a").value("runs").toArray().first().toObject().value("run_id").toString();return calls({call("v1","validate_run",{{"snapshot_id",snapshot},{"run_id",selected}})});
                case 3:return calls({call("v2","validate_run",{{"snapshot_id",snapshot},{"run_id",selected}})});
                case 4:return calls({call("stage","request_approval",{{"snapshot_id",snapshot},{"run_id",selected}})});
                default:return conclusion(selected);
                }
            };
            auto gateway=std::make_shared<SimulationGateway>(main_pump_failure(canonical_scenario()),"repeat-stage");
            const auto before=gateway->snapshot();session.start(gateway,"Investigate and stage for review.");
            QTRY_VERIFY_WITH_TIMEOUT(!session.running(),10000);QCOMPARE(model.requests,6);QCOMPARE(model.finalJsonRequests,1);
            QCOMPARE(toolNames(model.toolsPerRequest[model.toolsPerRequest.size()-2]),QStringList{"request_approval"});
            QCOMPARE(session.result().value("selected_run_id").toString(),selected);
            QCOMPARE(session.result().value("approval_status").toString(),QString("pending_operator"));
            QCOMPARE(gateway->stagedPlan().value("selected_run_id").toString(),selected);QCOMPARE(gateway->snapshot(),before);
        }
    }
    void newNumericalWorkInRepeatedValidationBatchKeepsAdaptiveToolsAvailable() {
        FakeModel model; QString snapshot, selected; int step=0; bool adaptive=false;
        model.respond=[&](const QJsonArray& h)->QJsonObject {
            switch(step++) {
            case 0:return calls({call("n","get_network_state"),call("c","get_control_catalog")});
            case 1:
                snapshot=resultFor(h,"n").value("snapshot_id").toString();
                return calls({call("a","simulate_candidates",candidateArguments(snapshot,"mixed",controlJson(incident_controls("mixed"))))});
            case 2:
                selected=resultFor(h,"a").value("runs").toArray().first().toObject().value("run_id").toString();
                return calls({call("v1","validate_run",{{"snapshot_id",snapshot},{"run_id",selected}})});
            case 3:return calls({call("v2","validate_run",{{"snapshot_id",snapshot},{"run_id",selected}}),
                call("b","simulate_candidates",candidateArguments(snapshot,"different",controlJson(incident_controls("bad"))))});
            default:adaptive=!model.tools.isEmpty();return conclusion(selected);
            }
        };
        auto gateway=std::make_shared<SimulationGateway>(main_pump_failure(canonical_scenario()),"completion-new-work");
        AgentOrchestrator session(&model);session.start(gateway,"Investigate alternatives.");
        QTRY_VERIFY_WITH_TIMEOUT(!session.running(),10000);QVERIFY(adaptive);QCOMPARE(model.finalJsonRequests,0);
        QCOMPARE(gateway->runs().size(),2);QVERIFY(session.result().value("verified_feasible").toBool());
    }
    void stagingCompletionKeepsCancellationAndRejectsExtraTools() {
        for(const bool cancel:{false,true}) {
            FakeModel model;QString snapshot,selected;int step=0;
            model.respond=[&](const QJsonArray& h)->QJsonObject {
                switch(step++) {
                case 0:return calls({call("n","get_network_state"),call("c","get_control_catalog")});
                case 1:snapshot=resultFor(h,"n").value("snapshot_id").toString();return calls({call("a","simulate_candidates",candidateArguments(snapshot,"mixed",controlJson(incident_controls("mixed"))))});
                case 2:selected=resultFor(h,"a").value("runs").toArray().first().toObject().value("run_id").toString();return calls({call("v","validate_run",{{"snapshot_id",snapshot},{"run_id",selected}})});
                case 3:return calls({call("stage","request_approval",{{"snapshot_id",snapshot},{"run_id",selected}})});
                default:return calls({call("extra","forecast_baseline",{{"snapshot_id",snapshot}})});
                }
            };
            auto gateway=std::make_shared<SimulationGateway>(main_pump_failure(canonical_scenario()),"stage-completion");
            const auto before=gateway->snapshot();AgentOrchestrator session(&model);
            connect(&session,&AgentOrchestrator::activity,&session,[&](const QJsonObject& e){if(cancel && e.value("type")=="completion_tools_closed")session.cancel();});
            session.start(gateway,"Stage a validated response.");QTRY_VERIFY_WITH_TIMEOUT(!session.running(),10000);
            QVERIFY(session.result().isEmpty());QCOMPARE(gateway->runs().size(),1);QCOMPARE(gateway->snapshot(),before);
            QCOMPARE(model.finalJsonRequests,cancel?0:1);
            if(cancel) QCOMPARE(session.events().last().toMap().value("type").toString(),QString("agent_cancelled"));
            else QCOMPARE(session.events().last().toMap().value("error").toMap().value("code").toString(),QString("completion_tools_closed"));
        }
    }
    void localSearchUsesRealNumericsWithoutAnyModelRequestsOrAutomaticApplication() {
        ProjectController project; project.loadExample("baseline");
        const auto before = project.editorDocument(); const auto hash = project.networkHash();
        FakeModel model; model.silent = true; AgentController controller(&project, &model);
        controller.searchLocally(); QTRY_VERIFY_WITH_TIMEOUT(!controller.running(), 20000);
        QCOMPARE(model.requests, 0); QVERIFY(controller.localOnly());
        const auto result = controller.recommendation();
        QCOMPARE(result.value("source_mode").toString(), "local_native");
        QCOMPARE(result.value("status").toString(), "validated_recommendation");
        QVERIFY(controller.runs().size() >= 2); QVERIFY(controller.canApprove());
        QVERIFY(!controller.review().value("validation_id").toString().isEmpty());
        QCOMPARE(project.editorDocument(), before); QCOMPARE(project.networkHash(), hash);
        for (const auto& value : controller.events()) QVERIFY(value.toMap().value("type").toString() != "model_request");
        project.moveAsset("tank_a", 180, 100); QVERIFY(controller.canApprove());
        project.reset(); QVERIFY(!controller.canApprove());
    }
    void localSearchIsRetainedAndReopensAsNonactionableHistory() {
        QTemporaryDir directory; history::SessionRepository repository(directory.filePath("local.sqlite"));
        history::HistoryController history(&repository); ProjectController project; project.loadExample("baseline");
        FakeModel model; model.silent = true; AgentController controller(&project, &model); controller.setHistory(&repository, &history);
        controller.searchLocally(); QTRY_VERIFY_WITH_TIMEOUT(!controller.running(), 20000);
        const auto id = controller.recommendation().value("session_id").toString(); QVERIFY(!id.isEmpty());
        QTRY_VERIFY_WITH_TIMEOUT(history.runs().size() >= 2, 20000);
        QTest::qWait(100); history.loadSession(id); QTRY_VERIFY_WITH_TIMEOUT(history.historical(), 20000);
        QVERIFY(history.runs().size() >= 2); QVERIFY(!controller.canApprove()); QCOMPARE(model.requests, 0);
    }
    void rehearsalCoordinatorExportsApprovedSearchFromEmptyInitialForecast_data() {
        QTest::addColumn<bool>("editDuringForecast");
        QTest::newRow("approved-search-export") << false;
        QTest::newRow("reject-editor-drift") << true;
    }
    void rehearsalCoordinatorExportsApprovedSearchFromEmptyInitialForecast() {
        QFETCH(bool, editDuringForecast);
        QTemporaryDir directory; QVERIFY(directory.isValid());
        history::SessionRepository repository(directory.filePath("coordinator.sqlite"));
        QSignalSpy ready(&repository, &history::SessionRepository::ready);
        history::HistoryController historyView(&repository);
        ProjectController project; project.loadExample("baseline");
        QVERIFY(!project.capturedForecast());
        FakeModel model;
        model.respond = [step = 0, snapshot = QString{}, run = QString{}](const QJsonArray& history) mutable -> QJsonObject {
            switch (step++) {
            case 0: return calls({call("state", "get_network_state"), call("catalog", "get_control_catalog")});
            case 1:
                snapshot = resultFor(history, "state").value("snapshot_id").toString();
                return calls({call("baseline", "forecast_baseline", {{"snapshot_id", snapshot}})});
            case 2: return calls({call("search", "search_interventions", {{"snapshot_id", snapshot}, {"axes", QJsonArray{
                QJsonObject{{"control_id", "route:backup_pump:flow"}, {"values", QJsonArray{32}}},
                QJsonObject{{"control_id", "route:bypass:flow"}, {"values", QJsonArray{20}}},
                QJsonObject{{"control_id", "source:source_a:curtail"}, {"values", QJsonArray{0}}},
                QJsonObject{{"control_id", "route:truck_route:flow"}, {"values", QJsonArray{20}}}}}, {"result_limit", 1}})});
            case 3:
                run = resultFor(history, "search").value("runs").toArray().first().toObject().value("run_id").toString();
                return calls({call("stage", "request_approval", {{"snapshot_id", snapshot}, {"run_id", run}})});
            case 4: return calls({call("validate-search", "validate_run", {{"snapshot_id", snapshot}, {"run_id", run}})});
            default: return conclusion(run);
            }
        };
        AgentController agent(&project, &model); agent.setHistory(&repository, &historyView);
        bool completed = false; QString failure;
        const auto destination = directory.filePath("approved.json");
        history::RehearsalArchiveCoordinator coordinator(project, agent, repository, destination,
            [&] { completed = true; }, [&](const QString& code) { failure = code; });
        QTRY_VERIFY_WITH_TIMEOUT(!ready.isEmpty(), 10000);
        coordinator.begin();
        agent.investigate("Use actual bounded search and stage a validated response for operator review.");
        QTRY_VERIFY_WITH_TIMEOUT(!agent.running(), 10000);
        const auto validation = resultFor(model.history, "validate-search");
        QVERIFY(validation.value("ok").toBool());
        const auto checked = validation.value("run").toObject();
        QCOMPARE(checked.value("run_id").toString(), agent.recommendation().value("selected_run_id").toString());
        QCOMPARE(checked.value("validation_source").toString(), QString("validate_run"));
        QVERIFY(checked.value("validation").toObject().value("feasible").toBool());
        QVERIFY(!checked.value("validation_id").toString().isEmpty());
        QCOMPARE(checked.value("validation_id").toString(), agent.recommendation().value("validation_id").toString());
        QTRY_VERIFY_WITH_TIMEOUT(agent.canApprove(), 10000);
        QVERIFY(agent.approvePlan());
        if (editDuringForecast) {
            project.select("tank_a"); project.editSelected("capacity", "121");
            project.forecast();
            QTRY_VERIFY_WITH_TIMEOUT(!failure.isEmpty(), 10000);
            QCOMPARE(failure, "approved_state_changed");
            QVERIFY(!completed); QVERIFY(!QFileInfo::exists(destination));
            QTRY_VERIFY_WITH_TIMEOUT(!project.busy(), 10000);
            QVERIFY(project.capturedForecast()); QVERIFY(project.capturedForecast()->validation.feasible);
            return;
        }
        QTRY_VERIFY_WITH_TIMEOUT(completed || !failure.isEmpty(), 10000);
        QVERIFY2(failure.isEmpty(), qPrintable(failure));
        QVERIFY(completed); QVERIFY(QFileInfo::exists(destination));
        QFile file(destination); QVERIFY(file.open(QIODevice::ReadOnly));
        const auto archive = QJsonDocument::fromJson(file.readAll()).object();
        QCOMPARE(archive.value("recommendation").toObject().value("approval_status").toString(), "operator_applied");
        QCOMPARE(archive.value("approvals").toArray().size(), 1);
        QVERIFY(history::SessionRepository::validateArchive(archive));
        QVERIFY(project.capturedForecast()); QVERIFY(project.capturedForecast()->validation.feasible);
        QCOMPARE(project.capturedForecast()->result.cost, 4800.0);
        QVERIFY(!historyView.historical());
    }
    void rehearsalExportRequiresAppliedFreshCaptureAndDurableHistoricalApproval() {
        for (const bool editDuringForecast : {false, true}) {
            QTemporaryDir directory; QVERIFY(directory.isValid());
            history::SessionRepository repository(directory.filePath("rehearsal.sqlite"));
            QSignalSpy ready(&repository, &history::SessionRepository::ready);
            QSignalSpy failures(&repository, &history::SessionRepository::failed);
            history::HistoryController historyView(&repository);
            ProjectController project; project.loadExample("baseline");
            project.forecast(); QTRY_VERIFY_WITH_TIMEOUT(!project.busy(), 10000);
            const auto priorCapture = project.runId(); QVERIFY(!priorCapture.isEmpty());
            history::RehearsalEvidenceGuard guard;
            guard.setIncident(project.document(), project.networkHash(), priorCapture);
            FakeModel model; adaptiveModel(model);
            AgentController agent(&project, &model); agent.setHistory(&repository, &historyView);
            bool pinned = false;
            connect(&agent, &AgentController::changed, &agent, [&] {
                if (!guard.pinned() && agent.recommendation().value("approval_status").toString() == "operator_applied")
                    pinned = guard.pinApproval(agent.recommendation(), project.simulationRevision());
            });
            QJsonObject stored;
            connect(&repository, &history::SessionRepository::sessionSaved, &agent, [&](const QString& id) {
                if (guard.pinned() && id == guard.sessionId()) repository.loadSession(id);
            });
            connect(&repository, &history::SessionRepository::sessionLoaded, &agent, [&](const QJsonObject& archive) {
                if (guard.storedApprovalMatches(archive)) stored = archive;
            });
            QTRY_VERIFY_WITH_TIMEOUT(!ready.isEmpty(), 10000);
            agent.investigate("Validate and stage a response for an operator-reviewed rehearsal.");
            QTRY_VERIFY_WITH_TIMEOUT(!agent.running(), 10000);
            QTRY_VERIFY_WITH_TIMEOUT(agent.canApprove(), 10000);
            QVERIFY(!guard.pinned()); QVERIFY(!guard.captureMatches(agent.recommendation(), project));
            QVERIFY(agent.approvePlan()); QVERIFY(pinned); QVERIFY(guard.pinned());
            if (editDuringForecast) {
                // The approved forecast's completion is still queued. A later
                // feasible editor forecast must not substitute for that result.
                project.select("tank_a"); project.editSelected("capacity", "121");
                project.forecast();
            }
            QTRY_VERIFY_WITH_TIMEOUT(!project.busy(), 10000);
            QVERIFY(project.capturedForecast()); QVERIFY(project.capturedForecast()->validation.feasible);
            QVERIFY(project.runId() != priorCapture);
            QCOMPARE(guard.acceptFreshCapture(agent.recommendation(), project), !editDuringForecast);
            QTRY_VERIFY_WITH_TIMEOUT(!stored.isEmpty(), 10000);
            QVERIFY(failures.isEmpty());
            QCOMPARE(stored.value("recommendation").toObject().value("approval_status").toString(), "historical");
            QVERIFY(history::SessionRepository::validateArchive(stored));
            QVERIFY(guard.storedApprovalMatches(stored));
            auto missingApproval = stored; missingApproval["approvals"] = QJsonArray{};
            QVERIFY(!guard.storedApprovalMatches(missingApproval));
            auto mismatched = stored; auto recommendation = mismatched.value("recommendation").toObject();
            recommendation["validation_id"] = "00000000-0000-0000-0000-000000000001";
            mismatched["recommendation"] = recommendation;
            QVERIFY(!guard.storedApprovalMatches(mismatched));
            if (!editDuringForecast) {
                project.moveAsset("tank_a", 180, 100);
                QVERIFY(guard.captureMatches(agent.recommendation(), project));
                project.select("tank_a"); project.editSelected("capacity", "121");
                project.forecast(); QTRY_VERIFY_WITH_TIMEOUT(!project.busy(), 10000);
                QVERIFY(project.capturedForecast()->validation.feasible);
                QVERIFY(!guard.captureMatches(agent.recommendation(), project));
            }
        }
    }
    void scientificToolUsesActualWorkerAndPreservesNativeApprovalAuthority() {
        const auto python = QString::fromUtf8(BREAKWATER_TEST_PYTHON);
        if (!QFileInfo(python).isFile()) QSKIP("Configure the CPU Python worker for the actual agent sensitivity exchange.");
        auto project = example_project(); project.network = main_pump_failure(project.network);
        const auto gateway = std::make_shared<SimulationGateway>(project.network, physics_hash(project));
        FakeModel model; QString snapshot, run;
        bool catalogBound = false, actualAnalysis = false, analysisDidNotValidate = false;
        model.respond = [&, step = 0](const QJsonArray& history) mutable -> QJsonObject {
            switch (step++) {
            case 0: return calls({call("state", "get_network_state"), call("controls", "get_control_catalog")});
            case 1:
                snapshot = resultFor(history, "state").value("snapshot_id").toString();
                return calls({call("candidate", "simulate_candidates", candidateArguments(snapshot, "candidate", controlJson(incident_controls("mixed"))))});
            case 2:
                run = resultFor(history, "candidate").value("runs").toArray().first().toObject().value("run_id").toString();
                return calls({call("sensitivity-catalog", "get_sensitivity_catalog", {{"snapshot_id", snapshot}, {"run_id", run}})});
            case 3:
                catalogBound = resultFor(history, "sensitivity-catalog").value("run_id") == run;
                return calls({call("sensitivities", "analyze_sensitivities", {{"snapshot_id", snapshot}, {"run_id", run},
                    {"control_ids", QJsonArray{"source:source_a:curtail", "route:bypass:flow"}}})});
            case 4: {
                const auto outcome = resultFor(history, "sensitivities"), analysis = outcome.value("analysis").toObject();
                actualAnalysis = outcome.value("ok").toBool() && outcome.value("advisory").toBool() &&
                    outcome.value("validation_status") == "not_performed" && analysis.value("within_tolerance").toBool() &&
                    analysis.value("derivatives").toObject().value("terminal_volume_jacobian_h").toArray() == QJsonArray{QJsonArray{-3.0, -3.0}, QJsonArray{0.0, 3.0}};
                analysisDidNotValidate = !gateway->recommendation(run).value("ok").toBool() && !gateway->stagedPlan().value("ok").toBool();
                return calls({call("validate", "validate_run", {{"snapshot_id", snapshot}, {"run_id", run}})});
            }
            case 5: return calls({call("stage", "request_approval", {{"snapshot_id", snapshot}, {"run_id", run}})});
            default: return conclusion(run);
            }
        };
        science::AgentSensitivityClient scientific(python, QStringLiteral(BREAKWATER_SOURCE_DIR "/science/worker.py"));
        AgentOrchestrator session(&model); session.setSensitivityClient(&scientific);
        QSignalSpy evidence(&session, &AgentOrchestrator::scientificEvidenceReady);
        session.start(gateway, "Investigate using local scientific sensitivities, then validate and stage a response.");
        QTRY_VERIFY_WITH_TIMEOUT(!session.running(), 90000);
        QVERIFY(catalogBound); QVERIFY(actualAnalysis); QVERIFY(analysisDidNotValidate); QCOMPARE(evidence.size(), 1);
        const auto archive = evidence.first().first().toJsonObject(); QString error;
        QVERIFY2(history::SessionRepository::validateArchive(archive, &error), qPrintable(error));
        const auto origin = archive.value("agent_origin").toObject();
        QCOMPARE(origin.value("agent_session_id").toString(), session.sessionId());
        QCOMPARE(origin.value("incident_snapshot_id").toString(), snapshot); QCOMPARE(origin.value("run_id").toString(), run);
        QCOMPARE(origin.value("tool_call_id").toString(), "sensitivities");
        QVERIFY(origin.value("effective_physical_hash") != origin.value("incident_physical_hash"));
        QCOMPARE(session.result().value("selected_run_id").toString(), run);
        QCOMPARE(session.result().value("status").toString(), "validated_recommendation");
        QVERIFY(gateway->stagedPlan().value("ok").toBool()); QVERIFY(gateway->isCurrent());
        QCOMPARE(gateway->physicalHash(), physics_hash(project));
        QCOMPARE(resultFor(model.history, "sensitivities").value("result_id"), archive.value("session_id"));
    }
    void unavailableScientificWorkerReturnsMatchedErrorAndNativePlanningContinues() {
        auto project = example_project(); project.network = main_pump_failure(project.network);
        const auto gateway = std::make_shared<SimulationGateway>(project.network, physics_hash(project));
        FakeModel model; QString snapshot, run; bool sawFailure = false;
        model.respond = [&, step = 0](const QJsonArray& history) mutable -> QJsonObject {
            switch (step++) {
            case 0: return calls({call("state", "get_network_state"), call("controls", "get_control_catalog")});
            case 1:
                snapshot = resultFor(history, "state").value("snapshot_id").toString();
                return calls({call("candidate", "simulate_candidates", candidateArguments(snapshot, "candidate", controlJson(incident_controls("mixed"))))});
            case 2:
                run = resultFor(history, "candidate").value("runs").toArray().first().toObject().value("run_id").toString();
                return calls({call("missing-worker", "analyze_sensitivities", {{"snapshot_id", snapshot}, {"run_id", run}, {"control_ids", QJsonArray{"source:source_a:curtail"}}})});
            case 3:
                sawFailure = !resultFor(history, "missing-worker").value("ok").toBool() &&
                    !resultFor(history, "missing-worker").value("error").toObject().value("code").toString().isEmpty();
                return calls({call("validate", "validate_run", {{"snapshot_id", snapshot}, {"run_id", run}})});
            case 4: return calls({call("stage", "request_approval", {{"snapshot_id", snapshot}, {"run_id", run}})});
            default: return conclusion(run);
            }
        };
        science::AgentSensitivityClient scientific("not-an-installed-python", "not-an-installed-worker");
        AgentOrchestrator session(&model); session.setSensitivityClient(&scientific);
        QSignalSpy evidence(&session, &AgentOrchestrator::scientificEvidenceReady);
        session.start(gateway, "Use scientific analysis when available and native validation for the plan.");
        QTRY_VERIFY_WITH_TIMEOUT(!session.running(), 10000);
        QVERIFY(sawFailure); QVERIFY(evidence.isEmpty()); QCOMPARE(session.result().value("selected_run_id").toString(), run);
        QVERIFY(gateway->stagedPlan().value("ok").toBool()); QVERIFY(gateway->isCurrent());
    }
    void cancellationDuringScientificToolStopsContinuationAndKeepsNativeRun() {
        const auto python = QString::fromUtf8(BREAKWATER_TEST_PYTHON);
        if (!QFileInfo(python).isFile()) QSKIP("Configure the CPU Python worker for scientific cancellation.");
        auto project = example_project(); project.network = main_pump_failure(project.network);
        const auto gateway = std::make_shared<SimulationGateway>(project.network, physics_hash(project));
        FakeModel model; QString snapshot, run;
        model.respond = [&, step = 0](const QJsonArray& history) mutable -> QJsonObject {
            switch (step++) {
            case 0: return calls({call("state", "get_network_state"), call("controls", "get_control_catalog")});
            case 1:
                snapshot = resultFor(history, "state").value("snapshot_id").toString();
                return calls({call("candidate", "simulate_candidates", candidateArguments(snapshot, "candidate", controlJson(incident_controls("mixed"))))});
            case 2:
                run = resultFor(history, "candidate").value("runs").toArray().first().toObject().value("run_id").toString();
                return calls({call("cancelled-analysis", "analyze_sensitivities", {{"snapshot_id", snapshot}, {"run_id", run}, {"control_ids", QJsonArray{"source:source_a:curtail"}}})});
            default: return conclusion(run);
            }
        };
        science::AgentSensitivityClient scientific(python, QStringLiteral(BREAKWATER_SOURCE_DIR "/science/worker.py"));
        AgentOrchestrator session(&model); session.setSensitivityClient(&scientific);
        QSignalSpy evidence(&session, &AgentOrchestrator::scientificEvidenceReady);
        connect(&session, &AgentOrchestrator::activity, &session, [&](const QJsonObject& event) {
            if (event.value("type") == "tool_started" && event.value("tool_name") == "analyze_sensitivities")
                QTimer::singleShot(50, &session, &AgentOrchestrator::cancel);
        });
        session.start(gateway, "Investigate until cancelled.");
        QTRY_VERIFY_WITH_TIMEOUT(!session.running(), 10000);
        QTRY_VERIFY_WITH_TIMEOUT(!scientific.busy(), 10000);
        QCOMPARE(model.requests, 3); QVERIFY(session.result().isEmpty()); QVERIFY(evidence.isEmpty());
        QCOMPARE(gateway->runs().size(), 1); QCOMPARE(gateway->runDetails(run).value("run").toObject().value("run_id").toString(), run);
        QVERIFY(!gateway->stagedPlan().value("ok").toBool());
        QVERIFY(resultFor(model.history, "cancelled-analysis").isEmpty());
    }
    void cancellationAndDeadlineRetainSanitizedRequestMetadata() {
        for (const bool timeout : {false, true}) {
            FakeModel model; model.silent = true; model.metadataOnCancel = true;
            AgentOrchestrator session(&model);
            session.start(std::make_shared<SimulationGateway>(canonical_scenario(), "metadata"), "Inspect.", {12, 3, timeout ? 5 : 1000});
            if (!timeout) session.cancel();
            QTRY_VERIFY(!session.running());
            const auto events = session.events(); QVERIFY(events.size() >= 2);
            QCOMPARE(events[events.size() - 2].toMap().value("type").toString(), "model_request_completed");
            QCOMPARE(events.last().toMap().value("type").toString(), timeout ? "agent_failed" : "agent_cancelled");
            QCOMPARE(events[events.size() - 2].toMap().value("request").toMap().value("error_code").toString(), "cancelled");
        }
    }
    void adaptiveRealToolCycleAndHumanApproval() {
        ProjectController project; project.loadExample("baseline");
        FakeModel model; adaptiveModel(model);
        AgentController agent(&project, &model);
        const auto original = project.networkHash();
        agent.investigate("Avoid shutdown over the fixed horizon at low modeled cost.");
        QTRY_VERIFY_WITH_TIMEOUT(!agent.running(), 10000);
        const auto recommendation = agent.recommendation();
        QCOMPARE(recommendation["status"].toString(), "validated_recommendation");
        QCOMPARE(recommendation["verified_cost"].toDouble(), 12780.0);
        QVERIFY(recommendation["verified_feasible"].toBool());
        QCOMPARE(agent.runs().size(), 3);
        QCOMPARE(project.networkHash(), original); // Model staging cannot apply controls.
        QVERIFY(agent.canApprove());
        bool rejected = false, validated = false;
        for (const auto& value : agent.events()) {
            const auto event = value.toMap();
            rejected |= event["type"] == "candidate_rejected";
            validated |= event["type"] == "candidate_validated";
        }
        QVERIFY(rejected); QVERIFY(validated);
        for (const auto& value : model.history) {
            const auto message = value.toObject();
            if (message["role"] == "tool") QVERIFY(!message["tool_call_id"].toString().isEmpty());
        }
        project.moveAsset("tank_a", 140, 100); QVERIFY(agent.canApprove());
        QVERIFY(agent.approvePlan()); QVERIFY(!agent.canApprove()); QVERIFY(!agent.approvePlan());
        QVERIFY(project.networkHash() != original);
        QCOMPARE(agent.recommendation()["approval_status"].toString(), "operator_applied");
        QTRY_VERIFY_WITH_TIMEOUT(!project.busy(), 10000);
        QVERIFY(project.summary().contains("PASS"));
    }
    void stagedRunDifferentFromRecommendationCannotBeApplied() {
        ProjectController project; project.loadExample("baseline"); const auto original = project.networkHash();
        FakeModel model;
        model.respond = [step = 0, snapshot = QString{}, first = QString{}, second = QString{}](const QJsonArray& history) mutable -> QJsonObject {
            switch (step++) {
            case 0: return calls({call("state", "get_network_state"), call("catalog", "get_control_catalog")});
            case 1: {
                snapshot = resultFor(history, "state")["snapshot_id"].toString();
                auto args = candidateArguments(snapshot, "first", controlJson(incident_controls("mixed")));
                auto candidates = args["candidates"].toArray();
                candidates.append(QJsonObject{{"candidate_id", "second"}, {"controls", controlJson(incident_controls("mixed"))}});
                args["candidates"] = candidates;
                return calls({call("candidates", "simulate_candidates", args)});
            }
            case 2: {
                const auto runs = resultFor(history, "candidates")["runs"].toArray();
                first = runs[0].toObject()["run_id"].toString(); second = runs[1].toObject()["run_id"].toString();
                return calls({call("validate-first", "validate_run", {{"snapshot_id", snapshot}, {"run_id", first}}),
                    call("validate-second", "validate_run", {{"snapshot_id", snapshot}, {"run_id", second}}),
                    call("stage-second", "request_approval", {{"snapshot_id", snapshot}, {"run_id", second}})});
            }
            default: return conclusion(first);
            }
        };
        AgentController agent(&project, &model); agent.investigate("Review the selected run.");
        QTRY_VERIFY(!agent.running()); QVERIFY(agent.recommendation()["verified_feasible"].toBool());
        QVERIFY(!agent.canApprove()); QVERIFY(!agent.approvePlan()); QCOMPARE(project.networkHash(), original);
    }
    void skippedValidationCannotBecomeARecommendation() {
        FakeModel model; adaptiveModel(model, true);
        AgentOrchestrator session(&model);
        const auto gateway = std::make_shared<SimulationGateway>(main_pump_failure(canonical_scenario()), "physical");
        session.start(gateway, "Find a validated response.");
        QTRY_VERIFY_WITH_TIMEOUT(!session.running(), 10000);
        QVERIFY(session.result().isEmpty());
        QCOMPARE(session.events().last().toMap()["error"].toMap()["code"].toString(), "semantic_recovery_tool_limit");
        QCOMPARE(gateway->runs().size(), 3); // Completed runs survive failure.
    }
    void resetAndNumericEditsInvalidateButLayoutDoesNot() {
        ProjectController project; FakeModel model; adaptiveModel(model);
        AgentController agent(&project, &model);
        project.loadExample("baseline"); agent.investigate("Avoid shutdown.");
        QTRY_VERIFY_WITH_TIMEOUT(!agent.running(), 10000); QVERIFY(agent.canApprove());
        project.select("tank_a"); project.editSelected("capacity", "121");
        QVERIFY(!agent.canApprove()); QVERIFY(!agent.approvePlan());
        QCOMPARE(agent.recommendation()["approval_status"].toString(), "stale");
        // Reset must also stop a session when it restores an identical physical hash.
        project.reset(); model.silent = true; agent.investigate("Inspect healthy operation."); QVERIFY(agent.running());
        const auto hash = project.networkHash(); project.moveAsset("tank_a", 200, 100); QVERIFY(agent.running());
        project.reset(); QCOMPARE(project.networkHash(), hash); QVERIFY(!agent.running());
    }
    void toolAndRefinementLimitsAreEnforced() {
        FakeModel model; model.respond = [](const QJsonArray&) { return calls({call("1", "get_network_state"), call("2", "get_control_catalog")}); };
        AgentOrchestrator session(&model);
        auto gateway = std::make_shared<SimulationGateway>(canonical_scenario(), "physical");
        session.start(gateway, "Inspect.", {1, 3, 1000});
        QTRY_VERIFY(!session.running());
        QCOMPARE(session.events().last().toMap()["error"].toMap()["code"].toString(), "tool_limit");
        FakeModel refining; adaptiveModel(refining);
        AgentOrchestrator limited(&refining);
        limited.start(std::make_shared<SimulationGateway>(main_pump_failure(canonical_scenario()), "incident"), "Investigate.", {12, 1, 10000});
        QTRY_VERIFY_WITH_TIMEOUT(!limited.running(), 10000);
        bool blocked = false;
        for (const auto& value : limited.events()) blocked |= value.toMap()["error"].toMap()["code"].toString() == "refinement_limit";
        QVERIFY(blocked);
    }
    void exhaustedRefinementKeepsEvidenceToolsAvailableAfterRejectedAttempt() {
        FakeModel model;
        QString snapshot, selected;
        QStringList afterAttempt;
        bool attemptRejected = false;
        int step = 0;
        model.respond = [&](const QJsonArray& history) -> QJsonObject {
            switch (step++) {
            case 0: return calls({call("state", "get_network_state"), call("catalog", "get_control_catalog")});
            case 1:
                snapshot = resultFor(history, "state")["snapshot_id"].toString();
                // An invalid candidate attempt still consumes the bounded refinement quota.
                return calls({call("attempt", "simulate_candidates", {{"snapshot_id", snapshot}, {"candidates", QJsonArray{}}})});
            case 2:
                attemptRejected = !resultFor(history, "attempt")["ok"].toBool(true);
                afterAttempt = toolNames(model.tools);
                return calls({call("baseline", "forecast_baseline", {{"snapshot_id", snapshot}})});
            case 3:
                selected = resultFor(history, "baseline")["run"].toObject()["run_id"].toString();
                return calls({call("validate", "validate_run", {{"snapshot_id", snapshot}, {"run_id", selected}})});
            case 4: return calls({call("stage", "request_approval", {{"snapshot_id", snapshot}, {"run_id", selected}})});
            default: return conclusion(selected);
            }
        };
        const auto gateway = std::make_shared<SimulationGateway>(canonical_scenario(), "healthy-budget");
        const auto original = gateway->snapshot();
        AgentOrchestrator session(&model);
        session.start(gateway, "Inspect and validate unchanged operation when appropriate.", {12, 1, 10000});
        QTRY_VERIFY_WITH_TIMEOUT(!session.running(), 10000);
        QVERIFY(attemptRejected);
        QVERIFY(!afterAttempt.contains("simulate_candidates"));
        QVERIFY(!afterAttempt.contains("search_interventions"));
        for (const auto& name : {"forecast_baseline", "validate_run", "compare_runs", "request_approval"})
            QVERIFY2(afterAttempt.contains(name), name);
        QVERIFY(session.result()["verified_feasible"].toBool());
        QCOMPARE(session.result()["selected_run_id"].toString(), selected);
        QCOMPARE(session.result()["approval_status"].toString(), "pending_operator");
        QCOMPARE(gateway->runs().size(), 1);
        QCOMPARE(gateway->snapshot(), original);
    }
    void exhaustedToolBudgetAllowsGroundedFinalCompletionWithoutMoreTools() {
        FakeModel model;
        QString snapshot, selected;
        bool finalToolsEmpty = false;
        int step = 0;
        model.respond = [&](const QJsonArray& history) -> QJsonObject {
            switch (step++) {
            case 0: return calls({call("state", "get_network_state"), call("catalog", "get_control_catalog")});
            case 1:
                snapshot = resultFor(history, "state")["snapshot_id"].toString();
                return calls({call("candidate", "simulate_candidates", candidateArguments(snapshot, "mixed", controlJson(incident_controls("mixed"))))});
            case 2:
                selected = resultFor(history, "candidate")["runs"].toArray().first().toObject()["run_id"].toString();
                return calls({call("validate", "validate_run", {{"snapshot_id", snapshot}, {"run_id", selected}})});
            case 3: return calls({call("stage", "request_approval", {{"snapshot_id", snapshot}, {"run_id", selected}})});
            default:
                finalToolsEmpty = model.tools.isEmpty();
                return conclusion(selected);
            }
        };
        const auto gateway = std::make_shared<SimulationGateway>(main_pump_failure(canonical_scenario()), "incident-budget");
        const auto original = gateway->snapshot();
        AgentOrchestrator session(&model);
        session.start(gateway, "Find a validated response and stage it for human review.", {5, 1, 10000});
        QTRY_VERIFY_WITH_TIMEOUT(!session.running(), 10000);
        QVERIFY(finalToolsEmpty);
        QCOMPARE(model.requests, 5);
        QCOMPARE(model.toolsPerRequest.size(), 5);
        QVERIFY(model.toolsPerRequest.last().isEmpty());
        QStringList executed;
        for (const auto& value : session.events()) {
            const auto event = value.toMap();
            if (event["type"] == "tool_completed") executed.append(event["tool_name"].toString());
            QVERIFY(event["type"] != "tool_failed");
        }
        QCOMPARE(executed, (QStringList{"get_network_state", "get_control_catalog", "simulate_candidates", "validate_run", "request_approval"}));
        QCOMPARE(session.result()["status"].toString(), "validated_recommendation");
        QCOMPARE(session.result()["selected_run_id"].toString(), selected);
        QCOMPARE(session.result()["verified_cost"].toDouble(), 12780.0);
        QVERIFY(session.result()["verified_feasible"].toBool());
        QVERIFY(!session.result()["validation_id"].toString().isEmpty());
        QCOMPARE(session.result()["approval_status"].toString(), "pending_operator");
        QCOMPARE(gateway->stagedPlan()["selected_run_id"].toString(), selected);
        QCOMPARE(gateway->runs().size(), 1);
        QCOMPARE(gateway->snapshot(), original);
    }
    void oneFormatCorrectionAfterExhaustedTools_data() {
        QTest::addColumn<QString>("malformed");
        QTest::newRow("prose") << "private reasoning must not be retained";
        QTest::newRow("duplicate-key") << "{\"selected_run_id\":\"invented\",\"selected_run_id\":null,\"explanation\":\"Duplicate key\"}";
        QTest::newRow("extra-field") << "{\"selected_run_id\":null,\"explanation\":\"No plan\",\"cost\":0}";
        QTest::newRow("wrong-type") << "{\"selected_run_id\":7,\"explanation\":\"No plan\"}";
        QTest::newRow("oversized") << QString(65537,'x');
    }
    void oneFormatCorrectionAfterExhaustedTools() {
        QFETCH(QString,malformed); FakeModel model; int step=0;
        model.respond=[&](const QJsonArray&)->QJsonObject {
            if (step++==0) return calls({call("state","get_network_state"),call("catalog","get_control_catalog")});
            if (step==2) return {{"content",malformed}};
            return conclusion();
        };
        AgentOrchestrator session(&model); const auto gateway=std::make_shared<SimulationGateway>(canonical_scenario(),"format-check");
        session.start(gateway,"Inspect, then finish.",{2,1,10000});
        QTRY_VERIFY_WITH_TIMEOUT(!session.running(),10000);
        QCOMPARE(session.result().value("status").toString(),QString("no_validated_plan"));
        QCOMPARE(model.requests,3); QCOMPARE(model.finalJsonRequests,2); QVERIFY(gateway->runs().isEmpty());
        int corrections=0,executed=0;
        for (const auto& row:session.events()) { const auto e=row.toMap(); corrections+=e.value("type")=="final_format_correction_requested"; executed+=e.value("type")=="tool_completed"; }
        QCOMPARE(corrections,1); QCOMPARE(executed,2);
        QVERIFY(!QJsonDocument(model.history).toJson().contains("private reasoning must not be retained"));
        QVERIFY(!QJsonDocument(QJsonArray::fromVariantList(session.events())).toJson().contains("private reasoning must not be retained"));
    }
    void formatCorrectionCannotExecuteToolsOrLoop() {
        for (const bool tools:{false,true}) {
            FakeModel model; int step=0;
            model.respond=[&](const QJsonArray&)->QJsonObject {
                if (step++==0) return calls({call("state","get_network_state"),call("catalog","get_control_catalog")});
                if (step==2 || !tools) return {{"content","malformed"}};
                return calls({call("extra","forecast_baseline",{{"snapshot_id","fake"}})});
            };
            AgentOrchestrator session(&model); const auto gateway=std::make_shared<SimulationGateway>(canonical_scenario(),"repair-bounds");
            session.start(gateway,"Inspect."); QTRY_VERIFY_WITH_TIMEOUT(!session.running(),10000);
            QVERIFY(session.result().isEmpty()); QVERIFY(gateway->runs().isEmpty()); QCOMPARE(model.requests,3);
            QCOMPARE(model.finalJsonRequests,1); QCOMPARE(session.events().last().toMap().value("error").toMap().value("code").toString(),QString("invalid_recommendation"));
        }
    }
    void repairedReferenceStillRequiresIndependentEvidence() {
        FakeModel model; int step=0;
        model.respond=[&](const QJsonArray&)->QJsonObject {
            if (step++==0) return calls({call("state","get_network_state"),call("catalog","get_control_catalog")});
            return step==2 ? QJsonObject{{"content","bad final"}}:conclusion("invented");
        };
        AgentOrchestrator session(&model); session.start(std::make_shared<SimulationGateway>(canonical_scenario(),"strict-repair"),"Inspect.");
        QTRY_VERIFY_WITH_TIMEOUT(!session.running(),10000); QVERIFY(session.result().isEmpty()); QCOMPARE(model.requests,3);
        QCOMPARE(session.events().last().toMap().value("error").toMap().value("code").toString(),QString("ungrounded_recommendation"));
    }
    void truncatedResponseUsesOnlyExistingFinalCorrection_data() {
        QTest::addColumn<QString>("secondFailure");
        QTest::newRow("recover-final")<<QString{};
        QTest::newRow("second-truncation-fails")<<QString("response_truncated");
        QTest::newRow("authentication-is-fatal")<<QString("authentication_failed");
    }
    void truncatedResponseUsesOnlyExistingFinalCorrection() {
        QFETCH(QString,secondFailure);
        FakeModel model; model.failures[2]="response_truncated";
        if(!secondFailure.isEmpty()) model.failures[3]=secondFailure;
        model.respond=[&](const QJsonArray&)->QJsonObject {
            if(model.requests==1) return calls({call("state","get_network_state"),call("catalog","get_control_catalog")});
            return conclusion();
        };
        auto gateway=std::make_shared<SimulationGateway>(canonical_scenario(),"truncated");
        const auto before=gateway->snapshot(); AgentOrchestrator session(&model);
        session.start(gateway,"Inspect."); QTRY_VERIFY_WITH_TIMEOUT(!session.running(),10000);
        QCOMPARE(model.requests,3); QCOMPARE(model.finalJsonRequests,1); QVERIFY(model.toolsPerRequest.last().isEmpty());
        QVERIFY(gateway->runs().isEmpty()); QCOMPARE(gateway->snapshot(),before);
        if(secondFailure.isEmpty()) QCOMPARE(session.result().value("status").toString(),QString("no_validated_plan"));
        else { QVERIFY(session.result().isEmpty()); QCOMPARE(session.events().last().toMap().value("error").toMap().value("code").toString(),secondFailure); }
        if(secondFailure.isEmpty()) QVERIFY(!QJsonDocument::fromVariant(session.events()).toJson().contains("Private response"));
    }
    void truncatedResponseCannotBypassInspectionOrCancellation() {
        for(const bool inspected:{false,true}) {
            FakeModel model; model.failures[inspected?2:1]="response_truncated";
            model.respond=[](const QJsonArray&){return calls({call("state","get_network_state"),call("catalog","get_control_catalog")});};
            AgentOrchestrator session(&model);
            connect(&session,&AgentOrchestrator::activity,&session,[&](const QJsonObject& e){
                if(inspected && e.value("type")=="final_format_correction_requested") session.cancel();
            });
            session.start(std::make_shared<SimulationGateway>(canonical_scenario(),"truncated-cancel"),"Inspect.");
            QTRY_VERIFY_WITH_TIMEOUT(!session.running(),10000); QTest::qWait(20);
            QVERIFY(session.result().isEmpty()); QCOMPARE(model.requests,inspected?2:1); QCOMPARE(model.finalJsonRequests,0);
        }
    }
    void truncatedCorrectionRetainsOverallDeadline() {
        FakeModel model; model.failures[2]="response_truncated";
        model.respond=[](const QJsonArray&){return calls({call("state","get_network_state"),call("catalog","get_control_catalog")});};
        AgentOrchestrator session(&model);
        connect(&session,&AgentOrchestrator::activity,&session,[&](const QJsonObject& e){
            if(e.value("type")=="final_format_correction_requested") model.silent=true;
        });
        session.start(std::make_shared<SimulationGateway>(canonical_scenario(),"truncated-deadline"),"Inspect.",{12,3,300});
        QTRY_VERIFY_WITH_TIMEOUT(!session.running(),2000); QCOMPARE(model.requests,3); QCOMPARE(model.finalJsonRequests,1);
        QCOMPARE(session.events().last().toMap().value("error").toMap().value("code").toString(),QString("deadline_exceeded"));
        emit model.replyReady(conclusion()); QVERIFY(session.result().isEmpty());
    }
    void completionLedgerUsesOnlyExactValidatedCurrentRuns() {
        FakeModel model; QString run,snapshot; QJsonObject unvalidated,validated,staged;
        model.respond=[&](const QJsonArray& messages)->QJsonObject {
            switch(model.requests) {
            case 1:return calls({call("state","get_network_state"),call("catalog","get_control_catalog")});
            case 2:
                snapshot=resultFor(messages,"state").value("snapshot_id").toString();
                return calls({call("candidate","simulate_candidates",candidateArguments(snapshot,"mixed",controlJson(incident_controls("mixed"))))});
            case 3:
                run=resultFor(messages,"candidate").value("runs").toArray().first().toObject().value("run_id").toString();
                unvalidated=completionLedger(messages);
                return calls({call("validate","validate_run",{{"snapshot_id",snapshot},{"run_id",run}})});
            case 4:validated=completionLedger(messages);return calls({call("stage","request_approval",{{"snapshot_id",snapshot},{"run_id",run}})});
            default:staged=completionLedger(messages);return conclusion(run);
            }
        };
        auto gateway=std::make_shared<SimulationGateway>(main_pump_failure(canonical_scenario()),"ledger");
        const auto original=gateway->snapshot(); AgentOrchestrator session(&model);session.start(gateway,"Inspect.");
        QTRY_VERIFY_WITH_TIMEOUT(!session.running(),10000);
        QVERIFY(unvalidated.value("eligible_validated_runs").toArray().isEmpty());
        const auto rows=validated.value("eligible_validated_runs").toArray();QCOMPARE(rows.size(),1);
        QCOMPARE(validated.value("snapshot_id").toString(),snapshot);QCOMPARE(rows[0].toObject().value("run_id").toString(),run);
        QCOMPARE(rows[0].toObject().value("validation_id"),gateway->recommendation(run).value("validation_id"));
        QVERIFY(validated.value("staged_run_id").isNull());QCOMPARE(staged.value("staged_run_id").toString(),run);
        QCOMPARE(gateway->runs().size(),1);QCOMPARE(gateway->snapshot(),original);
        QCOMPARE(session.result().value("status").toString(),QString("validated_recommendation"));
    }
    void formatCorrectionKeepsDeadlineAndCancellation() {
        for (const bool cancelAtCorrection:{false,true}) {
            FakeModel model; int step=0;
            model.respond=[&](const QJsonArray&)->QJsonObject {
                if (step++==0) return calls({call("state","get_network_state"),call("catalog","get_control_catalog")});
                model.silent=true; return {{"content","malformed"}};
            };
            AgentOrchestrator session(&model); bool correctionRequested = false;
            connect(&session,&AgentOrchestrator::activity,&session,[&](const QJsonObject& e) {
                if (e.value("type")=="final_format_correction_requested") {
                    correctionRequested = true;
                    if (cancelAtCorrection) session.cancel();
                }
            });
            const auto gateway = std::make_shared<SimulationGateway>(canonical_scenario(),"repair-retirement");
            constexpr int originalDeadlineMs = 10000;
            QElapsedTimer elapsed; elapsed.start();
            session.start(gateway,"Inspect.",{12,3,originalDeadlineMs});
            // Reach the correction before deliberately blocking timer delivery.
            // Cold worker/thread startup under sanitizer load can exceed 1 s.
            QTRY_VERIFY_WITH_TIMEOUT(model.requests >= (cancelAtCorrection ? 2 : 3) || !session.running(),15000);
            QVERIFY(correctionRequested);
            if (!cancelAtCorrection) {
                QCOMPARE(model.requests,3); QVERIFY(session.running());
                // Delay timer delivery, then send an otherwise valid correction.
                // It must still obey the original absolute session deadline.
                QTest::qSleep(static_cast<int>(std::max<qint64>(1,originalDeadlineMs+25-elapsed.elapsed())));
                emit model.replyReady(conclusion());
            }
            QVERIFY(!session.running()); QVERIFY(session.result().isEmpty());
            if (cancelAtCorrection) { QCOMPARE(model.requests,2); QCOMPARE(session.events().last().toMap().value("type").toString(),QString("agent_cancelled")); }
            else { QCOMPARE(model.requests,3); QCOMPARE(session.events().last().toMap().value("error").toMap().value("code").toString(),QString("deadline_exceeded")); }
            emit model.replyReady(conclusion()); QVERIFY(session.result().isEmpty());
        }
    }
    void duplicateToolArgumentsCannotBecomeNumericalInputs() {
        FakeModel model; int step=0;
        model.respond=[&](const QJsonArray& history)->QJsonObject {
            if (step++==0) return calls({call("state","get_network_state"),call("catalog","get_control_catalog")});
            if (step==2) {
                const auto id=resultFor(history,"state").value("snapshot_id").toString();
                auto c=call("duplicate","forecast_baseline"); auto f=c.value("function").toObject();
                f["arguments"]=QString("{\"snapshot_id\":\"%1\",\"snapshot_id\":\"%1\"}").arg(id); c["function"]=f; return calls({c});
            }
            return conclusion();
        };
        AgentOrchestrator session(&model); const auto gateway=std::make_shared<SimulationGateway>(canonical_scenario(),"duplicate-arguments");
        session.start(gateway,"Inspect."); QTRY_VERIFY_WITH_TIMEOUT(!session.running(),10000);
        QVERIFY(gateway->runs().isEmpty()); QCOMPARE(resultFor(model.history,"duplicate").value("error").toObject().value("code").toString(),QString("invalid_arguments"));
    }
    void timeoutCancelAndLateResponseCannotLeakResults() {
        FakeModel model; model.silent = true;
        AgentOrchestrator session(&model);
        auto gateway = std::make_shared<SimulationGateway>(canonical_scenario(), "physical");
        session.start(gateway, "Inspect.", {12, 3, 15});
        QTRY_VERIFY_WITH_TIMEOUT(!session.running(), 1000);
        QCOMPARE(session.events().last().toMap()["error"].toMap()["code"].toString(), "deadline_exceeded");
        emit model.replyReady(conclusion("invented")); QVERIFY(session.result().isEmpty());
        session.start(std::make_shared<SimulationGateway>(canonical_scenario(), "new"), "Inspect.");
        session.cancel(); QVERIFY(!session.running());
        QCOMPARE(session.events().last().toMap()["type"].toString(), "agent_cancelled");
    }
    void lateModelFinalCannotBeatAnOverdueDeadline() {
        FakeModel model; model.silent = true;
        const auto gateway = std::make_shared<SimulationGateway>(canonical_scenario(), "late-final");
        const auto snapshot = gateway->snapshotId();
        const auto run = gateway->execute("forecast_baseline", {{"snapshot_id", snapshot}}).value("run").toObject().value("run_id").toString();
        QVERIFY(!run.isEmpty());
        QVERIFY(gateway->execute("validate_run", {{"snapshot_id", snapshot}, {"run_id", run}}).value("ok").toBool());
        AgentOrchestrator session(&model); QElapsedTimer elapsed; elapsed.start();
        session.start(gateway, "Inspect then finish.", {12, 3, 1000});
        emit model.replyReady(calls({call("state", "get_network_state"), call("catalog", "get_control_catalog")}));
        QTRY_COMPARE_WITH_TIMEOUT(model.requests, 2, 10000);
        QVERIFY(session.running());
        // Keep the event loop blocked so the elapsed deadline, not timer delivery,
        // decides whether an otherwise grounded final can publish.
        QTest::qSleep(static_cast<int>(std::max<qint64>(1, 1025 - elapsed.elapsed())));
        emit model.replyReady(conclusion(run));
        QVERIFY(!session.running()); QVERIFY(session.result().isEmpty());
        QCOMPARE(session.events().last().toMap().value("error").toMap().value("code").toString(), QString("deadline_exceeded"));
        QCOMPARE(gateway->runs().size(), 1);
    }
    void deadlineBlocksDispatchAfterDelayedActivity_data() {
        QTest::addColumn<QString>("delayedEvent");
        QTest::newRow("requested") << "tool_requested";
        QTest::newRow("started") << "tool_started";
        QTest::newRow("completed-before-next-call") << "tool_completed";
    }
    void deadlineBlocksDispatchAfterDelayedActivity() {
        QFETCH(QString, delayedEvent);
        FakeModel model; model.silent = true;
        const auto gateway = std::make_shared<SimulationGateway>(canonical_scenario(), "delayed-dispatch");
        AgentOrchestrator session(&model); bool delayed = false;
        connect(&session, &AgentOrchestrator::activity, &session, [&](const QJsonObject& event) {
            if (!delayed && event.value("type") == delayedEvent) { delayed = true; QTest::qSleep(1025); }
        });
        session.start(gateway, "Inspect within the original deadline.", {12, 3, 1000});
        if (delayedEvent == "tool_completed")
            emit model.replyReady(calls({call("state", "get_network_state"), call("late", "forecast_baseline", {{"snapshot_id", gateway->snapshotId()}})}));
        else emit model.replyReady(calls({call("late", "forecast_baseline", {{"snapshot_id", gateway->snapshotId()}})}));
        QTRY_VERIFY_WITH_TIMEOUT(!session.running(), 10000);
        QTest::qWait(50); // A wrongly dispatched native result must not appear later.
        QVERIFY(delayed); QVERIFY(gateway->runs().isEmpty()); QVERIFY(session.result().isEmpty());
        QCOMPARE(model.requests, 1);
        QCOMPARE(session.events().last().toMap().value("error").toMap().value("code").toString(), QString("deadline_exceeded"));
    }
    void transportLossFailsImmediatelyAndRetainsCompletedRuns() {
        auto* model = new FakeModel; model->silent = true;
        const auto gateway = std::make_shared<SimulationGateway>(canonical_scenario(), "transport-loss");
        QVERIFY(gateway->execute("forecast_baseline", {{"snapshot_id", gateway->snapshotId()}}).value("ok").toBool());
        AgentOrchestrator session(model); session.start(gateway, "Inspect the retained snapshot.");
        delete model;
        QVERIFY(!session.running()); QVERIFY(session.result().isEmpty());
        QCOMPARE(session.events().last().toMap().value("error").toMap().value("code").toString(), QString("transport_unavailable"));
        QCOMPARE(gateway->runs().size(), 1);
        session.cancel(); // Cleanup must not call the destroyed transport.
        session.start(std::make_shared<SimulationGateway>(canonical_scenario(), "missing-transport"), "Start again.");
        QVERIFY(!session.running());
        QCOMPARE(session.events().last().toMap().value("error").toMap().value("code").toString(), QString("transport_unavailable"));
    }
    void transportLossDuringActivityPreventsPendingDispatch() {
        auto* model = new FakeModel; model->silent = true;
        const auto gateway = std::make_shared<SimulationGateway>(canonical_scenario(), "transport-dispatch-loss");
        AgentOrchestrator session(model);
        connect(&session, &AgentOrchestrator::activity, &session, [&](const QJsonObject& event) {
            if (event.value("type") == "tool_started") { delete model; model = nullptr; }
        });
        session.start(gateway, "Inspect.");
        emit model->replyReady(calls({call("late", "forecast_baseline", {{"snapshot_id", gateway->snapshotId()}})}));
        QVERIFY(!session.running()); QVERIFY(gateway->runs().isEmpty()); QVERIFY(session.result().isEmpty());
        QCOMPARE(session.events().last().toMap().value("error").toMap().value("code").toString(), QString("transport_unavailable"));
    }
    void localNativeSearchSurvivesTransportLoss() {
        auto* model = new FakeModel; model->silent = true;
        const auto gateway = std::make_shared<SimulationGateway>(canonical_scenario(), "local-transport-loss");
        AgentOrchestrator session(model);
        connect(&session, &AgentOrchestrator::activity, &session, [&](const QJsonObject& event) {
            if (model && event.value("type") == "tool_started") {
                QCOMPARE(model->requests, 0); delete model; model = nullptr;
            }
        });
        session.start(gateway, "Run native finite search.", {}, true);
        QTRY_VERIFY_WITH_TIMEOUT(!session.running(), 10000);
        QVERIFY(!model); QVERIFY(!gateway->runs().isEmpty());
        QCOMPARE(session.result().value("source_mode").toString(), QString("local_native"));
        QCOMPARE(session.events().last().toMap().value("type").toString(), QString("recommendation_ready"));
    }
    void malformedUnknownAndReusedToolCalls() {
        FakeModel model;
        model.respond = [](const QJsonArray&) { return calls({call("duplicate", "get_network_state"), call("duplicate", "get_control_catalog")}); };
        AgentOrchestrator session(&model);
        session.start(std::make_shared<SimulationGateway>(canonical_scenario(), "physical"), "Inspect.");
        QTRY_VERIFY(!session.running());
        QCOMPARE(session.events().last().toMap()["error"].toMap()["code"].toString(), "malformed_tool_calls");
        model.respond = [step = 0](const QJsonArray&) mutable {
            if (step++ == 0) {
                auto malformed = call("bad-json", "get_network_state");
                auto function = malformed["function"].toObject(); function["arguments"] = "{broken"; malformed["function"] = function;
                return calls({malformed, call("shell", "run_shell", {{"command", "arbitrary"}})});
            }
            return conclusion();
        };
        session.start(std::make_shared<SimulationGateway>(canonical_scenario(), "new"), "Inspect.");
        QTRY_VERIFY(!session.running());
        QCOMPARE(resultFor(model.history, "bad-json")["error"].toObject()["code"].toString(), "invalid_arguments");
        QCOMPARE(resultFor(model.history, "shell")["error"].toObject()["code"].toString(), "unknown_tool");
        QVERIFY(session.result().isEmpty());
    }
    void noValidatedPlanAndFabricatedRunAreExplicit() {
        FakeModel model;
        model.respond = [step = 0](const QJsonArray&) mutable { return step++ == 0 ? calls({call("s", "get_network_state"), call("c", "get_control_catalog")}) : conclusion(); };
        AgentOrchestrator session(&model);
        session.start(std::make_shared<SimulationGateway>(canonical_scenario(), "physical"), "Investigate.");
        QTRY_VERIFY(!session.running()); QCOMPARE(session.result()["status"].toString(), "no_validated_plan");
        QVERIFY(!session.result()["feasible"].toBool(true));
        model.respond = [step = 0](const QJsonArray&) mutable { return step++ == 0 ? calls({call("s", "get_network_state"), call("c", "get_control_catalog")}) : conclusion("invented"); };
        session.start(std::make_shared<SimulationGateway>(canonical_scenario(), "new"), "Investigate.");
        QTRY_VERIFY(!session.running()); QVERIFY(session.result().isEmpty());
    }
    void cancellationAndRestartDuringActivityPreventOldDispatch() {
        FakeModel model;
        AgentOrchestrator session(&model);
        auto old = std::make_shared<SimulationGateway>(canonical_scenario(), "old");
        model.respond = [old](const QJsonArray&) { return calls({call("old-call", "forecast_baseline", {{"snapshot_id", old->snapshotId()}})}); };
        const auto cancelConnection = connect(&session, &AgentOrchestrator::activity, &session, [&](const QJsonObject& event) {
            if (event["type"] == "tool_started") session.cancel();
        });
        session.start(old, "Cancel immediately before execution.");
        QTRY_VERIFY(!session.running()); QTest::qWait(50);
        QVERIFY(old->runs().isEmpty());
        disconnect(cancelConnection);
        const auto interrupted = std::make_shared<SimulationGateway>(canonical_scenario(), "interrupted");
        model.respond = [interrupted](const QJsonArray&) { return calls({call("old-call", "forecast_baseline", {{"snapshot_id", interrupted->snapshotId()}})}); };
        auto replacement = std::make_shared<SimulationGateway>(canonical_scenario(), "replacement");
        bool restarted = false;
        connect(&session, &AgentOrchestrator::activity, &session, [&](const QJsonObject& event) {
            if (!restarted && event["type"] == "tool_requested") {
                restarted = true;
                model.respond = [step = 0](const QJsonArray&) mutable {
                    return step++ == 0 ? calls({call("new-state", "get_network_state"), call("new-catalog", "get_control_catalog")}) : conclusion();
                };
                session.start(replacement, "Replacement investigation.");
            }
        });
        session.start(interrupted, "Old investigation.");
        QTRY_VERIFY(!session.running()); QVERIFY(restarted);
        QVERIFY(session.gateway() == replacement);
        QCOMPARE(session.result()["status"].toString(), "no_validated_plan");
        QVERIFY(interrupted->runs().isEmpty()); QVERIFY(replacement->runs().isEmpty());
        QVERIFY(resultFor(model.history, "old-call").isEmpty());
    }
    void modelUsesDiscoveredControlIdsOnADifferentGraph() {
        Network network;
        network.assets = {{"west", "West", AssetKind::tank, 100.0, 50.0},
            {"east", "East", AssetKind::tank, 100.0, 50.0}, {"north", "North", AssetKind::tank, 100.0, 50.0},
            {"plant", "Plant", AssetKind::treatment, 10.0}};
        network.routes = {{"west-east", "west", "east", 10.0, 1.0}, {"east-north", "east", "north", 10.0, 1.0},
            {"north-plant", "north", "plant", 10.0, 1.0}};
        FakeModel model;
        model.respond = [step = 0, snapshot = QString{}, run = QString{}](const QJsonArray& history) mutable -> QJsonObject {
            switch (step++) {
            case 0: return calls({call("state", "get_network_state"), call("catalog", "get_control_catalog")});
            case 1: {
                snapshot = resultFor(history, "state")["snapshot_id"].toString();
                const auto control = resultFor(history, "catalog")["controls"].toArray().first().toObject();
                const QJsonArray overrides{QJsonObject{{"control_id", control["control_id"]}, {"value", control["value"]}}};
                return calls({call("proposal", "simulate_candidates", candidateArguments(snapshot, "discovered", overrides))});
            }
            case 2:
                run = resultFor(history, "proposal")["runs"].toArray().first().toObject()["run_id"].toString();
                return calls({call("validate", "validate_run", {{"snapshot_id", snapshot}, {"run_id", run}})});
            default: return conclusion(run);
            }
        };
        AgentOrchestrator session(&model);
        session.start(std::make_shared<SimulationGateway>(network, "custom-graph"), "Evaluate an intervention using available controls.");
        QTRY_VERIFY(!session.running());
        QVERIFY(session.result()["verified_feasible"].toBool());
        QCOMPARE(session.result()["tanks"].toArray().size(), 3);
        QVERIFY(!QJsonDocument(session.result()).toJson().contains("tank_a"));
    }
    void cancellationCallbackCannotBeOverwrittenByOuterStart() {
        FakeModel model; model.silent = true;
        AgentOrchestrator session(&model);
        session.start(std::make_shared<SimulationGateway>(canonical_scenario(), "first"), "First.");
        const auto replacement = std::make_shared<SimulationGateway>(canonical_scenario(), "replacement");
        connect(&session, &AgentOrchestrator::activity, &session, [&](const QJsonObject& event) {
            if (event["type"] == "agent_cancelled") {
                model.silent = false;
                model.respond = [step = 0](const QJsonArray&) mutable {
                    return step++ == 0 ? calls({call("state", "get_network_state"), call("catalog", "get_control_catalog")}) : conclusion();
                };
                session.start(replacement, "Replacement from cancellation handler.");
            }
        });
        session.start(std::make_shared<SimulationGateway>(canonical_scenario(), "outer"), "Outer start.");
        QTRY_VERIFY(!session.running());
        QVERIFY(session.gateway() == replacement);
        QCOMPARE(session.result()["status"].toString(), "no_validated_plan");
    }
    void nativeSearchToolProducesStoredValidatedRecommendations() {
        FakeModel model; QString selected, searchValidation;
        bool validationOfferedAfterStage = false, ledgerRequiresExplicit = false, finalToolsClosed = false;
        model.respond = [&, step = 0, snapshot = QString{}](const QJsonArray& history) mutable -> QJsonObject {
            switch (step++) {
            case 0: return calls({call("state", "get_network_state"), call("catalog", "get_control_catalog")});
            case 1: {
                snapshot = resultFor(history, "state")["snapshot_id"].toString();
                const QJsonArray axes{
                    QJsonObject{{"control_id", "route:bypass:flow"}, {"values", QJsonArray{0, 5, 10, 15, 20, 25}}},
                    QJsonObject{{"control_id", "route:backup_pump:flow"}, {"values", QJsonArray{16, 25, 35}}},
                    QJsonObject{{"control_id", "route:truck_route:flow"}, {"values", QJsonArray{0, 5, 10, 15, 20, 22}}},
                    QJsonObject{{"control_id", "source:source_a:curtail"}, {"values", QJsonArray{0, 5, 10, 15}}}
                };
                return calls({call("search", "search_interventions", {{"snapshot_id", snapshot}, {"axes", axes}, {"result_limit", 5}})});
            }
            case 2: {
                const auto runs = resultFor(history, "search")["runs"].toArray();
                if (runs.isEmpty()) return conclusion();
                selected = runs.first().toObject()["run_id"].toString();
                searchValidation = runs.first().toObject()["validation_id"].toString();
                return calls({call("stage", "request_approval", {{"snapshot_id", snapshot}, {"run_id", selected}})});
            }
            case 3:
                validationOfferedAfterStage = toolNames(model.tools).contains("validate_run");
                for (const auto& row : completionLedger(history).value("eligible_validated_runs").toArray())
                    if (row.toObject().value("run_id").toString() == selected)
                        ledgerRequiresExplicit = row.toObject().value("explicit_validate_run_required").toBool();
                return calls({call("selected-validation", "validate_run", {{"snapshot_id", snapshot}, {"run_id", selected}})});
            default:
                finalToolsClosed = model.tools.isEmpty();
                return conclusion(selected);
            }
        };
        AgentOrchestrator session(&model);
        const auto gateway = std::make_shared<SimulationGateway>(main_pump_failure(canonical_scenario()), "search-incident");
        const auto original = gateway->snapshot(); session.start(gateway, "Find a low-cost validated response.");
        QTRY_VERIFY_WITH_TIMEOUT(!session.running(), 10000);
        QVERIFY(validationOfferedAfterStage); QVERIFY(ledgerRequiresExplicit); QVERIFY(finalToolsClosed);
        QCOMPARE(model.requests, 5); QCOMPARE(model.finalJsonRequests, 1);
        QCOMPARE(resultFor(model.history, "search")["diagnostics"].toObject()["candidates_evaluated"].toInt(), 432);
        const auto stage = resultFor(model.history, "stage").value("plan").toObject();
        QCOMPARE(stage.value("validation_source").toString(), QString("native_search_dense_reference"));
        QCOMPARE(stage.value("validation_id").toString(), searchValidation);
        const auto checked = resultFor(model.history, "selected-validation").value("run").toObject();
        QCOMPARE(checked.value("run_id").toString(), selected);
        QCOMPARE(checked.value("validation_source").toString(), QString("validate_run"));
        QVERIFY(checked.value("validation_id").toString() != searchValidation);
        QStringList executed;
        for (const auto& row : session.events()) if (row.toMap().value("type") == "tool_completed") executed.append(row.toMap().value("tool_name").toString());
        QCOMPARE(executed, (QStringList{"get_network_state", "get_control_catalog", "search_interventions", "request_approval", "validate_run"}));
        QVERIFY(session.result()["verified_feasible"].toBool());
        QCOMPARE(session.result().value("validation_source").toString(), QString("validate_run"));
        QCOMPARE(session.result()["verified_cost"].toDouble(), 4800.0);
        QCOMPARE(session.result()["approval_status"].toString(), "pending_operator");
        QVERIFY(!session.result()["validation_id"].toString().isEmpty());
        QVERIFY(session.result()["warnings"].toArray().size() > 0);
        QCOMPARE(gateway->snapshot(), original);
    }
    void hostedSearchSelectionRequiresItsOwnExplicitValidation_data() {
        QTest::addColumn<bool>("stageBeforeFinal");
        QTest::newRow("search-final") << false;
        QTest::newRow("search-stage-final") << true;
    }
    void hostedSearchSelectionRequiresItsOwnExplicitValidation() {
        QFETCH(bool, stageBeforeFinal);
        FakeModel model; QString snapshot, selected, other;
        bool validationOffered = false; int step = 0;
        model.respond = [&](const QJsonArray& history) -> QJsonObject {
            switch (step++) {
            case 0: return calls({call("state", "get_network_state"), call("catalog", "get_control_catalog")});
            case 1:
                snapshot = resultFor(history, "state").value("snapshot_id").toString();
                return calls({call("baseline", "forecast_baseline", {{"snapshot_id", snapshot}})});
            case 2:
                other = resultFor(history, "baseline").value("run").toObject().value("run_id").toString();
                return calls({call("other-validation", "validate_run", {{"snapshot_id", snapshot}, {"run_id", other}})});
            case 3: return calls({call("search", "search_interventions", {{"snapshot_id", snapshot}, {"axes", QJsonArray{}}, {"result_limit", 1}})});
            case 4: {
                const auto runs = resultFor(history, "search").value("runs").toArray();
                if (runs.isEmpty()) return conclusion();
                selected = runs.first().toObject().value("run_id").toString();
                if (stageBeforeFinal) return calls({call("stage", "request_approval", {{"snapshot_id", snapshot}, {"run_id", selected}})});
                [[fallthrough]];
            }
            default:
                validationOffered = toolNames(model.tools).contains("validate_run");
                return conclusion(selected);
            }
        };
        const auto gateway = std::make_shared<SimulationGateway>(canonical_scenario(), "own-search-validation");
        const auto original = gateway->snapshot(); AgentOrchestrator session(&model);
        session.start(gateway, "Compare the retained runs and select a response with its own explicit check.");
        QTRY_VERIFY_WITH_TIMEOUT(!session.running(), 10000);
        QVERIFY(validationOffered); QVERIFY(!selected.isEmpty()); QVERIFY(selected != other);
        QCOMPARE(model.requests, stageBeforeFinal ? 7 : 6); QCOMPARE(model.finalJsonRequests, 0);
        QVERIFY(session.result().isEmpty());
        QCOMPARE(session.events().last().toMap().value("error").toMap().value("code").toString(), QString("explicit_validation_required"));
        const auto native = gateway->recommendation(selected);
        QVERIFY(native.value("ok").toBool());
        QCOMPARE(native.value("validation_status").toString(), QString("validated"));
        QCOMPARE(native.value("validation_source").toString(), QString("native_search_dense_reference"));
        QCOMPARE(gateway->recommendation(other).value("validation_source").toString(), QString("validate_run"));
        QCOMPARE(gateway->snapshot(), original);
    }
    void prevalidatedGatewayRequiresThisSessionsModelCheck_data() {
        QTest::addColumn<bool>("modelChecksRun");
        QTest::newRow("external-check-only") << false;
        QTest::newRow("cached-model-check") << true;
    }
    void prevalidatedGatewayRequiresThisSessionsModelCheck() {
        QFETCH(bool, modelChecksRun);
        const auto gateway = std::make_shared<SimulationGateway>(canonical_scenario(), "prevalidated-gateway");
        const auto snapshot = gateway->snapshotId();
        const auto run = gateway->execute("forecast_baseline", {{"snapshot_id", snapshot}}).value("run").toObject().value("run_id").toString();
        const auto external = gateway->execute("validate_run", {{"snapshot_id", snapshot}, {"run_id", run}}).value("run").toObject();
        QCOMPARE(external.value("validation_source").toString(), QString("validate_run"));
        QVERIFY(external.value("validation").toObject().value("feasible").toBool());
        FakeModel model; int step = 0; bool offered = false, modelCheckMissing = false;
        model.respond = [&](const QJsonArray& history) -> QJsonObject {
            switch (step++) {
            case 0: return calls({call("state", "get_network_state"), call("catalog", "get_control_catalog")});
            case 1: return calls({call("stage", "request_approval", {{"snapshot_id", snapshot}, {"run_id", run}})});
            case 2:
                offered = toolNames(model.tools).contains("validate_run");
                for (const auto& row : completionLedger(history).value("eligible_validated_runs").toArray())
                    if (row.toObject().value("run_id").toString() == run)
                        modelCheckMissing = !row.toObject().value("model_validation_completed").toBool() &&
                            row.toObject().value("explicit_validate_run_required").toBool();
                if (modelChecksRun) return calls({call("model-check", "validate_run", {{"snapshot_id", snapshot}, {"run_id", run}})});
                [[fallthrough]];
            default: return conclusion(run);
            }
        };
        AgentOrchestrator session(&model); session.start(gateway, "Inspect the retained evidence and explicitly check the chosen run.");
        QTRY_VERIFY_WITH_TIMEOUT(!session.running(), 10000);
        QVERIFY(offered); QVERIFY(modelCheckMissing);
        QCOMPARE(model.requests, 4); // External proof alone gets one bounded model recovery turn.
        QCOMPARE(model.finalJsonRequests, modelChecksRun ? 1 : 0);
        QCOMPARE(gateway->recommendation(run).value("validation_id"), external.value("validation_id"));
        if (modelChecksRun) {
            QVERIFY(resultFor(model.history, "model-check").value("reused_validation").toBool());
            QVERIFY(session.result().value("verified_feasible").toBool());
            QCOMPARE(session.result().value("selected_run_id").toString(), run);
        } else {
            QVERIFY(session.result().isEmpty());
            QCOMPARE(session.events().last().toMap().value("error").toMap().value("code").toString(), QString("explicit_validation_required"));
        }
    }
    void modelValidationBeforeInspectionRetainsItsExactProof_data() {
        QTest::addColumn<bool>("inspectBeforeFinal");
        QTest::newRow("inspect-after-validation") << true;
        QTest::newRow("inspection-still-required") << false;
    }
    void modelValidationBeforeInspectionRetainsItsExactProof() {
        QFETCH(bool, inspectBeforeFinal);
        const auto gateway = std::make_shared<SimulationGateway>(canonical_scenario(), "early-model-validation");
        const auto snapshot = gateway->snapshotId();
        FakeModel model; QString run, validation; bool inspectionBlockedRecommendation = false, ledgerHasModelProof = false; int step = 0;
        model.respond = [&](const QJsonArray& history) -> QJsonObject {
            switch (step++) {
            case 0: return calls({call("baseline", "forecast_baseline", {{"snapshot_id", snapshot}})});
            case 1:
                run = resultFor(history, "baseline").value("run").toObject().value("run_id").toString();
                return calls({call("check", "validate_run", {{"snapshot_id", snapshot}, {"run_id", run}})});
            case 2:
                validation = resultFor(history, "check").value("run").toObject().value("validation_id").toString();
                inspectionBlockedRecommendation = gateway->recommendation(run).value("error").toObject().value("code") == "inspection_required";
                if (inspectBeforeFinal) return calls({call("state", "get_network_state"), call("catalog", "get_control_catalog")});
                [[fallthrough]];
            default:
                for (const auto& row : completionLedger(history).value("eligible_validated_runs").toArray())
                    if (row.toObject().value("run_id").toString() == run)
                        ledgerHasModelProof = row.toObject().value("model_validation_completed").toBool() &&
                            !row.toObject().value("explicit_validate_run_required").toBool();
                return conclusion(run);
            }
        };
        AgentOrchestrator session(&model); session.start(gateway, "Forecast, validate the chosen run, then inspect before concluding.");
        QTRY_VERIFY_WITH_TIMEOUT(!session.running(), 10000);
        QVERIFY(inspectionBlockedRecommendation); QVERIFY(!validation.isEmpty());
        QStringList executed;
        for (const auto& row : session.events()) if (row.toMap().value("type") == "tool_completed") executed.append(row.toMap().value("tool_name").toString());
        if (inspectBeforeFinal) {
            QVERIFY(ledgerHasModelProof); QVERIFY(session.result().value("verified_feasible").toBool());
            QCOMPARE(session.result().value("validation_id").toString(), validation);
            QCOMPARE(executed, (QStringList{"forecast_baseline", "validate_run", "get_network_state", "get_control_catalog"}));
        } else {
            QVERIFY(session.result().isEmpty());
            QCOMPARE(session.events().last().toMap().value("error").toMap().value("code").toString(), QString("inspection_required"));
            QCOMPARE(executed, (QStringList{"forecast_baseline", "validate_run"}));
        }
        QCOMPARE(model.requests, inspectBeforeFinal ? 4 : 3);
    }
};
QTEST_GUILESS_MAIN(OrchestratorTests)
#include "agent_orchestrator_tests.moc"
