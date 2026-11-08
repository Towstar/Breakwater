#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QGuiApplication>
#include <QImage>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJSValue>
#include <QQmlComponent>
#include <QQmlEngine>
#include <QQmlError>
#include <QQuickItem>
#include <QQuickStyle>
#include <QQuickWindow>
#include <QSignalSpy>
#include <QtTest>
#include <memory>
#include <stdexcept>

namespace {
QJsonObject fixture(const char* name = "detect-response.json") {
    QFile file(QStringLiteral(BREAKWATER_SOURCE_DIR "/contracts/eog/examples/") + QString::fromLatin1(name));
    if (!file.open(QIODevice::ReadOnly)) throw std::runtime_error("Accepted EOG evidence fixture is unavailable.");
    QJsonParseError error;
    const auto document = QJsonDocument::fromJson(file.readAll(), &error);
    if (error.error != QJsonParseError::NoError || !document.isObject()) throw std::runtime_error("Invalid evidence fixture JSON.");
    return document.object();
}
QJsonObject evidenceFor(const QJsonObject& response) {
    const auto result = response.value("result").toObject();
    return {{"event", result.value("events").toArray().first()}, {"evidence", result.value("evidence")},
        {"assumptions", result.value("assumptions")}, {"limitations", result.value("limitations")},
        {"analysis_request_id", response.value("request_id")}};
}
QJsonObject objectProperty(QObject* object, const char* name) {
    auto value = object->property(name);
    if (value.metaType() == QMetaType::fromType<QJSValue>()) value = value.value<QJSValue>().toVariant();
    return QJsonObject::fromVariantMap(value.toMap());
}
QList<QQuickItem*> visualItems(QQuickItem* root) {
    QList<QQuickItem*> result, pending{root};
    while (!pending.isEmpty()) {
        auto* item = pending.takeLast(); result.append(item);
        for (auto* child : item->childItems()) pending.append(child);
    }
    return result;
}
class RenderedDetails {
public:
    QStringList warnings;
    QString error;
    QQmlEngine engine;
    QQuickWindow window;
    std::unique_ptr<QQuickItem> root;

    RenderedDetails(const QJsonObject& event, const QJsonObject& evidence, int width = 1000) {
        QObject::connect(&engine, &QQmlEngine::warnings, &engine, [this](const QList<QQmlError>& values) {
            for (const auto& value : values) warnings.append(value.toString());
        });
        QQmlComponent component(&engine, QUrl::fromLocalFile(QStringLiteral(BREAKWATER_SOURCE_DIR "/desktop/qml/EogEventDetails.qml")));
        if (component.isError()) { error = component.errorString(); return; }
        auto* object = component.createWithInitialProperties({{"event", event.toVariantMap()}, {"evidence", evidence.toVariantMap()}});
        auto* item = qobject_cast<QQuickItem*>(object);
        if (!item) { error = component.errorString(); delete object; return; }
        root.reset(item); root->setParentItem(window.contentItem()); root->setWidth(width);
        QObject::connect(root.get(), &QQuickItem::implicitHeightChanged, root.get(), [this] { root->setHeight(root->implicitHeight()); });
        root->setHeight(root->implicitHeight());
        window.resize(width, 760); window.setColor(QColor("#0b1822")); window.show();
        if (!QTest::qWaitForWindowExposed(&window, 5000)) error = "The real Qt Quick event details window was not exposed.";
    }
    QQuickItem* text(const QString& contains) const {
        if (!root) return nullptr;
        for (auto* item : visualItems(root.get()))
            if (item->isVisible() && item->property("text").toString().contains(contains)) return item;
        return nullptr;
    }
    QImage show(QQuickItem* target = nullptr) {
        if (!error.isEmpty() || !root) return {};
        if (target) {
            const qreal localY = target->mapToItem(root.get(), QPointF{}).y();
            root->setY(-qMax(qreal(0), localY - 80));
        } else root->setY(0);
        QSignalSpy frames(&window, &QQuickWindow::frameSwapped); window.update();
        QElapsedTimer timer; timer.start();
        while (timer.elapsed() < 5000) {
            QTest::qWait(30);
            const auto image = window.grabWindow();
            if (!image.isNull() && !frames.isEmpty()) return image;
            window.update();
        }
        error = "The event details view did not produce a painted frame."; return {};
    }
    bool visibleInk(QQuickItem* target, const QImage& image) const {
        if (!target || image.isNull() || !target->isVisible()) return false;
        auto box = target->mapRectToScene(target->boundingRect()).intersected(QRectF(0, 0, window.width(), window.height()));
        for (auto* ancestor = target->parentItem(); ancestor; ancestor = ancestor->parentItem())
            if (ancestor->clip()) box = box.intersected(ancestor->mapRectToScene(ancestor->boundingRect()));
        if (box.isEmpty()) return false;
        const qreal rx = qreal(image.width()) / window.width(), ry = qreal(image.height()) / window.height();
        const auto pixels = image.copy(QRect(qRound(box.x() * rx), qRound(box.y() * ry),
            qRound(box.width() * rx), qRound(box.height() * ry)).intersected(image.rect()));
        int ink = 0; const QColor background("#0b1822");
        for (int y = 0; y < pixels.height(); ++y) for (int x = 0; x < pixels.width(); ++x) {
            const auto color = pixels.pixelColor(x, y);
            if (qAbs(color.red() - background.red()) + qAbs(color.green() - background.green()) + qAbs(color.blue() - background.blue()) > 60) ++ink;
        }
        return ink > 40;
    }
};
bool save(const QImage& image, const QString& name) {
    QDir folder(QDir::current().filePath("eog-event-details-artifacts"));
    return (folder.exists() || QDir().mkpath(folder.absolutePath())) && image.save(folder.filePath(name + ".png"));
}
}

