#include "history/EvidenceEnvelope.hpp"
#include "history/SessionRepository.hpp"
#include "telemetry/ObservationMonitor.hpp"
#include "telemetry/SensorProject.hpp"
#include "telemetry/TelemetryContracts.hpp"
#include "ProjectDocument.hpp"
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QSignalSpy>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QTemporaryDir>
#include <QUuid>
#include <QtTest>
#include <algorithm>
using namespace breakwater;
using namespace breakwater::desktop;
using namespace breakwater::desktop::history;
using namespace breakwater::desktop::telemetry;
namespace {
QByteArray wire(const QJsonObject& v) { return QJsonDocument(v).toJson(QJsonDocument::Compact)+'\n'; }
ProjectDocument project() {
    ProjectDocument p; p.network={{{"tank","Tank",AssetKind::tank,200,80},{"well","Well",AssetKind::source,0,0,36,0,36},
        {"destination","Destination",AssetKind::treatment,100}},
        {{"feed","well","tank",100,36,false,true},{"pump","tank","destination",100,12,true,true}}}; return p;
}
QJsonObject detectorArchive() {
    const auto p=project(); const auto config=volume_sensor_configuration(p); auto context=sensor_project_context(p); context["configuration"]=config;
    QJsonArray commands; for (const auto& c:control_catalog(p.network)) commands.append(QJsonObject{{"control_id",QString::fromStdString(c.id)},{"value",c.value},{"unit","m3/h"}});
    const auto frame=[&](const QString& type,int sequence,int time,const QJsonObject& payload) {
        return QJsonObject{{"telemetry_version",1},{"kind","stream_frame"},{"type",type},{"stream_id","test-source"},{"epoch_id","test-epoch"},
            {"sequence",sequence},{"configuration_hash",sensor_semantic_hash(config)},{"physical_hash",physics_hash(p)},{"event_time_s",time},{"payload",payload}};
    };
    TelemetryStream stream; stream.configure(config,context); stream.beginEpoch("synthetic_live");
    if (!stream.ingest(wire(frame("start",0,0,{{"mode","synthetic_live"},{"paused",false},{"clock_rate",1}})),0,0,"2026-11-19T00:00:00.000Z")) throw std::runtime_error("Cannot start detector fixture");
    ObservationMonitor monitor; monitor.reset(true);
    for (int time=0;time<=45;++time) {
        QJsonArray readings;
        for (const auto& value:config.value("sensors").toArray()) {
            const auto sensor=value.toObject(); const auto quantity=sensor.value("quantity").toString();
            const QJsonValue measurement=quantity=="inventory" ? QJsonValue(80+24.0*time/3600-18.0*std::max(0,time-5)/3600):
                quantity=="source_flow" ? QJsonValue(36):quantity=="route_flow" ? QJsonValue(12):QJsonValue("running");
            readings.append(QJsonObject{{"sensor_id",sensor.value("sensor_id")},{"event_time_s",time},{"acquisition_start_s",time},{"acquisition_end_s",time},
                {"value",measurement},{"unit",sensor.value("unit")},{"quality","good"}});
        }
        if (!stream.ingest(wire(frame("readings",time+1,time,{{"readings",readings}})),time,time,"2026-11-19T00:00:00.000Z")) throw std::runtime_error("Cannot ingest detector fixture");
        const auto capture=stream.captureObserved(context,commands,7,"snapshot-"+QString::number(time)); const auto changed=monitor.observe(capture);
        if (changed.value("events").toArray().isEmpty()) continue;
        const QJsonObject payload{{"evidence_kind","observation_detection"},{"evidence_version",1},{"events",changed.value("events")},
            {"snapshot",capture.document},{"context",capture.context},{"mapping",capture.diagnostics},{"effective_network",capture.effective_network},
            {"detector",monitor.report()},{"balance_witness",changed.value("balance_witness")},{"actionable",false}};
        return evidence_archive("observation_detection",payload,{{"snapshot_id",capture.document.value("snapshot_id")},{"physical_hash",physics_hash(p)},
            {"network",network_json(p.network)},{"horizon_h",3}},"Observed residual fixture");
    }
    throw std::runtime_error("Detector fixture did not activate");
}
QJsonObject legacy() {
    auto a=detectorArchive(); a["archive_version"]=1; a.remove("evidence_envelopes"); a.remove("session_kind"); return a;
}
bool createV1(const QString& path,const QJsonObject& archive) {
    const auto name="v1-"+QUuid::createUuid().toString(); bool ok=false;
    {
        auto db=QSqlDatabase::addDatabase("QSQLITE",name); db.setDatabaseName(path);
        if (db.open()) {
            QSqlQuery q(db);
            ok=q.exec("CREATE TABLE sessions(session_id TEXT PRIMARY KEY NOT NULL, checkpoint_sequence INTEGER NOT NULL CHECK(checkpoint_sequence>0), created_at TEXT NOT NULL, updated_at TEXT NOT NULL, status TEXT NOT NULL, objective TEXT NOT NULL, snapshot_id TEXT NOT NULL, physical_hash TEXT NOT NULL, archive BLOB NOT NULL)");
            if (ok) ok=q.exec("PRAGMA user_version=1");
            if (ok) {
                q.prepare("INSERT INTO sessions VALUES(?,?,?,?,?,?,?,?,?)");
                for (const auto* field:{"session_id","checkpoint_sequence","created_at","updated_at","status","objective"}) q.addBindValue(archive.value(QLatin1String(field)).toVariant());
                q.addBindValue(archive.value("snapshot").toObject().value("snapshot_id").toString());
                q.addBindValue(archive.value("snapshot").toObject().value("physical_hash").toString()); q.addBindValue(wire(archive)); ok=q.exec();
            }
            db.close();
        }
    }
    QSqlDatabase::removeDatabase(name); return ok;
}
}
class EvidenceV2Tests final:public QObject {
    Q_OBJECT
private slots:
    void strictDetectorWitnessAndDowngradeChecks() {
        const auto archive=detectorArchive(); QString error;
        QVERIFY2(SessionRepository::validateArchive(archive,&error),qPrintable(error));
        auto altered=archive; altered["archive_version"]=1; QVERIFY(!SessionRepository::validateArchive(altered));
        altered=archive; altered["actionable"]=true; QVERIFY(!SessionRepository::validateArchive(altered));
        altered=archive; altered["unknown"]=true; QVERIFY(!SessionRepository::validateArchive(altered));
        auto envelopeRow=archive.value("evidence_envelopes").toArray().first().toObject(); auto payload=envelopeRow.value("payload").toObject();
        auto events=payload.value("events").toArray(); auto event=events.first().toObject(); auto metric=event.value("evidence").toArray().first().toObject();
        metric["threshold"]=1; event["evidence"]=QJsonArray{metric}; events[0]=event; payload["events"]=events;
        envelopeRow["payload"]=payload; altered=archive; altered["evidence_envelopes"]=QJsonArray{envelopeRow};
        QVERIFY(!SessionRepository::validateArchive(altered));
    }
    void restartExportAndImportRetainV2WithoutAuthority() {
        QTemporaryDir dir; const auto path=dir.filePath("history.sqlite"); const auto archive=detectorArchive();
        {
            SessionRepository repo(path); QSignalSpy ready(&repo,&SessionRepository::ready),saved(&repo,&SessionRepository::sessionSaved),errors(&repo,&SessionRepository::failed);
            QTRY_COMPARE(ready.size(),1); repo.saveSession(archive); QTRY_COMPARE(saved.size(),1); QVERIFY(errors.isEmpty());
            auto replacement=archive; replacement["checkpoint_sequence"]=2; replacement["objective"]="Rewritten evidence";
            repo.saveSession(replacement); QTRY_COMPARE(errors.size(),1);
        }
        SessionRepository reopened(path); QSignalSpy ready(&reopened,&SessionRepository::ready),read(&reopened,&SessionRepository::sessionLoaded),done(&reopened,&SessionRepository::operationCompleted);
        QTRY_COMPARE(ready.size(),1); reopened.loadSession(archive.value("session_id").toString()); QTRY_COMPARE(read.size(),1);
        const auto historical=read.first().first().value<QJsonObject>(); QVERIFY(historical.value("historical").toBool()); QVERIFY(!historical.value("actionable").toBool());
        QCOMPARE(historical.value("evidence_envelopes"),archive.value("evidence_envelopes"));
        reopened.exportSession(archive.value("session_id").toString(),QUrl::fromLocalFile(dir.filePath("export.json")));
        QTRY_VERIFY(QFileInfo::exists(dir.filePath("export.json")));
        QFile file(dir.filePath("export.json")); QVERIFY(file.open(QIODevice::ReadOnly)); QCOMPARE(decode_document(file.readAll(),8*1024*1024),archive);
    }
    void migrationBacksUpV1BeforeChangingItsSchema() {
        QTemporaryDir dir; const auto path=dir.filePath("legacy.sqlite"); const auto archive=legacy(); QVERIFY(createV1(path,archive));
        SessionRepository repo(path); QSignalSpy ready(&repo,&SessionRepository::ready),read(&repo,&SessionRepository::sessionInspected),errors(&repo,&SessionRepository::failed);
        QTRY_COMPARE(ready.size(),1); QVERIFY(errors.isEmpty());
        const auto backups=QDir(dir.path()).entryList({"legacy.sqlite.v1-backup-*.sqlite"},QDir::Files); QCOMPARE(backups.size(),1);
        const auto name="backup-check-"+QUuid::createUuid().toString();
        {
            auto db=QSqlDatabase::addDatabase("QSQLITE",name); db.setDatabaseName(dir.filePath(backups.first())); QVERIFY(db.open());
            QSqlQuery q(db); QVERIFY(q.exec("PRAGMA user_version")); QVERIFY(q.next()); QCOMPARE(q.value(0).toInt(),1); q.finish();
            QVERIFY(q.exec("SELECT archive FROM sessions")); QVERIFY(q.next()); QCOMPARE(q.value(0).toByteArray(),wire(archive)); q.finish(); db.close();
        }
        QSqlDatabase::removeDatabase(name);
        repo.inspectSession(archive.value("session_id").toString()); QTRY_COMPARE(read.size(),1); QCOMPARE(read.first().first().value<QJsonObject>(),archive);
    }
    void duplicateKeysCannotBeNormalizedIntoAnImport() {
        QTemporaryDir dir; SessionRepository repo(dir.filePath("history.sqlite"));
        QSignalSpy ready(&repo,&SessionRepository::ready),errors(&repo,&SessionRepository::failed),saved(&repo,&SessionRepository::sessionSaved);
        QTRY_COMPARE(ready.size(),1);
        auto bytes=wire(detectorArchive()); bytes.replace("\"archive_version\":2","\"archive_version\":1,\"archive\\u005fversion\":2");
        const auto path=dir.filePath("duplicate.json"); QFile file(path); QVERIFY(file.open(QIODevice::WriteOnly)); QCOMPARE(file.write(bytes),bytes.size()); file.close();
        repo.importSession(QUrl::fromLocalFile(path)); QTRY_COMPARE(errors.size(),1); QVERIFY(saved.isEmpty());
        QCOMPARE(errors.first().first().toString(),QString("invalid_archive"));
    }
};
QTEST_GUILESS_MAIN(EvidenceV2Tests)
#include "evidence_v2_tests.moc"
