#include "ProjectController.hpp"
#include <QGuiApplication>
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
    QQmlApplicationEngine engine;
    engine.setInitialProperties({{"controller", QVariant::fromValue(&controller)}});
    QObject::connect(&engine, &QQmlApplicationEngine::objectCreationFailed, &app, [] { QCoreApplication::exit(1); }, Qt::QueuedConnection);
    engine.loadFromModule("Breakwater", "Main");
    const auto arguments = app.arguments();
    if (arguments.contains("--smoke") || arguments.contains("--screenshot")) {
        QTimer::singleShot(2500, &app, [&] {
            if (engine.rootObjects().isEmpty()) { app.exit(2); return; }
            auto* window = qobject_cast<QQuickWindow*>(engine.rootObjects().front());
            if (engine.rootObjects().front()->property("visibleAssetLabels").toInt() == 0) { app.exit(5); return; }
            const auto capture = window ? window->grabWindow() : QImage{};
            if (capture.isNull()) { app.exit(3); return; }
            const auto index = arguments.indexOf("--screenshot");
            if (index >= 0 && index + 1 < arguments.size() && !capture.save(arguments[index + 1])) { app.exit(4); return; }
            app.exit(0);
        });
    }
    return app.exec();
}