class EogEventDetailsTests final : public QObject {
    Q_OBJECT
private slots:
    void acceptedEvidenceRemainsReadable_data() {
        QTest::addColumn<QJsonObject>("event"); QTest::addColumn<QJsonObject>("evidence");
        QTest::addColumn<QStringList>("requiredText"); QTest::addColumn<int>("width");
        const auto canonical = evidenceFor(fixture());
        QTest::newRow("canonical") << canonical.value("event").toObject() << canonical
            << QStringList{"Event event-1 · Category pump_failure · State ongoing", "System system-1 · Facility field:A · Asset pump:A",
                "Coverage: available · Recommended action: plan_required", "Returned onset bounds:", "Unix 1000000000 s",
                "Shutdown countdown: 1.2 h · Lost revenue: 100 USD/h", "Threshold version: fixture-thresholds-1 · Analysis request: request-1",
                "Evidence sample-1", "Asset tank:A · Metric level_pct · Value 80 · Unit %", "Unix 1000003600 s", "Reported quality: good",
                "Synthetic example; no production detector is supplied."} << 1000;
        const auto renamed = evidenceFor(fixture("renamed-detect-response.json"));
        QTest::newRow("renamed-compact") << renamed.value("event").toObject() << renamed
            << QStringList{"Event event-renamed", "System 系统 α · Facility 油田 / A · Asset 泵 🚰",
                "No source samples returned for this selection.", "Unicode/renamed identity fixture."} << 480;
        QTest::newRow("no-selection") << QJsonObject{} << QJsonObject{}
            << QStringList{"No event is selected.", "Returned onset: unavailable", "Returned onset bounds: unavailable",
                "Shutdown countdown: unavailable · Lost revenue: unavailable", "No source samples returned for this selection."} << 480;
    }
    void acceptedEvidenceRemainsReadable() {
        QFETCH(QJsonObject, event); QFETCH(QJsonObject, evidence); QFETCH(QStringList, requiredText); QFETCH(int, width);
        RenderedDetails details(event, evidence, width); QVERIFY2(details.error.isEmpty(), qPrintable(details.error));
        const auto tag = QString::fromLatin1(QTest::currentDataTag());
        const auto summary = details.show(); QVERIFY2(!summary.isNull(), qPrintable(details.error)); QVERIFY(save(summary, tag + "-summary"));
        for (const auto& expected : requiredText) {
            auto* target = details.text(expected); QVERIFY2(target, qPrintable("Missing readable text: " + expected));
            const auto image = details.show(target); QVERIFY2(!image.isNull(), qPrintable(details.error));
            QVERIFY2(details.visibleInk(target, image), qPrintable("Returned evidence text was not drawn: " + expected));
        }
        QVERIFY(!details.root->findChild<QObject*>("advancedJson"));
        QCOMPARE(objectProperty(details.root.get(), "event"), event);
        QCOMPARE(objectProperty(details.root.get(), "evidence"), evidence);
        QVERIFY2(details.warnings.isEmpty(), qPrintable(details.warnings.join('\n')));
    }
    void rawValuesUnknownsAndQualificationsArePreserved() {
        auto context = evidenceFor(fixture()), event = context.value("event").toObject();
        event["onset_unix"] = QJsonValue::Null; event["onset_bounds"] = QJsonValue::Null;
        event["countdown_hours"] = QJsonValue::Null; event["lost_revenue_per_hour"] = 0;
        event["coverage"] = "ambiguous"; event["recommended_action"] = "unavailable";
        event["limitations"] = QJsonArray{"<b>Event uncertainty & raw evidence</b>"};
        QJsonArray samples;
        for (const auto value : {-2.5, 104.125}) {
            auto sample = context.value("evidence").toArray().first().toObject();
            sample["evidence_id"] = value < 0 ? "negative-reading" : "overflow-reading";
            sample["value"] = value; sample["quality"] = "stale"; samples.append(sample);
        }
        auto missing = samples.first().toObject(); missing["evidence_id"] = "missing-reading";
        missing["value"] = QJsonValue::Null; missing["quality"] = "missing"; samples.append(missing);
        context["event"] = event; context["evidence"] = samples;
        context["assumptions"] = QJsonArray{"<i>Analysis assumption is not a sensor reading</i>"};
        context["limitations"] = QJsonArray{"Global coverage remains incomplete."};
        RenderedDetails details(event, context); QVERIFY2(details.error.isEmpty(), qPrintable(details.error));
        QVERIFY(!details.show().isNull());
        const QStringList labels{"Returned onset: unavailable", "Returned onset bounds: unavailable", "Shutdown countdown: unavailable · Lost revenue: 0 USD/h",
            "Value -2.5 · Unit %", "Value 104.125 · Unit %", "Value unavailable · Unit %", "Reported quality: stale", "Reported quality: missing",
            "<b>Event uncertainty & raw evidence</b>", "<i>Analysis assumption is not a sensor reading</i>", "Global coverage remains incomplete."};
        auto* rows = details.root->findChild<QQuickItem*>("evidenceRows"); QVERIFY(rows);
        for (const auto& expected : labels) {
            if (expected.contains("missing")) QVERIFY(QMetaObject::invokeMethod(rows, "positionViewAtEnd"));
            QTRY_VERIFY2(details.text(expected), qPrintable("Missing raw evidence: " + expected));
            auto* target = details.text(expected); QCOMPARE(target->property("textFormat").toInt(), 0);
            const auto image = details.show(target); QVERIFY(details.visibleInk(target, image));
        }
        QVERIFY(save(details.show(details.text("Global coverage remains incomplete.")), "raw-unknown-qualified"));
        auto* disclosure = details.root->findChild<QQuickItem*>("advancedDisclosure"); QVERIFY(disclosure);
        QVERIFY(disclosure->setProperty("checked", true));
        QTRY_VERIFY(details.root->findChild<QQuickItem*>("advancedJson"));
        const auto exact = QJsonDocument::fromJson(details.root->findChild<QQuickItem*>("advancedJson")->property("text").toString().toUtf8()).object();
        QCOMPARE(exact.value("event").toObject(), event); QCOMPARE(exact.value("evidence").toObject(), context);
        QCOMPARE(objectProperty(details.root.get(), "event"), event); QCOMPARE(objectProperty(details.root.get(), "evidence"), context);
        QVERIFY2(details.warnings.isEmpty(), qPrintable(details.warnings.join('\n')));
    }
    void epochZeroIsNotUnavailableAndClearedSelectionRetiresText() {
        auto context = evidenceFor(fixture()), event = context.value("event").toObject();
        event["onset_unix"] = 0; event["onset_bounds"] = QJsonObject{{"earliest_unix", 0}, {"latest_unix", 0}};
        context["event"] = event;
        RenderedDetails details(event, context); QVERIFY2(details.error.isEmpty(), qPrintable(details.error));
        QVERIFY(!details.show().isNull());
        QVERIFY(details.text("Returned onset: 1970-01-01T00:00:00.000Z · Unix 0 s"));
        QVERIFY(details.text("Returned onset bounds: 1970-01-01T00:00:00.000Z · Unix 0 s →"));
        QVERIFY(details.root->setProperty("event", QVariantMap{})); QVERIFY(details.root->setProperty("evidence", QVariantMap{}));
        QTRY_VERIFY(details.text("No event is selected."));
        QTRY_VERIFY(!details.text("Event event-1")); QTRY_VERIFY(!details.text("Evidence sample-1"));
        auto* rows = details.root->findChild<QQuickItem*>("evidenceRows"); QVERIFY(rows);
        QTRY_COMPARE(rows->property("count").toInt(), 0);
        QVERIFY(details.text("Returned onset bounds: unavailable"));
        QVERIFY(save(details.show(), "cleared-selection"));
        QVERIFY2(details.warnings.isEmpty(), qPrintable(details.warnings.join('\n')));
    }
    void all4096EvidenceRowsRemainLazyAndImmutable() {
        // Presentation stress only; this does not qualify a detector or observations.
        auto context = evidenceFor(fixture()), event = context.value("event").toObject();
        QJsonArray samples;
        for (int i = 0; i < 4096; ++i)
            samples.append(QJsonObject{{"evidence_id", QStringLiteral("sample-%1").arg(i)}, {"asset_id", "tank:A"},
                {"metric", "level_pct"}, {"timestamp_unix", 1000000000 + i}, {"value", i - 3.25}, {"unit", "%"}, {"quality", "stale"}});
        context["evidence"] = samples;
        RenderedDetails details(event, context); QVERIFY2(details.error.isEmpty(), qPrintable(details.error));
        auto* rows = details.root->findChild<QQuickItem*>("evidenceRows"); QVERIFY(rows);
        QTRY_COMPARE(rows->property("count").toInt(), 4096);
        QVERIFY(!details.show(rows).isNull());
        int delegates = 0;
        for (auto* item : visualItems(rows))
            if (item->property("text").toString().startsWith("Evidence sample-")) ++delegates;
        QVERIFY(delegates > 0); QVERIFY(delegates < 128);
        QVERIFY(QMetaObject::invokeMethod(rows, "positionViewAtEnd"));
        QTRY_VERIFY(details.text("Evidence sample-4095\n"));
        auto* last = details.text("Evidence sample-4095\n");
        QVERIFY(last->property("text").toString().contains("Value 4091.75 · Unit %"));
        const auto image = details.show(last); QVERIFY(details.visibleInk(last, image)); QVERIFY(save(image, "evidence-final-row"));
        QCOMPARE(objectProperty(details.root.get(), "event"), event); QCOMPARE(objectProperty(details.root.get(), "evidence"), context);
        QVERIFY2(details.warnings.isEmpty(), qPrintable(details.warnings.join('\n')));
    }
};

int main(int argc, char** argv) {
    QGuiApplication app(argc, argv); QQuickStyle::setStyle("Basic");
    EogEventDetailsTests tests; return QTest::qExec(&tests, argc, argv);
}
#include "eog_event_details_tests.moc"
