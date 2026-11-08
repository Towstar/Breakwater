#include "ProjectController.hpp"
#include "agent/AgentController.hpp"
#include "history/HistoryController.hpp"
#include "quality/PipeTransportController.hpp"
#include "science/PythonWorkerClient.hpp"
#include "science/StorageScienceController.hpp"
#include <QGuiApplication>
#include <QElapsedTimer>
#include <QImage>
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
    QQuickStyle::setStyle("Basic");
    breakwater::desktop::ProjectController controller;
    const auto historyIndex = arguments.indexOf("--history-db");
    const auto historyPath = historyIndex >= 0 && historyIndex + 1 < arguments.size() ? arguments[historyIndex + 1] : QString{};
    breakwater::desktop::history::SessionRepository repository(historyPath);
    breakwater::desktop::history::HistoryController history(&repository);
    breakwater::desktop::agent::AgentController investigator(&controller);
    investigator.setHistory(&repository, &history);
    breakwater::desktop::quality::PipeTransportController pipeTransport(&controller, &history);
    breakwater::desktop::science::PythonWorkerClient scientificWorker;
    breakwater::desktop::science::StorageScienceController storageScience(&controller, &scientificWorker);
    const bool scienceCheck = arguments.contains("--science-check");
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
        {"pipeTransport", QVariant::fromValue(&pipeTransport)}, {"storageScience", QVariant::fromValue(&storageScience)}});
    QObject::connect(&engine, &QQmlApplicationEngine::objectCreationFailed, &app, [] { QCoreApplication::exit(1); }, Qt::QueuedConnection);
    engine.loadFromModule("Breakwater", "Main");
    const auto importIndex = arguments.indexOf("--import-session");
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
            if (!scienceStarted && storageScience.canRun()) { scienceStarted = true; storageScience.run(); }
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
            if (engine.rootObjects().isEmpty()) { app.exit(2); return; }
            auto* window = qobject_cast<QQuickWindow*>(engine.rootObjects().front());
            const auto capture = window ? window->grabWindow() : QImage{};
            const bool labelsVisible = engine.rootObjects().front()->property("visibleAssetLabels").toInt() > 0;
            const bool pipeDialogVisible = engine.rootObjects().front()->property("pipeTransportVisible").toBool();
            const bool scienceVisible = engine.rootObjects().front()->property("storageScienceVisible").toBool();
            const bool sciencePending = scienceCheck && (!scienceStarted || storageScience.busy() ||
                storageScience.result().isEmpty() || !scienceVisible);
            const bool evidencePending = (importIndex >= 0 && (history.busy() || history.selectedSessionId().isEmpty())) ||
                (pipeImportIndex >= 0 && (pipeTransport.busy() || !pipeTransport.viewingImportedEvidence() ||
                    !pipeDialogVisible));
            // An imported-evidence modal is the render target; scene labels behind
            // that modal are not a prerequisite for viewing its charts.
            if (capture.isNull() || (pipeImportIndex < 0 && !scienceCheck && !labelsVisible) || evidencePending || sciencePending || scientificWorker.busy()) {
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
