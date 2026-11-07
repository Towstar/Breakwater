#include "ProjectController.hpp"
#include "agent/AgentController.hpp"
#include "history/HistoryController.hpp"
#include "quality/PipeTransportController.hpp"
#include "science/PythonWorkerClient.hpp"
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
            [writeReport](const QString& code, const QString& message) { writeReport({{"status", "unavailable"}, {"error", QJsonObject{{"code", code}, {"message", message}}}}); });
    }
    QQmlApplicationEngine engine;
    engine.setInitialProperties({{"controller", QVariant::fromValue(&controller)}, {"investigator", QVariant::fromValue(&investigator)},
        {"history", QVariant::fromValue(&history)}, {"scientificWorker", QVariant::fromValue(&scientificWorker)},
        {"pipeTransport", QVariant::fromValue(&pipeTransport)}});
    QObject::connect(&engine, &QQmlApplicationEngine::objectCreationFailed, &app, [] { QCoreApplication::exit(1); }, Qt::QueuedConnection);
    engine.loadFromModule("Breakwater", "Main");
    const auto importIndex = arguments.indexOf("--import-session");
    if (importIndex >= 0 && importIndex + 1 < arguments.size())
        QTimer::singleShot(0, &history, [&history, path = arguments[importIndex + 1]] { history.importSession(QUrl::fromLocalFile(path)); });
    if (arguments.contains("--worker-health")) QTimer::singleShot(0, &scientificWorker, &breakwater::desktop::science::PythonWorkerClient::checkHealth);
    if (arguments.contains("--agent-panel") && !engine.rootObjects().isEmpty()) engine.rootObjects().front()->setProperty("showAgent", true);
    if (arguments.contains("--smoke") || arguments.contains("--screenshot")) {
        auto* smokeTimer = new QTimer(&app);
        QElapsedTimer smokeElapsed; smokeElapsed.start();
        QObject::connect(smokeTimer, &QTimer::timeout, &app, [&, smokeTimer, smokeElapsed] {
            if (engine.rootObjects().isEmpty()) { app.exit(2); return; }
            auto* window = qobject_cast<QQuickWindow*>(engine.rootObjects().front());
            const auto capture = window ? window->grabWindow() : QImage{};
            const bool labelsVisible = engine.rootObjects().front()->property("visibleAssetLabels").toInt() > 0;
            const bool evidencePending = importIndex >= 0 && (history.busy() || history.selectedSessionId().isEmpty());
            if (capture.isNull() || !labelsVisible || evidencePending || scientificWorker.busy()) {
                if (smokeElapsed.elapsed() >= 20000) app.exit(capture.isNull() ? 3 : 5);
                else if (window) window->requestUpdate();
                return;
            }
            smokeTimer->stop();
            const auto index = arguments.indexOf("--screenshot");
            if (index >= 0 && index + 1 < arguments.size() && !capture.save(arguments[index + 1])) { app.exit(4); return; }
            app.exit(0);
        });
        smokeTimer->start(100);
    }
    return app.exec();
}
