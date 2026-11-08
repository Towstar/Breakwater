#include "quality/PipeTransportController.hpp"
#include "quality/PipeTransportEvidence.hpp"
#include "agent/SimulationGateway.hpp"
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QJsonDocument>
#include <QPointer>
#include <QTemporaryDir>
#include <QTimer>
#include <QUuid>
#include <QtTest>
#include <algorithm>
#include <utility>

using namespace breakwater;
using namespace breakwater::desktop;
using namespace breakwater::desktop::agent;
using namespace breakwater::desktop::history;
using namespace breakwater::desktop::quality;

namespace {
bool capture(ProjectController& project, const QString& example) {
    project.loadExample(example);
    project.forecast();
    QElapsedTimer deadline; deadline.start();
    while (project.busy() && deadline.elapsed() < 10000) QTest::qWait(1);
    return !project.busy() && !project.capturedRun().isEmpty();
}
TransportResult reference(const PipeParameters& p, double flow, double horizon = 3.0) {
    TransportConfig config;
    config.horizon_h = horizon; config.output_step_h = p.output_step_h;
    config.cfl_safety = p.cfl_safety; config.boundary = p.boundary;
    return transport(p.pipe, std::vector<double>(p.pipe.cells, p.initial_mg_l),
        std::vector<FlowKnot>{{0.0, flow}}, std::vector<InletKnot>{{0.0, p.inlet_mg_l}},
        config, std::vector<double>(p.pipe.cells, p.source_mg_l_h));
}
double capturedFlow(const QJsonObject& document, const std::string& routeId,
                    const std::vector<ControlValue>& controls = {}) {
    const auto network = compile(parse_project(document).network, controls).network;
    const auto route = std::find_if(network.routes.begin(), network.routes.end(),
        [&](const Route& value) { return value.id == routeId; });
    if (route == network.routes.end()) return -1;
    const auto asset = std::find_if(network.assets.begin(), network.assets.end(),
        [&](const Asset& value) { return value.id == route->source_id; });
    if (asset->kind == AssetKind::source) return asset->inflow_m3h - asset->curtailment_m3h;
    return route->available ? route->setpoint_m3h : 0.0;
}
void comparePreview(const QJsonObject& result, const TransportResult& native) {
    const auto preview = result.value("preview").toObject();
    const auto times = preview.value("time_h").toArray(), positions = preview.value("x_m").toArray();
    const auto timeIndices = preview.value("time_indices").toArray(), cellIndices = preview.value("cell_indices").toArray();
    const auto field = preview.value("concentration_mg_l").toArray();
    QCOMPARE(times.size(), std::min<qsizetype>(256, static_cast<qsizetype>(native.time_h.size())));
    QCOMPARE(positions.size(), std::min<qsizetype>(128, static_cast<qsizetype>(native.x_m.size())));
    QCOMPARE(timeIndices.size(), times.size()); QCOMPARE(cellIndices.size(), positions.size());
    QCOMPARE(field.size(), times.size());
    QCOMPARE(timeIndices.first().toInteger(), 0); QCOMPARE(cellIndices.first().toInteger(), 0);
    QCOMPARE(timeIndices.last().toInteger(), static_cast<qint64>(native.time_h.size() - 1));
    QCOMPARE(cellIndices.last().toInteger(), static_cast<qint64>(native.x_m.size() - 1));
    QCOMPARE(preview.value("source_frames").toInteger(), static_cast<qint64>(native.time_h.size()));
    QCOMPARE(preview.value("source_cells").toInteger(), static_cast<qint64>(native.x_m.size()));
    for (qsizetype c = 0; c < positions.size(); ++c) {
        const auto index = cellIndices[c].toInteger();
        QVERIFY(index >= 0 && index < static_cast<qint64>(native.x_m.size()));
        if (c) QVERIFY(index > cellIndices[c - 1].toInteger());
        QCOMPARE(positions[c].toDouble(), native.x_m[static_cast<std::size_t>(index)]);
    }
    const std::pair<const char*, const std::vector<double>*> ledgers[] = {
        {"inventory_mg", &native.inventory_mg}, {"cumulative_inlet_mg", &native.cumulative_inlet_mg},
        {"cumulative_outlet_mg", &native.cumulative_outlet_mg}, {"cumulative_source_mg", &native.cumulative_source_mg},
        {"cumulative_decay_mg", &native.cumulative_decay_mg}, {"mass_balance_residual_mg", &native.mass_balance_residual_mg}};
    for (qsizetype t = 0; t < times.size(); ++t) {
        const auto index = timeIndices[t].toInteger();
        QVERIFY(index >= 0 && index < static_cast<qint64>(native.time_h.size()));
        if (t) QVERIFY(index > timeIndices[t - 1].toInteger());
        const auto nativeTime = static_cast<std::size_t>(index);
        QCOMPARE(times[t].toDouble(), native.time_h[nativeTime]);
        const auto row = field[t].toArray(); QCOMPARE(row.size(), positions.size());
        for (qsizetype c = 0; c < row.size(); ++c)
            QCOMPARE(row[c].toDouble(), native.concentration_mg_l[nativeTime][static_cast<std::size_t>(cellIndices[c].toInteger())]);
        QCOMPARE(preview.value("outlet_mg_l").toArray()[t].toDouble(), native.concentration_mg_l[nativeTime].back());
        for (const auto& [name, values] : ledgers) {
            const auto series = preview.value(QLatin1String(name)).toArray(); QCOMPARE(series.size(), times.size());
            QCOMPARE(series[t].toDouble(), (*values)[nativeTime]);
        }
    }
    const auto diagnostics = result.value("diagnostics").toObject();
    QCOMPARE(diagnostics.value("substeps").toInteger(), static_cast<qint64>(native.diagnostics.substeps));
    QCOMPARE(diagnostics.value("max_cfl_number").toDouble(), native.diagnostics.max_cfl_number);
    QCOMPARE(diagnostics.value("minimum_concentration_mg_l").toDouble(), native.diagnostics.minimum_concentration_mg_l);
    QCOMPARE(diagnostics.value("max_mass_balance_residual_mg").toDouble(), native.diagnostics.max_mass_balance_residual_mg);
}
QJsonObject candidateArchive(ProjectController& project, QString& candidateId, QString& baselineId) {
    const auto input = parse_project(project.document());
    SimulationGateway gateway(input.network, project.networkHash());
    gateway.execute("get_network_state", {}); gateway.execute("get_control_catalog", {});
    baselineId = gateway.execute("forecast_baseline", {{"snapshot_id", gateway.snapshotId()}})
        .value("run").toObject().value("run_id").toString();
    QJsonArray values;
    for (const auto& control : incident_controls("mixed"))
        values.append(QJsonObject{{"control_id", QString::fromStdString(control.id)}, {"value", control.value}});
    const auto simulated = gateway.execute("simulate_candidates", {{"snapshot_id", gateway.snapshotId()},
        {"candidates", QJsonArray{QJsonObject{{"candidate_id", "mixed"}, {"controls", values}}}}});
    candidateId = simulated.value("runs").toArray().first().toObject().value("run_id").toString();
    gateway.execute("validate_run", {{"snapshot_id", gateway.snapshotId()}, {"run_id", candidateId}});
    return {{"session_id", "pipe-test-session"}, {"checkpoint_sequence", 1}, {"status", "completed"},
        {"snapshot", gateway.snapshot()}, {"runs", QJsonArray{gateway.runDetails(baselineId), gateway.runDetails(candidateId)}},
        {"recommendation", gateway.recommendation(candidateId)}};
}
}

