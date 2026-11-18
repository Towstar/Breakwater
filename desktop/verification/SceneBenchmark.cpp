#include "SceneBenchmark.hpp"
#include "../ProjectController.hpp"
#include <QCoreApplication>
#include <QDateTime>
#include <QElapsedTimer>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QQuickWindow>
#include <QSGRendererInterface>
#include <QTimer>
#include <algorithm>
#include <memory>
#include <vector>

namespace breakwater::desktop {
void startSceneBenchmark(QQuickWindow* window, ProjectController* project, QCoreApplication* app, const QString& output) {
    if (project->assets().size() > 48 && !QMetaObject::invokeMethod(window, "frameScene", Q_ARG(QVariant, QVariant(false)))) {
        QTimer::singleShot(0, app, [app] { app->exit(9); }); return;
    }
    struct Samples { QElapsedTimer clock; std::vector<double> intervals; qint64 previous = 0; bool measuring = false; };
    const auto samples = std::make_shared<Samples>(); samples->clock.start();
    const auto hash = project->networkHash();
    auto* animation = new QTimer(app); animation->setTimerType(Qt::PreciseTimer); animation->setInterval(16);
    QObject::connect(animation, &QTimer::timeout, window, [window] { QMetaObject::invokeMethod(window, "benchmarkTick"); });
    QObject::connect(window, &QQuickWindow::frameSwapped, app, [samples] {
        const auto now = samples->clock.nsecsElapsed();
        if (samples->measuring && samples->previous) samples->intervals.push_back(double(now - samples->previous) / 1e6);
        samples->previous = now;
    });
    QTimer::singleShot(2500, app, [samples] { samples->measuring = true; samples->previous = 0; });
    QTimer::singleShot(12500, app, [=] {
        animation->stop(); samples->measuring = false;
        double elapsed = 0; for (const auto value : samples->intervals) elapsed += value;
        auto sorted = samples->intervals; std::sort(sorted.begin(), sorted.end());
        const auto percentile = [&](double fraction) { return sorted.empty() ? 0.0 : sorted[std::min(sorted.size() - 1, static_cast<std::size_t>(fraction * double(sorted.size() - 1)))]; };
        const auto fps = elapsed > 0 ? 1000.0 * double(samples->intervals.size()) / elapsed : 0;
        const int target = project->assets().size() > 48 ? 30 : 60;
        const QJsonObject report{{"status", "measured"}, {"recorded_utc", QDateTime::currentDateTimeUtc().toString(Qt::ISODate)},
            {"measurement", "continuous view-only camera sweep, 2.5 s warmup and 10 s sample"},
            {"frames", static_cast<int>(samples->intervals.size())}, {"elapsed_ms", elapsed}, {"fps", fps},
            {"frame_interval_p50_ms", percentile(0.5)}, {"frame_interval_p95_ms", percentile(0.95)},
            {"target_fps", target}, {"target_met", fps >= target}, {"assets", project->assets().size()}, {"routes", project->routes().size()},
            {"graphics_api", static_cast<int>(window->rendererInterface()->graphicsApi())},
            {"software_renderer_requested", qEnvironmentVariable("QSG_RHI_PREFER_SOFTWARE_RENDERER") == "1"},
            {"client_width", window->width()}, {"client_height", window->height()}, {"device_pixel_ratio", window->devicePixelRatio()},
            {"numerical_identity_unchanged", project->networkHash() == hash}};
        QFile file(output); const auto bytes = QJsonDocument(report).toJson(QJsonDocument::Indented);
        const bool saved = file.open(QIODevice::WriteOnly | QIODevice::NewOnly) && file.write(bytes) == bytes.size() && file.flush();
        app->exit(saved && !sorted.empty() && project->networkHash() == hash ? 0 : 9);
    });
    animation->start();
}
}
