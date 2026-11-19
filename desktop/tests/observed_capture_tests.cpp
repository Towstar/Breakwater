#include "telemetry/TelemetryController.hpp"
#include "telemetry/TelemetryStream.hpp"
#include "telemetry/TelemetryContracts.hpp"
#include "telemetry/SensorProject.hpp"
#include "ProjectDocument.hpp"
#include <QFile>
#include <QJsonDocument>
#include <QSignalSpy>
#include <QtTest>
#include <algorithm>
#include <stdexcept>

using namespace breakwater::desktop;
using namespace breakwater::desktop::telemetry;
namespace {
QJsonObject sourceJson(const char* name) {
    QFile file(QStringLiteral(BREAKWATER_SOURCE_DIR)+QString::fromLatin1(name));
    if (!file.open(QIODevice::ReadOnly)) throw std::runtime_error("Missing capture fixture");
    return decode_document(file.readAll());
}
QByteArray bytes(const QJsonObject& value) { return QJsonDocument(value).toJson(QJsonDocument::Compact)+'\n'; }
struct Fixture {
    ProjectDocument project=parse_project(sourceJson("/contracts/telemetry/examples/canonical-project-v1.json"));
    QJsonObject configuration=sourceJson("/contracts/telemetry/examples/canonical-sensors.json");
    QJsonObject context;
    QJsonArray readings,commands;
    TelemetryStream stream;
    Fixture() {
        const auto prototype=configuration.value("sensors").toArray().first().toObject();
        QJsonArray sensors;
        const auto add=[&](const QString& quantity,const QString& unit,const QString& attachment,const QJsonValue& value) {
            auto sensor=prototype; const auto id=quantity+"-"+attachment;
            sensor["sensor_id"]=id; sensor["channel_id"]=id; sensor["group_id"]=id; sensor["label"]=id;
            sensor["quantity"]=quantity; sensor["unit"]=unit;
            sensor["attachment"]=QJsonObject{{"kind",quantity=="inventory" || quantity=="source_flow" ? "asset":"route"},{"id",attachment}};
            sensors.append(sensor);
            readings.append(QJsonObject{{"sensor_id",id},{"event_time_s",100},{"acquisition_start_s",100},{"acquisition_end_s",100},
                {"value",value},{"unit",unit},{"quality","good"}});
        };
        for (const auto& asset:project.network.assets) {
            if (asset.kind==breakwater::AssetKind::tank) add("inventory","m3",QString::fromStdString(asset.id),asset.initial_m3);
            if (asset.kind==breakwater::AssetKind::source) add("source_flow","m3/h",QString::fromStdString(asset.id),asset.inflow_m3h-asset.curtailment_m3h);
        }
        for (const auto& route:project.network.routes) {
            const auto source=std::find_if(project.network.assets.begin(),project.network.assets.end(),[&](const auto& a){return a.id==route.source_id;});
            if (source->kind!=breakwater::AssetKind::tank) continue;
            add("route_flow","m3/h",QString::fromStdString(route.id),route.setpoint_m3h);
            if (route.has_pump) add("pump_state","state",QString::fromStdString(route.id),route.available ? "running":"unavailable");
        }
        configuration["sensors"]=sensors;
        context=sensor_project_context(project); context["configuration"]=configuration;
        for (const auto& control:breakwater::control_catalog(project.network)) commands.append(QJsonObject{
            {"control_id",QString::fromStdString(control.id)},{"value",control.value},{"unit",QString::fromStdString(control.unit)}});
    }
    QJsonObject frame(const QString& type,int sequence,double event,const QJsonObject& payload) const {
        return {{"telemetry_version",1},{"kind","stream_frame"},{"type",type},{"stream_id","local-plant"},{"epoch_id","epoch-one"},
            {"sequence",sequence},{"configuration_hash",sensor_semantic_hash(configuration)},{"physical_hash",physics_hash(project)},
            {"event_time_s",event},{"payload",payload}};
    }
    void begin() {
        stream.configure(configuration,context); stream.beginEpoch("synthetic_live");
        if (!stream.ingest(bytes(frame("start",0,100,{{"mode","synthetic_live"},{"clock_rate",1},{"paused",false}})),100,10,"2026-11-18T00:00:00.000Z") ||
            !stream.ingest(bytes(frame("readings",1,100,{{"readings",readings}})),100,10.01,"2026-11-18T00:00:00.010Z"))
            throw std::runtime_error("Cannot begin capture fixture");
    }
};
}
class ObservedCaptureTests final: public QObject {
    Q_OBJECT
private slots:
    void strictProjectionMapsSeparateImmutableNetwork() {
        Fixture f; f.begin();
        const auto capture=f.stream.captureObserved(f.context,f.commands,7,"capture-one");
        const auto parsed=parse_observed_snapshot(capture.document,capture.context);
        QCOMPARE(parsed.numerical_revision,quint64(7)); QCOMPARE(parsed.availability,std::string("available"));
        QVERIFY(!parsed.actionable); QVERIFY(!capture.effective_network.isEmpty());
        QCOMPARE(capture.diagnostics.value("mapper_version"),QJsonValue("bounded-observations-0.1.0"));
        QCOMPARE(capture.document.value("commands").toArray(),f.commands);
        auto changed=f.readings;
        for (qsizetype i=0;i<changed.size();++i) {
            auto row=changed[i].toObject(); row["event_time_s"]=101; row["acquisition_start_s"]=101; row["acquisition_end_s"]=101;
            if (row.value("sensor_id")=="inventory-tank_a") row["value"]=81;
            changed[i]=row;
        }
        QVERIFY(f.stream.ingest(bytes(f.frame("readings",2,101,{{"readings",changed}})),101,11,"2026-11-18T00:00:01.000Z"));
        const auto next=f.stream.captureObserved(f.context,f.commands,7,"capture-two");
        QVERIFY(next.effective_network!=capture.effective_network);
        QCOMPARE(capture.context.value("network"),f.context.value("network"));
        QCOMPARE(capture.document.value("captured_event_time_s"),QJsonValue(100));
        QCOMPARE(next.diagnostics.value("design_physical_hash"),QJsonValue(physics_hash(f.project)));
        QVERIFY(next.diagnostics.value("effective_physical_hash")!=next.diagnostics.value("design_physical_hash"));
    }
    void staleRawEvidenceCannotInitializeForecastAndIsNeverClipped() {
        Fixture f;
        auto row=f.readings.first().toObject(); row["value"]=-4; f.readings[0]=row;
        f.begin(); const auto negative=f.stream.captureObserved(f.context,f.commands,0,"negative");
        QVERIFY(negative.effective_network.isEmpty()); QCOMPARE(negative.document.value("availability"),QJsonValue("unavailable"));
        QCOMPARE(negative.document.value("samples").toArray().first().toObject().value("canonical_value"),QJsonValue(-4));
        f.stream.advance(110,20);
        const auto stale=f.stream.captureObserved(f.context,f.commands,0,"stale");
        QVERIFY(stale.effective_network.isEmpty()); QCOMPARE(stale.document.value("expires_event_time_s"),QJsonValue(110));
        (void)parse_observed_snapshot(stale.document,stale.context);
    }
    void unsupportedAuxiliarySensorIsNotRequiredForVolumeForecast() {
        Fixture f;
        auto auxiliary=f.configuration.value("sensors").toArray().first().toObject();
        auxiliary["sensor_id"]="temperature-a"; auxiliary["channel_id"]="temperature-a"; auxiliary["group_id"]="temperature-a";
        auxiliary["quantity"]="temperature"; auxiliary["unit"]="degC";
        auto sensors=f.configuration.value("sensors").toArray(); sensors.append(auxiliary); f.configuration["sensors"]=sensors;
        f.context["configuration"]=f.configuration; f.begin();
        const auto capture=f.stream.captureObserved(f.context,f.commands,0,"supported-operation");
        QCOMPARE(capture.document.value("availability"),QJsonValue("available"));
        QVERIFY(!capture.document.value("coverage").toObject().value("required_sensor_ids").toArray().contains("temperature-a"));
        QVERIFY(!capture.diagnostics.value("raw_coverage_complete").toBool());
    }
    void mismatchedContextAndCommandsRejectBeforePublishing() {
        Fixture f; f.begin(); auto context=f.context; context["physical_hash"]=QString(64,'0');
        QVERIFY_THROWS_EXCEPTION(std::invalid_argument,f.stream.captureObserved(context,f.commands,0,"bad-context"));
        auto commands=f.commands; auto command=commands[0].toObject(); command["value"]=999; commands[0]=command;
        QVERIFY_THROWS_EXCEPTION(std::invalid_argument,f.stream.captureObserved(f.context,commands,0,"bad-command"));
        commands=f.commands; commands.removeLast();
        QVERIFY_THROWS_EXCEPTION(std::invalid_argument,f.stream.captureObserved(f.context,commands,0,"missing-command"));
        QVERIFY_THROWS_EXCEPTION(std::invalid_argument,f.stream.captureObserved(f.context,f.commands,9007199254740992ULL,"bad-revision"));
    }
    void contradictoryFeedMetersRetainRawEvidenceButNoConflictingDerivedState() {
        Fixture f;
        auto sensor=f.configuration.value("sensors").toArray().first().toObject();
        sensor["sensor_id"]="feed-check"; sensor["channel_id"]="feed-check"; sensor["group_id"]="feed-check";
        sensor["quantity"]="route_flow"; sensor["unit"]="m3/h";
        sensor["attachment"]=QJsonObject{{"kind","route"},{"id","feed_a"}};
        auto sensors=f.configuration.value("sensors").toArray(); sensors.append(sensor); f.configuration["sensors"]=sensors;
        f.context["configuration"]=f.configuration;
        f.readings.append(QJsonObject{{"sensor_id","feed-check"},{"event_time_s",100},{"acquisition_start_s",100},
            {"acquisition_end_s",100},{"value",1},{"unit","m3/h"},{"quality","good"}});
        f.begin(); const auto capture=f.stream.captureObserved(f.context,f.commands,0,"conflicting-feed");
        QVERIFY(capture.effective_network.isEmpty());
        QCOMPARE(capture.document.value("availability"),QJsonValue("unavailable"));
        QVERIFY(capture.document.value("coverage").toObject().value("conflicting_sensor_ids").toArray().contains("feed-check"));
        for (const auto& state:capture.document.value("derived_state").toArray())
            QVERIFY(!state.toObject().value("source_sensor_ids").toArray().contains("feed-check"));
        (void)parse_observed_snapshot(capture.document,capture.context);
    }
    void receiverThreadCaptureRetiresOnNumericalEditButNotLabelOrLayoutEdit() {
        Fixture f; TelemetryController controller; controller.configure(f.configuration,f.context);
        QVERIFY(controller.setObservedCaptureContext(f.context,3,f.commands));
        QTRY_COMPARE(controller.status(),QString("ready"));
        controller.startProcess(QStringLiteral(BREAKWATER_TEST_PYTHON),{QStringLiteral(BREAKWATER_SOURCE_DIR "/desktop/tests/telemetry_fixtures/publisher_fixture.py"),"normal"});
        QTRY_VERIFY_WITH_TIMEOUT(!controller.snapshot().value("stream_id").toString().isEmpty(),10000);
        QSignalSpy captured(&controller,&TelemetryController::observedCaptureReady);
        QVERIFY(!controller.requestObservedCapture().isEmpty()); QTRY_COMPARE(captured.size(),1);
        auto original=controller.capturedObservation(); QVERIFY(original);
        auto registry=f.context;
        auto configuration=f.configuration; auto sensors=configuration.value("sensors").toArray(); auto sensor=sensors[0].toObject();
        sensor["label"]="A cosmetic label"; sensors[0]=sensor; configuration["sensors"]=sensors; registry["configuration"]=configuration;
        auto project=registry.value("project").toObject(); project["layout"]=QJsonObject{}; registry["project"]=project;
        controller.configure(configuration,registry); QVERIFY(controller.setObservedCaptureContext(registry,3,f.commands));
        QVERIFY(controller.observedCaptureDisposition()!="retired"); QCOMPARE(controller.capturedObservation(),original);
        QVERIFY(controller.setObservedCaptureContext(registry,4,f.commands));
        QCOMPARE(controller.observedCaptureDisposition(),QString("retired")); QCOMPARE(controller.capturedObservation(),original);
        controller.stop(); QTRY_VERIFY(!controller.active());
    }
    void cancellationAndSourceReplacementCannotPublishQueuedCapture() {
        Fixture f; TelemetryController controller; controller.configure(f.configuration,f.context);
        QVERIFY(controller.setObservedCaptureContext(f.context,0,f.commands)); QTRY_COMPARE(controller.status(),QString("ready"));
        controller.startProcess(QStringLiteral(BREAKWATER_TEST_PYTHON),{QStringLiteral(BREAKWATER_SOURCE_DIR "/desktop/tests/telemetry_fixtures/publisher_fixture.py"),"normal"});
        QTRY_VERIFY_WITH_TIMEOUT(!controller.snapshot().value("stream_id").toString().isEmpty(),10000);
        QSignalSpy ready(&controller,&TelemetryController::observedCaptureReady);
        const auto cancel=connect(&controller,&TelemetryController::observedCaptureChanged,&controller,[&] {
            if (controller.observedCaptureBusy()) controller.cancelObservedCapture();
        });
        QVERIFY(!controller.requestObservedCapture().isEmpty()); QTest::qWait(150); QVERIFY(ready.isEmpty());
        disconnect(cancel); controller.requestObservedCapture(); controller.manual();
        QTRY_COMPARE(controller.mode(),QString("manual")); QTest::qWait(100); QVERIFY(ready.isEmpty());
    }
};
QTEST_GUILESS_MAIN(ObservedCaptureTests)
#include "observed_capture_tests.moc"
