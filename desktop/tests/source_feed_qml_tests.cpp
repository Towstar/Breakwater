#include "ProjectController.hpp"
#include "agent/AgentController.hpp"
#include "agent/CredentialSettings.hpp"
#include "history/HistoryController.hpp"
#include "quality/PipeTransportController.hpp"
#include "science/StorageScienceController.hpp"
#include <QQmlApplicationEngine>
#include <QQuickStyle>
#include <QQuickItem>
#include <QQuickWindow>
#include <QTemporaryDir>
#include <QtTest>
using namespace breakwater::desktop;
namespace {
QQuickItem* control(QQuickItem* parent, const QString& name) {
    if (!parent) return nullptr;
    if (parent->objectName() == name) return parent;
    for (auto* child : parent->childItems()) if (auto* found = control(child, name)) return found;
    return nullptr;
}
}

class SourceFeedQmlTests final : public QObject {
    Q_OBJECT
private slots:
    void actualConnectionFunctionOpensAnAtomicSplitEditor() {
        QQuickStyle::setStyle("Basic");
        ProjectController project; project.addAsset("tank"); const auto target = project.selectedId();
        agent::AgentController agent(&project); QTemporaryDir directory;
        agent::CredentialSettings credentials(nullptr, directory.filePath("test.env"));
        history::HistoryController history(nullptr); quality::PipeTransportController pipe(&project, &history);
        science::PythonWorkerClient worker("unavailable-test-interpreter", "unavailable-test-worker");
        science::StorageScienceController science(&project, &worker);
        QQmlApplicationEngine engine;
        engine.setInitialProperties({{"controller", QVariant::fromValue(&project)}, {"investigator", QVariant::fromValue(&agent)},
            {"credentials", QVariant::fromValue(&credentials)}, {"history", QVariant::fromValue(&history)},
            {"pipeTransport", QVariant::fromValue(&pipe)}, {"scientificWorker", QVariant::fromValue(&worker)}, {"storageScience", QVariant::fromValue(&science)}});
        engine.load(QUrl::fromLocalFile(QStringLiteral(BREAKWATER_SOURCE_DIR "/desktop/qml/Main.qml")));
        QVERIFY(!engine.rootObjects().isEmpty()); auto* root = engine.rootObjects().first();
        auto* dialog = root->findChild<QObject*>("sourceFeedEditor"); QVERIFY(dialog);
        const auto before = project.document();
        project.select("source_b"); QVERIFY(QMetaObject::invokeMethod(root, "beginConnection"));
        QCOMPARE(root->property("connectionSource").toString(), "source_b");
        QVERIFY(QMetaObject::invokeMethod(root, "clickAsset", Q_ARG(QVariant, target)));
        QVERIFY2(root->property("connectionError").toString().isEmpty(), qPrintable(root->property("connectionError").toString()));
        QVERIFY2(dialog->property("visible").toBool(), qPrintable(QString("Dialog closed; selected=%1 source=%2").arg(project.selectedId(), root->property("connectionSource").toString())));
        QCOMPARE(dialog->property("totalBps").toInt(), 10000); QCOMPARE(project.document(), before);
        QQuickItem* field = nullptr;
        QTRY_VERIFY_WITH_TIMEOUT((field=control(dialog->property("contentItem").value<QQuickItem*>(), "sourceShare_tank_b")) != nullptr, 3000);
        auto* window = qobject_cast<QQuickWindow*>(root); QVERIFY(window);
        // Exercise actual text editing, not just the arrow buttons' valueModified.
        field->forceActiveFocus(); QTest::keyClick(window, Qt::Key_A, Qt::ControlModifier);
        for (const auto key : {Qt::Key_7, Qt::Key_5, Qt::Key_Period, Qt::Key_0, Qt::Key_0}) QTest::keyClick(window, key);
        QTest::keyClick(window, Qt::Key_Tab);
        QCOMPARE(field->property("text").toString(), "75.00");
        QCOMPARE(dialog->property("totalBps").toInt(), 12500);
        auto* apply = control(dialog->property("footer").value<QQuickItem*>(), "applySourceSplit"); QVERIFY(apply); QVERIFY(!apply->property("enabled").toBool());
        field->forceActiveFocus(); QTest::keyClick(window, Qt::Key_A, Qt::ControlModifier); QTest::keyClick(window, Qt::Key_Backspace);
        QVERIFY(!dialog->property("validInputs").toBool()); QVERIFY(!apply->property("enabled").toBool());
        QVERIFY(QMetaObject::invokeMethod(dialog, "close")); QCOMPARE(project.document(), before);
        QVERIFY(QMetaObject::invokeMethod(root, "beginConnection"));
        QVERIFY(QMetaObject::invokeMethod(root, "clickAsset", Q_ARG(QVariant, target)));
        QTRY_VERIFY_WITH_TIMEOUT((field=control(dialog->property("contentItem").value<QQuickItem*>(), "sourceShare_tank_b")) != nullptr, 3000);
        field->forceActiveFocus(); QTest::keyClick(window, Qt::Key_A, Qt::ControlModifier);
        for (const auto key : {Qt::Key_7, Qt::Key_5}) QTest::keyClick(window, key);
        auto* second = control(dialog->property("contentItem").value<QQuickItem*>(), "sourceShare_"+target); QVERIFY(second);
        second->forceActiveFocus(); QTest::keyClick(window, Qt::Key_A, Qt::ControlModifier);
        for (const auto key : {Qt::Key_2, Qt::Key_5}) QTest::keyClick(window, key);
        QTest::keyClick(window, Qt::Key_Tab);
        QCOMPARE(dialog->property("totalBps").toInt(), 10000); QVERIFY(apply->property("enabled").toBool());
        QVERIFY(QMetaObject::invokeMethod(apply, "clicked")); QVERIFY(!dialog->property("visible").toBool());
        QCOMPARE(project.sourceFeeds("source_b").size(), 2);
        QCOMPARE(project.sourceFeeds("source_b")[0].toMap().value("flow").toDouble(), 12.0);
        QCOMPARE(project.sourceFeeds("source_b")[1].toMap().value("flow").toDouble(), 4.0);
    }
};
QTEST_MAIN(SourceFeedQmlTests)
#include "source_feed_qml_tests.moc"
