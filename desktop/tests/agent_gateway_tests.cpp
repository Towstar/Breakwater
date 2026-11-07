#include "agent/SimulationGateway.hpp"
#include <QJsonDocument>
#include <QtTest>
#include <cmath>
#include <future>

using namespace breakwater;
using namespace breakwater::desktop::agent;
namespace {
QJsonObject args(const SimulationGateway& gateway, QJsonObject extra = {}) {
    extra["snapshot_id"] = gateway.snapshotId(); return extra;
}
QJsonArray controls(const std::vector<ControlValue>& values) {
    QJsonArray result;
    for (const auto& value : values)
        result.append(QJsonObject{{"control_id", QString::fromStdString(value.id)}, {"value", value.value}});
    return result;
}
QJsonObject batch(const SimulationGateway& gateway, const QString& name, const QJsonArray& values) {
    return args(gateway, {{"candidates", QJsonArray{QJsonObject{{"candidate_id", name}, {"controls", values}}}}});
}
QString errorCode(const QJsonObject& result) { return result["error"].toObject()["code"].toString(); }
void inspect(SimulationGateway& gateway) {
    (void)gateway.execute("get_network_state", {});
    (void)gateway.execute("get_control_catalog", {});
}
QJsonObject runCase(SimulationGateway& gateway, const std::string& name) {
    return gateway.execute("simulate_candidates", batch(gateway, QString::fromStdString(name), controls(incident_controls(name))))["runs"].toArray().first().toObject();
}
QJsonObject validateCase(SimulationGateway& gateway, const QJsonObject& run) {
    return gateway.execute("validate_run", args(gateway, {{"run_id", run["run_id"]}}));
}
}

