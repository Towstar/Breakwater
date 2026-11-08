#include "agent/SimulationGateway.hpp"
#include "ProjectDocument.hpp"
#include <QJsonDocument>
#include <QtTest>
#include <cmath>
#include <future>
#include <limits>

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
QJsonObject axis(const QString& id, const QJsonArray& values) { return {{"control_id", id}, {"values", values}}; }
QJsonArray canonicalSearchAxes() {
    return {axis("route:bypass:flow", {0, 5, 10, 15, 20, 25}),
        axis("route:backup_pump:flow", {16, 25, 35}),
        axis("route:truck_route:flow", {0, 5, 10, 15, 20, 22}),
        axis("source:source_a:curtail", {0, 5, 10, 15})};
}
}

class AgentGatewayTests : public QObject {
    Q_OBJECT
private slots:
    void exactRequestedHorizonRejectsTailOverflowInPlanningTools() {
        const double flow = std::ldexp(1.0, 50), horizon = 0.9000000000000001;
        Network network;
        network.assets = {{"tank", "Tank", AssetKind::tank, 0.9 * flow, 0.0},
            {"source", "Source", AssetKind::source, 0.0, 0.0, flow}};
        network.routes = {{"feed", "source", "tank", flow, flow}};
        SimulationGateway gateway(network, "exact-horizon-tail", horizon);
        inspect(gateway);
        const auto baseline = gateway.execute("forecast_baseline", args(gateway));
        QVERIFY(baseline["ok"].toBool());
        const auto run = baseline["run"].toObject();
        QCOMPARE(run["solver_version"].toString(), QString::fromLatin1(breakwater::solver_version));
        const auto details = gateway.runDetails(run["run_id"].toString());
        QCOMPARE(details["time_h"].toArray().last().toDouble(), horizon);
        QCOMPARE(details["volumes_m3"].toArray().first().toArray().last().toDouble(), 0.9 * flow + 0.125);
        const auto validation = validateCase(gateway, run);
        QVERIFY(validation["ok"].toBool());
        const auto report = validation["run"].toObject()["validation"].toObject();
        QVERIFY(report.contains("feasible")); QVERIFY(!report["feasible"].toBool());
        QVERIFY(!report["violations"].toArray().isEmpty());
        QCOMPARE(errorCode(gateway.execute("request_approval", args(gateway, {{"run_id", run["run_id"]}}))), "infeasible_run");
        const auto search = gateway.execute("search_interventions", args(gateway, {{"axes", QJsonArray{}}}));
        QVERIFY(search["ok"].toBool());
        QCOMPARE(search["search_status"].toString(), "no_feasible_plan_found");
        QVERIFY(search["run_ids"].toArray().isEmpty());
        QVERIFY(!gateway.stagedPlan()["ok"].toBool());
    }
    void overflowingCatalogPricesReturnErrorsWithoutStaging() {
        auto network = canonical_scenario();
        for (auto& asset : network.assets) if (asset.id == "trucking") asset.cost_per_m3 = std::numeric_limits<double>::max();
        for (auto& route : network.routes) if (route.id == "truck_route") route.cost_per_m3 = std::numeric_limits<double>::max();
        SimulationGateway gateway(network, "unrepresentable-unit-price");
        QCOMPARE(errorCode(gateway.execute("get_control_catalog", {})), "engine_rejected");
        QCOMPARE(errorCode(gateway.execute("forecast_baseline", args(gateway))), "engine_rejected");
        QVERIFY(gateway.runs().isEmpty()); QVERIFY(!gateway.stagedPlan()["ok"].toBool());
    }
    void publishedToolsMatchCurrentKickoff() {
        QStringList names;
        for (const auto& tool : SimulationGateway::toolDefinitions()) {
            const auto function = tool.toObject()["function"].toObject();
            names.append(function["name"].toString());
            QVERIFY(!function["parameters"].toObject()["additionalProperties"].toBool(true));
        }
        QCOMPARE(names, QStringList({"get_network_state", "get_control_catalog", "forecast_baseline",
            "simulate_candidates", "validate_run", "compare_runs", "get_sensitivity_catalog", "analyze_sensitivities",
            "request_approval", "search_interventions"}));
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
        QVERIFY(!gateway.snapshot().value("network").toObject().isEmpty());
        QVERIFY(gateway.runDetails(id).value("ok").toBool());
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
    void importedMaximumLengthIdsWorkThroughDiscoveredControls() {
        const std::string tank_id(256, 't'), source_id(256, 's'), destination_id(256, 'd');
        const std::string feed_id(256, 'f'), route_id(256, 'r');
        breakwater::desktop::ProjectDocument document;
        document.network.assets = {{tank_id, "Storage", AssetKind::tank, 200.0, 100.0},
            {source_id, "Production", AssetKind::source, 0.0, 0.0, 10.0, 0.0, 10.0, 1.0},
            {destination_id, "Treatment", AssetKind::treatment, 20.0}};
        document.network.routes = {{feed_id, source_id, tank_id, 10.0, 10.0},
            {route_id, tank_id, destination_id, 10.0, 5.0, true, true, 2.0}};
        const auto imported = breakwater::desktop::parse_project(breakwater::desktop::project_json(document));
        QVERIFY(inspect_network(imported.network).empty());
        SimulationGateway gateway(imported.network, breakwater::desktop::physics_hash(imported));
        inspect(gateway);
        const auto catalog = gateway.execute("get_control_catalog", {})["controls"].toArray();
        QString curtailment_id, flow_id;
        for (const auto& value : catalog) {
            const auto entry = value.toObject();
            if (entry["asset_id"].toString() == QString::fromStdString(source_id)) curtailment_id = entry["control_id"].toString();
            if (entry["asset_id"].toString() == QString::fromStdString(route_id)) flow_id = entry["control_id"].toString();
        }
        QCOMPARE(curtailment_id.size(), 271);
        QCOMPARE(flow_id.size(), 267);
        const QJsonArray discoveredControls{
            QJsonObject{{"control_id", curtailment_id}, {"value", 3.0}},
            QJsonObject{{"control_id", flow_id}, {"value", 6.0}}};
        const auto simulated = gateway.execute("simulate_candidates", batch(gateway, "long-id-candidate", discoveredControls));
        QVERIFY2(simulated["ok"].toBool(), QJsonDocument(simulated).toJson().constData());
        const auto run = simulated["runs"].toArray().first().toObject();
        QVERIFY(std::abs(run["tanks"].toArray().first().toObject()["final_m3"].toDouble() - 103.0) < 1e-8);
        QVERIFY(validateCase(gateway, run)["ok"].toBool());
        const auto searched = gateway.execute("search_interventions", args(gateway, {{"axes", QJsonArray{
            axis(curtailment_id, {0, 3}), axis(flow_id, {5, 6})}}}));
        QVERIFY2(searched["ok"].toBool(), QJsonDocument(searched).toJson().constData());
        QCOMPARE(searched["diagnostics"].toObject()["candidates_evaluated"].toInt(), 4);
        QCOMPARE(searched["runs"].toArray().size(), 4);
        const auto best = searched["runs"].toArray().first().toObject();
        QCOMPARE(best["cost"].toDouble(), 30.0);
        QVERIFY(gateway.recommendation(best["run_id"].toString())["ok"].toBool());

        // The enlarged control-ID envelope must not enlarge other identifier bounds.
        QCOMPARE(errorCode(gateway.execute("simulate_candidates", batch(gateway, QString(257, 'c'), discoveredControls))), "invalid_arguments");
        QCOMPARE(errorCode(gateway.execute("forecast_baseline", {{"snapshot_id", QString(257, 's')}})), "invalid_arguments");
        QCOMPARE(errorCode(gateway.execute("validate_run", args(gateway, {{"run_id", QString(257, 'r')}}))), "invalid_arguments");
        const QJsonArray tooLong{QJsonObject{{"control_id", QString(513, 'x')}, {"value", 0}}};
        QCOMPARE(errorCode(gateway.execute("simulate_candidates", batch(gateway, "too-long-control", tooLong))), "invalid_arguments");
        QCOMPARE(errorCode(gateway.execute("search_interventions", args(gateway, {{"axes", QJsonArray{axis(QString(513, 'x'), {0})}}}))), "invalid_arguments");
        for (const auto& value : SimulationGateway::toolDefinitions()) {
            const auto function = value.toObject()["function"].toObject();
            const auto properties = function["parameters"].toObject()["properties"].toObject();
            if (function["name"] == "simulate_candidates") {
                const auto candidate = properties["candidates"].toObject()["items"].toObject()["properties"].toObject();
                QCOMPARE(candidate["candidate_id"].toObject()["maxLength"].toInt(), 256);
                const auto control = candidate["controls"].toObject()["items"].toObject()["properties"].toObject();
                QCOMPARE(control["control_id"].toObject()["maxLength"].toInt(), 512);
            }
            if (function["name"] == "search_interventions") {
                const auto search = properties["axes"].toObject()["items"].toObject()["properties"].toObject();
                QCOMPARE(search["control_id"].toObject()["maxLength"].toInt(), 512);
                QCOMPARE(properties["snapshot_id"].toObject()["maxLength"].toInt(), 256);
            }
            if (function["name"] == "analyze_sensitivities") {
                const auto ids = properties["control_ids"].toObject();
                QCOMPARE(ids["minItems"].toInt(), 1); QCOMPARE(ids["maxItems"].toInt(), 8);
                QVERIFY(ids["uniqueItems"].toBool()); QCOMPARE(ids["items"].toObject()["maxLength"].toInt(), 512);
                QVERIFY(!properties.contains("values")); QVERIFY(!properties.contains("controls"));
            }
        }
    }
    void runStoreAndBatchAreBounded() {
        SimulationGateway gateway(canonical_scenario(), "physical");
        QJsonArray tooMany;
        for (int i = 0; i < 33; ++i) tooMany.append(QJsonObject{{"candidate_id", QString::number(i)}, {"controls", QJsonArray{}}});
        QCOMPARE(errorCode(gateway.execute("simulate_candidates", args(gateway, {{"candidates", tooMany}}))), "resource_limit");
        for (int i = 0; i < 128; ++i) QVERIFY(gateway.execute("forecast_baseline", args(gateway))["ok"].toBool());
        QCOMPARE(gateway.runs().size(), 128);
        QCOMPARE(errorCode(gateway.execute("forecast_baseline", args(gateway))), "resource_limit");
        QCOMPARE(errorCode(gateway.execute("search_interventions", args(gateway))), "invalid_arguments");
        QCOMPARE(errorCode(gateway.execute("search_interventions", args(gateway, {{"axes", QJsonArray{}}}))), "resource_limit");
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
    void nativeSearchStoresActualReferenceValidatedRecommendations() {
        SimulationGateway gateway(main_pump_failure(canonical_scenario()), "search-incident");
        const auto state = gateway.execute("get_network_state", {});
        QVERIFY(state["capabilities"].toObject()["search_interventions"].toBool());
        const auto searched = gateway.execute("search_interventions", args(gateway, {{"axes", canonicalSearchAxes()}}));
        QVERIFY2(searched["ok"].toBool(), QJsonDocument(searched).toJson().constData());
        QCOMPARE(searched["search_status"].toString(), "recommendations_ready");
        const auto diagnostics = searched["diagnostics"].toObject();
        QCOMPARE(diagnostics["candidates_evaluated"].toInt(), 432);
        QCOMPARE(diagnostics["screen_passes"].toInt(), 39);
        QCOMPARE(diagnostics["reference_checks"].toInt(), 5);
        QCOMPARE(searched["run_ids"].toArray().size(), 5);
        QCOMPARE(gateway.runs().size(), 5);
        const auto best = searched["runs"].toArray().first().toObject();
        QCOMPARE(best["snapshot_id"].toString(), gateway.snapshotId());
        QCOMPARE(best["horizon_h"].toDouble(), 3.0);
        QCOMPARE(best["solver"].toString(), "dense_reference");
        QCOMPARE(best["validation_source"].toString(), "native_search_dense_reference");
        QCOMPARE(best["validation_status"].toString(), "validated");
        QVERIFY(!best["validation_id"].toString().isEmpty());
        QVERIFY(best["validation"].toObject()["feasible"].toBool());
        QVERIFY(std::abs(best["cost"].toDouble() - 4800.0) < 1e-8);
        QVERIFY(std::abs(best["validation"].toObject()["minimum_headroom_m3"].toDouble() - 4.0) < 1e-8);
        const auto tanks = best["tanks"].toArray();
        QVERIFY(std::abs(tanks[0].toObject()["final_m3"].toDouble() - 116.0) < 1e-8);
        QVERIFY(std::abs(tanks[1].toObject()["final_m3"].toDouble() - 117.0) < 1e-8);
        const auto id = best["run_id"].toString();
        // Search performs actual independent validation, while inspection is still mandatory.
        QCOMPARE(errorCode(gateway.recommendation(id)), "inspection_required");
        QVERIFY(gateway.execute("get_control_catalog", {})["ok"].toBool());
        const auto recommendation = gateway.recommendation(id);
        QVERIFY(recommendation["ok"].toBool());
        QCOMPARE(recommendation["verified_cost"].toDouble(), best["cost"].toDouble());
        QVERIFY(gateway.execute("request_approval", args(gateway, {{"run_id", id}}))["ok"].toBool());
        QCOMPARE(gateway.stagedPlan()["selected_run_id"].toString(), id);
        // Staging does not modify the immutable incident, even after optimized search.
        const auto baseline = gateway.execute("forecast_baseline", args(gateway))["run"].toObject();
        QVERIFY(std::abs(baseline["tanks"].toArray().first().toObject()["final_m3"].toDouble() - 236.0) < 1e-8);
    }
    void nativeSearchReportsNoFeasiblePlanWithoutInventingRuns() {
        SimulationGateway gateway(main_pump_failure(canonical_scenario()), "restricted-search");
        const auto searched = gateway.execute("search_interventions", args(gateway,
            {{"axes", QJsonArray{axis("route:bypass:flow", {0, 5})}}}));
        QVERIFY(searched["ok"].toBool());
        QCOMPARE(searched["search_status"].toString(), "no_feasible_plan_found");
        QCOMPARE(searched["diagnostics"].toObject()["candidates_evaluated"].toInt(), 2);
        QCOMPARE(searched["diagnostics"].toObject()["reference_checks"].toInt(), 0);
        QVERIFY(searched["runs"].toArray().isEmpty());
        QVERIFY(gateway.runs().isEmpty());
        QVERIFY(searched["scope"].toString().contains("no global"));
        QVERIFY(!gateway.stagedPlan()["ok"].toBool());
        SimulationGateway healthy(canonical_scenario(), "unchanged-search");
        const auto unchanged = healthy.execute("search_interventions", args(healthy, {{"axes", QJsonArray{}}}));
        QVERIFY(unchanged["ok"].toBool());
        QCOMPARE(unchanged["diagnostics"].toObject()["candidates_evaluated"].toInt(), 1);
        QCOMPARE(unchanged["runs"].toArray().size(), 1);
    }
    void nativeSearchRejectsMalformedOutOfBoundsAndOversizedRequests() {
        SimulationGateway gateway(main_pump_failure(canonical_scenario()), "search-limits");
        const std::vector<QJsonArray> invalid_axes{
            {axis("invented", {0})}, {axis("route:bypass:flow", {-1})},
            {axis("route:bypass:flow", {26})}, {axis("route:main_pump:flow", {1})},
            {axis("route:bypass:flow", {true})}, {axis("route:bypass:flow", {"20"})},
            {axis("route:bypass:flow", {std::numeric_limits<double>::infinity()})},
            {axis("route:bypass:flow", {0}), axis("route:bypass:flow", {5})}
        };
        for (const auto& axes : invalid_axes) {
            QCOMPARE(errorCode(gateway.execute("search_interventions", args(gateway, {{"axes", axes}}))), "invalid_control");
            QVERIFY(gateway.runs().isEmpty());
        }
        const std::vector<QJsonObject> invalid_arguments{
            {{"axes", "not an array"}}, {{"axes", QJsonArray{axis("route:bypass:flow", {})}}},
            {{"axes", QJsonArray{QJsonObject{{"control_id", "route:bypass:flow"}, {"values", QJsonArray{0}}, {"extra", true}}}}},
            {{"axes", QJsonArray{QJsonObject{{"control_id", "route:bypass:flow"}, {"values", "not an array"}}}}},
            {{"axes", QJsonArray{42}}},
            {{"axes", QJsonArray{}}, {"horizon_h", 1.0}},
            {{"axes", QJsonArray{}}, {"result_limit", 0}}, {{"axes", QJsonArray{}}, {"result_limit", 6}},
            {{"axes", QJsonArray{}}, {"result_limit", 1.5}}, {{"axes", QJsonArray{}}, {"result_limit", true}}
        };
        for (const auto& request : invalid_arguments)
            QCOMPARE(errorCode(gateway.execute("search_interventions", args(gateway, request))), "invalid_arguments");
        QJsonArray excessive_axes;
        for (int i = 0; i < 5; ++i) excessive_axes.append(axis("route:bypass:flow", {0}));
        QCOMPARE(errorCode(gateway.execute("search_interventions", args(gateway, {{"axes", excessive_axes}}))), "resource_limit");
        QJsonArray over_hundred, hundred;
        for (int i = 0; i < 101; ++i) over_hundred.append(0);
        for (int i = 0; i < 100; ++i) hundred.append(16);
        const QJsonArray too_many_combinations{axis("route:bypass:flow", over_hundred), axis("route:backup_pump:flow", hundred)};
        QCOMPARE(errorCode(gateway.execute("search_interventions", args(gateway, {{"axes", too_many_combinations}}))), "resource_limit");
        QVERIFY(gateway.runs().isEmpty());
    }
    void nativeSearchHandlesCustomColonIdsFixedHorizonAndStaleness() {
        Network network;
        network.assets = {{"west:tank", "West", AssetKind::tank, 100.0, 50.0},
            {"east:tank", "East", AssetKind::tank, 100.0, 50.0}, {"north:tank", "North", AssetKind::tank, 100.0, 50.0},
            {"treatment:plant", "Plant", AssetKind::treatment, 10.0}};
        network.routes = {{"west:east", "west:tank", "east:tank", 10.0, 1.0},
            {"east:north", "east:tank", "north:tank", 10.0, 1.0},
            {"north:plant", "north:tank", "treatment:plant", 10.0, 1.0}};
        SimulationGateway gateway(network, "custom-search", 1.5); inspect(gateway);
        const auto id = gateway.execute("get_control_catalog", {})["controls"].toArray().first().toObject()["control_id"].toString();
        const auto searched = gateway.execute("search_interventions", args(gateway,
            {{"axes", QJsonArray{axis(id, {1.0, 2.0})}}, {"result_limit", 1}}));
        QVERIFY(searched["ok"].toBool());
        QCOMPARE(searched["diagnostics"].toObject()["candidates_evaluated"].toInt(), 2);
        QCOMPARE(searched["runs"].toArray().size(), 1);
        const auto run = searched["runs"].toArray().first().toObject();
        QCOMPARE(run["tanks"].toArray().size(), 3);
        QCOMPARE(run["horizon_h"].toDouble(), 1.5);
        QCOMPARE(run["controls"].toArray().first().toObject()["control_id"].toString(), id);
        QVERIFY(!QJsonDocument(run).toJson().contains("tank_a"));
        const auto run_id = run["run_id"].toString();
        QVERIFY(gateway.recommendation(run_id)["ok"].toBool());
        gateway.invalidate();
        QCOMPARE(errorCode(gateway.recommendation(run_id)), "stale_snapshot");
        QCOMPARE(errorCode(gateway.execute("request_approval", args(gateway, {{"run_id", run_id}}))), "stale_snapshot");
        QCOMPARE(errorCode(gateway.execute("search_interventions", args(gateway, {{"axes", QJsonArray{}}}))), "stale_snapshot");
        const auto retained = gateway.runDetails(run_id);
        QVERIFY(retained["ok"].toBool());
        QCOMPARE(retained["time_h"].toArray().last().toDouble(), 1.5);
        QVERIFY(!retained["run"].toObject()["current"].toBool(true));
        QCOMPARE(gateway.runs().size(), 1);
    }
    void scientificCaptureUsesEffectiveControlsOriginalConfigAndImmutableArrays() {
        const auto network = main_pump_failure(canonical_scenario());
        breakwater::desktop::ProjectDocument document; document.network = network;
        const auto incident_hash = breakwater::desktop::physics_hash(document);
        SimulationGateway gateway(network, incident_hash, 0.95); inspect(gateway);
        const QJsonArray overrides{QJsonObject{{"control_id", "source:source_a:curtail"}, {"value", 7.0}},
            QJsonObject{{"control_id", "route:bypass:flow"}, {"value", 12.0}}};
        const auto run = gateway.execute("simulate_candidates", batch(gateway, "interior", overrides))["runs"].toArray().first().toObject();
        const auto id = run["run_id"].toString(); const auto snapshot = gateway.snapshot();
        const auto before = gateway.runDetails(id);
        const auto request = args(gateway, {{"run_id", id}, {"control_ids", QJsonArray{"source:source_a:curtail", "route:bypass:flow"}}});
        const auto prepared = gateway.prepareSensitivity(request);
        QVERIFY2(prepared.error.isEmpty(), qPrintable(QString::fromUtf8(QJsonDocument(prepared.error).toJson())));
        QVERIFY(prepared.capture);
        QCOMPARE(prepared.capture->id, id); QCOMPARE(prepared.capture->revision, quint64{0});
        QCOMPARE(prepared.capture->config.horizon_h, 0.95); QCOMPARE(prepared.capture->config.step_h, 0.025);
        QCOMPARE(prepared.capture->result.time_h.back(), 0.95);
        QCOMPARE(prepared.capture->result.time_h.size(), static_cast<std::size_t>(before["time_h"].toArray().size()));
        for (std::size_t i = 0; i < prepared.capture->result.time_h.size(); ++i)
            QCOMPARE(prepared.capture->result.time_h[i], before["time_h"].toArray()[static_cast<qsizetype>(i)].toDouble());
        for (std::size_t i = 0; i < prepared.capture->result.volumes_m3.size(); ++i) {
            const auto values = before["volumes_m3"].toArray()[static_cast<qsizetype>(i)].toArray();
            for (std::size_t j = 0; j < prepared.capture->result.volumes_m3[i].size(); ++j)
                QCOMPARE(prepared.capture->result.volumes_m3[i][j], values[static_cast<qsizetype>(j)].toDouble());
        }
        document.network = prepared.capture->network;
        QCOMPARE(prepared.capture->physical_hash, breakwater::desktop::physics_hash(document));
        QVERIFY(prepared.capture->physical_hash != incident_hash);
        QCOMPARE(prepared.controls, QJsonArray({QJsonObject{{"control_id", "source:source_a:curtail"}, {"value", 7.0}},
            QJsonObject{{"control_id", "route:bypass:flow"}, {"value", 12.0}}}));
        const auto catalog = control_catalog(prepared.capture->network);
        for (const auto& control : catalog) {
            if (control.id == "route:backup_pump:flow") QCOMPARE(control.value, 16.0);
            if (control.id == "route:truck_route:flow") QCOMPARE(control.value, 0.0);
        }
        QCOMPARE(prepared.origin, QJsonObject({{"incident_snapshot_id", gateway.snapshotId()},
            {"incident_physical_hash", incident_hash}, {"run_id", id}, {"effective_physical_hash", prepared.capture->physical_hash}}));
        QCOMPARE(gateway.snapshot(), snapshot); QCOMPARE(gateway.runDetails(id), before);
        QCOMPARE(errorCode(gateway.recommendation(id)), "validation_required"); QVERIFY(!gateway.stagedPlan()["ok"].toBool());
        QCOMPARE(errorCode(gateway.execute("analyze_sensitivities", request)), "external_executor_required");
        QCOMPARE(gateway.runDetails(id), before); QCOMPARE(gateway.snapshot(), snapshot);
        gateway.invalidate();
        QCOMPARE(errorCode(gateway.prepareSensitivity(request).error), "stale_snapshot");
        // A retained capture remains immutable evidence after its gateway retires.
        QCOMPARE(prepared.capture->network.assets[2].curtailment_m3h, 7.0);
        QCOMPARE(prepared.capture->result.time_h.back(), 0.95);
    }
    void scientificCatalogReportsEffectiveEligibilityWithoutValidationOrStaging() {
        SimulationGateway gateway(main_pump_failure(canonical_scenario()), "incident"); inspect(gateway);
        const auto run = runCase(gateway, "mixed"); const auto id = run["run_id"].toString();
        const auto before = gateway.runDetails(id);
        const auto result = gateway.execute("get_sensitivity_catalog", args(gateway, {{"run_id", id}}));
        QVERIFY(result["ok"].toBool()); QCOMPARE(result["run_id"].toString(), id);
        QCOMPARE(result["snapshot_id"].toString(), gateway.snapshotId()); QCOMPARE(result["max_selected_controls"].toInt(), 8);
        QCOMPARE(result["controls"].toArray().size(), 6);
        QStringList actual_order;
        for (const auto& value : result["controls"].toArray()) {
            const auto item = value.toObject(); const auto control_id = item["control_id"].toString(); actual_order.append(control_id);
            QCOMPARE(item["unit"].toString(), "m3/h");
            const bool eligible = control_id == "source:source_a:curtail" || control_id == "route:bypass:flow";
            QCOMPARE(item["eligible"].toBool(), eligible); QCOMPARE(item["reason"].toString().isEmpty(), eligible);
            if (control_id == "source:source_a:curtail") QCOMPARE(item["captured_value"].toDouble(), 10.0);
            if (control_id == "route:bypass:flow") QCOMPARE(item["captured_value"].toDouble(), 20.0);
        }
        QCOMPARE(actual_order, QStringList({"source:source_a:curtail", "source:source_b:curtail", "route:main_pump:flow",
            "route:backup_pump:flow", "route:bypass:flow", "route:truck_route:flow"}));
        for (const auto control_id : {"source:source_b:curtail", "route:main_pump:flow", "route:backup_pump:flow", "route:truck_route:flow"}) {
            const auto prepared = gateway.prepareSensitivity(args(gateway, {{"run_id", id}, {"control_ids", QJsonArray{control_id}}}));
            QVERIFY(!prepared.capture); QCOMPARE(errorCode(prepared.error), "invalid_sensitivity_control");
        }
        const auto reversed = gateway.prepareSensitivity(args(gateway, {{"run_id", id},
            {"control_ids", QJsonArray{"route:bypass:flow", "source:source_a:curtail"}}}));
        QCOMPARE(errorCode(reversed.error), "invalid_control_order");
        QCOMPARE(gateway.runDetails(id), before); QCOMPARE(errorCode(gateway.recommendation(id)), "validation_required");
        QVERIFY(!gateway.stagedPlan()["ok"].toBool());
    }
    void scientificPreparationRejectsMalformedForeignAndUnknownRequests() {
        SimulationGateway first(main_pump_failure(canonical_scenario()), "first"), second(main_pump_failure(canonical_scenario()), "second");
        const auto run = runCase(first, "mixed"), foreign = runCase(second, "mixed"); const auto id = run["run_id"].toString();
        const auto valid = args(first, {{"run_id", id}, {"control_ids", QJsonArray{"source:source_a:curtail"}}});
        const QList<QPair<QJsonObject, QString>> invalid{
            {args(first, {{"run_id", id}}), "invalid_arguments"},
            {args(first, {{"run_id", id}, {"control_ids", "wrong"}}), "invalid_arguments"},
            {args(first, {{"run_id", id}, {"control_ids", QJsonArray{}}}), "resource_limit"},
            {args(first, {{"run_id", id}, {"control_ids", QJsonArray{true}}}), "invalid_arguments"},
            {args(first, {{"run_id", id}, {"control_ids", QJsonArray{QString(513, 'x')}}}), "invalid_arguments"},
            {args(first, {{"run_id", id}, {"control_ids", QJsonArray{"invented"}}}), "invalid_control"},
            {args(first, {{"run_id", id}, {"control_ids", QJsonArray{"source:source_a:curtail", "source:source_a:curtail"}}}), "invalid_control"},
            {args(first, {{"run_id", "unknown"}, {"control_ids", QJsonArray{"source:source_a:curtail"}}}), "unknown_run"},
            {args(first, {{"run_id", foreign["run_id"]}, {"control_ids", QJsonArray{"source:source_a:curtail"}}}), "unknown_run"}
        };
        for (const auto& row : invalid) {
            const auto prepared = first.prepareSensitivity(row.first);
            QVERIFY(!prepared.capture); QVERIFY(prepared.controls.isEmpty()); QVERIFY(prepared.origin.isEmpty());
            QCOMPARE(errorCode(prepared.error), row.second);
        }
        auto changed = valid; changed["snapshot_id"] = second.snapshotId();
        QCOMPARE(errorCode(first.prepareSensitivity(changed).error), "snapshot_mismatch");
        changed = valid; changed["values"] = QJsonArray{11.0};
        QCOMPARE(errorCode(first.prepareSensitivity(changed).error), "invalid_arguments");
        changed = valid; changed["horizon_h"] = 24.0;
        QCOMPARE(errorCode(first.prepareSensitivity(changed).error), "invalid_arguments");
        QCOMPARE(errorCode(first.execute("get_sensitivity_catalog", args(first, {{"run_id", foreign["run_id"]}}))), "unknown_run");
        QCOMPARE(errorCode(first.execute("get_sensitivity_catalog", args(first, {{"run_id", id}, {"extra", true}}))), "invalid_arguments");
        QCOMPARE(first.runs().size(), 1); QVERIFY(!first.stagedPlan()["ok"].toBool());
    }
    void scientificSelectionHonorsEightColumnBudgetAndCustomUnicodeIds() {
        Network chain;
        for (int i = 0; i < 10; ++i) chain.assets.push_back({"tank:" + std::to_string(i), "Tank", AssetKind::tank, 500, 100});
        QJsonArray eight, nine;
        for (int i = 0; i < 9; ++i) {
            const auto id = "link:" + std::to_string(i);
            chain.routes.push_back({id, chain.assets[static_cast<std::size_t>(i)].id, chain.assets[static_cast<std::size_t>(i + 1)].id, 5, 1});
            const auto control_id = QString::fromStdString("route:" + id + ":flow");
            nine.append(control_id); if (i < 8) eight.append(control_id);
        }
        SimulationGateway gateway(chain, "chain");
        const auto run_id = gateway.execute("forecast_baseline", args(gateway))["run"].toObject()["run_id"].toString();
        const auto selected = gateway.prepareSensitivity(args(gateway, {{"run_id", run_id}, {"control_ids", eight}}));
        QVERIFY(selected.error.isEmpty()); QCOMPARE(selected.controls.size(), 8);
        QCOMPARE(errorCode(gateway.prepareSensitivity(args(gateway, {{"run_id", run_id}, {"control_ids", nine}})).error), "resource_limit");
        Network unicode;
        unicode.assets = {{"tank:α", "West", AssetKind::tank, 1000, 100}, {"tank:β", "East", AssetKind::tank, 1000, 100},
            {"prod:λ", "Source", AssetKind::source, 0, 0, 10, 0, 5, 7}};
        unicode.routes = {{"feed:φ", "prod:λ", "tank:α", 10, 10}, {"link:μ", "tank:α", "tank:β", 8, 2}};
        SimulationGateway custom(unicode, "unicode", 1.3);
        const QJsonArray overrides{QJsonObject{{"control_id", "source:prod:λ:curtail"}, {"value", 1.0}},
            QJsonObject{{"control_id", "route:link:μ:flow"}, {"value", 3.0}}};
        const auto custom_id = custom.execute("simulate_candidates", batch(custom, "custom", overrides))["runs"].toArray().first().toObject()["run_id"].toString();
        const auto prepared = custom.prepareSensitivity(args(custom, {{"run_id", custom_id},
            {"control_ids", QJsonArray{"source:prod:λ:curtail", "route:link:μ:flow"}}}));
        QVERIFY(prepared.error.isEmpty()); QCOMPARE(prepared.controls, overrides);
        QCOMPARE(prepared.capture->config.horizon_h, 1.3); QCOMPARE(prepared.capture->result.tank_ids.size(), std::size_t{2});
        QCOMPARE(prepared.capture->network.assets.back().curtailment_m3h, 1.0);
        const auto baseline = custom.execute("forecast_baseline", args(custom))["run"].toObject()["run_id"].toString();
        const auto blocked = custom.prepareSensitivity(args(custom, {{"run_id", baseline}, {"control_ids", QJsonArray{"source:prod:λ:curtail"}}}));
        QCOMPARE(errorCode(blocked.error), "invalid_sensitivity_control");
    }
    void scientificSelectionRejectsInteriorSourceOnFeedCapacityBoundary() {
        Network network;
        network.assets = {{"tank", "Tank", AssetKind::tank, 100, 10},
            {"source", "Source", AssetKind::source, 0, 0, 20, 5, 10, 2}};
        network.routes = {{"feed", "source", "tank", 15, 15}};
        SimulationGateway gateway(network, "feed-boundary");
        const auto id = gateway.execute("forecast_baseline", args(gateway))["run"].toObject()["run_id"].toString();
        const auto catalog = gateway.execute("get_sensitivity_catalog", args(gateway, {{"run_id", id}}))["controls"].toArray();
        QCOMPARE(catalog.size(), 1); QVERIFY(!catalog.first().toObject()["eligible"].toBool());
        QVERIFY(catalog.first().toObject()["reason"].toString().contains("feed-capacity"));
        const auto prepared = gateway.prepareSensitivity(args(gateway, {{"run_id", id}, {"control_ids", QJsonArray{"source:source:curtail"}}}));
        QCOMPARE(errorCode(prepared.error), "invalid_sensitivity_control");
    }
};

QTEST_GUILESS_MAIN(AgentGatewayTests)
#include "agent_gateway_tests.moc"
