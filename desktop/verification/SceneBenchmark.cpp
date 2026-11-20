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
#include <cmath>
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
    // Reproduce the hosted Windows runner's available client area locally too.
    const bool compactFixture = qEnvironmentVariableIntValue("BREAKWATER_BENCHMARK_COMPACT") == 1;
    if (compactFixture) window->resize(1008,672);
    QQmlExpression labels(QQmlEngine::contextForObject(window),window,QStringLiteral("workspaceSettings.setValue('allLabels',true)"));
    labels.evaluate();
    if (labels.hasError()) { QTimer::singleShot(0,app,[app] { app->exit(9); }); return; }
    // Even six assets need the existing view-only Fit on a compact display:
    // the saved orbit can put a label anchor behind the navigation toolbar.
    if (!QMetaObject::invokeMethod(window, "frameScene", Q_ARG(QVariant, QVariant(false)))) {
        QTimer::singleShot(0, app, [app] { app->exit(9); }); return;
    }
    struct Samples {
        QElapsedTimer clock; std::vector<double> intervals; qint64 previous=0; bool measuring=false;
        qint64 measurementStarted=0, measurementStopped=0;
        int settledFrames=0; bool finalized=false;
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
    const double initialYaw = window->property("yaw").toDouble();
    auto* animation = new QTimer(app); animation->setTimerType(Qt::PreciseTimer); animation->setInterval(16);
    // Timer deliveries slow down under WARP/ASan. Follow wall time so every
    // renderer traverses the same camera path rather than a different angle.
    QObject::connect(animation, &QTimer::timeout, window, [window,samples,initialYaw] {
        window->setProperty("yaw",std::fmod(initialYaw + double(samples->clock.nsecsElapsed()) / 1e6 / 160.0,360.0));
    });
    QObject::connect(window, &QQuickWindow::frameSwapped, app, [samples] {
        const auto now = samples->clock.nsecsElapsed();
        if (samples->measuring && samples->previous) samples->intervals.push_back(double(now - samples->previous) / 1e6);
        samples->previous = now;
    });
    const auto finishReport = [=] {
        if (samples->finalized) return;
        samples->finalized=true;
        double elapsed = 0; for (const auto value : samples->intervals) elapsed += value;
        auto sorted = samples->intervals; std::sort(sorted.begin(), sorted.end());
        const auto percentile = [&](double fraction) { return sorted.empty() ? 0.0 : sorted[std::min(sorted.size() - 1, static_cast<std::size_t>(fraction * double(sorted.size() - 1)))]; };
        const auto fps = elapsed > 0 ? 1000.0 * double(samples->intervals.size()) / elapsed : 0;
        const int target = project->assets().size() > 48 ? 30 : 60;
        QQmlExpression picks(QQmlEngine::contextForObject(window), window, QStringLiteral(
            "JSON.stringify((function(){const bodies=new Map(),seen=new Set();"
            "function visit(node){if(!node||seen.has(node)||node.visible===false)return;seen.add(node);"
            "if(node.pickable===true&&String(node.objectName).indexOf('asset:')===0){"
            "const id=String(node.objectName).substring(6);if(!bodies.has(id))bodies.set(id,[]);bodies.get(id).push(node)}"
            "for(const child of node.children||[])visit(child);if(node.item)visit(node.item)}visit(view.scene);"
            "let projected=0,pickable=0;const failed=[],probes=[];for(const row of app.controller.assets){"
            "let visible=false,found=false;const models=bodies.get(row.id)||[],attempts=[];for(const model of models){"
            "const p=view.mapFrom3DScene(model.scenePosition);"
            "if(p.z>0&&p.x>=0&&p.y>=0&&p.x<view.width&&p.y<view.height){visible=true;const hit=view.pick(p.x,p.y);"
            "const name=hit.objectHit?hit.objectHit.objectName:'';attempts.push({x:p.x,y:p.y,hit:name});"
            "if(name==='asset:'+row.id){found=true;break}}}"
            "if(visible)++projected;if(found)++pickable;else failed.push(row.id);"
            "probes.push({asset_id:row.id,body_models:models.length,projected:visible,pickable:found,attempts:attempts})}"
            "const labels=sceneLabelLayer.positioned,shown=new Set(labels.map(label=>label.id));"
            "let overlaps=0;for(let i=0;i<labels.length;++i)for(let j=0;j<i;++j){"
            "const a=labels[i],b=labels[j];if(a.x<b.x+b.width&&a.x+a.width>b.x&&a.y<b.y+b.height&&a.y+a.height>b.y)++overlaps}"
            "return {projected:projected,pickable:pickable,labels:labels.length,label_overlaps:overlaps,"
            "failed_asset_ids:failed,body_probes:probes,"
            "culled_label_ids:app.controller.assets.filter(row=>!shown.has(row.id)).map(row=>row.id),"
            "label_layout:labels.map(label=>({asset_id:label.id,x:label.x,y:label.y,width:label.width,height:label.height})),"
            "label_viewport:{width:sceneLabelLayer.width,height:sceneLabelLayer.height,top_inset:sceneLabelLayer.topInset}}})())"));
        const auto picked = QJsonDocument::fromJson(picks.evaluate().toString().toUtf8()).object();
        QFile screenshot(output + ".png"); const auto picture = window->grabWindow();
        const bool imageSaved = !picture.isNull() && screenshot.open(QIODevice::WriteOnly | QIODevice::NewOnly)
            && picture.save(&screenshot, "PNG") && screenshot.flush();
        const QJsonObject report{{"benchmark_version",2},{"status", samples->settledFrames<2 ? "settle_timeout" : sorted.empty() ? "not_rendered" : "measured"}, {"recorded_utc", QDateTime::currentDateTimeUtc().toString(Qt::ISODate)},
            {"measurement", "continuous view-only camera sweep, 2.5 s warmup and 10 s sample"},
            {"frames", static_cast<int>(samples->intervals.size())}, {"elapsed_ms", elapsed}, {"fps", fps},
            {"measurement_window_ms",double(samples->measurementStopped-samples->measurementStarted)/1e6},
            {"post_stop_rendered_frames",samples->settledFrames},{"camera_yaw_start",initialYaw},{"camera_yaw_end",window->property("yaw").toDouble()},
            {"frame_interval_p50_ms", percentile(0.5)}, {"frame_interval_p95_ms", percentile(0.95)},
            {"target_fps", target}, {"target_met", fps >= target}, {"assets", project->assets().size()}, {"routes", project->routes().size()},
            {"effective_scene_quality", window->property("sceneQuality").toInt()}, {"detail_visible", window->property("detailedScene").toBool()},
            {"actual_body_picks", picked}, {"picking_error",picks.hasError() ? picks.error().toString() : QString{}},
            {"screenshot_written", imageSaved},
            {"all_bodies_pickable",picked.value("pickable").toInt()==project->assets().size()},
            {"visible_label_count",picked.value("labels")},{"label_overlap_pairs",picked.value("label_overlaps")},
            {"graphics_api", static_cast<int>(window->rendererInterface()->graphicsApi())},
            {"renderer",samples->renderer},
            {"hardware_target_met",samples->renderer.value("status")=="identified" && samples->renderer.value("software")==false && fps>=target},
            {"software_renderer_requested", qEnvironmentVariable("QSG_RHI_PREFER_SOFTWARE_RENDERER") == "1"},
            {"compact_fixture_requested",compactFixture},{"fixture_fitted",window->property("fittedView").toBool()},
            {"client_width", window->width()}, {"client_height", window->height()}, {"device_pixel_ratio", window->devicePixelRatio()},
            {"numerical_identity_unchanged", project->networkHash() == hash}};
        QFile file(output); const auto bytes = QJsonDocument(report).toJson(QJsonDocument::Indented);
        const bool saved = file.open(QIODevice::WriteOnly | QIODevice::NewOnly) && file.write(bytes) == bytes.size() && file.flush();
        app->exit(saved && imageSaved && samples->settledFrames>=2 && !sorted.empty() && !picks.hasError() && picked.value("pickable").toInt()==project->assets().size()
            && picked.value("label_overlaps").toInt()==0
            && project->networkHash() == hash ? 0 : 9);
    };
    QTimer::singleShot(2500, Qt::PreciseTimer, app, [=] {
        samples->measuring = true; samples->previous = 0;
        samples->measurementStarted=samples->clock.nsecsElapsed();
        // Start the ten-second sample from the actual warmup completion. Two
        // independent absolute timers could leave only six seconds under load.
        QTimer::singleShot(10000, Qt::PreciseTimer, app, [=] {
            animation->stop(); samples->measuring = false;
            samples->measurementStopped=samples->clock.nsecsElapsed();
            // mapFrom3DScene reads current frontend transforms; pick reads the
            // synchronized renderer scene. Wait for a complete post-stop frame
            // even if another frame was already in flight when the timer fired.
            QObject::connect(window,&QQuickWindow::frameSwapped,app,[=] {
                if (samples->finalized) return;
                if (++samples->settledFrames>=2) QTimer::singleShot(0,app,finishReport);
                else window->update();
            });
            window->update();
            QTimer::singleShot(10000,app,finishReport);
        });
    });
    animation->start();
}
}
