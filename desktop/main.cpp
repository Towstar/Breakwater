#include "ProjectController.hpp"
#include "agent/AgentController.hpp"
#include "agent/CredentialSettings.hpp"
#include "history/HistoryController.hpp"
#include "history/RehearsalArchiveCoordinator.hpp"
#include "quality/PipeTransportController.hpp"
#include "science/PythonWorkerClient.hpp"
#include "science/StorageScienceController.hpp"
#include "telemetry/TelemetryController.hpp"
#include <QGuiApplication>
#include <QDir>
#include <QElapsedTimer>
#include <QFileInfo>
#include <QImage>
#include <QJsonArray>
#include <QJsonDocument>
#include <QSaveFile>
#include <QQmlApplicationEngine>
#include <QQuickStyle>
#include <QQuickWindow>
#include <QTimer>

int main(int argc, char** argv) {
    QGuiApplication app(argc, argv);
    QCoreApplication::setApplicationName("BREAKWATER");
    QCoreApplication::setOrganizationName("Breakwater");
    const auto arguments = app.arguments();
    QString startupProject, startupObjective, rehearsalArchive, startupHistory, startupTelemetryReplay;
    const auto singleValue = [&](const QString& name, QString& destination) {
        if (!arguments.contains(name)) return true;
        const auto index = arguments.indexOf(name);
        if (arguments.count(name) != 1 || index + 1 >= arguments.size() ||
            arguments[index + 1].startsWith("--") || arguments[index + 1].trimmed().isEmpty()) return false;
        destination = arguments[index + 1];
        return true;
    };
    const bool startupInvestigation = arguments.contains("--investigate");
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
    QQuickStyle::setStyle("Basic");
    breakwater::desktop::ProjectController controller;
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
    const auto configureTelemetry = [&] {
        const auto configuration = controller.sensorConfiguration();
        if (configuration == telemetryConfiguration) return;
        telemetryConfiguration = configuration;
        telemetry.configure(configuration, controller.sensorContext());
    };
    QObject::connect(&controller, &breakwater::desktop::ProjectController::sensorConfigurationChanged, &telemetry, configureTelemetry);
    QObject::connect(&controller, &breakwater::desktop::ProjectController::changed, &telemetry, configureTelemetry);
    configureTelemetry();
    if (startupInvestigation && !controller.draftIssues().isEmpty()) {
        qCritical("The startup project has draft issues; no investigation was started.");
        return 8;
    }
    const auto historyIndex = arguments.indexOf("--history-db");
    const auto historyPath = historyIndex >= 0 && historyIndex + 1 < arguments.size() ? arguments[historyIndex + 1] : QString{};
    breakwater::desktop::history::SessionRepository repository(historyPath);
    bool historyReady = false;
    QObject::connect(&repository, &breakwater::desktop::history::SessionRepository::ready, &app, [&] { historyReady = true; });
    breakwater::desktop::history::HistoryController history(&repository);
    breakwater::desktop::agent::AgentController investigator(&controller);
    breakwater::desktop::agent::CredentialSettings credentials;
    investigator.setHistory(&repository, &history);
    breakwater::desktop::quality::PipeTransportController pipeTransport(&controller, &history);
    breakwater::desktop::science::PythonWorkerClient scientificWorker;
    breakwater::desktop::science::StorageScienceController storageScience(&controller, &scientificWorker);
    storageScience.setHistoryRepository(&repository);
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
        const auto writeReport = [path = arguments[reportIndex + 1]](const QJsonObject& report) {
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
    QQmlApplicationEngine engine;
    engine.setInitialProperties({{"controller", QVariant::fromValue(&controller)}, {"investigator", QVariant::fromValue(&investigator)},
        {"history", QVariant::fromValue(&history)}, {"scientificWorker", QVariant::fromValue(&scientificWorker)},
        {"pipeTransport", QVariant::fromValue(&pipeTransport)}, {"storageScience", QVariant::fromValue(&storageScience)},
        {"credentials", QVariant::fromValue(&credentials)}, {"telemetry", QVariant::fromValue(&telemetry)}});
    QObject::connect(&engine, &QQmlApplicationEngine::objectCreationFailed, &app, [] { QCoreApplication::exit(1); }, Qt::QueuedConnection);
    engine.loadFromModule("Breakwater", "Main");
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
            const bool pipeDialogVisible = engine.rootObjects().front()->property("pipeTransportVisible").toBool();
            const bool scienceVisible = engine.rootObjects().front()->property("storageScienceVisible").toBool();
            const bool sensorPending = sensorEditorCheck && (!engine.rootObjects().front()->property("sensorEditorVisible").toBool()
                || controller.sensorMarkers().isEmpty());
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
            if (capture.isNull() || (pipeImportIndex < 0 && !scienceCheck && !scientificHistoryCheck && !labelsVisible) || evidencePending || sciencePending || sensorPending || telemetryPending || scientificWorker.busy()) {
                if (smokeElapsed.elapsed() >= (scienceCheck ? 90000 : 20000)) app.exit(capture.isNull() ? 3 : 5);
                else if (window) window->requestUpdate();
                return;
            }
            smokeTimer->stop();
            if (scienceCheck && !storageScience.result().value("within_tolerance").toBool()) { app.exit(6); return; }
            const auto index = arguments.indexOf("--screenshot");
            if (index >= 0 && index + 1 < arguments.size() && !capture.save(arguments[index + 1])) { app.exit(4); return; }
            app.exit(0);
        });
        smokeTimer->start(100);
    }
    return app.exec();
}
