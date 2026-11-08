#include <QDir>
#include <QElapsedTimer>
#include <QGuiApplication>
#include <QImage>
#include <QJsonArray>
#include <QJsonObject>
#include <QQmlComponent>
#include <QQmlEngine>
#include <QQmlError>
#include <QQuickItem>
#include <QQuickStyle>
#include <QQuickWindow>
#include <QSignalSpy>
#include <QtTest>
#include <algorithm>
#include <cmath>
#include <functional>
#include <initializer_list>
#include <memory>

namespace {
constexpr auto cyan = "#5cdbd1";
constexpr auto amber = "#e5bd72";
constexpr auto capacityColor = "#8399a5";
constexpr auto shutdownColor = "#ed8181";

QJsonArray numbers(std::initializer_list<double> values) {
    QJsonArray result;
    for (const auto value : values) result.append(value);
    return result;
}
QJsonObject trajectory(const QString& id, const QJsonArray& volumes,
                       const QJsonArray& elapsed = numbers({0, 900, 1800, 3600}),
                       double capacity = 100, double shutdown = 90) {
    return {{"facility_id", id}, {"elapsed_s", elapsed}, {"volumes_bbl", volumes},
            {"capacity_bbl", capacity}, {"shutdown_volume_bbl", shutdown}};
}
QJsonObject evaluation(const QJsonObject& row) { return {{"trajectories", QJsonArray{row}}}; }

struct Ink {
    int pixels = 0;
    QRect bounds;
    int maximumColumnPixels = 0;
};
// These assertions inspect actual rendered ink, not a second copy of the chart's
// coordinate formulas. The Canvas crop excludes the legend's identical colors.
Ink ink(const QImage& canvas, const char* color) {
    const QColor target(QString::fromLatin1(color));
    const QColor background("#0b1822");
    const double tr = target.red() - background.red(), tg = target.green() - background.green(),
        tb = target.blue() - background.blue(), norm = tr * tr + tg * tg + tb * tb;
    Ink result;
    // Ignore the left-hand tick labels/axis, and tolerate normal anti-aliasing
    // against the Canvas background (one-pixel guides need not have opaque ink).
    for (int x = canvas.width() * 15 / 100; x < canvas.width(); ++x) {
        int column = 0;
        for (int y = 0; y < canvas.height(); ++y) {
            const auto actual = canvas.pixelColor(x, y);
            const double ar = actual.red() - background.red(), ag = actual.green() - background.green(),
                ab = actual.blue() - background.blue(), coverage = (ar * tr + ag * tg + ab * tb) / norm;
            if (actual.alpha() < 220 || coverage < 0.35 || coverage > 1.05 ||
                std::abs(ar - coverage * tr) > 10 || std::abs(ag - coverage * tg) > 10 ||
                std::abs(ab - coverage * tb) > 10) continue;
            ++result.pixels; ++column;
            result.bounds = result.bounds.united(QRect(x, y, 1, 1));
        }
        result.maximumColumnPixels = std::max(result.maximumColumnPixels, column);
    }
    return result;
}
Ink guideInk(const QImage& canvas, const char* color) {
    // The retained positive boundaries in these fixtures are above the bottom
    // quarter. Excluding that quarter prevents the gray time axis from passing
    // a missing-capacity-guide assertion.
    return ink(canvas.copy(0, 0, canvas.width(), canvas.height() * 3 / 4), color);
}

class RenderedChart {
public:
    QStringList warnings;
    QString error;
    QQmlEngine engine;
    QQuickWindow window;
    std::unique_ptr<QQuickItem> root;
    QQuickItem* canvas = nullptr;

    RenderedChart(const QJsonObject& candidate, const QJsonObject& baseline, bool compatible) {
        QObject::connect(&engine, &QQmlEngine::warnings, &engine, [this](const QList<QQmlError>& values) {
            for (const auto& value : values) warnings.append(value.toString());
        });
        QQmlComponent component(&engine, QUrl::fromLocalFile(
            QStringLiteral(BREAKWATER_SOURCE_DIR "/desktop/qml/EogPlanChart.qml")));
        if (component.isError()) { error = component.errorString(); return; }
        auto* object = component.createWithInitialProperties({{"candidate", candidate.toVariantMap()},
            {"baselineRun", baseline.toVariantMap()}, {"compatible", compatible}});
        auto* item = qobject_cast<QQuickItem*>(object);
        if (!item) { error = component.errorString(); delete object; return; }
        root.reset(item);
        root->setParentItem(window.contentItem());
        root->setWidth(900); root->setHeight(350);
        for (auto* child : root->findChildren<QQuickItem*>())
            if (QByteArray(child->metaObject()->className()).contains("Canvas")) { canvas = child; break; }
        if (!canvas) { error = "The actual EogPlanChart Canvas was not created."; return; }
        window.setColor(Qt::black); window.resize(900, 350); window.show();
        if (!QTest::qWaitForWindowExposed(&window, 5000)) error = "The Qt Quick render window was not exposed.";
    }

