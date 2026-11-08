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
QJsonObject fixture(const char* name) {
    QFile file(QStringLiteral(BREAKWATER_SOURCE_DIR "/contracts/eog/examples/") + QString::fromLatin1(name));
    if (!file.open(QIODevice::ReadOnly)) throw std::runtime_error("Accepted EOG presentation fixture is unavailable.");
    QJsonParseError error;
    const auto document = QJsonDocument::fromJson(file.readAll(), &error);
    if (error.error != QJsonParseError::NoError || !document.isObject()) throw std::runtime_error("Invalid presentation fixture JSON.");
    return document.object();
}
QJsonObject evaluated(const char* name) { return fixture(name).value("result").toObject().value("evaluation").toObject(); }
QJsonObject validation() { return fixture("validate-plan-response.json").value("result").toObject().value("validation").toObject(); }
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

    RenderedDetails(const QJsonObject& evaluation, const QJsonObject& checked) {
        QObject::connect(&engine, &QQmlEngine::warnings, &engine, [this](const QList<QQmlError>& values) {
            for (const auto& value : values) warnings.append(value.toString());
        });
        QQmlComponent component(&engine, QUrl::fromLocalFile(QStringLiteral(BREAKWATER_SOURCE_DIR "/desktop/qml/EogPlanDetails.qml")));
        if (component.isError()) { error = component.errorString(); return; }
        auto* object = component.createWithInitialProperties({{"evaluation", evaluation.toVariantMap()}, {"validation", checked.toVariantMap()}});
        auto* item = qobject_cast<QQuickItem*>(object);
        if (!item) { error = component.errorString(); delete object; return; }
        root.reset(item); root->setParentItem(window.contentItem()); root->setWidth(1000);
        QObject::connect(root.get(), &QQuickItem::implicitHeightChanged, root.get(), [this] { root->setHeight(root->implicitHeight()); });
        root->setHeight(root->implicitHeight());
        window.resize(1000, 760); window.setColor(QColor("#0b1822")); window.show();
        if (!QTest::qWaitForWindowExposed(&window, 5000)) error = "The real Qt Quick details window was not exposed.";
    }

    QQuickItem* text(const QString& contains) const {
        if (!root) return nullptr;
        // ListView delegates belong to its visual content item; their QObject
        // ownership need not descend from the view. Follow the rendered tree.
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
        error = "The details view did not produce a painted frame."; return {};
    }
    bool visibleInk(QQuickItem* target, const QImage& image) const {
        if (!target || image.isNull() || !target->isVisible()) return false;
        const auto box = target->mapRectToScene(target->boundingRect());
        if (!QRectF(0, 0, window.width(), window.height()).contains(box.center())) return false;
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
    QDir folder(QDir::current().filePath("eog-plan-details-artifacts"));
    return (folder.exists() || QDir().mkpath(folder.absolutePath())) && image.save(folder.filePath(name + ".png"));
}
}

