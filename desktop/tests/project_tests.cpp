#include "ProjectController.hpp"
#include <QJsonArray>
#include <QFile>
#include <QJsonDocument>
#include <QTemporaryDir>
#include <QDir>
#include <limits>
#include <QtTest>

using namespace breakwater::desktop;
class ProjectTests : public QObject {
    Q_OBJECT
private slots:
    void sourceSplitIsAtomicVersionedAndInvalidatesEvidence() {
        ProjectController c;c.addAsset("tank",-40,220);const auto extra=c.selectedId();
        const auto hash=c.networkHash();
        const auto priorFeed=c.sourceFeeds("source_b").first().toMap().value("routeId").toString();
        const auto parameters=c.pipeParameters(priorFeed);QVERIFY(c.setPipeParameters(priorFeed,parameters));
        const QVariantMap equal{{"tank_b",5000},{extra,5000}};
        QVERIFY(c.setSourceFeeds("source_b",equal,hash));QCOMPARE(c.sourceFeeds("source_b").size(),2);
        QCOMPARE(c.document().value("version").toInt(),2);QCOMPARE(c.pipeParameters(priorFeed),parameters);
        QCOMPARE(c.sourceFeeds("source_b").first().toMap().value("routeId").toString(),priorFeed);
        double flow=0;for(const auto& v:c.sourceFeeds("source_b")){QCOMPARE(v.toMap().value("flow").toDouble(),8.);flow+=v.toMap().value("flow").toDouble();}QCOMPARE(flow,16.);
        c.forecast();QTRY_VERIFY_WITH_TIMEOUT(!c.busy(),10000);QVERIFY(c.canApprove());
        const auto capture=c.capturedForecast();QVERIFY(capture);QVERIFY(std::abs(capture->result.volumes_m3[0].back()-104.)<1e-9);
        QVERIFY(std::abs(capture->result.volumes_m3[1].back()-90.)<1e-9);QVERIFY(std::abs(capture->result.volumes_m3[2].back()-24.)<1e-9);
        const auto run=c.runId();const auto revision=c.simulationRevision();
        QVERIFY(c.setSourceFeeds("source_b",equal,c.networkHash()));QCOMPARE(c.runId(),run);QCOMPARE(c.simulationRevision(),revision);
        c.select("source_b");(void)c.sourceFeeds("source_b");QCOMPARE(c.runId(),run);
        c.moveAsset(extra,-20,240);QCOMPARE(c.networkHash(),capture->physical_hash);
        QVERIFY(c.setSourceFeeds("source_b",{{"tank_b",7500},{extra,2500}},c.networkHash()));QVERIFY(c.runId().isEmpty());QVERIFY(!c.canApprove());
        // Captured immutable controls and original native arrays survive editor changes.
        QCOMPARE(capture->network.routes.size(),std::size_t{7});QVERIFY(std::abs(capture->result.volumes_m3[1].back()-90.)<1e-9);
        const auto after=c.document();QVERIFY(!c.setSourceFeeds("source_b",equal,hash));QCOMPARE(c.document(),after);
        c.undo();QCOMPARE(c.sourceFeeds("source_b").first().toMap().value("shareBps").toInt(),5000);QVERIFY(c.runId().isEmpty());
        c.redo();QCOMPARE(c.sourceFeeds("source_b").first().toMap().value("shareBps").toInt(),7500);
    }
    void invalidSourceAllocationsDoNotModifyTheProject() {
        ProjectController c;c.addAsset("tank");const auto extra=c.selectedId();const auto before=c.document();const auto hash=c.networkHash();
        for(const auto& invalid:QVariantList{true,-1,10001,5000.5,QString("5000"),std::numeric_limits<double>::infinity()}) {
            QVERIFY(!c.setSourceFeeds("source_b",{{"tank_b",5000},{extra,invalid}},hash));QCOMPARE(c.document(),before);
        }
        for(const auto& shares:std::vector<QVariantMap>{{{"tank_b",5000},{extra,4999}},{{"tank_b",5000},{extra,5001}},{{extra,10000}},{{"foreign",10000}},{{"source_a",10000}}}) {
            QVERIFY(!c.setSourceFeeds("source_b",shares,hash));QCOMPARE(c.document(),before);
        }
        QVERIFY(!c.setSourceFeeds("tank_a",{{"tank_b",10000}},hash));QCOMPARE(c.document(),before);
    }
    void newSourceFeedCanBeConfiguredWithoutDeletingExistingPipes() {
        ProjectController c;c.addAsset("tank");const auto extra=c.selectedId();const auto before=c.document();
        c.connectAssets("source_b",extra);QCOMPARE(c.sourceFeeds("source_b").size(),2);
        QCOMPARE(c.sourceFeeds("source_b").last().toMap().value("shareBps").toInt(),0);
        QVERIFY(c.draftIssues().isEmpty());const auto after=c.document();c.connectAssets("source_b",extra);QCOMPARE(c.document(),after);
        c.undo();QCOMPARE(c.document(),before);QCOMPARE(c.document().value("version").toInt(),1);
        c.redo();QCOMPARE(c.document().value("version").toInt(),2);
    }
    void sourceSplitSaveLoadPreservesLayoutIdsAndFormat() {
        QTemporaryDir dir;QVERIFY(dir.isValid());ProjectController c;c.addAsset("tank",-40,220);const auto extra=c.selectedId();
        const auto artifact=qEnvironmentVariable("BREAKWATER_TEST_ARTIFACT_DIR");
        if(!artifact.isEmpty()){QDir().mkpath(artifact);QVERIFY(c.save(QUrl::fromLocalFile(artifact+"/source-split-before.json")));}
        QVERIFY(c.setSourceFeeds("source_b",{{"tank_b",5000},{extra,5000}},c.networkHash()));const auto expected=c.document();
        const auto url=QUrl::fromLocalFile(dir.filePath("split.json"));QVERIFY(c.save(url));ProjectController reopened;QVERIFY(reopened.load(url));QCOMPARE(reopened.document(),expected);
        if(!artifact.isEmpty())QVERIFY(c.save(QUrl::fromLocalFile(artifact+"/source-split-after.json")));
        auto downgraded=expected;downgraded["version"]=1;QVERIFY(!reopened.restore(downgraded));QCOMPARE(reopened.document(),expected);
        auto malformed=expected;auto net=malformed["network"].toObject();auto routes=net["routes"].toArray();auto route=routes.last().toObject();route["source_share_bps"]=true;routes[routes.size()-1]=route;net["routes"]=routes;malformed["network"]=net;
        QVERIFY(!reopened.restore(malformed));QCOMPARE(reopened.document(),expected);
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
        auto invalid = project_json(p); invalid["version"] = 999;
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
