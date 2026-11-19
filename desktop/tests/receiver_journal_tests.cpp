#include "telemetry/ReceiverJournal.hpp"
#include "telemetry/TelemetryController.hpp"
#include "telemetry/TelemetryStream.hpp"
#include "telemetry/TelemetryContracts.hpp"
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QSaveFile>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QtTest>

using namespace breakwater::desktop::telemetry;
namespace {
const QString utc="2026-11-18T00:00:00.000Z";
QJsonObject source(const char* relative) {
    QFile file(QStringLiteral(BREAKWATER_SOURCE_DIR)+QString::fromLatin1(relative));
    if (!file.open(QIODevice::ReadOnly)) throw std::runtime_error("Missing journal fixture");
    return decode_document(file.readAll());
}
QByteArray bytes(const QJsonObject& value) { return QJsonDocument(value).toJson(QJsonDocument::Compact)+'\n'; }
bool write(const QString& path,const QByteArray& value) {
    QSaveFile file(path); return file.open(QIODevice::WriteOnly) && file.write(value)==value.size() && file.commit();
}
struct Fixture {
    QJsonObject configuration=source("/contracts/telemetry/examples/canonical-sensors.json");
    QJsonObject context=source("/contracts/telemetry/fixtures/validation-cases.json").value("contexts").toObject().value("canonical").toObject();
    QString hash=sensor_semantic_hash(configuration);
    QJsonObject frame(const QString& type,int sequence,double event,const QJsonObject& payload) const {
        return {{"telemetry_version",1},{"kind","stream_frame"},{"type",type},{"stream_id","original-source"},{"epoch_id","original-epoch"},
            {"sequence",sequence},{"configuration_hash",hash},{"physical_hash",configuration.value("physical_hash")},
            {"event_time_s",event},{"payload",payload}};
    }
    QJsonObject reading(int sequence,double event,double value) const {
        return frame("readings",sequence,event,{{"readings",QJsonArray{QJsonObject{{"sensor_id","tank-a-volume"},{"event_time_s",event},
            {"acquisition_start_s",event},{"acquisition_end_s",event},{"value",value},{"unit","m3"},{"quality","good"}}}}});
    }
    ReceiverJournal journal() const { ReceiverJournal result; result.reset(hash,configuration.value("physical_hash").toString()); return result; }
    QByteArray adversarialRecording() const {
        auto out=journal();
        const auto append=[&](const QJsonObject& frame,double elapsed) { out.append({bytes(frame),10+elapsed,100+elapsed,utc}); };
        append(frame("start",0,100,{{"mode","synthetic_live"},{"paused",false},{"clock_rate",1}}),0);
        append(reading(1,100,80),0.01);
        append(frame("heartbeat",2,100.1,{{"paused",false},{"clock_rate",1}}),0.1);
        append(reading(3,98,-4),0.12); append(reading(4,102,150),0.13); append(reading(3,98,-4),0.14);
        auto retired=reading(99,5000,-999); retired["stream_id"]="retired-source"; retired["epoch_id"]="retired-epoch"; append(retired,0.15);
        append(frame("heartbeat",5,100.3,{{"paused",false},{"clock_rate",1}}),0.3);
        append(frame("end",6,100.31,{{"reason","Recorded local epoch ended"}}),0.31);
        return out.exportBytes(true,false);
    }
};
}
class ReceiverJournalTests final: public QObject {
    Q_OBJECT
private slots:
    void bytesAndReceiptClocksRoundTripExactly() {
        Fixture f; auto journal=f.journal();
        auto raw=bytes(f.frame("start",0,100,{{"mode","synthetic_live"},{"paused",false},{"clock_rate",1}}));
        raw.prepend("  "); raw.chop(1); raw.append("\r\n");
        journal.append({raw,42.125,100.25,utc});
        const auto saved=journal.exportBytes(false,false); const auto lines=saved.split('\n');
        validate_journal_header(decode_document(lines[0]),f.hash,f.configuration.value("physical_hash").toString());
        const auto receipt=parse_journal_receipt(decode_document(lines[1],journalLineLimit));
        QCOMPARE(receipt.frame,raw); QCOMPARE(receipt.monotonic_s,42.125); QCOMPARE(receipt.event_s,100.25); QCOMPARE(receipt.utc,utc);
        const auto footer=decode_document(lines[2]); validate_journal_end(footer); QVERIFY(!footer.value("complete").toBool());
        QCOMPARE(saved,journal.exportBytes(false,false));
    }
    void malformedEntriesVersionsAndDuplicateKeysReject() {
        Fixture f; auto header=journal_header(f.hash,f.configuration.value("physical_hash").toString());
        header["recording_version"]=2; QVERIFY_THROWS_EXCEPTION(std::invalid_argument,validate_journal_header(header,f.hash,f.configuration.value("physical_hash").toString()));
        auto journal=f.journal(); journal.append({QByteArray("{}\n"),1,1,utc});
        const auto good=decode_document(journal.exportBytes(false,false).split('\n')[1],journalLineLimit);
        for (const auto* field:{"raw_frame_base64","received_utc","received_monotonic_s","received_event_time_s"}) {
            auto bad=good; bad[QLatin1String(field)]=QString("invalid");
            QVERIFY_THROWS_EXCEPTION(std::invalid_argument,parse_journal_receipt(bad));
        }
        auto negative=good; negative["received_monotonic_s"]=-1;
        QVERIFY_THROWS_EXCEPTION(std::invalid_argument,parse_journal_receipt(negative));
        QVERIFY_THROWS_EXCEPTION(std::invalid_argument,decode_document("{\"recording_version\":1,\"recording_version\":2}"));
        QVERIFY_THROWS_EXCEPTION(std::invalid_argument,validate_journal_end({{"type","journal_end"},{"complete",true},{"truncated",true},{"termination","received_end"}}));
    }
    void boundedRecordingCannotClaimComplete() {
        Fixture f; auto journal=f.journal(); QByteArray raw(256*1024-1,' '); raw.append('\n');
        for (int i=0;i<200 && !journal.truncated();++i) journal.append({raw,double(i),double(i),utc});
        QVERIFY(journal.truncated()); const auto saved=journal.exportBytes(true,false);
        QVERIFY(saved.size()<=journalByteLimit); auto rows=saved.split('\n'); rows.removeLast();
        const auto footer=decode_document(rows.last()); QVERIFY(!footer.value("complete").toBool());
        QCOMPARE(footer.value("termination"),QJsonValue("size_limit"));
    }
    void replayPreservesLateFutureDuplicateAndForeignEpochClassification_data() {
        QTest::addColumn<bool>("historical"); QTest::newRow("playback-clock")<<false; QTest::newRow("historical")<<true;
    }
    void replayPreservesLateFutureDuplicateAndForeignEpochClassification() {
        QFETCH(bool,historical); Fixture f; QTemporaryDir folder; QVERIFY(folder.isValid());
        const auto path=folder.filePath("receiver.ndjson"); QVERIFY(write(path,f.adversarialRecording()));
        TelemetryController controller; controller.configure(f.configuration,f.context); QTRY_COMPARE(controller.status(),QString("ready"));
        if (historical) controller.showHistorical(QUrl::fromLocalFile(path)); else controller.startReplay(QUrl::fromLocalFile(path));
        QTRY_COMPARE_WITH_TIMEOUT(controller.status(),QString("replay_finished"),10000);
        QVERIFY2(controller.error().isEmpty(),qPrintable(controller.error()));
        const auto counters=controller.counters();
        QCOMPARE(counters.value("late_frames").toInt(),1); QCOMPARE(counters.value("future_frames").toInt(),1);
        QCOMPARE(counters.value("duplicate_frames").toInt(),1); QCOMPARE(counters.value("old_epoch_frames").toInt(),1);
        QCOMPARE(controller.snapshot().value("recording_format").toString(),QString("receiver_journal_v1"));
        QVERIFY(controller.snapshot().value("journal_complete").toBool());
        QVERIFY(!controller.snapshot().value("journal_truncated").toBool());
        QCOMPARE(controller.snapshot().value("journal_termination").toString(),QString("received_end"));
        QCOMPARE(controller.snapshot().value("journal_bytes").toLongLong(),QFileInfo(path).size());
        bool late=false,future=false,foreign=false;
        for (const auto& value:controller.tracePreview()) {
            const auto row=value.toMap(); late|=row.value("disposition")=="history_only" && row.value("canonical_value")==-4;
            future|=row.value("disposition")=="future"; foreign|=row.value("disposition")=="old_epoch";
        }
        QVERIFY(late && future && foreign); QVERIFY(!controller.snapshot().value("actionable").toBool());
    }
    void sourceLossUsesRecordedReceiverClockEvenWhenSourceEventClockIsPaused() {
        Fixture f; TelemetryStream stream; stream.configure(f.configuration,f.context); stream.beginEpoch("replay",true);
        auto start=f.frame("start",0,100,{{"mode","replay"},{"paused",true},{"clock_rate",1}});
        QVERIFY(stream.ingest(bytes(start),100,10,utc));
        QVERIFY(stream.ingest(bytes(f.reading(1,100,80)),100,10.01,utc));
        stream.advance(100,16); QVERIFY(!stream.report().value("source_available").toBool());
        auto heartbeat=f.frame("heartbeat",2,100,{{"paused",true},{"clock_rate",1}});
        QVERIFY(stream.ingest(bytes(heartbeat),100,16.01,utc)); QVERIFY(stream.report().value("source_available").toBool());
    }
    void missingOrConflictingFooterCannotClaimCompletedEvidence_data() {
        QTest::addColumn<QString>("variation");
        QTest::newRow("prefix")<<QString("prefix"); QTest::newRow("missing-footer")<<QString("missing-footer");
        QTest::newRow("false-completion")<<QString("false-completion"); QTest::newRow("backwards-receipt")<<QString("backwards-receipt");
        QTest::newRow("size-limit")<<QString("size-limit");
    }
    void missingOrConflictingFooterCannotClaimCompletedEvidence() {
        QFETCH(QString,variation); Fixture f; auto journal=f.journal();
        journal.append({bytes(f.frame("start",0,100,{{"mode","synthetic_live"},{"paused",false},{"clock_rate",1}})),10,100,utc});
        if (variation=="backwards-receipt") journal.append({bytes(f.reading(1,100,80)),9,100,utc});
        auto saved=journal.exportBytes(variation=="false-completion",false);
        if (variation=="size-limit") saved.replace("\"termination\":\"prefix\",\"truncated\":false", "\"termination\":\"size_limit\",\"truncated\":true");
        if (variation=="missing-footer") { const auto lines=saved.split('\n'); saved=lines[0]+'\n'+lines[1]+'\n'; }
        QTemporaryDir folder; const auto path=folder.filePath("prefix.ndjson"); QVERIFY(write(path,saved));
        TelemetryController controller; controller.configure(f.configuration,f.context); QTRY_COMPARE(controller.status(),QString("ready"));
        controller.showHistorical(QUrl::fromLocalFile(path));
        const bool fault=variation=="false-completion" || variation=="backwards-receipt";
        QTRY_COMPARE(controller.status(),fault ? QString("fault") : QString("replay_prefix_finished"));
        QVERIFY(!controller.snapshot().value("actionable").toBool());
        QVERIFY(!controller.snapshot().value("journal_complete").toBool());
        if (variation=="size-limit") {
            QVERIFY(controller.snapshot().value("journal_truncated").toBool());
            QCOMPARE(controller.snapshot().value("journal_termination").toString(),QString("size_limit"));
        }
    }
};
QTEST_GUILESS_MAIN(ReceiverJournalTests)
#include "receiver_journal_tests.moc"
