#include "SceneBenchmark.hpp"
#include "../ProjectController.hpp"
#include <QCoreApplication>
#include <QDateTime>
#include <QElapsedTimer>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QQmlExpression>
#include <QQmlEngine>
#include <QImage>
#include <QQuickWindow>
#include <QSGRendererInterface>
#include <QTimer>
#include <algorithm>
#include <atomic>
#include <memory>
#include <vector>
#ifdef Q_OS_WIN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <d3d11.h>
#include <dxgi1_2.h>
#include <wrl/client.h>
#endif

namespace breakwater::desktop {
void startSceneBenchmark(QQuickWindow* window, ProjectController* project, QCoreApplication* app, const QString& output) {
    // main.cpp supplies isolated temporary preferences for this helper.
    QQmlExpression labels(QQmlEngine::contextForObject(window),window,QStringLiteral("workspaceSettings.setValue('allLabels',true)"));
    labels.evaluate();
    if (labels.hasError()) { QTimer::singleShot(0,app,[app] { app->exit(9); }); return; }
    if (project->assets().size() > 48 && !QMetaObject::invokeMethod(window, "frameScene", Q_ARG(QVariant, QVariant(false)))) {
        QTimer::singleShot(0, app, [app] { app->exit(9); }); return;
    }
    struct Samples {
        QElapsedTimer clock; std::vector<double> intervals; qint64 previous=0; bool measuring=false;
        std::atomic<bool> rendererRead=false;
        QJsonObject renderer{{"status","unavailable"}};
    };
    const auto samples = std::make_shared<Samples>(); samples->clock.start();
    // Device resources belong to the render thread. Query once during a frame,
    // then send an immutable description back to the GUI-thread report.
    QObject::connect(window,&QQuickWindow::beforeRendering,window,[window,app,samples] {
        if (samples->rendererRead.load()) return;
        QJsonObject renderer{{"status","unsupported_backend"}};
#ifdef Q_OS_WIN
        if (window->rendererInterface()->graphicsApi()==QSGRendererInterface::Direct3D11) {
            auto* device=static_cast<ID3D11Device*>(window->rendererInterface()->getResource(window,QSGRendererInterface::DeviceResource));
            if (!device) return;
            Microsoft::WRL::ComPtr<IDXGIDevice> dxgi; Microsoft::WRL::ComPtr<IDXGIAdapter> adapter;
            Microsoft::WRL::ComPtr<IDXGIAdapter1> adapter1; DXGI_ADAPTER_DESC1 description{};
            renderer={{"status","adapter_unavailable"}};
            if (SUCCEEDED(device->QueryInterface(IID_PPV_ARGS(&dxgi))) && SUCCEEDED(dxgi->GetAdapter(&adapter)) &&
                    SUCCEEDED(adapter.As(&adapter1)) && SUCCEEDED(adapter1->GetDesc1(&description)))
                renderer={{"status","identified"},{"adapter",QString::fromWCharArray(description.Description)},
                    {"vendor_id",static_cast<qint64>(description.VendorId)},{"device_id",static_cast<qint64>(description.DeviceId)},
                    {"software",bool(description.Flags & DXGI_ADAPTER_FLAG_SOFTWARE)}};
        }
#endif
        samples->rendererRead.store(true);
        QMetaObject::invokeMethod(app,[samples,renderer] { samples->renderer=renderer; },Qt::QueuedConnection);
    },Qt::DirectConnection);
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
        QQmlExpression picks(QQmlEngine::contextForObject(window), window, QStringLiteral(
            "JSON.stringify((function(){let projected=0,pickable=0;for(const row of app.controller.assets){"
            "let visible=false,found=false;for(const y of [28,40,52,12]){for(const dx of [0,8,-8]){"
            "const p=view.mapFrom3DScene(Qt.vector3d(row.x+dx,y,row.z));"
            "if(p.z>0&&p.x>=0&&p.y>=0&&p.x<view.width&&p.y<view.height){visible=true;const hit=view.pick(p.x,p.y);"
            "if(hit.objectHit&&hit.objectHit.objectName==='asset:'+row.id){found=true;break}}}if(found)break}"
            "if(visible)++projected;if(found)++pickable}"
            "const labels=sceneLabelLayer.positioned;let overlaps=0;for(let i=0;i<labels.length;++i)for(let j=0;j<i;++j){"
            "const a=labels[i],b=labels[j];if(a.x<b.x+b.width&&a.x+a.width>b.x&&a.y<b.y+b.height&&a.y+a.height>b.y)++overlaps}"
            "return {projected:projected,pickable:pickable,labels:labels.length,label_overlaps:overlaps}})())"));
        const auto picked = QJsonDocument::fromJson(picks.evaluate().toString().toUtf8()).object();
        QFile screenshot(output + ".png"); const auto picture = window->grabWindow();
        const bool imageSaved = !picture.isNull() && screenshot.open(QIODevice::WriteOnly | QIODevice::NewOnly)
            && picture.save(&screenshot, "PNG") && screenshot.flush();
        const QJsonObject report{{"status", sorted.empty() ? "not_rendered" : "measured"}, {"recorded_utc", QDateTime::currentDateTimeUtc().toString(Qt::ISODate)},
            {"measurement", "continuous view-only camera sweep, 2.5 s warmup and 10 s sample"},
            {"frames", static_cast<int>(samples->intervals.size())}, {"elapsed_ms", elapsed}, {"fps", fps},
            {"frame_interval_p50_ms", percentile(0.5)}, {"frame_interval_p95_ms", percentile(0.95)},
            {"target_fps", target}, {"target_met", fps >= target}, {"assets", project->assets().size()}, {"routes", project->routes().size()},
            {"effective_scene_quality", window->property("sceneQuality").toInt()}, {"detail_visible", window->property("detailedScene").toBool()},
            {"actual_body_picks", picked}, {"screenshot_written", imageSaved},
            {"all_bodies_pickable",picked.value("pickable").toInt()==project->assets().size()},
            {"visible_label_count",picked.value("labels")},{"label_overlap_pairs",picked.value("label_overlaps")},
            {"graphics_api", static_cast<int>(window->rendererInterface()->graphicsApi())},
            {"renderer",samples->renderer},
            {"hardware_target_met",samples->renderer.value("status")=="identified" && samples->renderer.value("software")==false && fps>=target},
            {"software_renderer_requested", qEnvironmentVariable("QSG_RHI_PREFER_SOFTWARE_RENDERER") == "1"},
            {"client_width", window->width()}, {"client_height", window->height()}, {"device_pixel_ratio", window->devicePixelRatio()},
            {"numerical_identity_unchanged", project->networkHash() == hash}};
        QFile file(output); const auto bytes = QJsonDocument(report).toJson(QJsonDocument::Indented);
        const bool saved = file.open(QIODevice::WriteOnly | QIODevice::NewOnly) && file.write(bytes) == bytes.size() && file.flush();
        app->exit(saved && imageSaved && !sorted.empty() && !picks.hasError() && picked.value("pickable").toInt()==project->assets().size()
            && picked.value("label_overlaps").toInt()==0
            && project->networkHash() == hash ? 0 : 9);
    });
    animation->start();
}
}