class EogPlanDetailsTests final : public QObject {
    Q_OBJECT
private slots:
    void acceptedFixturesPresentReadableEvidence_data() {
        QTest::addColumn<QJsonObject>("evaluation"); QTest::addColumn<QJsonObject>("checked");
        QTest::addColumn<QStringList>("requiredText");
        QTest::newRow("canonical-truck") << evaluated("evaluate-plan-response.json") << validation()
            << QStringList{"Plan plan-1", "Admissibility: admissible", "Outcome: avoids shutdown", "Truck cost: 100 USD", "Total: 100 USD",
                "Job job-1 · Truck truck-1 · Facility field:A", "Call 0 s after start · Available again 5400 s after start",
                "Trip 1 · Arrival 1800 s after start · Load 150 bbl · Cost 100 USD", "Truck truck-1 · Job job-1",
                "Validation validation-1", "Evaluator: event-evaluator-v1 · Validator: independent-validator-v1", "Minimum shutdown margin: 70 bbl"};
        QTest::newRow("well-only") << evaluated("well-only-evaluation-response.json") << QJsonObject{}
            << QStringList{"Plan well-only", "Total: 425 USD", "No truck jobs proposed.", "Well well:A · Facility field:A · 1080 s after start → 7200 s after start", "Independent validation unavailable."};
        QTest::newRow("automatic-no-action") << evaluated("do-nothing-evaluation-response.json") << QJsonObject{}
            << QStringList{"Admissibility: admissible · Outcome: automatic shutdown", "Truck cost: 0 USD · Lost revenue: 80 USD · Total: 80 USD", "No truck jobs proposed.", "No manual well shut-ins proposed.", "Facility field:A · automatic shutdown at 4320 s after start"};
        QTest::newRow("shared-fleet-infeasible") << evaluated("shared-fleet-infeasible-response.json") << QJsonObject{}
            << QStringList{"Admissibility: inadmissible", "Coverage: partial", "Truck cost: unavailable · Lost revenue: unavailable · Total: unavailable", "fleet_conflict · asset field:B", "The same truck is assigned to two overlapping facility jobs."};
        QTest::newRow("empty-unavailable") << QJsonObject{} << QJsonObject{}
            << QStringList{"No evaluated plan is available.", "Truck cost: unavailable", "Truck schedule unavailable.", "Independent validation unavailable.", "Minimum shutdown margin: unavailable"};
    }
    void acceptedFixturesPresentReadableEvidence() {
        QFETCH(QJsonObject, evaluation); QFETCH(QJsonObject, checked); QFETCH(QStringList, requiredText);
        RenderedDetails details(evaluation, checked); QVERIFY2(details.error.isEmpty(), qPrintable(details.error));
        const auto tag = QString::fromLatin1(QTest::currentDataTag());
        const auto first = details.show(); QVERIFY2(!first.isNull(), qPrintable(details.error)); QVERIFY(save(first, tag + "-summary"));
        for (const auto& expected : requiredText) {
            auto* target = details.text(expected); QVERIFY2(target, qPrintable("Missing readable text: " + expected));
            const auto frame = details.show(target); QVERIFY2(!frame.isNull(), qPrintable(details.error));
            QVERIFY2(details.visibleInk(target, frame), qPrintable("Required text was not drawn in the exposed window: " + expected));
        }
        auto* margin = details.root->findChild<QQuickItem*>("validationMargin"); QVERIFY(margin);
        const auto bottom = details.show(margin); QVERIFY(save(bottom, tag + "-validation"));
        QVERIFY2(details.warnings.isEmpty(), qPrintable(details.warnings.join('\n')));
        QVERIFY(!details.root->findChild<QObject*>("advancedJson"));
        QCOMPARE(objectProperty(details.root.get(), "evaluation"), evaluation);
        QCOMPARE(objectProperty(details.root.get(), "validation"), checked);
    }

    void negativeNullMarginsAndExactAuditDataRemainExplicit() {
        const auto evaluation = evaluated("evaluate-plan-response.json"); auto checked = validation();
        checked["minimum_shutdown_margin_bbl"] = -12.5; checked["feasible"] = false;
        checked["coverage"] = "partial"; checked["outcome"] = "unavailable"; checked["cost"] = QJsonValue::Null;
        RenderedDetails details(evaluation, checked); QVERIFY2(details.error.isEmpty(), qPrintable(details.error));
        auto* margin = details.root->findChild<QQuickItem*>("validationMargin"); QVERIFY(margin);
        QVERIFY(margin->property("text").toString().contains("-12.5 bbl"));
        QCOMPARE(margin->property("color").value<QColor>(), QColor("#ed8181"));
        auto frame = details.show(margin); QVERIFY(details.visibleInk(margin, frame)); QVERIFY(save(frame, "negative-validation-margin"));
        checked["minimum_shutdown_margin_bbl"] = QJsonValue::Null;
        QVERIFY(details.root->setProperty("validation", checked.toVariantMap()));
        QTRY_VERIFY(margin->property("text").toString().contains("unavailable"));
        QVERIFY(!margin->property("text").toString().contains("0 bbl"));
        auto* disclosure = details.root->findChild<QQuickItem*>("advancedDisclosure"); QVERIFY(disclosure);
        QVERIFY(!details.root->findChild<QObject*>("advancedJson"));
        QVERIFY(disclosure->setProperty("checked", true));
        QTRY_VERIFY(details.root->findChild<QQuickItem*>("advancedJson"));
        auto* exact = details.root->findChild<QQuickItem*>("advancedJson");
        const auto restored = QJsonDocument::fromJson(exact->property("text").toString().toUtf8()).object();
        QCOMPARE(restored.value("evaluation").toObject(), evaluation); QCOMPARE(restored.value("validation").toObject(), checked);
        QVERIFY2(details.warnings.isEmpty(), qPrintable(details.warnings.join('\n')));
    }

