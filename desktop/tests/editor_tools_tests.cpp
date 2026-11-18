#include "ProjectController.hpp"
#include <QJsonArray>
#include <QSet>
#include <QtTest>
#include <limits>
#include <cmath>
using namespace breakwater::desktop;
class EditorToolsTests final : public QObject {
    Q_OBJECT
private slots:
    void nativeChartRetainsOverflowAndDoesNotEditTheProject() {
        ProjectController c; c.loadExample("baseline"); c.forecast(); QTRY_VERIFY_WITH_TIMEOUT(!c.busy(), 10000);
        const auto before = c.editorDocument(); const auto revision = c.simulationRevision(); const auto id = c.runId();
        const auto chart = c.forecastChart("tank_a"); QVERIFY(chart.value("available").toBool());
        const auto points = chart.value("series").toList().first().toMap().value("points").toList();
        QCOMPARE(points.first().toMap().value("volume_m3").toDouble(), 80.0);
        QVERIFY(std::abs(points.last().toMap().value("volume_m3").toDouble() - 236.0) < 1e-9);
        QVERIFY(chart.value("y_max").toDouble() > chart.value("capacity_m3").toDouble());
        QVERIFY(!c.forecastChart("unknown").value("available").toBool());
        c.forecastChart("tank_b"); QCOMPARE(c.editorDocument(), before); QCOMPARE(c.simulationRevision(), revision); QCOMPARE(c.runId(), id);
        c.select("tank_a"); c.editSelected("initial", "70"); QVERIFY(!c.forecastChart("tank_a").value("available").toBool());
    }
    void nativeChartPreservesNegativePredictions() {
        ProjectController c; c.select("trucking_route");
        // Use actual route endpoints, avoiding any dependence on display labels.
        for (const auto& value : c.routes()) {
            const auto route = value.toMap();
            if (route.value("source") == "tank_a" && route.value("target") == "trucking") { c.select(route.value("id").toString()); c.editSelected("flow", "22"); }
        }
        c.select("bypass"); c.editSelected("flow", "18"); c.forecast(); QTRY_VERIFY_WITH_TIMEOUT(!c.busy(), 10000);
        const auto chart = c.forecastChart("tank_a"); QVERIFY(chart.value("available").toBool());
        QVERIFY(chart.value("y_min").toDouble() < 0); QVERIFY(!c.canApprove());
    }
    void selectionIsBoundedExclusiveAndDoesNotEditEvidence() {
        ProjectController c; const auto document = c.editorDocument(); const auto hash = c.networkHash();
        const auto revision = c.editorRevision();
        c.selectAssets({"tank_a", "unknown", "tank_b", "tank_a"}, "tank_a");
        QCOMPARE(c.selectedIds(), QStringList({"tank_a", "tank_b"})); QCOMPARE(c.selectedId(), "tank_a");
        c.toggleAssetSelection("tank_a"); QCOMPARE(c.selectedIds(), QStringList{"tank_b"});
        c.toggleAssetSelection("tank_b", true); QCOMPARE(c.selectedIds(), QStringList{"tank_b"});
        c.select("bypass"); QVERIFY(c.selectedIds().isEmpty());
        c.selectAssets({"tank_a"}); QVERIFY(c.addSensor("inventory", "tank_a"));
        const auto sensor = c.selectedSensorId(); c.selectSensor(sensor);
        QVERIFY(c.selectedId().isEmpty()); QVERIFY(c.selectedIds().isEmpty());
        c.undo(); QCOMPARE(c.editorDocument(), document); QCOMPARE(c.networkHash(), hash);
        QVERIFY(c.editorRevision() > revision);
        const auto selectedRevision = c.editorRevision(); c.selectAssets({"tank_b", "tank_a"});
        QCOMPARE(c.editorRevision(), selectedRevision);
    }
    void groupMoveIsOneUndoAndPreservesForecastApprovalAndSensorIdentity() {
        ProjectController c; c.loadExample("mixed"); QVERIFY(c.addSensor("inventory", "tank_a"));
        c.forecast(); QTRY_VERIFY_WITH_TIMEOUT(!c.busy(), 10000); QVERIFY(c.canApprove());
        c.selectAssets({"tank_a", "tank_b"}, "tank_a");
        const auto before = c.editorDocument(); const auto parsed = parse_project(c.document());
        const auto hash = c.networkHash(), run = c.runId(), sensorHash = c.sensorConfigurationHash();
        const auto physicsRevision = c.simulationRevision(); const auto revision = c.editorRevision();
        const auto marker = c.sensorMarkers().first().toMap();
        QVERIFY(c.moveSelection(21, 39, revision));
        const auto moved = parse_project(c.document());
        for (const auto& id : c.selectedIds()) QCOMPARE(moved.positions[id], parsed.positions[id] + QVector3D(20, 0, 40));
        QCOMPARE(c.sensorMarkers().first().toMap().value("x").toDouble(), marker.value("x").toDouble() + 20);
        QCOMPARE(c.sensorMarkers().first().toMap().value("z").toDouble(), marker.value("z").toDouble() + 40);
        QCOMPARE(c.networkHash(), hash); QCOMPARE(c.sensorConfigurationHash(), sensorHash);
        QCOMPARE(c.simulationRevision(), physicsRevision); QCOMPARE(c.runId(), run); QVERIFY(c.canApprove());
        c.undo(); QCOMPARE(c.editorDocument(), before); QCOMPARE(c.runId(), run); QVERIFY(c.canApprove());
        c.redo(); QCOMPARE(parse_project(c.document()).positions["tank_a"], moved.positions["tank_a"]);
    }
    void groupMoveRetainsRelativeOffsetsOfImportedOffGridPositions() {
        ProjectController c; auto doc = parse_project(c.document());
        doc.positions["tank_a"] = QVector3D(1.5f, 0, 2.5f); doc.positions["tank_b"] = QVector3D(66.25f, 0, 23.25f);
        QVERIFY(c.restore(project_json(doc))); c.selectAssets({"tank_a", "tank_b"});
        QVERIFY(c.moveSelection(20, -20, c.editorRevision())); const auto after = parse_project(c.document());
        QCOMPARE(after.positions["tank_b"] - after.positions["tank_a"], doc.positions["tank_b"] - doc.positions["tank_a"]);
    }
    void staleAndOutOfRangeGroupMovesAreAtomic() {
        ProjectController c; c.selectAssets({"tank_a", "tank_b"}); const auto revision = c.editorRevision();
        c.moveAsset("source_a", 240, 260); const auto changed = c.editorDocument();
        QVERIFY(!c.moveSelection(20, 20, revision)); QCOMPARE(c.editorDocument(), changed);
        QCOMPARE(c.actionFeedback().value("code").toString(), "placement.stale");
        c.moveAsset("tank_a", 10000, 0); const auto edge = c.editorDocument();
        QVERIFY(!c.moveSelection(20, 0, c.editorRevision())); QCOMPARE(c.editorDocument(), edge);
        QVERIFY(!c.moveSelection(std::numeric_limits<double>::quiet_NaN(), 0, c.editorRevision())); QCOMPARE(c.editorDocument(), edge);
    }
    void duplicateRemapsInternalRoutesAndParametersWithoutCopyingSensors() {
        ProjectController c; QVERIFY(c.setPipeParameters("bypass", c.pipeParameters("bypass")));
        QVERIFY(c.addSensor("inventory", "tank_a"));
        const auto before = c.editorDocument(); const auto original = parse_project(c.document());
        QStringList ids; QSet<QString> oldIds;
        for (const auto& asset : original.network.assets) { ids.append(QString::fromStdString(asset.id)); oldIds.insert(QString::fromStdString(asset.id)); }
        c.selectAssets(ids); const auto originalHash = c.networkHash(); QVERIFY(c.duplicateSelection());
        const auto copied = parse_project(c.document());
        QCOMPARE(copied.network.assets.size(), original.network.assets.size() * 2);
        QCOMPARE(copied.network.routes.size(), original.network.routes.size() * 2);
        QCOMPARE(c.sensorConfiguration().value("sensors").toArray().size(), 1);
        QCOMPARE(c.selectedIds().size(), ids.size()); QVERIFY(c.networkHash() != originalHash);
        for (const auto& id : c.selectedIds()) QVERIFY(!oldIds.contains(id));
        for (std::size_t i = original.network.routes.size(); i < copied.network.routes.size(); ++i) {
            const auto& route = copied.network.routes[i]; const auto& previous = original.network.routes[i - original.network.routes.size()];
            QVERIFY(route.id != previous.id); QVERIFY(!oldIds.contains(QString::fromStdString(route.source_id)));
            QVERIFY(!oldIds.contains(QString::fromStdString(route.target_id))); QCOMPARE(route.has_pump, previous.has_pump);
            if (previous.id == "bypass") QCOMPARE(c.pipeParameters(QString::fromStdString(route.id)), c.pipeParameters("bypass"));
        }
        c.undo(); QCOMPARE(c.editorDocument(), before);
    }
    void duplicateDoesNotCopyExternalConnectionsAndRetiresForecast() {
        ProjectController c; c.loadExample("mixed"); c.forecast(); QTRY_VERIFY_WITH_TIMEOUT(!c.busy(), 10000);
        QVERIFY(c.canApprove()); const auto routes = c.routes().size(); c.selectAssets({"tank_a"});
        QVERIFY(c.duplicateSelection()); QCOMPARE(c.routes().size(), routes); QVERIFY(c.runId().isEmpty()); QVERIFY(!c.canApprove());
    }
    void duplicateChecksLimitsBeforeChangingAnything() {
        ProjectController c; auto doc = parse_project(c.document());
        while (doc.network.assets.size() < 128) {
            auto asset = doc.network.assets.front(); asset.id = "extra-" + std::to_string(doc.network.assets.size());
            doc.positions[QString::fromStdString(asset.id)] = QVector3D(0, 0, 0); doc.network.assets.push_back(asset);
        }
        QVERIFY(c.restore(project_json(doc))); c.selectAssets({"tank_a"}); const auto before = c.editorDocument();
        QVERIFY(!c.duplicateSelection()); QCOMPARE(c.editorDocument(), before); QVERIFY(!c.canUndo());
        c.reset(); c.moveAsset("tank_a", 10000, 0); c.selectAssets({"tank_a", "tank_b"}); const auto edge = c.editorDocument();
        QVERIFY(!c.duplicateSelection()); QCOMPARE(c.editorDocument(), edge);
    }
    void alignmentAndDistributionAreLayoutOnlyAndUndoAsAGroup() {
        ProjectController c; c.moveAsset("tank_a", -200, -200); c.moveAsset("tank_b", 60, -120); c.moveAsset("source_a", 200, 200);
        c.selectAssets({"source_a", "tank_b", "tank_a"}, "tank_b"); const auto hash = c.networkHash();
        const auto before = c.editorDocument(); QVERIFY(c.alignSelection("x"));
        auto positions = parse_project(c.document()).positions;
        for (const auto& id : c.selectedIds()) QCOMPARE(positions[id].x(), 60.0f);
        QCOMPARE(c.networkHash(), hash); c.undo(); QCOMPARE(c.editorDocument(), before);
        c.selectAssets({"source_a", "tank_b", "tank_a"}, "tank_b"); QVERIFY(c.alignSelection("z", true));
        positions = parse_project(c.document()).positions;
        QCOMPARE(positions["tank_a"].z(), -200.0f); QCOMPARE(positions["source_a"].z(), 200.0f); QCOMPARE(positions["tank_b"].z(), 0.0f);
        c.undo(); QCOMPARE(c.editorDocument(), before);
    }
    void collapsedDistributionAndSensorBoundDeletionLeaveTheDocumentAlone() {
        ProjectController c; c.selectAssets({"tank_a", "tank_b", "source_a"}); QVERIFY(c.alignSelection("x"));
        const auto aligned = c.editorDocument(); QVERIFY(!c.alignSelection("x", true)); QCOMPARE(c.editorDocument(), aligned);
        QVERIFY(c.addSensor("inventory", "tank_b")); c.selectAssets({"tank_a", "tank_b"}); const auto guarded = c.editorDocument();
        c.deleteSelected(); QCOMPARE(c.editorDocument(), guarded); QCOMPARE(c.actionFeedback().value("code").toString(), "delete.sensor_attached");
    }
    void groupDeletionRemovesIncidentRoutesAndUndoRestoresEverything() {
        ProjectController c; c.selectAssets({"tank_a", "tank_b"}); const auto before = c.editorDocument();
        c.deleteSelected(); const auto parsed = parse_project(c.document());
        QVERIFY(!parsed.positions.contains("tank_a")); QVERIFY(!parsed.positions.contains("tank_b")); QVERIFY(parsed.network.routes.empty());
        c.undo(); QCOMPARE(c.editorDocument(), before);
    }
    void connectionPreviewUsesExecutionRulesWithoutEditingTheDocument() {
        ProjectController c; c.forecast(); QTRY_VERIFY_WITH_TIMEOUT(!c.busy(), 10000);
        const auto before = c.editorDocument(); const auto revision = c.editorRevision(), physicsRevision = c.simulationRevision(); const auto run = c.runId();
        QVERIFY(c.connectionEligibility("tank_a", "trucking").value("allowed").toBool());
        const auto invalid = c.connectionEligibility("source_a", "tank_b"); QVERIFY(!invalid.value("allowed").toBool());
        QCOMPARE(c.editorDocument(), before); QCOMPARE(c.editorRevision(), revision); QCOMPARE(c.simulationRevision(), physicsRevision); QCOMPARE(c.runId(), run);
        c.connectAssets("source_a", "tank_b"); QCOMPARE(c.actionFeedback().value("code"), invalid.value("code")); QCOMPARE(c.editorDocument(), before);
    }
};
QTEST_GUILESS_MAIN(EditorToolsTests)
#include "editor_tools_tests.moc"
