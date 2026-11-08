#include "ProjectController.hpp"
#include "agent/AgentController.hpp"
#include "history/SessionRepository.hpp"
#include "telemetry/TelemetryContracts.hpp"
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTimer>
#include <QtTest>

using namespace breakwater::desktop;
namespace {
QJsonObject toolResult(const QJsonArray& messages, const QString& id) {
    for (const auto& value : messages) {
        const auto message = value.toObject();
        if (message.value("role") == "tool" && message.value("tool_call_id") == id)
            return QJsonDocument::fromJson(message.value("content").toString().toUtf8()).object();
    }
    return {};
}
QJsonObject toolCall(const QString& id, const QString& name, const QJsonObject& arguments = {}) {
    return {{"id", id}, {"type", "function"}, {"function", QJsonObject{{"name", name},
        {"arguments", QString::fromUtf8(QJsonDocument(arguments).toJson(QJsonDocument::Compact))}}}};
}
// Offline transport only. The production dispatcher still executes real native
// simulation, independent validation and staging before the operator action.
class ApprovalFixtureModel final : public agent::ModelTransport {
public:
    using ModelTransport::ModelTransport;
    int step = 0, generation = 0;
    QString snapshot, selected;
    void request(const QJsonArray& messages, const QJsonArray&) override {
        emit requestStarted(); QJsonArray calls; QJsonObject reply;
        switch (step++) {
        case 0: calls = {toolCall("state", "get_network_state"), toolCall("catalog", "get_control_catalog")}; break;
        case 1: {
            snapshot = toolResult(messages, "state").value("snapshot_id").toString();
            QJsonArray controls;
            for (const auto& control : breakwater::incident_controls("mixed"))
                controls.append(QJsonObject{{"control_id", QString::fromStdString(control.id)}, {"value", control.value}});
            calls = {toolCall("candidate", "simulate_candidates", {{"snapshot_id", snapshot},
                {"candidates", QJsonArray{QJsonObject{{"candidate_id", "offline-approved-candidate"}, {"controls", controls}}}}})};
            break;
        }
        case 2: {
            const auto runs = toolResult(messages, "candidate").value("runs").toArray();
            selected = runs.isEmpty() ? QString{} : runs.first().toObject().value("run_id").toString();
            calls = {toolCall("validate", "validate_run", {{"snapshot_id", snapshot}, {"run_id", selected}})}; break;
        }
        case 3: calls = {toolCall("stage", "request_approval", {{"snapshot_id", snapshot}, {"run_id", selected}})}; break;
        default:
            reply = {{"role", "assistant"}, {"content", QString::fromUtf8(QJsonDocument(QJsonObject{
                {"selected_run_id", selected}, {"explanation", "Review the actual offline native validation evidence."}}).toJson(QJsonDocument::Compact))}};
        }
        if (!calls.isEmpty()) reply = {{"role", "assistant"}, {"content", QJsonValue::Null}, {"tool_calls", calls}};
        const auto epoch = generation;
        QTimer::singleShot(0, this, [this, reply, epoch] { if (epoch == generation) emit replyReady(reply); });
    }
    void cancel() override { ++generation; }
};
}
class SensorEditorTests : public QObject {
    Q_OBJECT
private:
    static void forecast(ProjectController& project) {
        project.loadExample("mixed"); project.forecast();
        QTRY_VERIFY_WITH_TIMEOUT(!project.busy(), 10000);
        QVERIFY(project.canApprove());
    }
    static QVariantMap levelCalibration() {
        return {{"calibration", QVariantMap{{"calibration_id", "declared-level-table"}, {"version", 1},
            {"kind", "level_volume_piecewise_linear"}, {"error_bound", 0.2},
            {"knots", QVariantList{QVariantMap{{"level_m", 0.0}, {"volume_m3", 0.0}},
                                    QVariantMap{{"level_m", 1.0}, {"volume_m3", 30.0}},
                                    QVariantMap{{"level_m", 2.0}, {"volume_m3", 80.0}}}}}}};
    }
private slots:
    void emptyEditorIsUnchangedV1AndValidContext() {
        ProjectController project;
        QCOMPARE(project.editorDocument(), project.document());
        QCOMPARE(project.document().value("version").toInt(), 1);
        QCOMPARE(project.sensorConfiguration(), project.sensorConfiguration());
        telemetry::parse_sensor_configuration(project.sensorConfiguration(), project.sensorContext());
    }
    void sensorWrapperRoundTripsWithoutChangingNumericalPayload() {
        ProjectController project; const auto legacy = project.document();
        QVERIFY(project.addSensor("inventory", "tank_a"));
        const auto id = project.selectedSensorId(); const auto wrapper = project.editorDocument();
        QCOMPARE(wrapper.value("version").toInt(), 2); QCOMPARE(wrapper.value("project").toObject(), legacy);
        QCOMPARE(project.document(), legacy);
        QVERIFY_THROWS_EXCEPTION(std::invalid_argument, parse_project(wrapper));
        ProjectController reopened; QVERIFY(reopened.restore(wrapper));
        QCOMPARE(reopened.editorDocument(), wrapper); QCOMPARE(reopened.document(), legacy);
        QCOMPARE(reopened.sensorMarkers().front().toMap().value("id").toString(), id);
    }
    void labelAndMarkerEditsPreserveIdentityAndManualEvidence() {
        ProjectController project; forecast(project); QVERIFY(project.addSensor("inventory", "tank_a"));
        const auto id = project.selectedSensorId(), hash = project.sensorConfigurationHash();
        const auto network = project.document(); const auto run = project.runId(); const auto revision = project.simulationRevision();
        QSignalSpy changes(&project, &ProjectController::sensorConfigurationChanged);
        QVERIFY(project.configureSensor(id, {{"label", "Operator label"}}));
        QVERIFY(project.moveSensor(id, 12, 60, -20));
        QCOMPARE(project.sensorConfigurationHash(), hash); QCOMPARE(changes.count(), 0);
        QCOMPARE(project.document(), network); QCOMPARE(project.runId(), run);
        QCOMPARE(project.simulationRevision(), revision); QVERIFY(project.canApprove());
        QCOMPARE(project.selectedSensor().value("marker").toMap().value("x").toDouble(), 12.0);
    }
    void semanticChangesNotifyWithoutOverwritingCommands() {
        ProjectController project; forecast(project); QVERIFY(project.addSensor("inventory", "tank_a"));
        const auto id = project.selectedSensorId(), hash = project.sensorConfigurationHash(); const auto before = project.document();
        QSignalSpy changes(&project, &ProjectController::sensorConfigurationChanged);
        QVERIFY(project.configureSensor(id, {{"uncertainty_bound", 0.7}}));
        QVERIFY(project.sensorConfigurationHash() != hash); QCOMPARE(changes.count(), 1);
        QCOMPARE(project.document(), before); QVERIFY(project.canApprove());
    }
    void rejectedMappingAndAttachmentAreAtomic() {
        ProjectController project; QVERIFY(project.addSensor("inventory", "tank_a"));
        const auto id = project.selectedSensorId(); const auto before = project.editorDocument();
        QVERIFY(!project.configureSensor(id, {{"unit", "m"}})); QCOMPARE(project.editorDocument(), before);
        QVERIFY(!project.configureSensor(id, {{"attachment", QVariantMap{{"kind", "asset"}, {"id", "treatment"}}}}));
        QCOMPARE(project.editorDocument(), before);
        QVERIFY(!project.addSensor("pump_state", "bypass")); QCOMPARE(project.editorDocument(), before);
    }
    void levelNeedsExplicitMonotoneCalibrationAndVersionsChanges() {
        ProjectController project; const auto before = project.editorDocument();
        QVERIFY(!project.addSensor("level", "tank_a")); QCOMPARE(project.editorDocument(), before);
        QVERIFY(project.addSensor("level", "tank_a", levelCalibration()));
        const auto id = project.selectedSensorId(), hash = project.sensorConfigurationHash();
        auto calibration = project.selectedSensor().value("calibration").toMap(); calibration["error_bound"] = 0.3;
        QVERIFY(project.configureSensor(id, {{"calibration", calibration}}));
        QCOMPARE(project.selectedSensor().value("calibration").toMap().value("version").toInt(), 2);
        QVERIFY(project.sensorConfigurationHash() != hash);
    }
    void markersFollowEquipmentLayoutAndUndoPreservesIds() {
        ProjectController project; QVERIFY(project.addSensor("inventory", "tank_a"));
        const auto id = project.selectedSensorId(), hash = project.sensorConfigurationHash(); const auto before = project.editorDocument();
        const auto x = project.sensorMarkers().front().toMap().value("x").toDouble();
        project.moveAsset("tank_a", 100, 30);
        QCOMPARE(project.sensorMarkers().front().toMap().value("x").toDouble(), x + 200);
        QCOMPARE(project.sensorConfigurationHash(), hash);
        project.undo(); QCOMPARE(project.editorDocument(), before);
        project.undo(); QCOMPARE(project.editorDocument().value("version").toInt(), 1);
        project.redo(); QCOMPARE(project.sensorMarkers().front().toMap().value("id").toString(), id);
    }
    void deletionRequiresExplicitSensorDisposition() {
        ProjectController project; QVERIFY(project.addSensor("route_flow", "bypass"));
        const auto sensor = project.selectedSensorId(); const auto before = project.editorDocument();
        project.select("tank_a"); project.deleteSelected(); QCOMPARE(project.editorDocument(), before);
        QVERIFY(project.summary().contains("Remove or reattach sensor"));
        QVERIFY(project.deleteSensor(sensor)); project.deleteSelected();
        QVERIFY(project.routes().size() < 6); project.undo(); project.undo();
        QCOMPARE(project.editorDocument(), before);
    }
    void topologyChangesRebindPhysicalIdentity() {
        ProjectController project; QVERIFY(project.addSensor("inventory", "tank_a"));
        const auto hash = project.sensorConfigurationHash();
        QSignalSpy changes(&project, &ProjectController::sensorConfigurationChanged);
        project.select("tank_a"); project.editSelected("capacity", "121");
        QVERIFY(project.sensorConfigurationHash() != hash); QCOMPARE(changes.count(), 1);
        QCOMPARE(project.sensorConfiguration().value("physical_hash").toString(), project.networkHash());
        telemetry::parse_sensor_configuration(project.sensorConfiguration(), project.sensorContext());
    }
    void customNetworkUsesItsActualStableIds() {
        auto custom = example_project(); const auto positions = custom.positions; custom.positions.clear();
        for (auto& asset : custom.network.assets) {
            const auto old = QString::fromStdString(asset.id); asset.id = "other_" + asset.id;
            asset.label = "Custom " + asset.label; custom.positions[QString::fromStdString(asset.id)] = positions.value(old);
        }
        for (auto& route : custom.network.routes) {
            route.id = "other_" + route.id; route.source_id = "other_" + route.source_id; route.target_id = "other_" + route.target_id;
        }
        ProjectController project; QVERIFY(project.restore(project_json(custom))); const auto before = project.document();
        QVERIFY(project.addSensor("inventory", "other_tank_a"));
        QVERIFY(project.addSensor("route_flow", "other_bypass"));
        const auto wrapper = project.editorDocument();
        QVERIFY(!project.addSensor("inventory", "tank_a")); QCOMPARE(project.editorDocument(), wrapper);
        QCOMPARE(project.document(), before);
        ProjectController reopened; QVERIFY(reopened.restore(wrapper)); QCOMPARE(reopened.editorDocument(), wrapper);
    }
    void concentrationRequiresDeclaredGeometryAndRetiresOnGeometryChanges() {
        ProjectController project;
        QVariantMap fields{{"physical_support", QVariantMap{{"kind", "cell_average"}, {"cell_index", 3}, {"acquisition_window_s", 0.1}, {"delay_s", 0.2}}}};
        QVERIFY(!project.addSensor("concentration", "bypass", fields));
        auto parameters = project.pipeParameters("bypass"); parameters["cells"] = 4;
        QVERIFY(project.setPipeParameters("bypass", parameters)); QVERIFY(project.addSensor("concentration", "bypass", fields));
        const auto id = project.selectedSensorId(); const auto before = project.editorDocument();
        parameters["cells"] = 2; QVERIFY(!project.setPipeParameters("bypass", parameters)); QCOMPARE(project.editorDocument(), before);
        fields["physical_support"] = QVariantMap{{"kind", "cell_average"}, {"cell_index", 1}, {"acquisition_window_s", 0.1}, {"delay_s", 0.2}};
        QVERIFY(project.configureSensor(id, fields)); const auto hash = project.sensorConfigurationHash();
        QVERIFY(project.setPipeParameters("bypass", parameters)); QVERIFY(project.sensorConfigurationHash() != hash);
        telemetry::validate_document(project.editorDocument(), "project_v2", project.sensorContext());
    }
    void primaryTransferIsAtomicAndGroupsCannotMixPhysicalQuantities() {
        ProjectController project; QVERIFY(project.addSensor("inventory", "tank_a", {{"group_id", "tank-a"}}));
        const auto first = project.selectedSensorId();
        QVERIFY(project.addSensor("inventory", "tank_a", {{"group_id", "tank-a"}, {"role", "redundant"}}));
        const auto second = project.selectedSensorId();
        QVERIFY(project.configureSensor(second, {{"role", "primary"}}));
        project.selectSensor(first); QCOMPARE(project.selectedSensor().value("role").toString(), "redundant");
        const auto before = project.editorDocument();
        QVERIFY(!project.addSensor("inventory", "tank_b", {{"group_id", "tank-a"}, {"role", "redundant"}}));
        QCOMPARE(project.editorDocument(), before);
    }
    void saveLoadStrictDecodeAndMalformedWrapperLeaveStateUnchanged() {
        QTemporaryDir directory; QVERIFY(directory.isValid()); ProjectController project;
        QVERIFY(project.addSensor("inventory", "tank_a")); const auto before = project.editorDocument();
        const auto path = directory.filePath("sensors.json"); QVERIFY(project.save(QUrl::fromLocalFile(path)));
        project.reset(); QVERIFY(project.load(QUrl::fromLocalFile(path))); QCOMPARE(project.editorDocument(), before);
        auto malformed = before; malformed["sensor_configuration"] = QJsonObject{};
        QVERIFY(!project.restore(malformed)); QCOMPARE(project.editorDocument(), before);
        malformed = before; malformed["version"] = 3;
        QVERIFY(!project.restore(malformed)); QCOMPARE(project.editorDocument(), before);
        QFile file(path); QVERIFY(file.open(QIODevice::WriteOnly)); file.write("{\"version\":1,\"version\":2}"); file.close();
        QVERIFY(!project.load(QUrl::fromLocalFile(path))); QCOMPARE(project.editorDocument(), before);
    }
    void rejectedApprovalMutationCannotReportApplication() {
        ProjectController project; forecast(project); QVERIFY(project.addSensor("inventory", "tank_a"));
        auto wrapper = project.editorDocument();
        auto configuration = wrapper.value("sensor_configuration").toObject();
        configuration["revision"] = 9007199254740991.0;
        wrapper["sensor_configuration"] = configuration;
        QVERIFY(project.restore(wrapper));
        telemetry::parse_sensor_configuration(project.sensorConfiguration(), project.sensorContext());
        const auto before = project.editorDocument(); const auto hash = project.networkHash();
        const auto run = project.runId(); const auto revision = project.simulationRevision();
        QVERIFY(project.canApprove());
        QSignalSpy configurationChanges(&project, &ProjectController::sensorConfigurationChanged);
        QVERIFY(!project.applyControlsForApproval({{"route:bypass:flow", 19.0}}, hash));
        QCOMPARE(project.editorDocument(), before); QCOMPARE(project.networkHash(), hash);
        QCOMPARE(project.runId(), run); QCOMPARE(project.simulationRevision(), revision);
        QCOMPARE(configurationChanges.count(), 0); QVERIFY(project.canApprove());
        QVERIFY(!project.summary().contains("Operator applied"));
    }
    void failedSensorTransactionCannotRecordAgentOperatorApproval() {
        QTemporaryDir directory; QVERIFY(directory.isValid());
        history::SessionRepository repository(directory.filePath("approval.sqlite"));
        QSignalSpy ready(&repository, &history::SessionRepository::ready);
        QSignalSpy failures(&repository, &history::SessionRepository::failed);
        ProjectController project; project.loadExample("baseline");
        QVERIFY(project.addSensor("inventory", "tank_a"));
        auto wrapper = project.editorDocument(); auto configuration = wrapper.value("sensor_configuration").toObject();
        configuration["revision"] = 9007199254740991.0; wrapper["sensor_configuration"] = configuration;
        QVERIFY(project.restore(wrapper));
        const auto before = project.editorDocument(); const auto revision = project.simulationRevision();
        ApprovalFixtureModel model; QJsonObject archive;
        agent::AgentController investigator(&project, &model); investigator.setHistory(&repository, nullptr);
        connect(&repository, &history::SessionRepository::sessionSaved, &investigator,
                [&repository](const QString& id) { repository.inspectSession(id); });
        connect(&repository, &history::SessionRepository::sessionInspected, &investigator,
                [&archive](const QJsonObject& value) { if (value.value("status") == "completed") archive = value; });
        QTRY_VERIFY_WITH_TIMEOUT(!ready.isEmpty(), 10000);
        investigator.investigate("Stage the actual mixed intervention for offline operator review.");
        QTRY_VERIFY_WITH_TIMEOUT(!investigator.running(), 10000);
        QVERIFY2(investigator.canApprove(), qPrintable(investigator.status()));
        QTRY_VERIFY_WITH_TIMEOUT(!archive.isEmpty(), 10000);
        QVERIFY(archive.value("approvals").toArray().isEmpty());
        QVERIFY(!investigator.approvePlan());
        QCOMPARE(project.editorDocument(), before); QCOMPARE(project.simulationRevision(), revision);
        QVERIFY(!project.busy());
        QVERIFY(investigator.recommendation().value("approval_status").toString() != "operator_applied");
        QVERIFY(!project.summary().contains("Operator applied"));
        QSignalSpy inspected(&repository, &history::SessionRepository::sessionInspected);
        repository.inspectSession(investigator.recommendation().value("session_id").toString());
        QTRY_VERIFY_WITH_TIMEOUT(!inspected.isEmpty(), 10000);
        const auto stored = inspected.last().first().toJsonObject();
        QVERIFY(stored.value("approvals").toArray().isEmpty());
        QVERIFY(stored.value("recommendation").toObject().value("approval_status").toString() != "operator_applied");
        QVERIFY(failures.isEmpty());
    }
};
QTEST_GUILESS_MAIN(SensorEditorTests)
#include "sensor_editor_tests.moc"
