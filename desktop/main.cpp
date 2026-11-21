#include "ProjectController.hpp"
#include "WorkspaceSettings.hpp"
#include "LocalRuntime.hpp"
#include "verification/WorkbenchUiCheck.hpp"
#include "verification/EogWorkspaceCheck.h"
#include "verification/SceneBenchmark.hpp"
#include "agent/AgentController.hpp"
#include "agent/ObservedInvestigationController.hpp"
#include "agent/CredentialSettings.hpp"
#include "history/HistoryController.hpp"
#include "history/RehearsalArchiveCoordinator.hpp"
#include "quality/PipeTransportController.hpp"
#include "science/PythonWorkerClient.hpp"
#include "science/StorageScienceController.hpp"
#include "telemetry/TelemetryController.hpp"
#include "eog/EogDatasetController.hpp"
#include "eog/EogAnalysisController.hpp"
#include "agent/NemotronClient.hpp"
#include <QGuiApplication>
#include <QDir>
#include <QElapsedTimer>
#include <QFileInfo>
#include <QImage>
#include <QIcon>
#include <QJsonArray>
#include <QJsonDocument>
#include <QSaveFile>
#include <limits>
#include <QSet>
#include <QUuid>
#include "history/EvidenceEnvelope.hpp"
#include <QQmlApplicationEngine>
#include <QQuickStyle>
#include <QQuickWindow>
#include <QScreen>
#include <QTimer>
#include <QTemporaryDir>
#include <memory>

namespace {
QString resolvedLocalPath(const QString& path) {
    const QFileInfo info(path);
    return info.exists() ? info.canonicalFilePath() : info.absoluteFilePath();
}
bool eogOutputSafe(const breakwater::desktop::eog::EogDatasetController& eog, const QString& output) {
    if (!eog.loaded()) return true;
    for (const auto* name : {"network.json", "ops.json", "telemetry.csv"})
        if (resolvedLocalPath(output).compare(resolvedLocalPath(QDir(eog.sourceFolder()).filePath(name)), Qt::CaseInsensitive) == 0)
            return false;
    return true;
}
}