    QImage grab(const std::function<bool(const QImage&)>& ready) {
        if (!error.isEmpty() || !canvas) return {};
        QSignalSpy frames(&window, &QQuickWindow::frameSwapped);
        QMetaObject::invokeMethod(canvas, "requestPaint"); window.update();
        QElapsedTimer timer; timer.start();
        while (timer.elapsed() < 5000) {
            QTest::qWait(30);
            auto image = window.grabWindow();
            if (image.isNull() || frames.isEmpty()) continue;
            const qreal ratioX = qreal(image.width()) / window.width(), ratioY = qreal(image.height()) / window.height();
            const auto region = canvas->mapRectToScene(canvas->boundingRect());
            const QRect pixels(qRound(region.x() * ratioX), qRound(region.y() * ratioY),
                qRound(region.width() * ratioX), qRound(region.height() * ratioY));
            image = image.copy(pixels.intersected(image.rect())).convertToFormat(QImage::Format_ARGB32);
            if (!image.isNull() && ready(image)) return image;
            window.update();
        }
        error = "No painted frame satisfied the visible trajectory/guide assertions within five seconds.";
        return {};
    }

    QQuickItem* selector() const {
        for (auto* child : root->findChildren<QQuickItem*>())
            if (QByteArray(child->metaObject()->className()).contains("ComboBox")) return child;
        return nullptr;
    }
};

bool retained(const QImage& image) {
    return ink(image, cyan).pixels > 100 && ink(image, amber).pixels > 100 &&
        guideInk(image, capacityColor).pixels > 100 && guideInk(image, shutdownColor).pixels > 100;
}
bool save(const QImage& image, const QString& name) {
    QDir output(QDir::current().filePath("eog-plan-chart-artifacts"));
    return (output.exists() || QDir().mkpath(output.absolutePath())) && image.save(output.filePath(name + ".png"));
}
}

class EogPlanChartTests final : public QObject {
    Q_OBJECT
private slots:
    void retainedTrajectoriesRender_data() {
        QTest::addColumn<QJsonObject>("candidate");
        QTest::addColumn<QJsonObject>("baseline");
        QTest::newRow("normal") << evaluation(trajectory("FAC-raw-A", numbers({20, 30, 35, 50})))
            << evaluation(trajectory("FAC-raw-A", numbers({70, 65, 75, 80})));
        QTest::newRow("overflow") << evaluation(trajectory("FAC-raw-A", numbers({110, 130, 145, 160})))
            << evaluation(trajectory("FAC-raw-A", numbers({70, 60, 50, 40})));
        QTest::newRow("negative") << evaluation(trajectory("FAC-raw-A", numbers({-50, -25, -10, 0})))
            << evaluation(trajectory("FAC-raw-A", numbers({20, 30, 35, 40})));
        QTest::newRow("finite-extrema") << evaluation(trajectory("FAC-raw-A", numbers({-1e308, 0, 1e308, -1e308}),
            numbers({0, 900, 1800, 3600}), 1e308, 8e307))
            << evaluation(trajectory("FAC-raw-A", numbers({6e307, 6e307, 6e307, 6e307}),
                numbers({0, 900, 1800, 3600}), 1e308, 8e307));
        QTest::newRow("near-binary64-maximum") << evaluation(trajectory("FAC-raw-A", numbers({1.6e308, 1.7e308, 1.79e308, 1.65e308}),
            numbers({0, 900, 1800, 3600}), 1.5e308, 1.4e308))
            << evaluation(trajectory("FAC-raw-A", numbers({7e307, 8e307, 9e307, 1e308}),
                numbers({0, 900, 1800, 3600}), 1.5e308, 1.4e308));
    }

    void retainedTrajectoriesRender() {
        QFETCH(QJsonObject, candidate); QFETCH(QJsonObject, baseline);
        RenderedChart chart(candidate, baseline, true);
        QVERIFY2(chart.error.isEmpty(), qPrintable(chart.error));
        const auto image = chart.grab(retained);
        QVERIFY2(!image.isNull(), qPrintable(chart.error + chart.warnings.join('\n')));
        QVERIFY2(chart.warnings.isEmpty(), qPrintable(chart.warnings.join('\n')));
        const auto name = QString::fromLatin1(QTest::currentDataTag());
        QVERIFY(save(image, name));
        const auto candidateInk = ink(image, cyan), baselineInk = ink(image, amber), capacityInk = guideInk(image, capacityColor);
        QVERIFY(candidateInk.bounds.width() > image.width() / 2);
        QVERIFY(baselineInk.bounds.width() > image.width() / 2);
        // Actual above-capacity and negative values remain visible on the correct
        // side of the retained guide/other trajectory; neither is clipped.
        if (name == "overflow" || name == "near-binary64-maximum")
            QVERIFY(candidateInk.bounds.bottom() < capacityInk.bounds.top());
        if (name == "negative") QVERIFY(candidateInk.bounds.top() > baselineInk.bounds.bottom());
        if (name == "overflow" || name == "negative") QVERIFY(candidateInk.bounds.height() > image.height() / 10);
        if (name == "finite-extrema") QVERIFY(candidateInk.bounds.height() > image.height() / 2);
    }