    void rawIdsAndAssumptionsArePlainTextAndJobChoiceDoesNotMutatePlan() {
        auto evaluation = evaluated("shared-fleet-infeasible-response.json");
        evaluation["assumptions"] = QJsonArray{"<b>Recorded & untrusted assumption</b>"};
        RenderedDetails details(evaluation, {}); QVERIFY2(details.error.isEmpty(), qPrintable(details.error));
        QVERIFY(!details.show().isNull());
        auto* choice = details.root->findChild<QQuickItem*>("jobChoice"); QVERIFY(choice);
        QCOMPARE(choice->property("count").toInt(), 2); QVERIFY(choice->setProperty("currentIndex", 1));
        QTRY_VERIFY(details.text("Job job-2 · Truck truck-1 · Facility field:B"));
        auto* target = details.text("Job job-2 · Truck truck-1 · Facility field:B");
        const auto frame = details.show(target); QVERIFY(details.visibleInk(target, frame)); QVERIFY(save(frame, "shared-fleet-second-job"));
        target = details.text("<b>Recorded & untrusted assumption</b>"); QVERIFY(target);
        QCOMPARE(target->property("textFormat").toInt(), 0); // Qt::PlainText
        const auto assumption = details.show(target); QVERIFY(details.visibleInk(target, assumption)); QVERIFY(save(assumption, "plain-text-assumption"));
        QCOMPARE(objectProperty(details.root.get(), "evaluation"), evaluation);
        QVERIFY2(details.warnings.isEmpty(), qPrintable(details.warnings.join('\n')));
    }

    void longTripScheduleRemainsLazyAndAllRowsAreViewable() {
        // A presentation stress fixture, not an evaluated numerical schedule.
        auto evaluation = evaluated("evaluate-plan-response.json"), plan = evaluation.value("plan").toObject();
        auto jobs = plan.value("truck_jobs").toArray(); auto job = jobs.first().toObject(); QJsonArray trips;
        for (int i = 0; i < 4096; ++i)
            trips.append(QJsonObject{{"arrival_elapsed_s", 1800.0 + i * 3600.0}, {"volume_bbl", 150}, {"cost", 100}});
        job["trips"] = trips; jobs[0] = job; plan["truck_jobs"] = jobs; evaluation["plan"] = plan;
        RenderedDetails details(evaluation, {}); QVERIFY2(details.error.isEmpty(), qPrintable(details.error));
        auto* rows = details.root->findChild<QQuickItem*>("tripRows"); QVERIFY(rows);
        QTRY_COMPARE(rows->property("count").toInt(), 4096);
        QVERIFY(!details.show(rows).isNull());
        int visibleDelegates = 0;
        for (auto* item : visualItems(rows))
            if (item->property("text").toString().startsWith("Trip ")) ++visibleDelegates;
        QVERIFY(visibleDelegates > 0); QVERIFY(visibleDelegates < 128);
        QVERIFY(QMetaObject::invokeMethod(rows, "positionViewAtEnd"));
        QTRY_VERIFY(details.text("Trip 4096 · Arrival "));
        auto* last = details.text("Trip 4096 · Arrival ");
        const auto frame = details.show(last); QVERIFY(details.visibleInk(last, frame)); QVERIFY(save(frame, "long-schedule-final-trip"));
        QCOMPARE(objectProperty(details.root.get(), "evaluation"), evaluation);
        QVERIFY2(details.warnings.isEmpty(), qPrintable(details.warnings.join('\n')));
    }
};

int main(int argc, char** argv) {
    QGuiApplication app(argc, argv); QQuickStyle::setStyle("Basic");
    EogPlanDetailsTests tests; return QTest::qExec(&tests, argc, argv);
}
#include "eog_plan_details_tests.moc"
