#include "agent/AgentController.hpp"
#include <QJsonDocument>
#include <QSignalSpy>
#include <QtTest>
#include <functional>

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

// This is an explicit test model. Every tool execution still uses the real engine.
class FakeModel final : public ModelTransport {
public:
    using ModelTransport::ModelTransport;
    std::function<QJsonObject(const QJsonArray&)> respond;
    QJsonArray history;
    int requests = 0, cancellations = 0, generation = 0;
    bool silent = false;
    void request(const QJsonArray& messages, const QJsonArray&) override {
        history = messages; ++requests; emit requestStarted();
        if (silent) return;
        const auto reply = respond(messages);
        const auto epoch = generation;
        QTimer::singleShot(0, this, [this, reply, epoch] { if (epoch == generation) emit replyReady(reply); });
    }
    void cancel() override { ++generation; ++cancellations; }
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
};
QTEST_GUILESS_MAIN(OrchestratorTests)
#include "agent_orchestrator_tests.moc"