int main(int argc, char** argv) {
    QGuiApplication app(argc, argv);
    QCoreApplication::setApplicationName("BREAKWATER");
    QCoreApplication::setOrganizationName("Breakwater");
    QCoreApplication::setApplicationVersion(BREAKWATER_APP_VERSION);
    QGuiApplication::setWindowIcon(QIcon(":/breakwater/assets/breakwater.ico"));
    breakwater::desktop::runtime::configureBundledPython();
    const auto arguments = app.arguments();
    const bool uiOverhaulCheck = arguments.contains("--ui-overhaul-check");
    const bool eogWorkspaceCheck = arguments.contains("--eog-workspace-check");
    std::unique_ptr<QTemporaryDir> eogWorkspaceFixture;
    QString startupProject, startupObjective, rehearsalArchive, startupHistory, startupTelemetryReplay;
    QString startupWorkspace, startupEogDataset, startupEogFacility, eogReport, benchmarkOutput;
    const auto singleValue = [&](const QString& name, QString& destination) {
        if (!arguments.contains(name)) return true;
        const auto index = arguments.indexOf(name);
        if (arguments.count(name) != 1 || index + 1 >= arguments.size() ||
            arguments[index + 1].startsWith("--") || arguments[index + 1].trimmed().isEmpty()) return false;
        destination = arguments[index + 1];
        return true;
    };
    const bool startupInvestigation = arguments.contains("--investigate");
    if (!singleValue("--scene-benchmark", benchmarkOutput)) return 8;
    const bool benchmark = !benchmarkOutput.isEmpty();
    if (benchmark) {
        if (!QDir::isAbsolutePath(benchmarkOutput) || QFileInfo::exists(benchmarkOutput)) return 8;
        for (const auto* flag : {"--ui-overhaul-check", "--eog-workspace-check", "--investigate", "--smoke", "--screenshot", "--science-check", "--import-session",
                 "--eog-dataset", "--eog-analysis-panel", "--workspace", "--worker-health", "--agent-panel", "--sensor-editor-check", "--telemetry-replay"})
            if (arguments.contains(QLatin1String(flag))) return 8;
    }
    if (!singleValue("--workspace", startupWorkspace) ||
        !singleValue("--eog-dataset", startupEogDataset) ||
        !singleValue("--eog-facility", startupEogFacility) ||
        !singleValue("--eog-report", eogReport) ||
        (!startupWorkspace.isEmpty() && startupWorkspace != "eog" && startupWorkspace != "workbench")) {
        qCritical("Use --workspace eog|workbench and single local EOG dataset/report arguments."); return 8;
    }
    if (!singleValue("--project", startupProject) || !singleValue("--objective", startupObjective) ||
        !singleValue("--telemetry-replay", startupTelemetryReplay) ||
        !singleValue("--rehearsal-archive", rehearsalArchive) ||
        (startupInvestigation && !singleValue("--history-db", startupHistory)) ||
        arguments.count("--investigate") > 1 || startupObjective.size() > 4000 ||
        (!startupObjective.isEmpty() && !startupInvestigation) ||
        (startupInvestigation && startupObjective.trimmed().isEmpty()) ||
        (!rehearsalArchive.isEmpty() && (!startupInvestigation || startupHistory.isEmpty())) ||
        ((!startupProject.isEmpty() || startupInvestigation) && arguments.contains("--science-check")) ||
        (startupInvestigation && (arguments.contains("--smoke") || arguments.contains("--screenshot") ||
            arguments.contains("--import-session") || arguments.contains("--import-pipe-evidence") ||
            arguments.contains("--open-scientific-history") || arguments.contains("--sensor-editor-check") ||
            !startupTelemetryReplay.isEmpty()))) {
        qCritical("Invalid startup options. --investigate requires one explicit --objective; rehearsal export also requires --history-db. Do not combine live investigation with smoke/import/science modes.");
        return 8;
    }
    if (uiOverhaulCheck) {
        if (arguments.count("--ui-overhaul-check") != 1 || startupWorkspace == "eog") return 8;
        for (const auto* flag : {"--project", "--investigate", "--history-db", "--science-check", "--smoke", "--screenshot",
                 "--import-session", "--import-pipe-evidence", "--open-scientific-history", "--telemetry-replay", "--sensor-editor-check",
                 "--eog-dataset", "--eog-facility", "--eog-report", "--eog-analysis-panel", "--worker-health", "--agent-panel"})
            if (arguments.contains(QLatin1String(flag))) return 8;
    }
    if (eogWorkspaceCheck) {
        if (arguments.count("--eog-workspace-check") != 1 || startupWorkspace != "eog" || startupEogDataset.isEmpty()) return 8;
        for (const auto* flag : {"--ui-overhaul-check", "--project", "--investigate", "--history-db", "--science-check", "--smoke", "--screenshot",
                 "--import-session", "--import-pipe-evidence", "--open-scientific-history", "--telemetry-replay", "--sensor-editor-check",
                 "--eog-facility", "--eog-report", "--eog-analysis-panel", "--worker-health", "--agent-panel", "--rehearsal-archive"})
            if (arguments.contains(QLatin1String(flag))) return 8;
    }
    bool workbenchOperation = uiOverhaulCheck || !startupProject.isEmpty() || startupInvestigation || !startupTelemetryReplay.isEmpty();
    for (const auto& flag : {"--science-check", "--import-session", "--import-pipe-evidence",
             "--open-scientific-history", "--sensor-editor-check", "--agent-panel", "--worker-health"})
        workbenchOperation |= arguments.contains(flag);
    if ((!startupEogDataset.isEmpty() && (workbenchOperation || startupWorkspace == "workbench")) ||
        (startupWorkspace == "eog" && workbenchOperation) ||
        ((!startupEogFacility.isEmpty() || !eogReport.isEmpty()) && startupEogDataset.isEmpty()) ||
        (!eogReport.isEmpty() && !arguments.contains("--smoke") && !arguments.contains("--screenshot"))) {
        qCritical("EOG input cannot be combined with workbench operations; an EOG render report requires a dataset and smoke/screenshot mode."); return 8;
    }
    if (startupWorkspace.isEmpty()) startupWorkspace = startupEogDataset.isEmpty() ? "workbench" : "eog";
    const bool eogAnalysisPanel = arguments.contains("--eog-analysis-panel");
    if (arguments.count("--eog-analysis-panel") > 1 || (eogAnalysisPanel && startupWorkspace != "eog")) {
        qCritical("--eog-analysis-panel requires the EOG workspace."); return 8;
    }
    if (!startupEogDataset.isEmpty()) {
        const QFileInfo directory(startupEogDataset);
        if (!directory.isDir()) { qCritical("The EOG dataset must be a local directory."); return 8; }
        startupEogDataset = directory.canonicalFilePath();
        QString screenshot, eogHistory;
        if (!singleValue("--screenshot", screenshot) || !singleValue("--history-db", eogHistory) ||
            arguments.contains("--health-report") || arguments.contains("--science-report")) {
            qCritical("EOG startup accepts its own report only; legacy worker reports belong to workbench mode."); return 8;
        }
        const QStringList outputs{eogReport, screenshot, eogHistory};
        QStringList resolvedOutputs;
        for (const auto& output : outputs) {
            if (output.isEmpty()) continue;
            const auto resolved = resolvedLocalPath(output);
            if (resolvedOutputs.contains(resolved.toCaseFolded())) {
                qCritical("EOG report, screenshot and history outputs must be distinct."); return 8;
            }
            resolvedOutputs.append(resolved.toCaseFolded());
            for (const auto* name : {"network.json", "ops.json", "telemetry.csv"})
                if (resolved.compare(resolvedLocalPath(QDir(startupEogDataset).filePath(name)), Qt::CaseInsensitive) == 0) {
                    qCritical("EOG outputs must be separate from the three immutable input files."); return 8;
                }
        }
        if (!eogHistory.isEmpty()) for (const auto& output : {eogReport, screenshot}) {
            if (output.isEmpty()) continue;
            for (const auto* suffix : {"-wal", "-shm", "-journal"})
                if (resolvedLocalPath(output).compare(resolvedLocalPath(eogHistory + suffix), Qt::CaseInsensitive) == 0) {
                    qCritical("EOG reports must be separate from history journals."); return 8;
                }
        }
    }
    if (!rehearsalArchive.isEmpty()) {
        const auto archive = QFileInfo(rehearsalArchive);
        const auto database = QFileInfo(startupHistory).absoluteFilePath();
        bool reserved = false;
        for (const auto suffix : {QString{}, QStringLiteral("-wal"), QStringLiteral("-shm"), QStringLiteral("-journal")})
            reserved |= archive.absoluteFilePath().compare(database + suffix, Qt::CaseInsensitive) == 0;
        if (archive.exists() || reserved || !QDir().mkpath(archive.absolutePath())) {
            qCritical("The rehearsal archive must be a new local file separate from the history database and journals.");
            return 8;
        }
        rehearsalArchive = archive.absoluteFilePath();
    }
    if (eogWorkspaceCheck) {
        eogWorkspaceFixture = breakwater::desktop::prepareEogWorkspaceFixture(startupEogDataset);
        if (!eogWorkspaceFixture) { qCritical("Cannot prepare the isolated EOG workspace check."); return 8; }
        startupEogDataset = eogWorkspaceFixture->path();
        qputenv("NVIDIA_API_KEY", QByteArray{});
    }
    QQuickStyle::setStyle("Basic");
    breakwater::desktop::ProjectController controller;
    breakwater::desktop::eog::EogDatasetController eog;
    if (!startupProject.isEmpty()) {
        const QFileInfo projectFile(startupProject);
        if (!projectFile.isFile() || projectFile.size() > 2 * 1024 * 1024 ||
            !controller.load(QUrl::fromLocalFile(projectFile.absoluteFilePath()))) {
            qCritical("The startup project must be a readable supported editor project no larger than 2 MiB.");
            return 8;
        }
    } else if (startupInvestigation) controller.loadExample("baseline");
    const bool sensorEditorCheck = arguments.contains("--sensor-editor-check");
    if (sensorEditorCheck && !controller.addSensor("inventory", "tank_a")) {
        qCritical("Cannot prepare the sensor editor render check."); return 8;
    }
    breakwater::desktop::telemetry::TelemetryController telemetry;
    QObject::connect(&telemetry, &breakwater::desktop::telemetry::TelemetryController::failed, &app,
        [&](const QString&, const QString&) { if (!startupTelemetryReplay.isEmpty()) app.exit(9); });
    QJsonObject telemetryConfiguration;
    quint64 telemetryRevision=std::numeric_limits<quint64>::max();
    const auto configureTelemetry = [&] {
        const auto configuration = controller.sensorConfiguration();
        if (configuration == telemetryConfiguration && telemetryRevision==controller.simulationRevision()) return;
        if (configuration != telemetryConfiguration) {
            telemetryConfiguration=configuration; telemetry.configure(configuration,controller.sensorContext());
        }
        telemetryRevision=controller.simulationRevision();
        if (!configuration.value("sensors").toArray().isEmpty() && controller.draftIssues().isEmpty()) {
          try {
            auto registry=controller.sensorContext(); registry["configuration"]=configuration;
            QJsonArray commands;
            for (const auto& control:breakwater::control_catalog(breakwater::desktop::parse_project(controller.document()).network))
                commands.append(QJsonObject{{"control_id",QString::fromStdString(control.id)},{"value",control.value},{"unit",QString::fromStdString(control.unit)}});
            telemetry.setObservedCaptureContext(registry,controller.simulationRevision(),commands);
          } catch (const std::exception&) { telemetry.clearObservedCaptureContext(); }
        } else telemetry.clearObservedCaptureContext();
    };
    QObject::connect(&controller, &breakwater::desktop::ProjectController::sensorConfigurationChanged, &telemetry, configureTelemetry);
    QObject::connect(&controller, &breakwater::desktop::ProjectController::changed, &telemetry, configureTelemetry);
    configureTelemetry();
    if (startupInvestigation && !controller.draftIssues().isEmpty()) {
        qCritical("The startup project has draft issues; no investigation was started.");
        return 8;
    }
    const auto historyIndex = arguments.indexOf("--history-db");
    std::unique_ptr<QTemporaryDir> uiCheckStorage;
    if (uiOverhaulCheck || eogWorkspaceCheck || benchmark) {
        uiCheckStorage = std::make_unique<QTemporaryDir>();
        if (!uiCheckStorage->isValid()) return 8;
    }
    const auto historyPath = uiCheckStorage ? uiCheckStorage->filePath("ui-check.sqlite")
        : historyIndex >= 0 && historyIndex + 1 < arguments.size() ? arguments[historyIndex + 1] : QString{};
    breakwater::desktop::history::SessionRepository repository(historyPath);
    QSet<QString> pendingIncidentHistory;
    QObject::connect(&repository,&breakwater::desktop::history::SessionRepository::ready,&telemetry,[&] {
        telemetry.setEvidenceStorageStatus("Incident transitions and bounded updates are saved automatically to History.");
    });
    QObject::connect(&repository,&breakwater::desktop::history::SessionRepository::sessionSaved,&telemetry,[&](const QString& id) {
        if (pendingIncidentHistory.remove(id)) telemetry.setEvidenceStorageStatus("Incident evidence saved to History.");
    });
    QObject::connect(&repository,&breakwater::desktop::history::SessionRepository::checkpointFailed,&telemetry,
        [&](const QString& id,const QString&,const QString& message) {
            if (pendingIncidentHistory.remove(id)) telemetry.setEvidenceStorageStatus("Incident history error: "+message+" Acquisition remains available.");
        });
    QObject::connect(&repository,&breakwater::desktop::history::SessionRepository::failed,&telemetry,
        [&](const QString& code,const QString& message) {
            if (code=="storage_unavailable" || code=="storage_corrupt" || code=="unsupported_database_version" || code.startsWith("migration_"))
                telemetry.setEvidenceStorageStatus("Incident history unavailable: "+message);
        });
    QObject::connect(&telemetry,&breakwater::desktop::telemetry::TelemetryController::detectionEvidenceReady,&repository,
        [&](const QJsonObject& evidence) {
            const auto id=QUuid::createUuid().toString(QUuid::WithoutBraces); pendingIncidentHistory.insert(id);
            telemetry.setEvidenceStorageStatus("Saving incident evidence…");
            repository.enqueueCheckpoint([evidence,id] {
                const auto context=evidence.value("context").toObject(),capture=evidence.value("snapshot").toObject();
                const QJsonObject snapshot{{"snapshot_id",capture.value("snapshot_id")},{"physical_hash",context.value("physical_hash")},
                    {"network",context.value("network")},{"horizon_h",3.0}};
                const auto kind=evidence.value("evidence_kind").toString();
                return breakwater::desktop::history::evidence_archive(kind,evidence,snapshot,
                    kind=="observation_acknowledgement" ? "Observed incident acknowledgement":"Observed incident and pre-assimilation witness",id);
            },id);
        });
    bool historyReady = false;
    QObject::connect(&repository, &breakwater::desktop::history::SessionRepository::ready, &app, [&] { historyReady = true; });
    breakwater::desktop::history::HistoryController history(&repository);
    // The EOG UI probe must not discover checkout or saved-user credentials.
    // Explicit transports also outlive the controllers that borrow them.
    std::unique_ptr<breakwater::desktop::eog_workspace_check::RejectingModelTransport> isolatedWorkbenchModel, isolatedObservedModel, isolatedEogModel;
    if (eogWorkspaceCheck) {
        isolatedWorkbenchModel = std::make_unique<breakwater::desktop::eog_workspace_check::RejectingModelTransport>();
        isolatedObservedModel = std::make_unique<breakwater::desktop::eog_workspace_check::RejectingModelTransport>();
        isolatedEogModel = std::make_unique<breakwater::desktop::eog_workspace_check::RejectingModelTransport>();
    }
    breakwater::desktop::agent::AgentController investigator(&controller, isolatedWorkbenchModel.get());
    breakwater::desktop::history::HistoryController observedHistory(&repository);
    breakwater::desktop::agent::ObservedInvestigationController observedInvestigator(&controller,&telemetry,isolatedObservedModel.get());
    observedInvestigator.setHistory(&repository,&observedHistory);
    breakwater::desktop::agent::CredentialSettings credentials(nullptr,
        eogWorkspaceCheck ? uiCheckStorage->filePath("credentials.env") : QString{});
    // EOG has separate provider processes, model conversation and history. The
    // workbench investigator and its approval state retain their own ownership.
    breakwater::desktop::eog::EogAnalysisClient eogClient;
    breakwater::desktop::eog::EogHistoryRepository eogHistory(historyPath.isEmpty() ? QString{} : historyPath + ".eog.sqlite");
    std::unique_ptr<breakwater::desktop::agent::NemotronClient> eogModel;
    if (!eogWorkspaceCheck) eogModel = std::make_unique<breakwater::desktop::agent::NemotronClient>();
    auto* eogTransport = eogWorkspaceCheck
        ? static_cast<breakwater::desktop::agent::ModelTransport*>(isolatedEogModel.get()) : eogModel.get();
    if (eogWorkspaceCheck && (credentials.configured() || credentials.saved())) {
        qCritical("EOG workspace verification must use isolated empty credentials."); return 8;
    }
    breakwater::desktop::eog::EogAnalysisController eogAnalysis(&eog, &eogClient, &eogHistory, eogTransport);
    investigator.setHistory(&repository, &history);
    breakwater::desktop::quality::PipeTransportController pipeTransport(&controller, &history);
    breakwater::desktop::science::PythonWorkerClient scientificWorker;
    breakwater::desktop::science::StorageScienceController storageScience(&controller, &scientificWorker);
    storageScience.setHistoryRepository(&repository);
    storageScience.setHistoryController(&history);
    pipeTransport.setHistoryRepository(&repository);
    const bool scienceCheck = arguments.contains("--science-check");
    const bool scienceDerivatives = scienceCheck && arguments.contains("--science-derivatives");
    QStringList scienceControls;
    bool scienceControlArgumentsValid = true;
    for (qsizetype index = 0; index < arguments.size(); ++index) {
        if (arguments[index] != "--science-control") continue;
        if (index + 1 >= arguments.size() || arguments[index + 1].startsWith("--") || scienceControls.contains(arguments[index + 1]))
            scienceControlArgumentsValid = false;
        else scienceControls.append(arguments[++index]);
    }
    const auto scienceReportIndex = arguments.indexOf("--science-report");
    bool scienceReportFailed = false;
    const auto writeScienceReport = [&](const QJsonObject& report) {
        if (scienceReportIndex < 0 || scienceReportIndex + 1 >= arguments.size()) return;
        if (!eogOutputSafe(eog, arguments[scienceReportIndex + 1])) {
            scienceReportFailed = true; qWarning("Scientific reports cannot replace loaded EOG input files.");
            if (scienceCheck && (arguments.contains("--smoke") || arguments.contains("--screenshot"))) app.exit(7);
            return;
        }
        QSaveFile file(arguments[scienceReportIndex + 1]);
        const auto bytes = QJsonDocument(report).toJson(QJsonDocument::Indented);
        if (bytes.size() > 4 * 1024 * 1024 || !file.open(QIODevice::WriteOnly) || file.write(bytes) != bytes.size() || !file.commit()) {
            scienceReportFailed = true;
            qWarning("Cannot save the requested scientific evidence report.");
            if (scienceCheck && (arguments.contains("--smoke") || arguments.contains("--screenshot"))) app.exit(7);
        }
    };
    QObject::connect(&storageScience, &breakwater::desktop::science::StorageScienceController::completed, &app, writeScienceReport);
    QObject::connect(&storageScience, &breakwater::desktop::science::StorageScienceController::failed, &app,
        [&](const QString& code, const QString& message) {
            writeScienceReport({{"status", "failed"}, {"error", QJsonObject{{"code", code}, {"message", message}}}});
            if (scienceCheck && (arguments.contains("--smoke") || arguments.contains("--screenshot"))) app.exit(scienceReportFailed ? 7 : 6);
        });
    const auto reportIndex = arguments.indexOf("--health-report");
    if (reportIndex >= 0 && reportIndex + 1 < arguments.size()) {
        const auto writeReport = [path = arguments[reportIndex + 1], &eog](const QJsonObject& report) {
            if (!eogOutputSafe(eog, path)) { qWarning("Worker reports cannot replace loaded EOG input files."); return; }
            QSaveFile file(path);
            if (file.open(QIODevice::WriteOnly)) {
                const auto bytes = QJsonDocument(report).toJson(QJsonDocument::Indented);
                if (file.write(bytes) == bytes.size()) file.commit();
            }
        };
        QObject::connect(&scientificWorker, &breakwater::desktop::science::PythonWorkerClient::healthReady, &app, writeReport);
        QObject::connect(&scientificWorker, &breakwater::desktop::science::PythonWorkerClient::failed, &app,
            [&scientificWorker, writeReport](const QString& code, const QString& message) {
                if (scientificWorker.operation() == "health")
                    writeReport({{"status", "unavailable"}, {"error", QJsonObject{{"code", code}, {"message", message}}}});
            });
    }
    std::unique_ptr<QTemporaryDir> preferenceStorage;
    if (uiOverhaulCheck || eogWorkspaceCheck || benchmark || arguments.contains("--smoke") || arguments.contains("--screenshot"))
        preferenceStorage = std::make_unique<QTemporaryDir>();
    if (preferenceStorage && !preferenceStorage->isValid()) {
        qCritical("Cannot prepare isolated verification preferences."); return 8;
    }
    breakwater::desktop::WorkspaceSettings workspaceSettings(preferenceStorage ? preferenceStorage->filePath("workspace.ini") : QString{});
    auto portfolioExamples = breakwater::desktop::runtime::exampleFiles(QString::fromUtf8(BREAKWATER_EXAMPLE_DIRECTORY), QString::fromUtf8(BREAKWATER_RUNTIME_DEV_BIN));
    if (eogWorkspaceCheck) {
        portfolioExamples["operations"] = QUrl::fromLocalFile(startupEogDataset);
        portfolioExamples["operationsPath"] = startupEogDataset;
    }
    QQmlApplicationEngine engine;
    engine.setInitialProperties({{"controller", QVariant::fromValue(&controller)}, {"investigator", QVariant::fromValue(&investigator)},
        {"observedInvestigator",QVariant::fromValue(&observedInvestigator)},{"observedHistory",QVariant::fromValue(&observedHistory)},
        {"history", QVariant::fromValue(&history)}, {"scientificWorker", QVariant::fromValue(&scientificWorker)},
        {"pipeTransport", QVariant::fromValue(&pipeTransport)}, {"storageScience", QVariant::fromValue(&storageScience)},
        {"credentials", QVariant::fromValue(&credentials)}, {"telemetry", QVariant::fromValue(&telemetry)},
        {"eog", QVariant::fromValue(&eog)}, {"eogAnalysis", QVariant::fromValue(&eogAnalysis)}, {"initialWorkspace", startupWorkspace},
        {"workspaceSettings", QVariant::fromValue(&workspaceSettings)},
        {"availableClientSize", app.primaryScreen() ? app.primaryScreen()->availableGeometry().size() - QSize(16, 56) : QSize(1440, 900)},
        {"buildInfo", QString("%1 · %2").arg(BREAKWATER_APP_VERSION, BREAKWATER_SOURCE_REVISION)},
        {"portfolioExamples", portfolioExamples}});
    QObject::connect(&engine, &QQmlApplicationEngine::objectCreationFailed, &app, [] { QCoreApplication::exit(1); }, Qt::QueuedConnection);
    engine.loadFromModule("Breakwater", "Main");
    if (benchmark) {
        auto* window = engine.rootObjects().isEmpty() ? nullptr : qobject_cast<QQuickWindow*>(engine.rootObjects().front());
        if (!window) return 8;
        breakwater::desktop::startSceneBenchmark(window, &controller, &app, benchmarkOutput);
    }
    if (uiOverhaulCheck) {
        auto* window = engine.rootObjects().isEmpty() ? nullptr : qobject_cast<QQuickWindow*>(engine.rootObjects().front());
        if (!window) return 8;
        breakwater::desktop::startWorkbenchUiCheck(window, &controller, &app);
    }
    if (eogWorkspaceCheck) {
        auto* window = engine.rootObjects().isEmpty() ? nullptr : qobject_cast<QQuickWindow*>(engine.rootObjects().front());
        if (!window) return 8;
        breakwater::desktop::startEogWorkspaceCheck(window, &controller, &eog, &eogAnalysis, &app,
            {isolatedWorkbenchModel.get(), isolatedObservedModel.get(), isolatedEogModel.get()});
    }
    if (eogAnalysisPanel && !engine.rootObjects().isEmpty())
        QMetaObject::invokeMethod(engine.rootObjects().front(), "openEogAnalysis");
    const auto workbenchBeforeEog = controller.editorDocument();
    bool startupEogPending = !startupEogDataset.isEmpty() && !eogWorkspaceCheck;
    if (startupEogPending) {
        QObject::connect(&eog, &breakwater::desktop::eog::EogDatasetController::completed, &app, [&] {
            if (!startupEogPending) return;
            startupEogPending = false;
            if (startupEogFacility.isEmpty()) return;
            if (!eog.capturedDataset()->facilities.contains(startupEogFacility)) {
                qCritical("The requested EOG facility is not in this dataset."); app.exit(8); return;
            }
            eog.showFacility(startupEogFacility);
        });
        QObject::connect(&eog, &breakwater::desktop::eog::EogDatasetController::failed, &app,
            [&](const QString&) {
                if (!startupEogPending) return;
                startupEogPending = false;
                qCritical("The startup EOG input was rejected; source observations were not replaced."); app.exit(10);
            });
        QObject::connect(&eog, &breakwater::desktop::eog::EogDatasetController::cancelled, &app,
            [&] { startupEogPending = false; });
        QTimer::singleShot(0, &eog, [&] { eog.loadFolder(QUrl::fromLocalFile(startupEogDataset)); });
    }
    if (sensorEditorCheck || !startupTelemetryReplay.isEmpty()) {
        QTimer::singleShot(0, &app, [&] {
            if (engine.rootObjects().isEmpty()) return;
            if (sensorEditorCheck) QMetaObject::invokeMethod(engine.rootObjects().front(), "openSensors");
            if (!startupTelemetryReplay.isEmpty()) {
                telemetry.startReplay(QUrl::fromLocalFile(QFileInfo(startupTelemetryReplay).absoluteFilePath()));
                QMetaObject::invokeMethod(engine.rootObjects().front(), "openTelemetry");
            }
        });
    }
    bool startupLaunched = false;
    const auto rehearsalFailed = [&](const QString& code) {
        if (!engine.rootObjects().isEmpty()) {
            if (auto* window = qobject_cast<QQuickWindow*>(engine.rootObjects().front()))
                window->setTitle("BREAKWATER | Rehearsal archive failed (" + code + ")");
        }
        qWarning("Approved rehearsal export failed (%s). Its database evidence was retained; no success is claimed.", qPrintable(code));
    };
    breakwater::desktop::history::RehearsalArchiveCoordinator rehearsal(controller, investigator, repository, rehearsalArchive,
        [&] {
            if (!engine.rootObjects().isEmpty()) {
                if (auto* window = qobject_cast<QQuickWindow*>(engine.rootObjects().front()))
                    window->setTitle("BREAKWATER | Approved rehearsal archived");
            }
            qInfo("Approved rehearsal evidence was exported after operator review and a fresh feasible native forecast.");
        }, rehearsalFailed);
    if (startupInvestigation) {
        const auto startInvestigation = [&] {
            if (startupLaunched || !historyReady || engine.rootObjects().isEmpty()) return;
            startupLaunched = true;
            rehearsal.begin();
            engine.rootObjects().front()->setProperty("showAgent", true);
            investigator.investigate(startupObjective);
        };
        QObject::connect(&repository, &breakwater::desktop::history::SessionRepository::ready, &app, startInvestigation);
        QTimer::singleShot(0, &app, startInvestigation);
        QObject::connect(&repository, &breakwater::desktop::history::SessionRepository::failed, &app,
            [&](const QString&, const QString&) {
                if (!startupLaunched) { qCritical("History is unavailable; no startup investigation was launched."); app.exit(9); }
            });
    }
    const auto importIndex = arguments.indexOf("--import-session");
    const bool scientificHistoryCheck = arguments.contains("--open-scientific-history");
    if (scientificHistoryCheck) {
        QObject::connect(&history, &breakwater::desktop::history::HistoryController::changed, &app, [&] {
            if (history.historical() && !history.scientificResult().isEmpty() && !engine.rootObjects().isEmpty())
                QMetaObject::invokeMethod(engine.rootObjects().front(), "openStorageScience");
        });
    }
    if (importIndex >= 0 && importIndex + 1 < arguments.size())
        QTimer::singleShot(0, &history, [&history, path = arguments[importIndex + 1]] { history.importSession(QUrl::fromLocalFile(path)); });
    const auto pipeImportIndex = arguments.indexOf("--import-pipe-evidence");
    if (pipeImportIndex >= 0 && pipeImportIndex + 1 < arguments.size()) {
        QTimer::singleShot(0, &pipeTransport, [&, path = arguments[pipeImportIndex + 1]] {
            pipeTransport.importResult(QUrl::fromLocalFile(path));
            if (!engine.rootObjects().isEmpty()) QMetaObject::invokeMethod(engine.rootObjects().front(), "openPipeTransport");
        });
    }
    if (arguments.contains("--worker-health")) QTimer::singleShot(0, &scientificWorker, &breakwater::desktop::science::PythonWorkerClient::checkHealth);
    if (arguments.contains("--agent-panel") && !engine.rootObjects().isEmpty()) engine.rootObjects().front()->setProperty("showAgent", true);
    bool scienceStarted = false;
    if (scienceCheck) {
        const auto tryScience = [&] {
            if (scienceStarted || !storageScience.canRun()) return;
            scienceStarted = true;
            if (!scienceDerivatives) { storageScience.run(); return; }
            bool valid = scienceControlArgumentsValid && !scienceControls.isEmpty();
            for (const auto& id : scienceControls) valid = storageScience.selectControl(id, true) && valid;
            if (!valid || !storageScience.canRunSensitivities()) {
                writeScienceReport({{"status", "failed"}, {"error", QJsonObject{{"code", "invalid_controls"},
                    {"message", "Select at least one supported interior captured control for sensitivities."}}}});
                if (arguments.contains("--smoke") || arguments.contains("--screenshot")) app.exit(scienceReportFailed ? 7 : 6);
                return;
            }
            storageScience.runSensitivities();
        };
        QObject::connect(&controller, &breakwater::desktop::ProjectController::changed, &app, tryScience);
        QObject::connect(&scientificWorker, &breakwater::desktop::science::PythonWorkerClient::changed, &app, tryScience, Qt::QueuedConnection);
        QTimer::singleShot(0, &app, [&] {
            if (!engine.rootObjects().isEmpty()) QMetaObject::invokeMethod(engine.rootObjects().front(), "openStorageScience");
            const auto exampleIndex = arguments.indexOf("--science-example");
            controller.loadExample(exampleIndex >= 0 && exampleIndex + 1 < arguments.size() ? arguments[exampleIndex + 1] : "mixed");
            controller.forecast();
        });
    }
    if (arguments.contains("--smoke") || arguments.contains("--screenshot")) {
        auto* smokeTimer = new QTimer(&app);
        QElapsedTimer smokeElapsed; smokeElapsed.start();
        QObject::connect(smokeTimer, &QTimer::timeout, &app, [&, smokeTimer, smokeElapsed] {
            if (scienceReportFailed) { app.exit(7); return; }
            if (scienceCheck && storageScience.historyFailed() && !storageScience.historyPending()) {
                writeScienceReport({{"status", "failed"}, {"error", QJsonObject{{"code", "history_storage_failed"},
                    {"message", storageScience.historyStatus()}}}});
                app.exit(7); return;
            }
            if (engine.rootObjects().isEmpty()) { app.exit(2); return; }
            auto* window = qobject_cast<QQuickWindow*>(engine.rootObjects().front());
            const auto capture = window ? window->grabWindow() : QImage{};
            const bool labelsVisible = engine.rootObjects().front()->property("visibleAssetLabels").toInt() > 0;
            const bool eogMode = engine.rootObjects().front()->property("workspace").toString() == "eog";
            const bool eogPending = eogMode && !startupEogDataset.isEmpty() &&
                (eog.busy() || !eog.loaded() || !engine.rootObjects().front()->property("eogSceneReady").toBool() ||
                    engine.rootObjects().front()->property("eogVisibleLabels").toInt() < 1 ||
                    !engine.rootObjects().front()->property("eogTrendReady").toBool() ||
                    engine.rootObjects().front()->property("eogTrendSamples").toInt() < 1 ||
                    (eog.metric() == "level_pct" && engine.rootObjects().front()->property("eogTrendGuides").toInt() != 3));
            const bool pipeDialogVisible = engine.rootObjects().front()->property("pipeTransportVisible").toBool();
            const bool scienceVisible = engine.rootObjects().front()->property("storageScienceVisible").toBool();
            const bool sensorPending = sensorEditorCheck && (!engine.rootObjects().front()->property("sensorEditorVisible").toBool()
                || controller.sensorMarkers().isEmpty());
            const bool eogAnalysisPending = eogAnalysisPanel && !engine.rootObjects().front()->property("eogAnalysisVisible").toBool();
            const bool telemetryPending = !startupTelemetryReplay.isEmpty() &&
                (!engine.rootObjects().front()->property("telemetryVisible").toBool() || telemetry.tracePreview().isEmpty()
                    || engine.rootObjects().front()->property("telemetryTraceSamples").toInt() < 1 || telemetry.active());
            if (!startupTelemetryReplay.isEmpty() && !telemetry.error().isEmpty()) { app.exit(9); return; }
            const bool sciencePending = scienceCheck && (!scienceStarted || storageScience.busy() ||
                storageScience.historyPending() || storageScience.result().isEmpty() || !scienceVisible ||
                storageScience.historySavedResultId() != storageScience.result().value("result_id").toString() ||
                (scienceDerivatives && storageScience.result().value("analysis_mode").toString() != "sensitivities"));
            const bool evidencePending = (importIndex >= 0 && (history.busy() || history.selectedSessionId().isEmpty())) ||
                (scientificHistoryCheck && (!history.historical() || history.scientificResult().isEmpty() || !scienceVisible)) ||
                (pipeImportIndex >= 0 && (pipeTransport.busy() || !pipeTransport.viewingImportedEvidence() ||
                    !pipeDialogVisible));
            // An imported-evidence modal is the render target; scene labels behind
            // that modal are not a prerequisite for viewing its charts.
            if (capture.isNull() || (!eogMode && pipeImportIndex < 0 && !scienceCheck && !scientificHistoryCheck && !labelsVisible) || eogPending || eogAnalysisPending || evidencePending || sciencePending || sensorPending || telemetryPending || scientificWorker.busy()) {
                if (smokeElapsed.elapsed() >= (scienceCheck ? 90000 : 20000)) app.exit(capture.isNull() ? 3 : 5);
                else if (window) window->requestUpdate();
                return;
            }
            smokeTimer->stop();
            if (scienceCheck && !storageScience.result().value("within_tolerance").toBool()) { app.exit(6); return; }
            const auto index = arguments.indexOf("--screenshot");
            if (index >= 0 && index + 1 < arguments.size()) {
                const auto path = arguments[index + 1];
                QSaveFile screenshot(path);
                const auto format = QFileInfo(path).suffix().toLatin1();
                // Atomic replacement also preserves a source file if an output
                // pathname happens to be another hard link to its contents.
                if (!eogOutputSafe(eog, path) || !screenshot.open(QIODevice::WriteOnly) ||
                    !capture.save(&screenshot, format.constData()) || !screenshot.commit()) { app.exit(4); return; }
            }
            if (eogMode && !eogReport.isEmpty()) {
                const bool unchanged = controller.editorDocument() == workbenchBeforeEog;
                const auto data = eog.capturedDataset();
                const QJsonObject report{{"evidence_version", 1}, {"workspace", "eog"},
                    {"dataset_identity", eog.datasetIdentity()}, {"source_hashes", data ? data->source_hashes : QJsonObject{}},
                    {"cutoff_unix", eog.cursorUnix()}, {"scene_mode", eog.sceneMode()},
                    {"scene_nodes", eog.sceneNodes().size()}, {"scene_routes", eog.sceneRoutes().size()},
                    {"visible_labels", engine.rootObjects().front()->property("eogVisibleLabels").toInt()},
                    {"trend_ready", engine.rootObjects().front()->property("eogTrendReady").toBool()},
                    {"trend_samples", engine.rootObjects().front()->property("eogTrendSamples").toInt()},
                    {"trend_guides", engine.rootObjects().front()->property("eogTrendGuides").toInt()},
                    {"rendered_frame_ready", engine.rootObjects().front()->property("eogSceneReady").toBool()},
                    {"workbench_unchanged", unchanged}, {"analysis_status", "not_requested"},
                    {"provider_configured", eogClient.configured()}, {"provider_execution_verified", eogClient.executionVerified()},
                    {"analysis_panel_visible", engine.rootObjects().front()->property("eogAnalysisVisible").toBool()},
                    {"hosted_model_requests", "not_run"}};
                QSaveFile file(eogReport);
                const auto bytes = QJsonDocument(report).toJson(QJsonDocument::Indented);
                if (!unchanged || !eogOutputSafe(eog, eogReport) || !file.open(QIODevice::WriteOnly) || file.write(bytes) != bytes.size() || !file.commit()) {
                    qCritical("Cannot retain complete EOG render evidence."); app.exit(7); return;
                }
            }
            app.exit(0);
        });
        smokeTimer->start(100);
    }
    return app.exec();
}
