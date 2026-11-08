#include "ProjectController.hpp"
#include <QJsonArray>
#include <QFile>
#include <QJsonDocument>
#include <QTemporaryDir>
#include <QSignalSpy>
#include <limits>
#include <QtTest>

using namespace breakwater::desktop;
class ProjectTests : public QObject {
    Q_OBJECT
private slots:
    void rejectedActionsHaveFreshFeedbackWithoutMutatingEvidence() {
        ProjectController c; breakwater::SimulationConfig config; config.horizon_h = 1.25; c.forecastWithConfig(config);
        QTRY_VERIFY_WITH_TIMEOUT(!c.busy(), 10000); QVERIFY(!c.runId().isEmpty());
        const auto document = c.editorDocument(); const auto hash = c.networkHash(), run = c.runId();
        const auto revision = c.simulationRevision(); const auto approval = c.canApprove();
        QVERIFY(!c.canUndo()); QCOMPARE(c.forecastHorizonH(), 1.25);
        QSignalSpy notices(&c, &ProjectController::actionFeedbackChanged);
        c.connectAssets("source_a", "treatment");
        QCOMPARE(c.actionFeedback().value("code").toString(), "connection.unsupported_types");
        const auto first = c.actionFeedback().value("id").toString();
        c.connectAssets("source_a", "treatment");
        QVERIFY(c.actionFeedback().value("id").toString() != first); QCOMPARE(notices.size(), 2);
        QVERIFY(!c.actionFeedback().value("model_changed").toBool());
        c.connectAssets("tank_a", "tank_a"); QCOMPARE(c.actionFeedback().value("code").toString(), "connection.self");
        c.connectAssets("deleted", "tank_a"); QCOMPARE(c.actionFeedback().value("code").toString(), "connection.missing_target");
        c.connectAssets("source_a", "tank_b"); QCOMPARE(c.actionFeedback().value("code").toString(), "connection.source_already_connected");
        c.attachPump({}); QCOMPARE(c.actionFeedback().value("code").toString(), "pump.choose_route");
        QString sourceRoute;
        for (const auto& value : c.routes()) if (value.toMap().value("source").toString() == "source_a") sourceRoute = value.toMap().value("id").toString();
        if (sourceRoute.isEmpty()) for (const auto& route : parse_project(c.document()).network.routes) if (route.source_id == "source_a") sourceRoute = QString::fromStdString(route.id);
        QVERIFY(!sourceRoute.isEmpty()); c.attachPump(sourceRoute);
        QCOMPARE(c.actionFeedback().value("code").toString(), "pump.source_feed");
        c.addAsset("tank", std::numeric_limits<double>::quiet_NaN(), 0);
        QCOMPARE(c.actionFeedback().value("code").toString(), "placement.out_of_range");
        c.select("tank_a"); c.editSelected("capacity", "NaN");
        QCOMPARE(c.actionFeedback().value("code").toString(), "property.invalid_number");
        QCOMPARE(c.actionFeedback().value("field").toString(), "capacity");
        c.select({}); c.failSelectedPump(); QCOMPARE(c.actionFeedback().value("code").toString(), "pump.not_available");
        c.deleteSelected(); QCOMPARE(c.actionFeedback().value("code").toString(), "delete.missing_selection");
        QCOMPARE(c.editorDocument(), document); QCOMPARE(c.networkHash(), hash); QCOMPARE(c.simulationRevision(), revision);
        QCOMPARE(c.runId(), run); QCOMPARE(c.canApprove(), approval); QVERIFY(!c.canUndo());
    }
    void validConnectionAndPumpAttachmentRetainUndoSemantics() {
        ProjectController c; c.connectAssets("tank_a", "trucking");
        QVERIFY(c.canUndo()); const auto route = c.selectedId(); QVERIFY(!route.isEmpty());
        const auto before = c.networkHash(); c.attachPump(route); QVERIFY(c.selected().value("pump").toBool());
        QVERIFY(c.networkHash() != before); const auto attached = c.editorDocument();
        c.attachPump(route); QCOMPARE(c.actionFeedback().value("code").toString(), "pump.already_attached");
        QCOMPARE(c.actionFeedback().value("severity").toString(), "info"); QCOMPARE(c.editorDocument(), attached);
        c.undo(); QVERIFY(!c.selected().value("pump").toBool()); QCOMPARE(c.networkHash(), before);
        c.reset(); QVERIFY(c.actionFeedback().isEmpty()); QCOMPARE(c.forecastHorizonH(), 0.0);
    }
    void pipeParametersPersistWithoutChangingVolumeEvidence() {
        ProjectController controller; controller.loadExample("mixed"); controller.forecast();
        QTRY_VERIFY_WITH_TIMEOUT(!controller.busy(), 10000);
        QVERIFY(controller.canApprove());
        const auto hash = controller.networkHash(), run = controller.runId();
        const auto revision = controller.simulationRevision();
        QVERIFY(!controller.document().contains("pipe_transport"));
        auto parameters = controller.pipeParameters("bypass"); parameters["length_m"] = 250.0;
        QVERIFY(controller.setPipeParameters("bypass", parameters));
        QCOMPARE(controller.networkHash(), hash); QCOMPARE(controller.simulationRevision(), revision);
        QCOMPARE(controller.runId(), run); QVERIFY(controller.canApprove());
        QCOMPARE(controller.capturedRun().value("run_id").toString(), run);
        QCOMPARE(controller.capturedRun().value("horizon_h").toDouble(), 3.0);
        const auto stored = controller.document();
        const auto restored = parse_project(stored);
        QCOMPARE(restored.pipe_parameters.value("bypass").pipe.length_m, 250.0);
        QCOMPARE(project_json(restored), stored);
        controller.moveAsset("tank_a", 180, 100);
        QCOMPARE(controller.pipeParameters("bypass"), parameters); QCOMPARE(controller.runId(), run);
        controller.undo(); controller.undo();
        QVERIFY(!controller.document().contains("pipe_transport")); QVERIFY(controller.canApprove());
        QCOMPARE(controller.runId(), run);
    }
    void pipeParametersRejectInvalidEntriesAndFollowRouteDeletion() {
        ProjectController controller;
        const auto original = controller.document();
        auto parameters = controller.pipeParameters("bypass");
        QVERIFY(!controller.setPipeParameters("foreign-route", parameters)); QCOMPARE(controller.document(), original);
        auto invalid = parameters; invalid["cells"] = 2.5;
        QVERIFY(!controller.setPipeParameters("bypass", invalid)); QCOMPARE(controller.document(), original);
        auto malformed = original;
        malformed["pipe_transport"] = QJsonObject{{"version", 1}, {"routes", QJsonObject{{"foreign-route", parameters}}}};
        QVERIFY(!controller.restore(malformed)); QCOMPARE(controller.document(), original);
        malformed["pipe_transport"] = QJsonObject{{"version", 2}, {"routes", QJsonObject{}}};
        QVERIFY(!controller.restore(malformed)); QCOMPARE(controller.document(), original);
        QVERIFY(controller.setPipeParameters("bypass", parameters));
        controller.select("bypass"); controller.deleteSelected();
        QVERIFY(!controller.document().contains("pipe_transport"));
        controller.undo();
        QCOMPARE(controller.pipeParameters("bypass"), parameters);
        QVERIFY(controller.document().contains("pipe_transport"));
    }
    void sourceFixtureMatchesCanonical() {
        QFile fixture(QStringLiteral(BREAKWATER_SOURCE_DIR "/scenarios/pump_failure.json"));
        QVERIFY(fixture.open(QIODevice::ReadOnly));
        const auto saved = parse_project(QJsonDocument::fromJson(fixture.readAll()).object());
        QCOMPARE(physics_hash(saved), physics_hash(example_project()));
    }
    void serializationAndDrafts() {
        const auto p = example_project();
        const auto restored = parse_project(project_json(p));
        QCOMPARE(physics_hash(p), physics_hash(restored));
        QCOMPARE(restored.positions, p.positions);
        auto draft = p; draft.network.routes.erase(draft.network.routes.begin());
        const auto reopened = parse_project(project_json(draft));
        QVERIFY(!breakwater::inspect_network(reopened.network).empty());
        auto invalid = project_json(p); invalid["version"] = 2;
        QVERIFY_THROWS_EXCEPTION(std::invalid_argument, parse_project(invalid));
    }
    void editingAndUndo() {
        ProjectController c;
        const auto hash = c.networkHash();
        c.moveAsset("tank_a", 40, 60);
        QCOMPARE(c.networkHash(), hash);
        QCOMPARE(c.document()["layout"].toObject()["positions"].toObject()["tank_a"].toObject()["x"].toDouble(), 40.0);
        c.undo(); QCOMPARE(c.document()["layout"].toObject()["positions"].toObject()["tank_a"].toObject()["x"].toDouble(), -100.0);
        c.redo(); QCOMPARE(c.networkHash(), hash);
        c.addAsset("tank", 200, 200);
        const auto id = c.selectedId();
        QVERIFY(c.networkHash() != hash);
        c.connectAssets("tank_a", id);
        QCOMPARE(c.routes().size(), 7);
        c.select(id); c.deleteSelected(); QCOMPARE(c.routes().size(), 6);
        c.undo(); QCOMPARE(c.routes().size(), 7); QCOMPARE(c.assets().size(), 7);
    }
    void asynchronousForecastAndApprovalInvalidation() {
        ProjectController c; c.loadExample("mixed"); c.forecast();
        QTRY_VERIFY_WITH_TIMEOUT(!c.busy(), 10000);
        QVERIFY(!c.runId().isEmpty()); QVERIFY(c.canApprove());
        const auto run = c.runId();
        c.moveAsset("tank_a", 180, 40); QCOMPARE(c.runId(), run); QVERIFY(c.canApprove());
        c.approve(); QVERIFY(!c.canApprove());
        c.select("tank_a"); c.editSelected("capacity", "121");
        QVERIFY(c.runId().isEmpty()); QVERIFY(!c.canApprove());
        c.undo(); QVERIFY(c.runId().isEmpty());
    }
    void invalidDraftAndFailedForecastCannotBeApproved() {
        ProjectController c; c.addAsset("source", 0, 0);
        QVERIFY(!c.draftIssues().isEmpty()); c.forecast(); QVERIFY(!c.busy()); QVERIFY(c.runId().isEmpty());
        c.loadExample("bad"); c.forecast(); QTRY_VERIFY_WITH_TIMEOUT(!c.busy(), 10000);
        QVERIFY(!c.runId().isEmpty()); QVERIFY(!c.canApprove()); QVERIFY(c.summary().contains("tank_b"));
    }
    void staleBackgroundResultIsDiscarded() {
        ProjectController c; c.forecast(); c.addAsset("source", 0, 0);
        QTest::qWait(200);
        QVERIFY(c.runId().isEmpty()); QVERIFY(!c.busy());
    }
    void saveAndLoadAreAtomicAndPreserveState() {
        QTemporaryDir directory; QVERIFY(directory.isValid());
        ProjectController c; c.moveAsset("tank_b", 300, 140);
        c.setCamera(40.0, -60.0, 900.0, 80.0, -40.0);
        const auto original = c.document();
        const auto path = QUrl::fromLocalFile(directory.filePath("project.json"));
        QVERIFY(c.save(path)); c.reset(); QVERIFY(c.load(path)); QCOMPARE(c.document(), original);
        QVERIFY(!c.load(QUrl("https://example.com/project.json")));
        const auto after = c.document(); QVERIFY(!c.restore(QJsonObject{{"version", 999}})); QCOMPARE(c.document(), after);
    }
};
QTEST_GUILESS_MAIN(ProjectTests)
#include "project_tests.moc"