class PipeTransportTests final : public QObject {
    Q_OBJECT
private slots:
    void manualCapturedRunMatchesNativeAndPreservesVolumeAuthority() {
        ProjectController project; QVERIFY(capture(project, "mixed"));
        project.select("truck_route");
        const auto document = project.document(), captured = project.capturedRun();
        const auto hash = project.networkHash(); const auto revision = project.simulationRevision();
        QVERIFY(project.canApprove());
        PipeTransportController pipe(&project); QVERIFY(pipe.canRun());
        QCOMPARE(pipe.selectedRunId(), project.runId());
        QCOMPARE(pipe.routeId(), "truck_route");
        pipe.run(); QVERIFY(pipe.busy()); QTRY_VERIFY_WITH_TIMEOUT(!pipe.busy(), 10000);
        const auto result = QJsonObject::fromVariantMap(pipe.result()); QVERIFY(!result.isEmpty());
        QCOMPARE(result.value("result_version").toInt(), 1);
        QVERIFY(!QUuid::fromString(result.value("result_id").toString()).isNull());
        QCOMPARE(result.value("route_id").toString(), "truck_route");
        QCOMPARE(result.value("run_id"), captured.value("run_id"));
        QCOMPARE(result.value("snapshot_id"), captured.value("snapshot_id"));
        QCOMPARE(result.value("physical_hash").toString(), hash);
        QCOMPARE(result.value("numerical_revision").toInteger(), static_cast<qint64>(revision));
        QCOMPARE(result.value("method").toString(), "native_pipe_transport");
        QCOMPARE(result.value("method_version").toString(), QString::fromLatin1(transport_solver_version));
        QCOMPARE(result.value("quality_status").toString(), "native_scalar_reference");
        QVERIFY(result.value("volume_feasible").toBool()); QVERIFY(!result.value("historical").toBool());
        const auto parameters = parse_pipe_parameters(project.pipeParameters("truck_route"));
        QCOMPARE(result.value("parameter_hash").toString(), pipe_parameters_hash(parameters));
        const auto flow = capturedFlow(document, "truck_route"); QCOMPARE(flow, 22.0);
        QCOMPARE(result.value("flow_m3h").toDouble(), flow);
        comparePreview(result, reference(parameters, flow));
        QCOMPARE(project.document(), document); QCOMPARE(project.capturedRun(), captured);
        QCOMPARE(project.networkHash(), hash); QCOMPARE(project.simulationRevision(), revision); QVERIFY(project.canApprove());
    }
    void sourceFlowUsesCapturedProductionMinusCurtailmentAndFailedPumpStillDiffuses() {
        ProjectController project; QVERIFY(capture(project, "mixed"));
        project.select("feed_a"); PipeTransportController pipe(&project);
        const auto sourceFlow = capturedFlow(project.document(), "feed_a");
        pipe.run(); QTRY_VERIFY_WITH_TIMEOUT(!pipe.busy(), 10000);
        QVERIFY(!pipe.result().isEmpty()); QCOMPARE(pipe.result().value("flow_m3h").toDouble(), sourceFlow);
        const auto network = parse_project(project.document()).network;
        const auto source = std::find_if(network.assets.begin(), network.assets.end(), [](const Asset& value) { return value.id == "source_a"; });
        QCOMPARE(sourceFlow, source->inflow_m3h - source->curtailment_m3h);
        QVERIFY(source->curtailment_m3h > 0);
        project.select("main_pump");
        pipe.setParameter("initial_mg_l", "1"); pipe.setParameter("inlet_mg_l", "0");
        pipe.run(); QTRY_VERIFY_WITH_TIMEOUT(!pipe.busy(), 10000);
        const auto result = QJsonObject::fromVariantMap(pipe.result()); QVERIFY(!result.isEmpty());
        QCOMPARE(result.value("flow_m3h").toDouble(), 0.0);
        comparePreview(result, reference(parse_pipe_parameters(project.pipeParameters("main_pump")), 0.0));
        const auto preview = result.value("preview").toObject();
        QVERIFY(preview.value("cumulative_inlet_mg").toArray().last().toDouble() < 0.0);
        QCOMPARE(preview.value("cumulative_outlet_mg").toArray().last().toDouble(), 0.0);
    }
    void selectedCandidateUsesStoredOverridesAndRejectsMismatchedHistory() {
        ProjectController project; project.loadExample("baseline");
        const auto original = project.document(); const auto revision = project.simulationRevision();
        QString candidate, baseline; auto archive = candidateArchive(project, candidate, baseline);
        QVERIFY(!candidate.isEmpty()); QVERIFY(!baseline.isEmpty());
        HistoryController history(nullptr); history.setLiveSession(archive);
        project.select("truck_route"); PipeTransportController pipe(&project, &history);
        QCOMPARE(pipe.selectedRunId(), candidate); QVERIFY(pipe.canRun());
        pipe.run(); QTRY_VERIFY_WITH_TIMEOUT(!pipe.busy(), 10000);
        const auto result = QJsonObject::fromVariantMap(pipe.result()); QVERIFY(!result.isEmpty());
        QCOMPARE(result.value("run_id").toString(), candidate);
        QCOMPARE(result.value("snapshot_id"), archive.value("snapshot").toObject().value("snapshot_id"));
        QCOMPARE(result.value("flow_m3h").toDouble(), 22.0);
        QCOMPARE(capturedFlow(original, "truck_route"), 0.0);
        comparePreview(result, reference(parse_pipe_parameters(project.pipeParameters("truck_route")),
            capturedFlow(original, "truck_route", incident_controls("mixed"))));
        project.select("feed_a"); pipe.run(); QTRY_VERIFY_WITH_TIMEOUT(!pipe.busy(), 10000);
        QCOMPARE(pipe.result().value("flow_m3h").toDouble(), capturedFlow(original, "feed_a", incident_controls("mixed")));
        pipe.selectRun(baseline); QVERIFY(pipe.result().isEmpty()); QVERIFY(pipe.canRun());
        pipe.run(); QTRY_VERIFY_WITH_TIMEOUT(!pipe.busy(), 10000);
        QCOMPARE(pipe.result().value("run_id").toString(), baseline);
        QCOMPARE(pipe.result().value("flow_m3h").toDouble(), capturedFlow(original, "feed_a"));
        auto snapshot = archive.value("snapshot").toObject(); snapshot["physical_hash"] = "foreign-snapshot"; archive["snapshot"] = snapshot;
        history.setLiveSession(archive);
        QVERIFY(pipe.result().isEmpty()); QVERIFY(pipe.selectedRunId().isEmpty()); QVERIFY(!pipe.canRun());
        QCOMPARE(project.document(), original); QCOMPARE(project.simulationRevision(), revision); QVERIFY(!project.canApprove());
    }
    void physicalParametersInvalidateQualityButLayoutAndVolumeApprovalRemainStable() {
        ProjectController project; QVERIFY(capture(project, "healthy")); project.select("backup_pump");
        PipeTransportController pipe(&project); const auto hash = project.networkHash();
        const auto revision = project.simulationRevision(); const auto run = project.runId();
        const auto network = project.document().value("network");
        pipe.run(); project.moveAsset("tank_a", 200, 140);
        QTRY_VERIFY_WITH_TIMEOUT(!pipe.busy(), 10000); QVERIFY(!pipe.result().isEmpty());
        const auto resultId = pipe.result().value("result_id").toString();
        project.moveAsset("tank_a", 240, 180); QCOMPARE(pipe.result().value("result_id").toString(), resultId);
        pipe.setParameter("length_m", "120"); QVERIFY(pipe.result().isEmpty()); QVERIFY(pipe.canRun());
        QCOMPARE(pipe.parameters().value("length_m").toDouble(), 120.0);
        QCOMPARE(project.document().value("network"), network); QCOMPARE(project.networkHash(), hash);
        QCOMPARE(project.simulationRevision(), revision); QCOMPARE(project.runId(), run); QVERIFY(project.canApprove());
        pipe.run(); QTRY_VERIFY_WITH_TIMEOUT(!pipe.busy(), 10000); QVERIFY(!pipe.result().isEmpty());
        QVERIFY(pipe.result().value("result_id").toString() != resultId);
    }
    void staleCompletionsAreRetiredForGeometryNumericalResetDeletionAndSelection() {
        for (const auto& change : {"geometry", "numerical", "reset", "deletion", "selection"}) {
            ProjectController project; QVERIFY(capture(project, "healthy")); project.select("backup_pump");
            PipeTransportController pipe(&project); pipe.run(); QVERIFY(pipe.busy());
            const QString operation = QLatin1String(change);
            if (operation == "geometry") pipe.setParameter("area_m2", "0.2");
            else if (operation == "numerical") project.editSelected("flow", "10");
            else if (operation == "reset") project.reset();
            else if (operation == "deletion") project.deleteSelected();
            else project.select("tank_a");
            QVERIFY(pipe.busy()); QVERIFY(pipe.result().isEmpty());
            pipe.run(); // An invalidated worker must finish before another can start.
            QTRY_VERIFY_WITH_TIMEOUT(!pipe.busy(), 10000); QVERIFY(pipe.result().isEmpty());
            if (operation == "geometry") QVERIFY(pipe.canRun());
            else QVERIFY(!pipe.canRun());
        }
    }
    void cancellationAndDestructionCannotPublishLateOrStartAnotherWorker() {
        ProjectController project; QVERIFY(capture(project, "mixed")); project.select("truck_route");
        PipeTransportController pipe(&project);
        pipe.run(); QVERIFY(pipe.busy()); pipe.cancel(); QVERIFY(pipe.busy());
        QVERIFY(!pipe.canRun()); QVERIFY(pipe.result().isEmpty()); pipe.run();
        QTRY_VERIFY_WITH_TIMEOUT(!pipe.busy(), 10000); QVERIFY(pipe.result().isEmpty()); QVERIFY(pipe.canRun());
        QVERIFY(pipe.status().contains("Cancelled")); QVERIFY(!pipe.status().contains("finishing"));
        pipe.run(); QTRY_VERIFY_WITH_TIMEOUT(!pipe.busy(), 10000); QVERIFY(!pipe.result().isEmpty());
        QPointer<PipeTransportController> retired = new PipeTransportController(&project);
        retired->run(); QVERIFY(retired->busy()); delete retired.data(); QVERIFY(retired.isNull());
        // The event loop remains usable while any detached numerical capture retires.
        bool serviced = false; QTimer::singleShot(0, &project, [&] { serviced = true; });
        QTRY_VERIFY(serviced);
    }
    void boundedPreviewAndAtomicExportRetainNativeSamplesAndDiagnostics() {
        ProjectController project; QVERIFY(capture(project, "mixed")); project.select("truck_route");
        PipeTransportController pipe(&project);
        pipe.setParameter("cells", "129"); pipe.setParameter("output_step_h", "0.01");
        pipe.setParameter("initial_mg_l", "0.5"); pipe.setParameter("inlet_mg_l", "0.25");
        pipe.setParameter("source_mg_l_h", "0.03");
        const auto parameters = parse_pipe_parameters(project.pipeParameters("truck_route"));
        const auto native = reference(parameters, 22.0);
        QVERIFY(native.time_h.size() > 256); QVERIFY(native.x_m.size() > 128);
        pipe.run(); QTRY_VERIFY_WITH_TIMEOUT(!pipe.busy(), 10000);
        const auto result = QJsonObject::fromVariantMap(pipe.result()); QVERIFY(!result.isEmpty()); comparePreview(result, native);
        QTemporaryDir directory; QVERIFY(directory.isValid()); const auto path = directory.filePath("transport.json");
        { QFile file(path); QVERIFY(file.open(QIODevice::WriteOnly)); file.write("previous evidence"); }
        QVERIFY(pipe.exportResult(QUrl::fromLocalFile(path)));
        QFile exported(path); QVERIFY(exported.open(QIODevice::ReadOnly)); const auto bytes = exported.readAll(); exported.close();
        QVERIFY(!bytes.isEmpty()); QVERIFY(bytes.size() <= 1024 * 1024);
        QJsonParseError error; const auto document = QJsonDocument::fromJson(bytes, &error);
        QCOMPARE(error.error, QJsonParseError::NoError); QCOMPARE(document.object(), result);
        QCOMPARE(QDir(directory.path()).entryList(QDir::Files).size(), 1);
        QVERIFY(!pipe.exportResult(QUrl("https://example.invalid/transport.json")));
        QVERIFY(!pipe.exportResult(QUrl::fromLocalFile(directory.path())));
        QVERIFY(exported.open(QIODevice::ReadOnly)); QCOMPARE(exported.readAll(), bytes); exported.close();
        pipe.setParameter("decay_per_h", "0.2"); QVERIFY(pipe.result().isEmpty());
        QVERIFY(!pipe.exportResult(QUrl::fromLocalFile(path)));
        QVERIFY(exported.open(QIODevice::ReadOnly)); QCOMPARE(exported.readAll(), bytes);
    }
    void invalidParameterEditsLeaveCurrentEvidenceAndControlsUntouched() {
        ProjectController project; QVERIFY(capture(project, "mixed")); project.select("truck_route");
        PipeTransportController pipe(&project); pipe.run(); QTRY_VERIFY_WITH_TIMEOUT(!pipe.busy(), 10000);
        QVERIFY(!pipe.result().isEmpty()); const auto result = pipe.result(), parameters = pipe.parameters();
        const auto captured = project.capturedRun(); const auto document = project.document();
        for (const auto& [field, value] : {std::pair{"length_m", "0"}, {"cells", "1.5"}, {"cells", "4097"},
                {"inlet_mg_l", "nan"}, {"decay_per_h", "-1"}, {"boundary", "invented"}, {"unknown", "1"}}) {
            pipe.setParameter(QLatin1String(field), QLatin1String(value));
            QCOMPARE(pipe.parameters(), parameters); QCOMPARE(pipe.result(), result);
        }
        QCOMPARE(project.document(), document); QCOMPARE(project.capturedRun(), captured); QVERIFY(project.canApprove());
    }
    void exportedNativeEvidenceReopensAfterRestartWithoutForecastOrApproval() {
        QTemporaryDir directory; QVERIFY(directory.isValid());
        const auto path = directory.filePath("native.json");
        QVariantMap expected;
        {
            ProjectController source; QVERIFY(capture(source, "mixed")); source.select("main_pump");
            PipeTransportController pipe(&source);
            pipe.setParameter("initial_mg_l", "1"); pipe.setParameter("inlet_mg_l", "0");
            pipe.run(); QTRY_VERIFY_WITH_TIMEOUT(!pipe.busy(), 10000); expected = pipe.result();
            QVERIFY(!expected.isEmpty());
            QVERIFY(expected.value("preview").toMap().value("cumulative_inlet_mg").toList().last().toDouble() < 0.0);
            QVERIFY(pipe.exportResult(QUrl::fromLocalFile(path)));
            const auto artifactDirectory = qEnvironmentVariable("BREAKWATER_TEST_ARTIFACT_DIR");
            if (!artifactDirectory.isEmpty()) {
                QVERIFY(QDir().mkpath(artifactDirectory));
                QVERIFY(pipe.exportResult(QUrl::fromLocalFile(QDir(artifactDirectory).filePath("native-transport-evidence.json"))));
            }
        }
        ProjectController project; project.loadExample("healthy");
        const auto document = project.document(), captured = project.capturedRun();
        const auto revision = project.simulationRevision(); const auto hash = project.networkHash();
        PipeTransportController reopened(&project); QVERIFY(!project.canApprove()); QVERIFY(!reopened.canRun());
        reopened.importResult(QUrl::fromLocalFile(path)); QVERIFY(reopened.busy());
        bool serviced = false; QTimer::singleShot(0, &project, [&] { serviced = true; });
        QTRY_VERIFY(serviced); QTRY_VERIFY_WITH_TIMEOUT(!reopened.busy(), 10000);
        QVERIFY(reopened.viewingImportedEvidence()); QCOMPARE(reopened.result(), expected);
        QVERIFY(reopened.status().contains("unverified")); QVERIFY(!reopened.canRun());
        QCOMPARE(project.document(), document); QCOMPARE(project.capturedRun(), captured);
        QCOMPARE(project.simulationRevision(), revision); QCOMPARE(project.networkHash(), hash); QVERIFY(!project.canApprove());
        const auto reexport = directory.filePath("copy.json"); QVERIFY(reopened.exportResult(QUrl::fromLocalFile(reexport)));
        QFile original(path), copy(reexport); QVERIFY(original.open(QIODevice::ReadOnly)); QVERIFY(copy.open(QIODevice::ReadOnly));
        QCOMPARE(copy.readAll(), original.readAll());
        reopened.showCurrentResult(); QVERIFY(!reopened.viewingImportedEvidence()); QVERIFY(reopened.result().isEmpty());
        QVERIFY(!project.canApprove());
    }
    void importedMatchingEvidenceRemainsDetachedThroughEditsAndReset() {
        QTemporaryDir directory; QVERIFY(directory.isValid()); const auto path = directory.filePath("same-network.json");
        ProjectController project; QVERIFY(capture(project, "mixed")); project.select("truck_route");
        PipeTransportController pipe(&project); pipe.run(); QTRY_VERIFY_WITH_TIMEOUT(!pipe.busy(), 10000);
        const auto current = pipe.result(); QVERIFY(!current.isEmpty()); QVERIFY(pipe.exportResult(QUrl::fromLocalFile(path)));
        const auto document = project.document(), captured = project.capturedRun(); const auto revision = project.simulationRevision();
        QVERIFY(project.canApprove());
        pipe.importResult(QUrl::fromLocalFile(path)); QTRY_VERIFY_WITH_TIMEOUT(!pipe.busy(), 10000);
        QVERIFY(pipe.viewingImportedEvidence()); QCOMPARE(pipe.result(), current); QVERIFY(!pipe.canRun());
        pipe.run(); QVERIFY(!pipe.busy()); pipe.setParameter("length_m", "120");
        QCOMPARE(project.document(), document); QCOMPARE(project.capturedRun(), captured); QCOMPARE(project.simulationRevision(), revision);
        QVERIFY(project.canApprove());
        pipe.showCurrentResult(); QVERIFY(!pipe.viewingImportedEvidence()); QCOMPARE(pipe.result(), current); QVERIFY(pipe.canRun());
        pipe.importResult(QUrl::fromLocalFile(path)); QTRY_VERIFY_WITH_TIMEOUT(!pipe.busy(), 10000);
        project.editSelected("flow", "10"); QVERIFY(!project.canApprove());
        QVERIFY(pipe.viewingImportedEvidence()); QCOMPARE(pipe.result(), current); QVERIFY(!pipe.canRun());
        project.reset(); QVERIFY(!project.canApprove());
        QVERIFY(pipe.viewingImportedEvidence()); QCOMPARE(pipe.result(), current);
        pipe.showCurrentResult(); QVERIFY(pipe.result().isEmpty()); QVERIFY(!pipe.canRun()); QVERIFY(!project.canApprove());
    }
    void rejectedImportsPreserveCurrentOrPreviouslyImportedEvidenceAtomically() {
        QTemporaryDir directory; QVERIFY(directory.isValid()); const auto good = directory.filePath("good.json");
        ProjectController project; QVERIFY(capture(project, "mixed")); project.select("truck_route");
        PipeTransportController pipe(&project); pipe.run(); QTRY_VERIFY_WITH_TIMEOUT(!pipe.busy(), 10000);
        const auto current = pipe.result(), document = project.document().toVariantMap();
        QVERIFY(pipe.exportResult(QUrl::fromLocalFile(good)));
        for (const bool imported : {false, true}) {
            if (imported) { pipe.importResult(QUrl::fromLocalFile(good)); QTRY_VERIFY_WITH_TIMEOUT(!pipe.busy(), 10000); }
            for (const auto& name : {"missing.json", "malformed.json", "oversized.json", "wrong-version.json"}) {
                const auto path = directory.filePath(QLatin1String(name));
                if (QLatin1String(name) != "missing.json") {
                    QFile file(path); QVERIFY(file.open(QIODevice::WriteOnly));
                    if (QLatin1String(name) == "oversized.json") QVERIFY(file.write(QByteArray(maximumPipeEvidenceBytes + 1, ' ')) > maximumPipeEvidenceBytes);
                    else if (QLatin1String(name) == "malformed.json") QCOMPARE(file.write("{broken"), 7);
                    else { auto object = QJsonObject::fromVariantMap(current); object["result_version"] = 2; QVERIFY(file.write(QJsonDocument(object).toJson()) > 0); }
                }
                pipe.importResult(QUrl::fromLocalFile(path)); QVERIFY(pipe.busy()); QTRY_VERIFY_WITH_TIMEOUT(!pipe.busy(), 10000);
                QCOMPARE(pipe.viewingImportedEvidence(), imported); QCOMPARE(pipe.result(), current);
                QCOMPARE(project.document().toVariantMap(), document); QVERIFY(project.canApprove());
            }
            pipe.importResult(QUrl("https://example.invalid/evidence.json")); QVERIFY(!pipe.busy());
            QCOMPARE(pipe.viewingImportedEvidence(), imported); QCOMPARE(pipe.result(), current); QVERIFY(project.canApprove());
        }
    }
    void cancelledImportsCannotPublishLateOrOverwriteNativeEvidence() {
        QTemporaryDir directory; QVERIFY(directory.isValid()); const auto path = directory.filePath("native.json");
        ProjectController project; QVERIFY(capture(project, "mixed")); project.select("truck_route");
        PipeTransportController pipe(&project); pipe.run(); QTRY_VERIFY_WITH_TIMEOUT(!pipe.busy(), 10000);
        const auto current = pipe.result(); QVERIFY(pipe.exportResult(QUrl::fromLocalFile(path)));
        pipe.importResult(QUrl::fromLocalFile(path)); QVERIFY(pipe.busy()); pipe.cancel();
        pipe.importResult(QUrl::fromLocalFile(path)); pipe.run(); // Neither job may overlap the retiring import.
        QVERIFY(pipe.busy()); QTRY_VERIFY_WITH_TIMEOUT(!pipe.busy(), 10000);
        QVERIFY(!pipe.viewingImportedEvidence()); QCOMPARE(pipe.result(), current); QVERIFY(pipe.canRun());
        QVERIFY(pipe.status().contains("cancelled")); QVERIFY(project.canApprove());
        QPointer<PipeTransportController> retired = new PipeTransportController(&project);
        retired->importResult(QUrl::fromLocalFile(path)); QVERIFY(retired->busy()); delete retired.data(); QVERIFY(retired.isNull());
        bool serviced = false; QTimer::singleShot(0, &project, [&] { serviced = true; }); QTRY_VERIFY(serviced);
        pipe.importResult(QUrl::fromLocalFile(path)); QVERIFY(pipe.busy()); project.reset();
        QTRY_VERIFY_WITH_TIMEOUT(!pipe.busy(), 10000);
        QVERIFY(pipe.viewingImportedEvidence()); QCOMPARE(pipe.result(), current);
        QVERIFY(project.capturedRun().isEmpty()); QVERIFY(!project.canApprove());
    }
};
QTEST_GUILESS_MAIN(PipeTransportTests)
#include "pipe_transport_tests.moc"
