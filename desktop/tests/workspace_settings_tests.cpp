#include "WorkspaceSettings.hpp"
#include <QFile>
#include <QTemporaryDir>
#include <QtTest>
#include <limits>
using breakwater::desktop::WorkspaceSettings;
class WorkspaceSettingsTests final : public QObject {
    Q_OBJECT
private slots:
    void preferencesSurviveRestartAndRejectInvalidValues() {
        QTemporaryDir directory; QVERIFY(directory.isValid());
        const auto path = directory.filePath("preferences.ini");
        { WorkspaceSettings settings(path); QVERIFY(settings.setValue("dockHeight", 380));
          QVERIFY(settings.setValue("navigatorOpen", false));
          QVERIFY(!settings.setValue("quality", std::numeric_limits<double>::infinity()));
          QVERIFY(!settings.setValue("dockHeight", 1)); QVERIFY(!settings.setValue("navigatorOpen", "true"));
          QVERIFY(!settings.setValue("credentials", "never stored")); }
        WorkspaceSettings restored(path); QCOMPARE(restored.values().value("dockHeight").toInt(), 380);
        QCOMPARE(restored.values().value("navigatorOpen").toBool(), false);
        restored.reset(); QCOMPARE(restored.values().value("dockHeight").toInt(), 270);
    }
    void futureAndCorruptSettingsUseSafeDefaults() {
        QTemporaryDir directory; const auto path = directory.filePath("preferences.ini");
        { QSettings raw(path, QSettings::IniFormat); raw.setValue("version", 99); raw.setValue("view/dockHeight", -1); }
        WorkspaceSettings future(path); QVERIFY(!future.status().isEmpty());
        QCOMPARE(future.values().value("dockHeight").toInt(), 270);
        future.reset(); QVERIFY(future.status().isEmpty());
        { QSettings raw(path, QSettings::IniFormat); raw.setValue("view/dockHeight", "NaN"); raw.setValue("view/quality", 100); }
        WorkspaceSettings corrupt(path); QCOMPARE(corrupt.values().value("dockHeight").toInt(), 270);
        QCOMPARE(corrupt.values().value("quality").toInt(), 1);
    }
    void unsupportedVersionIsNotTruncatedOrOverwritten() {
        QTemporaryDir directory;
        for (const auto& version : {QVariant(99), QVariant(1.5), QVariant(true), QVariant("invalid")}) {
            const auto path = directory.filePath("version-" + version.toString() + ".ini");
            { QSettings raw(path, QSettings::IniFormat); raw.setValue("version", version); }
            QFile original(path); QVERIFY(original.open(QIODevice::ReadOnly)); const auto bytes = original.readAll(); original.close();
            { WorkspaceSettings settings(path); QVERIFY(!settings.status().isEmpty()); QVERIFY(!settings.setValue("quality", 0)); }
            QVERIFY(original.open(QIODevice::ReadOnly)); QCOMPARE(original.readAll(), bytes);
        }
    }
};
QTEST_GUILESS_MAIN(WorkspaceSettingsTests)
#include "workspace_settings_tests.moc"
