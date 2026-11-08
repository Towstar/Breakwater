#include "agent/AgentController.hpp"
#include "science/AgentSensitivityClient.hpp"
#include "history/SessionRepository.hpp"
#include "history/HistoryController.hpp"
#include "history/RehearsalEvidenceGuard.hpp"
#include "history/RehearsalArchiveCoordinator.hpp"
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QtTest>
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

// This is an explicit test model. Every tool execution still uses the real engine.
class FakeModel final : public ModelTransport {
public:
    using ModelTransport::ModelTransport;
    std::function<QJsonObject(const QJsonArray&)> respond;
    QJsonArray history;
    QJsonArray tools;
    QList<QJsonArray> toolsPerRequest;
    int requests = 0, cancellations = 0, generation = 0;
    bool silent = false;
    bool metadataOnCancel = false;
    void request(const QJsonArray& messages, const QJsonArray& definitions) override {
        history = messages; tools = definitions; toolsPerRequest.append(definitions); ++requests; emit requestStarted();
        if (silent) return;
        const auto reply = respond(messages);
        const auto epoch = generation;
        QTimer::singleShot(0, this, [this, reply, epoch] { if (epoch == generation) emit replyReady(reply); });
    }
    void cancel() override {
        ++generation; ++cancellations;
        if (metadataOnCancel) emit requestCompleted(QJsonObject{{"request_id", "cancelled-test-request"}, {"outcome", "cancelled"}, {"error_code", "cancelled"}, {"http_status", 0}});
    }
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
        QCOMPARE(session.events().last().toMap()["error"].toMap()["code"].toString(), "ungrounded_recommendation");
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
        FakeModel model;
        model.respond = [step = 0, snapshot = QString{}, selected = QString{}](const QJsonArray& history) mutable -> QJsonObject {
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
                return calls({call("stage", "request_approval", {{"snapshot_id", snapshot}, {"run_id", selected}})});
            }
            default: return conclusion(selected);
            }
        };
        AgentOrchestrator session(&model);
        session.start(std::make_shared<SimulationGateway>(main_pump_failure(canonical_scenario()), "search-incident"), "Find a low-cost validated response.");
        QTRY_VERIFY_WITH_TIMEOUT(!session.running(), 10000);
        QCOMPARE(resultFor(model.history, "search")["diagnostics"].toObject()["candidates_evaluated"].toInt(), 432);
        QVERIFY(session.result()["verified_feasible"].toBool());
        QCOMPARE(session.result()["verified_cost"].toDouble(), 4800.0);
        QCOMPARE(session.result()["approval_status"].toString(), "pending_operator");
        QVERIFY(!session.result()["validation_id"].toString().isEmpty());
        QVERIFY(session.result()["warnings"].toArray().size() > 0);
    }
};
QTEST_GUILESS_MAIN(OrchestratorTests)
#include "agent_orchestrator_tests.moc"