class AgentGatewayTests : public QObject {
    Q_OBJECT
private slots:
    void publishedToolsMatchCurrentKickoff() {
        QStringList names;
        for (const auto& tool : SimulationGateway::toolDefinitions()) {
            const auto function = tool.toObject()["function"].toObject();
            names.append(function["name"].toString());
            QVERIFY(!function["parameters"].toObject()["additionalProperties"].toBool(true));
        }
        QCOMPARE(names, QStringList({"get_network_state", "get_control_catalog", "forecast_baseline",
            "simulate_candidates", "validate_run", "compare_runs", "request_approval"}));
    }
    void baselineUsesActualEngineEvidence() {
        SimulationGateway gateway(main_pump_failure(canonical_scenario()), "hash-incident");
        const auto state = gateway.execute("get_network_state", {});
        QVERIFY(state["ok"].toBool());
        QCOMPARE(state["network"].toObject()["assets"].toArray().size(), 6);
        const auto result = gateway.execute("forecast_baseline", args(gateway));
        QVERIFY(result["ok"].toBool());
        const auto run = result["run"].toObject();
        QVERIFY(!run["run_id"].toString().isEmpty());
        QCOMPARE(run["snapshot_id"].toString(), gateway.snapshotId());
        QCOMPARE(run["validation_status"].toString(), "not_requested");
        const auto report = run["validation"].toObject();
        QVERIFY(!report["feasible"].toBool());
        const auto limit = report["limits"].toArray().first().toObject();
        QCOMPARE(limit["asset_id"].toString(), "tank_a");
        QVERIFY(std::abs(limit["time_h"].toDouble() - 40.0 / 52.0) < 1e-8);
        QVERIFY(std::abs(run["tanks"].toArray()[0].toObject()["final_m3"].toDouble() - 236.0) < 1e-8);
    }
    void recommendationRequiresInspectionAndIndependentValidation() {
        SimulationGateway gateway(main_pump_failure(canonical_scenario()), "physical");
        const auto run = runCase(gateway, "mixed");
        const auto id = run["run_id"].toString();
        QCOMPARE(errorCode(gateway.recommendation(id)), "inspection_required");
        inspect(gateway);
        QCOMPARE(errorCode(gateway.recommendation(id)), "validation_required");
        QVERIFY(validateCase(gateway, run)["ok"].toBool());
        const auto recommendation = gateway.recommendation(id);
        QVERIFY(recommendation["ok"].toBool());
        QVERIFY(recommendation["verified_feasible"].toBool());
        QVERIFY(std::abs(recommendation["verified_cost"].toDouble() - 12780.0) < 1e-8);
        QVERIFY(!recommendation["validation_id"].toString().isEmpty());
        QCOMPARE(recommendation["approval_status"].toString(), "advisory");
        const auto tankResults = recommendation["tanks"].toArray();
        QVERIFY(std::abs(tankResults[0].toObject()["final_m3"].toDouble() - 80.0) < 1e-8);
        QVERIFY(std::abs(tankResults[1].toObject()["final_m3"].toDouble() - 117.0) < 1e-8);
    }
    void failedCandidateCannotBeRecommendedOrStaged() {
        SimulationGateway gateway(main_pump_failure(canonical_scenario()), "physical"); inspect(gateway);
        const auto run = runCase(gateway, "bad");
        const auto report = validateCase(gateway, run)["run"].toObject()["validation"].toObject();
        QVERIFY(!report["feasible"].toBool());
        const auto limit = report["limits"].toArray().first().toObject();
        QCOMPARE(limit["asset_id"].toString(), "tank_b");
        QVERIFY(std::abs(limit["time_h"].toDouble() - 16.0 / 6.0) < 1e-8);
        const auto id = run["run_id"].toString();
        QCOMPARE(errorCode(gateway.recommendation(id)), "infeasible_run");
        QCOMPARE(errorCode(gateway.execute("request_approval", args(gateway, {{"run_id", id}}))), "infeasible_run");
        QVERIFY(gateway.stagedControls().empty());
        QCOMPARE(errorCode(gateway.stagedPlan()), "no_staged_plan");
    }
    void stagingChangesNoSnapshotControls() {
        const auto incident = main_pump_failure(canonical_scenario());
        SimulationGateway gateway(incident, "physical"); inspect(gateway);
        const auto run = runCase(gateway, "mixed");
        QVERIFY(validateCase(gateway, run)["ok"].toBool());
        const auto stage = gateway.execute("request_approval", args(gateway, {{"run_id", run["run_id"]}}));
        QVERIFY(stage["ok"].toBool());
        QVERIFY(!stage["applied"].toBool(true));
        QCOMPARE(stage["approval_status"].toString(), "pending_operator");
        QCOMPARE(gateway.stagedPlan()["selected_run_id"], run["run_id"]);
        QCOMPARE(gateway.stagedControls().size(), incident_controls("mixed").size());
        const auto baseline = gateway.execute("forecast_baseline", args(gateway))["run"].toObject();
        QVERIFY(std::abs(baseline["tanks"].toArray()[0].toObject()["final_m3"].toDouble() - 236.0) < 1e-8);
        QCOMPARE(gateway.execute("get_network_state", {})["network"].toObject()["routes"].toArray()[4].toObject()["setpoint_m3h"].toDouble(), 0.0);
    }
    void malformedControlsNeverProduceRuns() {
        SimulationGateway gateway(main_pump_failure(canonical_scenario()), "physical");
        const std::vector<QJsonArray> invalid{
            {QJsonObject{{"control_id", "invented"}, {"value", 1.0}}},
            {QJsonObject{{"control_id", "route:bypass:flow"}, {"value", -1.0}}},
            {QJsonObject{{"control_id", "route:bypass:flow"}, {"value", 26.0}}},
            {QJsonObject{{"control_id", "route:main_pump:flow"}, {"value", 1.0}}},
            {QJsonObject{{"control_id", "route:bypass:flow"}, {"value", true}}},
            {QJsonObject{{"control_id", "route:bypass:flow"}, {"value", "20"}}},
            {QJsonObject{{"control_id", "route:bypass:flow"}, {"value", 20.0}, {"extra", 1.0}}},
            {QJsonObject{{"control_id", "route:bypass:flow"}, {"value", 20.0}}, QJsonObject{{"control_id", "route:bypass:flow"}, {"value", 10.0}}}
        };
        for (const auto& invalidControls : invalid) {
            QVERIFY(!gateway.execute("simulate_candidates", batch(gateway, "invalid", invalidControls))["ok"].toBool());
            QVERIFY(gateway.runs().empty());
        }
        QVERIFY(!gateway.execute("get_network_state", {{"extra", 0}})["ok"].toBool());
        QCOMPARE(errorCode(gateway.execute("arbitrary_shell", {})), "unknown_tool");
    }
    void inventedForeignAndStaleRunIdsAreRejected() {
        SimulationGateway first(main_pump_failure(canonical_scenario()), "same-hash"); inspect(first);
        SimulationGateway second(main_pump_failure(canonical_scenario()), "same-hash"); inspect(second);
        const auto run = runCase(first, "mixed"); QVERIFY(validateCase(first, run)["ok"].toBool());
        QCOMPARE(errorCode(first.recommendation("fabricated")), "unknown_run");
        QCOMPARE(errorCode(second.recommendation(run["run_id"].toString())), "unknown_run");
        QCOMPARE(errorCode(second.execute("validate_run", args(second, {{"run_id", run["run_id"]}}))), "unknown_run");
        QCOMPARE(errorCode(first.execute("validate_run", {{"snapshot_id", second.snapshotId()}, {"run_id", run["run_id"]}})), "snapshot_mismatch");
        QVERIFY(first.execute("request_approval", args(first, {{"run_id", run["run_id"]}}))["ok"].toBool());
        first.invalidate();
        QVERIFY(!first.isCurrent());
        QCOMPARE(errorCode(first.recommendation(run["run_id"].toString())), "stale_snapshot");
        QCOMPARE(errorCode(first.stagedPlan()), "stale_snapshot");
        QVERIFY(first.stagedControls().empty());
        QCOMPARE(first.runs().size(), 1);
        QVERIFY(!first.runs().first().toObject()["current"].toBool(true));
    }
    void fixedHorizonAndUnspecifiedControlsArePreserved() {
        SimulationGateway gateway(main_pump_failure(canonical_scenario()), "physical", 2.0);
        auto request = args(gateway); request["horizon_h"] = 1.0;
        QCOMPARE(errorCode(gateway.execute("forecast_baseline", request)), "invalid_arguments");
        const auto run = gateway.execute("simulate_candidates", batch(gateway, "one-change", QJsonArray{QJsonObject{{"control_id", "route:bypass:flow"}, {"value", 20.0}}}))["runs"].toArray().first().toObject();
        QCOMPARE(run["horizon_h"].toDouble(), 2.0);
        const auto tanks = run["tanks"].toArray();
        // A receives 52 and bypasses 20. B receives 16 + 20 and retains backup 16.
        QVERIFY(std::abs(tanks[0].toObject()["final_m3"].toDouble() - 144.0) < 1e-8);
        QVERIFY(std::abs(tanks[1].toObject()["final_m3"].toDouble() - 154.0) < 1e-8);
        QVERIFY_THROWS_EXCEPTION(std::invalid_argument, SimulationGateway(canonical_scenario(), "hash", 0.0));
    }
    void candidateBatchIsIsolatedAndComparable() {
        SimulationGateway gateway(main_pump_failure(canonical_scenario()), "physical"); inspect(gateway);
        const QJsonArray candidates{
            QJsonObject{{"candidate_id", "bad"}, {"controls", controls(incident_controls("bad"))}},
            QJsonObject{{"candidate_id", "mixed"}, {"controls", controls(incident_controls("mixed"))}}
        };
        const auto result = gateway.execute("simulate_candidates", args(gateway, {{"candidates", candidates}}));
        QVERIFY(result["ok"].toBool());
        const auto runs = result["runs"].toArray(); QCOMPARE(runs.size(), 2);
        const auto first = runs[0].toObject(), second = runs[1].toObject();
        QVERIFY(first["run_id"] != second["run_id"]);
        QVERIFY(!first["validation"].toObject()["feasible"].toBool());
        QVERIFY(second["validation"].toObject()["feasible"].toBool());
        const auto comparison = gateway.execute("compare_runs", args(gateway, {{"run_ids", QJsonArray{first["run_id"], second["run_id"]}}}));
        QVERIFY(comparison["ok"].toBool()); QCOMPARE(comparison["runs"].toArray().size(), 2);
        QCOMPARE(errorCode(gateway.execute("compare_runs", args(gateway, {{"run_ids", QJsonArray{first["run_id"], "fake"}}}))), "unknown_run");
    }
    void customNetworkUsesDiscoveredIdsAndGraphShape() {
        Network network;
        network.assets = {{"west", "West", AssetKind::tank, 100.0, 50.0},
            {"east", "East", AssetKind::tank, 100.0, 50.0}, {"north", "North", AssetKind::tank, 100.0, 50.0},
            {"plant", "Plant", AssetKind::treatment, 10.0}};
        network.routes = {{"west-east", "west", "east", 10.0, 1.0}, {"east-north", "east", "north", 10.0, 1.0},
            {"north-plant", "north", "plant", 10.0, 1.0}};
        SimulationGateway gateway(network, "custom"); inspect(gateway);
        const auto catalog = gateway.execute("get_control_catalog", {})["controls"].toArray(); QCOMPARE(catalog.size(), 3);
        const auto run = gateway.execute("simulate_candidates", batch(gateway, "candidate", QJsonArray{QJsonObject{{"control_id", "route:west-east:flow"}, {"value", 2.0}}}))["runs"].toArray().first().toObject();
        QCOMPARE(run["tanks"].toArray().size(), 3);
        QVERIFY(validateCase(gateway, run)["ok"].toBool());
        QVERIFY(gateway.recommendation(run["run_id"].toString())["ok"].toBool());
        QVERIFY(!QJsonDocument(run).toJson().contains("tank_a"));
        Network oneTank; oneTank.assets = {{"solo", "Solo", AssetKind::tank, 20.0, 10.0}};
        SimulationGateway smaller(oneTank, "smaller"); inspect(smaller);
        const auto forecast = smaller.execute("forecast_baseline", args(smaller))["run"].toObject();
        QVERIFY(validateCase(smaller, forecast)["ok"].toBool());
        QVERIFY(smaller.recommendation(forecast["run_id"].toString())["ok"].toBool());
    }
    void runStoreAndBatchAreBounded() {
        SimulationGateway gateway(canonical_scenario(), "physical");
        QJsonArray tooMany;
        for (int i = 0; i < 33; ++i) tooMany.append(QJsonObject{{"candidate_id", QString::number(i)}, {"controls", QJsonArray{}}});
        QCOMPARE(errorCode(gateway.execute("simulate_candidates", args(gateway, {{"candidates", tooMany}}))), "resource_limit");
        for (int i = 0; i < 128; ++i) QVERIFY(gateway.execute("forecast_baseline", args(gateway))["ok"].toBool());
        QCOMPARE(gateway.runs().size(), 128);
        QCOMPARE(errorCode(gateway.execute("forecast_baseline", args(gateway))), "resource_limit");
        QCOMPARE(errorCode(gateway.execute("search_interventions", args(gateway))), "capability_unavailable");
    }
    void concurrentRunsRetainIndependentEvidence() {
        SimulationGateway gateway(main_pump_failure(canonical_scenario()), "physical");
        auto candidate = std::async(std::launch::async, [&] { return gateway.execute("simulate_candidates", batch(gateway, "mixed", controls(incident_controls("mixed")))); });
        auto baseline = std::async(std::launch::async, [&] { return gateway.execute("forecast_baseline", args(gateway)); });
        QVERIFY(candidate.get()["ok"].toBool());
        QVERIFY(baseline.get()["ok"].toBool());
        QCOMPARE(gateway.runs().size(), 2);
        gateway.invalidate();
        QVERIFY(gateway.stagedControls().empty());
        QCOMPARE(errorCode(gateway.execute("get_network_state", {})), "stale_snapshot");
    }
    void completedBatchRunsSurviveLaterEngineFailure() {
        Network network;
        network.assets = {{"reservoir", "Reservoir", AssetKind::tank, 1e308, 1e307},
            {"feed", "Feed", AssetKind::source, 0.0, 0.0, 1e308, 0.0, 1e308}};
        network.routes = {{"inlet", "feed", "reservoir", 1e308, 1e308}};
        SimulationGateway gateway(network, "extreme-finite-input");
        const QJsonArray candidates{
            QJsonObject{{"candidate_id", "zero-inflow"}, {"controls", QJsonArray{QJsonObject{{"control_id", "source:feed:curtail"}, {"value", 1e308}}}}},
            QJsonObject{{"candidate_id", "numerical-overflow"}, {"controls", QJsonArray{}}}
        };
        QCOMPARE(errorCode(gateway.execute("simulate_candidates", args(gateway, {{"candidates", candidates}}))), "engine_failed");
        QCOMPARE(gateway.runs().size(), 1);
        const auto retained = gateway.runs().first().toObject();
        QCOMPARE(retained["candidate_id"].toString(), "zero-inflow");
        gateway.invalidate();
        const auto details = gateway.runDetails(retained["run_id"].toString());
        QVERIFY(details["ok"].toBool());
        QCOMPARE(details["time_h"].toArray().size(), 121);
        QCOMPARE(details["volumes_m3"].toArray().size(), 1);
        QVERIFY(!details["run"].toObject()["current"].toBool(true));
    }
};

QTEST_GUILESS_MAIN(AgentGatewayTests)
#include "agent_gateway_tests.moc"
