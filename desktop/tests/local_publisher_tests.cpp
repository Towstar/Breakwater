#include "telemetry/TelemetryController.hpp"
#include "telemetry/TelemetryStream.hpp"
#include "telemetry/TelemetryContracts.hpp"
#include "telemetry/SensorProject.hpp"
#include "ProjectDocument.hpp"
#include "ProjectController.hpp"
#include <QFile>
#include <QJsonDocument>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QtTest>

using namespace breakwater::desktop;
using namespace breakwater::desktop::telemetry;
namespace {
struct Environment {
    QByteArray name,previous;
    Environment(QByteArray key,QByteArray value):name(std::move(key)),previous(qgetenv(name.constData())) { qputenv(name.constData(),value); }
    ~Environment() { if (previous.isNull()) qunsetenv(name.constData()); else qputenv(name.constData(),previous); }
};
struct Fixture {
    ProjectDocument project;
    QJsonObject configuration,context;
    QJsonArray commands;
    Fixture() {
        using breakwater::AssetKind;
        project.network.assets={{"tank","Tank",AssetKind::tank,200,80}, {"well","Well",AssetKind::source,0,0,36,0,36},
                                {"destination","Destination",AssetKind::treatment,60}};
        project.network.routes={{"feed","well","tank",60,36,false,true}, {"delivery","tank","destination",60,12,true,true}};
        configuration=empty_sensor_configuration(project);
        QFile file(QStringLiteral(BREAKWATER_SOURCE_DIR "/contracts/telemetry/examples/canonical-sensors.json"));
        if (!file.open(QIODevice::ReadOnly)) throw std::runtime_error("Missing publisher sensor fixture");
        const auto prototype=decode_document(file.readAll()).value("sensors").toArray().first().toObject();
        QJsonArray sensors;
        const auto add=[&](const QString& id,const QString& quantity,const QString& attachment,const QString& unit) {
            auto row=prototype; row["sensor_id"]=id; row["channel_id"]=id; row["group_id"]=id; row["label"]=id;
            row["quantity"]=quantity; row["unit"]=unit; row["uncertainty_bound"]=0.001;
            row["attachment"]=QJsonObject{{"kind",quantity=="inventory" || quantity=="source_flow" ? "asset":"route"},{"id",attachment}};
            sensors.append(row);
        };
        add("volume","inventory","tank","m3"); add("production","source_flow","well","m3/h");
        add("flow","route_flow","delivery","m3/h"); add("feedback","pump_state","delivery","state");
        configuration["sensors"]=sensors; context=sensor_project_context(project); context["configuration"]=configuration;
        for (const auto& control:breakwater::control_catalog(project.network)) commands.append(QJsonObject{
            {"control_id",QString::fromStdString(control.id)},{"value",control.value},{"unit",QString::fromStdString(control.unit)}});
    }
};
QVariantMap sensor(const TelemetryController& controller,const QString& id) {
    for (const auto& value:controller.sensors()) if (value.toMap().value("sensor_id")==id) return value.toMap();
    return {};
}
}
class LocalPublisherTests final: public QObject {
    Q_OBJECT
private slots:
    void instrumentedExampleIsOneUndoableProjectEdit() {
        ProjectController project; const auto original=project.editorDocument(); const auto hash=project.networkHash();
        project.loadExample("monitoring");
        QCOMPARE(project.sensorConfiguration().value("sensors").toArray().size(),10);
        QVERIFY(project.draftIssues().isEmpty()); QVERIFY(project.networkHash()!=hash);
        validate_document(project.editorDocument(),"project_v2",project.sensorContext());
        project.undo(); QCOMPARE(project.editorDocument(),original); QCOMPARE(project.networkHash(),hash);
    }
    void actualPythonPublisherFlowsThroughAcquisitionAndFrozenNativeMapping() {
        Fixture f; TelemetryController controller; controller.setTimeoutMs(15000);
        controller.configure(f.configuration,f.context); QVERIFY(controller.setObservedCaptureContext(f.context,0,f.commands));
        QTRY_COMPARE(controller.status(),QString("ready"));
        int uiTicks=0; QTimer timer; timer.setInterval(5); connect(&timer,&QTimer::timeout,this,[&]{++uiTicks;}); timer.start();
        controller.startProcess(QStringLiteral(BREAKWATER_TEST_PYTHON),{"-B",QStringLiteral(BREAKWATER_SOURCE_DIR "/science/telemetry_publisher.py"),
            "--preset","pump_loss","--seed","9","--onset-s","0.25","--recovery-s","30"});
        QTRY_COMPARE_WITH_TIMEOUT(sensor(controller,"feedback").value("canonical_value").toString(),QString("delivery_loss"),15000);
        QVERIFY2(controller.error().isEmpty(),qPrintable(controller.error())); QVERIFY(uiTicks>10);
        QSignalSpy captured(&controller,&TelemetryController::observedCaptureReady);
        controller.requestObservedCapture(); QTRY_COMPARE(captured.size(),1);
        const auto observation=controller.capturedObservation(); QVERIFY(observation);
        QCOMPARE(observation->document.value("availability"),QJsonValue("available"));
        QVERIFY(!observation->document.value("actionable").toBool());
        const auto effective=parse_project({{"version",1},{"network",observation->effective_network},{"layout",QJsonObject{}}});
        QVERIFY(!effective.network.routes[1].available); QCOMPARE(effective.network.routes[1].setpoint_m3h,0.0);
        QVERIFY(f.project.network.routes[1].available); QCOMPARE(f.project.network.routes[1].setpoint_m3h,12.0);
        controller.pause(); QTRY_VERIFY(controller.paused());
        const auto paused=controller.eventTime(); QTest::qWait(120); QVERIFY(std::abs(controller.eventTime()-paused)<0.03);
        controller.setRate(2); QTRY_COMPARE(controller.rate(),2.0); controller.resume(); QTRY_VERIFY(!controller.paused());
        const auto epoch=controller.snapshot().value("epoch_id"); controller.restart();
        QTRY_VERIFY_WITH_TIMEOUT(controller.snapshot().value("epoch_id")!=epoch,10000);
        QCOMPARE(controller.observedCaptureDisposition(),QString("retired"));
        QCOMPARE(controller.capturedObservation(),observation);
        controller.stop(); QTRY_VERIFY_WITH_TIMEOUT(!controller.active(),10000);
        QVERIFY2(controller.error().isEmpty(),qPrintable(controller.error()));
        QTRY_VERIFY_WITH_TIMEOUT(controller.snapshot().value("journal_complete").toBool(),10000);
        QTemporaryDir recordings; QVERIFY(recordings.isValid()); const auto recording=QUrl::fromLocalFile(recordings.filePath("actual-timed.ndjson"));
        QSignalSpy saved(&controller,&TelemetryController::recordingExported);
        controller.exportReceiverJournal(recording); QTRY_COMPARE(saved.size(),1);
        QFile journal(recording.toLocalFile()); QVERIFY(journal.open(QIODevice::ReadOnly));
        QVERIFY(journal.readAll().contains("receiver_journal"));
        controller.startReplay(recording); QTRY_COMPARE_WITH_TIMEOUT(controller.status(),QString("replay_finished"),10000);
        QVERIFY(controller.error().isEmpty()); QCOMPARE(controller.observedCaptureDisposition(),QString("retired"));
    }
    void bundledDefaultUsesExplicitPythonAndPreservesInvalidCustomOverride() {
        Environment python("BREAKWATER_PYTHON",QByteArray(BREAKWATER_TEST_PYTHON));
        Environment program("BREAKWATER_TELEMETRY_EXECUTABLE",{});
        Environment arguments("BREAKWATER_TELEMETRY_ARGUMENTS_JSON",{});
        TelemetryController bundled; QVERIFY(bundled.publisherConfigured()); QVERIFY(bundled.bundledPublisher());
        QVERIFY(bundled.syntheticPresets().contains("pump_loss"));
        QSignalSpy rejected(&bundled,&TelemetryController::failed);
        bundled.startSynthetic("invalid",1); QCOMPARE(rejected.size(),1);
        Environment invalid("BREAKWATER_TELEMETRY_EXECUTABLE","Z:/missing/publisher.exe");
        TelemetryController configured; QVERIFY(!configured.publisherConfigured()); QVERIFY(!configured.bundledPublisher());
    }
};
QTEST_GUILESS_MAIN(LocalPublisherTests)
#include "local_publisher_tests.moc"