    void duplicateTimeTruckRemovalDrawsVerticalStep() {
        const auto elapsed = numbers({0, 1800, 1800, 3600});
        RenderedChart chart(evaluation(trajectory("Facility:truck-step", numbers({80, 85, 15, 30}), elapsed)),
            evaluation(trajectory("Facility:truck-step", numbers({50, 55, 60, 70}))), true);
        QVERIFY2(chart.error.isEmpty(), qPrintable(chart.error));
        const auto image = chart.grab(retained);
        QVERIFY2(!image.isNull(), qPrintable(chart.error + chart.warnings.join('\n')));
        QVERIFY2(chart.warnings.isEmpty(), qPrintable(chart.warnings.join('\n')));
        QVERIFY(save(image, "duplicate-time-truck-step"));
        const auto candidateInk = ink(image, cyan);
        QVERIFY2(candidateInk.maximumColumnPixels > image.height() / 3,
            "A simultaneous before/after truck arrival must retain its vertical volume step.");
    }

    void incompatibleOverlayRemovesOnlyBaseline() {
        RenderedChart chart(evaluation(trajectory("FAC-A", numbers({20, 30, 35, 50}))),
            evaluation(trajectory("FAC-A", numbers({70, 65, 75, 80}))), true);
        QVERIFY2(chart.error.isEmpty(), qPrintable(chart.error));
        auto image = chart.grab(retained);
        QVERIFY2(!image.isNull(), qPrintable(chart.error)); QVERIFY(save(image, "compatible-overlay"));
        QVERIFY(chart.root->setProperty("compatible", false));
        image = chart.grab([](const QImage& pixels) {
            return ink(pixels, amber).pixels == 0 && ink(pixels, cyan).pixels > 100 &&
                guideInk(pixels, capacityColor).pixels > 100 && guideInk(pixels, shutdownColor).pixels > 100;
        });
        QVERIFY2(!image.isNull(), qPrintable(chart.error + chart.warnings.join('\n')));
        QVERIFY(save(image, "incompatible-candidate-only"));
        QVERIFY(chart.root->setProperty("compatible", true));
        image = chart.grab(retained);
        QVERIFY2(!image.isNull(), qPrintable(chart.error));
        QVERIFY2(chart.warnings.isEmpty(), qPrintable(chart.warnings.join('\n')));
    }

    void facilitySelectionKeepsItsRawIdentityAfterRowsReorder() {
        const auto a = trajectory("FAC:original-A", numbers({20, 30, 35, 50}));
        const auto b = trajectory("FAC:original-B", numbers({70, 65, 75, 80}));
        RenderedChart chart({{"trajectories", QJsonArray{a, b}}}, {}, false);
        QVERIFY2(chart.error.isEmpty(), qPrintable(chart.error));
        auto* selector = chart.selector(); QVERIFY(selector);
        QVERIFY(selector->setProperty("currentIndex", 1));
        QVERIFY(QMetaObject::invokeMethod(selector, "activated", Q_ARG(int, 1)));
        QCOMPARE(chart.root->property("facilityId").toString(), QStringLiteral("FAC:original-B"));
        QVERIFY(chart.root->setProperty("candidate", QJsonObject{{"trajectories", QJsonArray{b, a}}}.toVariantMap()));
        QTRY_COMPARE(selector->property("currentText").toString(), QStringLiteral("FAC:original-B"));
        QCOMPARE(chart.root->property("facilityId").toString(), QStringLiteral("FAC:original-B"));
        const auto image = chart.grab([](const QImage& pixels) { return ink(pixels, cyan).pixels > 100; });
        QVERIFY2(!image.isNull(), qPrintable(chart.error + chart.warnings.join('\n')));
        QVERIFY2(chart.warnings.isEmpty(), qPrintable(chart.warnings.join('\n')));
        QVERIFY(save(image, "raw-facility-selection"));
    }
};

int main(int argc, char** argv) {
    QGuiApplication app(argc, argv);
    QQuickStyle::setStyle("Basic");
    EogPlanChartTests tests;
    return QTest::qExec(&tests, argc, argv);
}
#include "eog_plan_chart_tests.moc"
