#include "ProjectController.hpp"
#include "agent/AgentController.hpp"
#include <QGuiApplication>
#include <QElapsedTimer>
#include <QImage>
#include <QQmlApplicationEngine>
#include <QQuickStyle>
#include <QQuickWindow>
#include <QTimer>

int main(int argc, char** argv) {
    QGuiApplication app(argc, argv);
    QCoreApplication::setApplicationName("BREAKWATER");
    QCoreApplication::setOrganizationName("Breakwater");
    QQuickStyle::setStyle("Basic");
    breakwater::desktop::ProjectController controller;
    breakwater::desktop::agent::AgentController investigator(&controller);
    QQmlApplicationEngine engine;
    engine.setInitialProperties({{"controller", QVariant::fromValue(&controller)}, {"investigator", QVariant::fromValue(&investigator)}});
    QObject::connect(&engine, &QQmlApplicationEngine::objectCreationFailed, &app, [] { QCoreApplication::exit(1); }, Qt::QueuedConnection);
    engine.loadFromModule("Breakwater", "Main");
    const auto arguments = app.arguments();
    if (arguments.contains("--agent-panel") && !engine.rootObjects().isEmpty()) engine.rootObjects().front()->setProperty("showAgent", true);
    if (arguments.contains("--smoke") || arguments.contains("--screenshot")) {
        auto* smokeTimer = new QTimer(&app);
        QElapsedTimer smokeElapsed; smokeElapsed.start();
        QObject::connect(smokeTimer, &QTimer::timeout, &app, [&, smokeTimer, smokeElapsed] {
            if (engine.rootObjects().isEmpty()) { app.exit(2); return; }
            auto* window = qobject_cast<QQuickWindow*>(engine.rootObjects().front());
            const auto capture = window ? window->grabWindow() : QImage{};
            const bool labelsVisible = engine.rootObjects().front()->property("visibleAssetLabels").toInt() > 0;
            if (capture.isNull() || !labelsVisible) {
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
