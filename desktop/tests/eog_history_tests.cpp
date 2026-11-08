#include "eog/EogHistoryRepository.hpp"
#include "eog/EogContracts.hpp"
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QSignalSpy>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QTemporaryDir>
#include <QTimer>
#include <QUuid>
#include <QtTest>
#include <memory>
#include <stdexcept>

using namespace breakwater::desktop::eog;
namespace {
QString id() { return QUuid::createUuid().toString(QUuid::WithoutBraces); }
QString failureText(const QSignalSpy& failures) {
    return failures.isEmpty() ? QStringLiteral("No repository failure signal was retained.") :
        failures.last().at(0).toString() + ": " + failures.last().at(2).toString() + ": " + failures.last().at(3).toString();
}
QByteArray bytes(const QString& path) {
    QFile file(path); if (!file.open(QIODevice::ReadOnly)) throw std::runtime_error("Cannot read test evidence.");
    return file.readAll();
}
bool write(const QString& path, const QByteArray& value) {
    QFile file(path); return file.open(QIODevice::WriteOnly | QIODevice::Truncate) && file.write(value) == value.size();
}
QJsonObject request(const std::shared_ptr<const Capture>& capture, const QString& requestId, const QString& operation = "detect") {
    const auto manifest = capture->manifest();
    QJsonObject result{{"eog_version", 1}, {"kind", "analysis_request"}, {"request_id", requestId}, {"operation", operation},
        {"method_version", 1}, {"dataset_identity", manifest.value("dataset_identity")}, {"capture_id", manifest.value("capture_id")},
        {"cutoff_unix", manifest.value("cutoff_unix")}, {"capture_directory", capture->directory()}, {"capture_manifest", manifest},
        {"arguments", QJsonObject{{"system_ids", QJsonArray{}}}}};
    if (operation != "detect") {
        QJsonObject plan{{"plan_id", "protocol-fixture-plan"}, {"context", manifest.value("context")}, {"start_unix", manifest.value("cutoff_unix")},
            {"repair_deadlines", QJsonArray{QJsonObject{{"facility_id", "facility-Z"}, {"repair_elapsed_s", 25200}}}},
            {"truck_jobs", QJsonArray{QJsonObject{{"job_id", "fixture-job"}, {"truck_id", "fixture-truck"}, {"system_id", "CUSTOM-system"},
                {"facility_id", "facility-Z"}, {"called_elapsed_s", 0}, {"available_again_elapsed_s", 18180}, {"response_min", 84}, {"cycle_min", 219},
                {"trips", QJsonArray{QJsonObject{{"arrival_elapsed_s", 5040}, {"volume_bbl", 137}, {"cost", 919}}}}}}},
            {"well_shut_ins", QJsonArray{}}, {"inspection_assets", QJsonArray{}}};
        plan["plan_hash"] = plan_identity(plan);
        result["arguments"] = operation == "validate_plan" ?
            QJsonObject{{"plan", plan}, {"evaluator_method", "protocol-fixture-evaluator"}} : QJsonObject{{"plan", plan}};
    }
    return result;
}
QJsonObject response(const QJsonObject& request) {
    auto result = request; result.remove("capture_directory"); result.remove("capture_manifest"); result.remove("arguments");
    result["kind"] = "analysis_response"; result["status"] = "completed"; result["diagnostics"] = QJsonArray{};
    const QJsonObject sample{{"evidence_id", "fixture-tank-sample"}, {"asset_id", "tank-Z"}, {"metric", "level_pct"},
        {"timestamp_unix", request.value("cutoff_unix")}, {"value", 80}, {"unit", "%"}, {"quality", "good"}};
    const auto operation = request.value("operation").toString();
    if (operation == "detect") {
        const QJsonObject event{{"event_id", "fixture-event"}, {"system_id", "CUSTOM-system"}, {"facility_id", "facility-Z"},
            {"asset_id", "pump-Z"}, {"category", "pump_failure"}, {"state", "ongoing"}, {"coverage", "available"},
            {"threshold_version", "protocol-fixture-v1"}, {"recommended_action", "plan_required"},
            {"onset_unix", request.value("cutoff_unix")}, {"onset_bounds", QJsonValue()}, {"updated_unix", request.value("cutoff_unix")},
            {"end_unix", QJsonValue()}, {"evidence_ids", QJsonArray{"fixture-tank-sample"}}, {"countdown_hours", 1.0},
            {"lost_revenue_per_hour", 10}, {"limitations", QJsonArray{"Protocol fixture only; no numerical execution."}}};
        result["result"] = QJsonObject{{"analysis_id", "fixture-detection"}, {"events", QJsonArray{event}}, {"evidence", QJsonArray{sample}},
            {"assumptions", QJsonArray{}}, {"coverage", "available"}, {"limitations", QJsonArray{"Protocol fixture only."}}};
    } else {
        const auto plan = request.value("arguments").toObject().value("plan").toObject();
        const QJsonObject cost{{"truck_cost", 919}, {"lost_revenue", 0}, {"total", 919}};
        if (operation == "evaluate_plan") {
            const QJsonObject trajectory{{"facility_id", "facility-Z"}, {"tank_id", "tank-Z"}, {"capacity_bbl", 725},
                {"shutdown_volume_bbl", 681.5}, {"elapsed_s", QJsonArray{0, 5040, 5040, 25200}}, {"volumes_bbl", QJsonArray{580, 581, 444, 450}}};
            result["result"] = QJsonObject{{"analysis_id", "fixture-evaluation"}, {"evaluation", QJsonObject{{"plan", plan}, {"feasible", true},
                {"coverage", "available"}, {"outcome", "avoids_shutdown"}, {"shutdowns", QJsonArray{}}, {"cost", cost},
                {"trajectories", QJsonArray{trajectory}}, {"fleet_occupancy", QJsonArray{QJsonObject{{"truck_id", "fixture-truck"},
                    {"job_id", "fixture-job"}, {"start_elapsed_s", 0}, {"end_elapsed_s", 18180}}}},
                {"violations", QJsonArray{}}, {"assumptions", QJsonArray{"Interface storage fixture."}}, {"limitations", QJsonArray{"No numerical execution."}}}}};
        } else {
            result["result"] = QJsonObject{{"analysis_id", "fixture-validation"}, {"validation", QJsonObject{{"validation_id", "fixture-validation-record"},
                {"plan_id", plan.value("plan_id")}, {"plan_hash", plan.value("plan_hash")}, {"evaluator_method", "protocol-fixture-evaluator"},
                {"validator_method", "protocol-fixture-validator"}, {"independent", true}, {"feasible", true}, {"coverage", "available"},
                {"outcome", "avoids_shutdown"}, {"shutdowns", QJsonArray{}}, {"validated_horizons", plan.value("repair_deadlines")},
                {"cost", cost}, {"violations", QJsonArray{}}, {"minimum_shutdown_margin_bbl", 100},
                {"evidence_ids", QJsonArray{"fixture-tank-sample"}}, {"evidence", QJsonArray{sample}}}}};
        }
    }
    validate_document(result, "analysis_response", {{"request", request}});
    return result;
}
QVariant sql(const QString& path, const QString& statement, const QVariantList& arguments = {}) {
    const auto connection = "eog-history-test-" + id(); QVariant result;
    {
        auto database = QSqlDatabase::addDatabase("QSQLITE", connection); database.setDatabaseName(path);
        if (!database.open()) throw std::runtime_error("Cannot inspect SQLite test evidence.");
        {
            QSqlQuery query(database); query.prepare(statement); for (const auto& argument : arguments) query.addBindValue(argument);
            if (!query.exec()) throw std::runtime_error(query.lastError().text().toStdString());
            if (query.isSelect()) { if (!query.next()) throw std::runtime_error("Missing SQLite test row."); result = query.value(0); }
        }
        database.close();
    }
    QSqlDatabase::removeDatabase(connection); return result;
}
}
class EogHistoryTests final : public QObject {
    Q_OBJECT
    QString fixture_;
    std::shared_ptr<const Capture> capture(bool omitChannel = false, bool extendClock = false, bool originalReadings = false) const {
        QTemporaryDir source;
        for (const auto* name : {"network.json", "ops.json", "telemetry.csv"})
            if (!QFile::copy(QDir(fixture_).filePath(name), source.filePath(name))) throw std::runtime_error("Cannot copy history fixture.");
        auto telemetry = bytes(source.filePath("telemetry.csv"));
        if (!originalReadings) telemetry.replace(",104", ",80");
        if (omitChannel) {
            QByteArray retained;
            for (const auto& line : telemetry.split('\n'))
                if (!line.isEmpty() && !line.contains(",pump-Z,discharge_psi,")) retained += line + '\n';
            telemetry = retained;
        }
        if (extendClock) telemetry += "2026-10-05T00:15:00Z,1791159300,facility-Z,inflow_bpd,17.5\n";
        if (!write(source.filePath("telemetry.csv"), telemetry)) throw std::runtime_error("Cannot write history fixture.");
        const auto data = load_dataset(source.path()); return prepare_capture(data, data->now_unix);
    }
private slots:
    void initTestCase() {
        const auto path = QFINDTESTDATA("eog_fixtures/render/network.json");
        QVERIFY(!path.isEmpty()); fixture_ = QFileInfo(path).absolutePath();
    }
#ifdef Q_OS_WIN
    void originalSamplesUseTheSameLookupForLiveAppendAndArchives() {
        QTemporaryDir directory; const auto database = directory.filePath("source-references.sqlite"), session = id();
        const auto lease = capture(true, true, true); const auto captured = load_dataset(lease->directory());
        QVERIFY(!captured->warnings.isEmpty());
        EogHistoryRepository repository(database); QSignalSpy ready(&repository, &EogHistoryRepository::ready);
        QSignalSpy saved(&repository, &EogHistoryRepository::sessionSaved), failed(&repository, &EogHistoryRepository::failed);
        QTRY_COMPARE(ready.size(), 1); repository.beginSession(session, "Original-source reference checks", lease); QTRY_COMPARE(saved.size(), 1);
        const auto first = lease->manifest().value("source_first_unix"), cutoff = lease->manifest().value("cutoff_unix");
        const QJsonArray samples{
            QJsonObject{{"evidence_id", "fixture-tank-sample"}, {"asset_id", "tank-Z"}, {"metric", "level_pct"},
                {"timestamp_unix", first}, {"value", -2}, {"unit", "%"}, {"quality", "stale"}},
            QJsonObject{{"evidence_id", "actual-overflow"}, {"asset_id", "tank-Z"}, {"metric", "level_pct"},
                {"timestamp_unix", first.toDouble() + 300}, {"value", 104}, {"unit", "%"}, {"quality", "stale"}},
            QJsonObject{{"evidence_id", "actual-current"}, {"asset_id", "facility-Z"}, {"metric", "inflow_bpd"},
                {"timestamp_unix", cutoff}, {"value", 17.5}, {"unit", "bpd"}, {"quality", "good"}},
            QJsonObject{{"evidence_id", "absent-channel-missing"}, {"asset_id", "pump-Z"}, {"metric", "discharge_psi"},
                {"timestamp_unix", cutoff}, {"value", QJsonValue::Null}, {"unit", "psi"}, {"quality", "missing"}}};
        int failures = 0, retained = 0;
        for (const auto& operation : {QStringLiteral("detect"), QStringLiteral("validate_plan")}) {
            const auto input = request(lease, operation + "-grounding", operation); auto output = response(input);
            auto result = output.value("result").toObject();
            auto assessment = operation == "detect" ? result : result.value("validation").toObject();
            assessment["evidence"] = samples;
            if (operation == "validate_plan") assessment["evidence_ids"] = QJsonArray{"fixture-tank-sample", "actual-overflow", "actual-current", "absent-channel-missing"};
            if (operation == "detect") result = assessment; else result["validation"] = assessment;
            output["result"] = result;
            for (const auto& mutation : {QStringLiteral("value"), QStringLiteral("time"), QStringLiteral("channel")}) {
                auto bad = output, badResult = result, badAssessment = assessment; auto references = samples;
                auto sample = references.first().toObject();
                if (mutation == "value") sample["value"] = 999;
                if (mutation == "time") sample["timestamp_unix"] = first.toDouble() + 1;
                if (mutation == "channel") { sample["asset_id"] = "pump-Z"; sample["metric"] = "discharge_psi"; sample["unit"] = "psi"; }
                references[0] = sample; badAssessment["evidence"] = references;
                if (operation == "detect") badResult = badAssessment; else badResult["validation"] = badAssessment;
                bad["result"] = badResult;
                ++failures; repository.appendAnalysis(session, input, bad); QTRY_COMPARE(failed.size(), failures);
                QCOMPARE(failed.back().at(2).toString(), "invalid_evidence");
                QCOMPARE(sql(database, "SELECT count(*) FROM eog_analyses").toInt(), retained);
            }
            ++retained; repository.appendAnalysis(session, input, output); QTRY_COMPARE(saved.size(), retained + 1);
        }
        QCOMPARE(sql(database, "SELECT count(*) FROM eog_analyses").toInt(), 2);
        repository.finishSession(session); QTRY_COMPARE(saved.size(), 4);
        QSignalSpy exported(&repository, &EogHistoryRepository::archiveExported), imported(&repository, &EogHistoryRepository::archiveImported);
        const auto path = QUrl::fromLocalFile(directory.filePath("original-samples.eog-session.json"));
        repository.exportSession(session, path); QTRY_COMPARE(exported.size(), 1);
        auto archive = QJsonDocument::fromJson(bytes(path.toLocalFile())).object(); archive["session_id"] = id();
        const auto copied = QUrl::fromLocalFile(directory.filePath("copied-original-samples.eog-session.json"));
        QVERIFY(write(copied.toLocalFile(), QJsonDocument(archive).toJson(QJsonDocument::Compact)));
        repository.importSession(copied); QTRY_COMPARE(imported.size(), 1); QCOMPARE(failed.size(), 6);
    }
    void completedEvidenceRoundTripsWithFullCaptureAndTrajectory() {
        QTemporaryDir directory; const auto database = directory.filePath("history.sqlite"), session = id(); const auto lease = capture();
        EogHistoryRepository repository(database); QSignalSpy ready(&repository, &EogHistoryRepository::ready);
        QSignalSpy saved(&repository, &EogHistoryRepository::sessionSaved), failures(&repository, &EogHistoryRepository::failed);
        QTRY_COMPARE(ready.size(), 1); repository.beginSession(session, "Protocol history round-trip", lease); QTRY_COMPARE(saved.size(), 1);
        const auto detect = request(lease, "detect-1"), evaluate = request(lease, "evaluate-1", "evaluate_plan"), validate = request(lease, "validate-1", "validate_plan");
        repository.appendAnalysis(session, detect, response(detect), {{"model_identity", "protocol-fixture-model"}, {"http_status", 200}, {"duration_ms", 1.25}, {"retry_count", 0}});
        repository.appendAnalysis(session, evaluate, response(evaluate));
        repository.appendAnalysis(session, validate, response(validate)); QTRY_COMPARE(saved.size(), 4);
        QSignalSpy records(&repository, &EogHistoryRepository::operatorRecordSaved);
        repository.acknowledgeEvent(session, "detect-1", "fixture-event", "Reviewed synthetic event"); QTRY_COMPARE(records.size(), 1);
        repository.finishSession(session); QTRY_COMPARE(saved.size(), 6);
        repository.recordOperatorReview(session, "validate-1"); QTRY_VERIFY(records.size() == 2 || !failures.isEmpty());
        QVERIFY2(records.size() == 2, qPrintable(failureText(failures)));
        QSignalSpy exported(&repository, &EogHistoryRepository::archiveExported);
        const auto path = QUrl::fromLocalFile(directory.filePath("complete.eog-session.json"));
        repository.exportSession(session, path); QTRY_VERIFY(exported.size() == 1 || !failures.isEmpty());
        QVERIFY2(exported.size() == 1, qPrintable(failureText(failures)));
        const auto raw = bytes(path.toLocalFile()); const auto archive = QJsonDocument::fromJson(raw).object();
        QVERIFY(!raw.contains(lease->directory().toUtf8())); QVERIFY(!raw.contains("capture_directory"));
        QCOMPARE(archive.value("eog_session_version").toInt(), 1); QVERIFY(archive.value("historical").toBool()); QVERIFY(!archive.value("actionable").toBool());
        QCOMPARE(archive.value("capture_manifest").toObject(), lease->manifest()); QCOMPARE(archive.value("captured_inputs").toObject().size(), 3);
        const auto analyses = archive.value("analyses").toArray(); QCOMPARE(analyses.size(), 3); QCOMPARE(archive.value("operator_records").toArray().size(), 2);
        const auto storedEvaluation = analyses[1].toObject().value("response").toObject().value("result").toObject().value("evaluation").toObject();
        QCOMPARE(storedEvaluation, response(evaluate).value("result").toObject().value("evaluation").toObject());
        QCOMPARE(sql(database, "SELECT count(*) FROM eog_files").toInt(), 3);
        EogHistoryRepository restored(directory.filePath("restored.sqlite")); QSignalSpy restoredReady(&restored, &EogHistoryRepository::ready);
        QSignalSpy imported(&restored, &EogHistoryRepository::archiveImported), loaded(&restored, &EogHistoryRepository::sessionLoaded);
        QSignalSpy restoredFailures(&restored, &EogHistoryRepository::failed);
        QTRY_COMPARE(restoredReady.size(), 1); restored.importSession(path); QTRY_COMPARE(imported.size(), 1);
        restored.loadSession(session); QTRY_COMPARE(loaded.size(), 1); QCOMPARE(loaded.first().first().toJsonObject(), archive);
        restored.recordOperatorReview(session, "validate-1"); QTRY_COMPARE(restoredFailures.size(), 1);
        QVERIFY(restoredFailures.first().at(3).toString().contains("Imported, reopened or retired"));
        QSignalSpy localLoaded(&repository, &EogHistoryRepository::sessionLoaded); repository.loadSession(session); QTRY_COMPARE(localLoaded.size(), 1);
        repository.recordOperatorReview(session, "validate-1"); QTRY_COMPARE(failures.size(), 1);
    }
    void recoveryInterruptsUnfinishedEvidenceAndLockProtectsLiveOwner() {
        QTemporaryDir directory; const auto database = directory.filePath("recovery.sqlite"), session = id(); const auto lease = capture();
        {
            EogHistoryRepository first(database); QSignalSpy ready(&first, &EogHistoryRepository::ready), saved(&first, &EogHistoryRepository::sessionSaved);
            QTRY_COMPARE(ready.size(), 1); first.beginSession(session, "Interrupted fixture", lease); QTRY_COMPARE(saved.size(), 1);
            {
                EogHistoryRepository second(database); QSignalSpy secondReady(&second, &EogHistoryRepository::ready), failure(&second, &EogHistoryRepository::failed);
                QTRY_COMPARE(failure.size(), 1); QCOMPARE(secondReady.size(), 0); QCOMPARE(failure.first().at(2).toString(), "history_in_use");
                QCOMPARE(sql(database, "SELECT status FROM eog_sessions WHERE session_id=?", {session}).toString(), "running");
            }
            const auto input = request(lease, "retained-before-restart"); first.appendAnalysis(session, input, response(input)); QTRY_COMPARE(saved.size(), 2);
        }
        EogHistoryRepository reopened(database); QSignalSpy ready(&reopened, &EogHistoryRepository::ready), listed(&reopened, &EogHistoryRepository::sessionsListed);
        QSignalSpy loaded(&reopened, &EogHistoryRepository::sessionLoaded), failure(&reopened, &EogHistoryRepository::failed);
        QTRY_COMPARE(ready.size(), 1); reopened.listSessions(); QTRY_COMPARE(listed.size(), 1);
        QCOMPARE(listed.first().first().toJsonArray().first().toObject().value("status").toString(), "interrupted");
        reopened.loadSession(session); QTRY_COMPARE(loaded.size(), 1); QCOMPARE(loaded.first().first().toJsonObject().value("analyses").toArray().size(), 1);
        reopened.acknowledgeEvent(session, "retained-before-restart", "fixture-event"); QTRY_COMPARE(failure.size(), 1);
    }
    void duplicateConflictAndSqlFailureNeverPartiallyReplaceEvidence() {
        QTemporaryDir directory; const auto database = directory.filePath("rollback.sqlite"), session = id(); const auto lease = capture();
        EogHistoryRepository repository(database); QSignalSpy ready(&repository, &EogHistoryRepository::ready), saved(&repository, &EogHistoryRepository::sessionSaved);
        QSignalSpy failed(&repository, &EogHistoryRepository::failed);
        QTRY_COMPARE(ready.size(), 1); repository.beginSession(session, "Transactional fixture", lease); QTRY_COMPARE(saved.size(), 1);
        const auto input = request(lease, "stable-request"), output = response(input);
        sql(database, "CREATE TRIGGER test_abort BEFORE INSERT ON eog_analyses BEGIN SELECT RAISE(ABORT,'test rollback'); END");
        repository.appendAnalysis(session, input, output); QTRY_COMPARE(failed.size(), 1);
        QCOMPARE(sql(database, "SELECT count(*) FROM eog_analyses").toInt(), 0);
        sql(database, "DROP TRIGGER test_abort");
        repository.appendAnalysis(session, input, output); QTRY_COMPARE(saved.size(), 2);
        repository.appendAnalysis(session, input, output); QTRY_COMPARE(saved.size(), 3);
        QCOMPARE(sql(database, "SELECT count(*) FROM eog_analyses").toInt(), 1);
        auto conflict = output; auto result = conflict.value("result").toObject(); result["analysis_id"] = "replacement";
        conflict["result"] = result; repository.appendAnalysis(session, input, conflict); QTRY_COMPARE(failed.size(), 2);
        QCOMPARE(sql(database, "SELECT count(*) FROM eog_analyses").toInt(), 1);
        repository.finishSession(session); QTRY_COMPARE(saved.size(), 4);
        const auto late = request(lease, "late-request"); repository.appendAnalysis(session, late, response(late)); QTRY_COMPARE(failed.size(), 3);
        QCOMPARE(sql(database, "SELECT count(*) FROM eog_analyses").toInt(), 1);
        repository.beginSession(id(), "Deduplicated input", lease); QTRY_COMPARE(saved.size(), 5);
        QCOMPARE(sql(database, "SELECT count(*) FROM eog_captures").toInt(), 1); QCOMPARE(sql(database, "SELECT count(*) FROM eog_files").toInt(), 3);
    }
    void failedCaptureWriteRollsBackEveryInputAndSession() {
        QTemporaryDir directory; const auto database = directory.filePath("capture-rollback.sqlite"), session = id(); const auto lease = capture();
        EogHistoryRepository repository(database); QSignalSpy ready(&repository, &EogHistoryRepository::ready);
        QSignalSpy saved(&repository, &EogHistoryRepository::sessionSaved), failed(&repository, &EogHistoryRepository::failed);
        QTRY_COMPARE(ready.size(), 1);
        sql(database, "CREATE TRIGGER abort_input BEFORE INSERT ON eog_files WHEN NEW.name='telemetry.csv' BEGIN SELECT RAISE(ABORT,'test capture rollback'); END");
        repository.beginSession(session, "Capture rollback fixture", lease); QTRY_COMPARE(failed.size(), 1); QCOMPARE(saved.size(), 0);
        QCOMPARE(sql(database, "SELECT count(*) FROM eog_sessions").toInt(), 0);
        QCOMPARE(sql(database, "SELECT count(*) FROM eog_captures").toInt(), 0); QCOMPARE(sql(database, "SELECT count(*) FROM eog_files").toInt(), 0);
        sql(database, "DROP TRIGGER abort_input"); repository.beginSession(session, "Capture rollback fixture", lease); QTRY_COMPARE(saved.size(), 1);
    }
    void invalidBindingMetadataAndReviewNeverGainAuthority() {
        QTemporaryDir directory; const auto session = id(); const auto lease = capture();
        EogHistoryRepository repository(directory.filePath("bindings.sqlite")); QSignalSpy ready(&repository, &EogHistoryRepository::ready);
        QSignalSpy saved(&repository, &EogHistoryRepository::sessionSaved), failed(&repository, &EogHistoryRepository::failed), records(&repository, &EogHistoryRepository::operatorRecordSaved);
        QTRY_COMPARE(ready.size(), 1); repository.beginSession(session, "Binding fixture", lease); QTRY_COMPARE(saved.size(), 1);
        const auto input = request(lease, "detect"); auto wrong = response(input); wrong["capture_id"] = QString(64, '0');
        repository.appendAnalysis(session, input, wrong); QTRY_COMPARE(failed.size(), 1);
        repository.appendAnalysis(session, input, response(input), {{"authorization", "NOT_A_SECRET_TEST_ONLY"}}); QTRY_COMPARE(failed.size(), 2);
        repository.appendAnalysis(session, input, response(input)); QTRY_COMPARE(saved.size(), 2);
        repository.recordOperatorReview(session, "detect"); QTRY_COMPARE(failed.size(), 3); QCOMPARE(records.size(), 0);
        const auto validation = request(lease, "validate", "validate_plan"); repository.appendAnalysis(session, validation, response(validation)); QTRY_COMPARE(saved.size(), 3);
        repository.finishSession(session); QTRY_COMPARE(saved.size(), 4);
        repository.recordOperatorReview(session, "detect"); QTRY_COMPARE(failed.size(), 4);
        repository.acknowledgeEvent(session, "detect", "unknown-event"); QTRY_COMPARE(failed.size(), 5);
        repository.retireSession(session); repository.recordOperatorReview(session, "validate"); QTRY_COMPARE(failed.size(), 6); QCOMPARE(records.size(), 0);
    }
    void agentTraceRetainsSelectedToolOrderAndRejectsTampering() {
        QTemporaryDir directory; const auto lease = capture(); const auto session = id();
        EogHistoryRepository repository(directory.filePath("agent.sqlite")); QSignalSpy ready(&repository, &EogHistoryRepository::ready);
        QSignalSpy saved(&repository, &EogHistoryRepository::sessionSaved), failed(&repository, &EogHistoryRepository::failed), exported(&repository, &EogHistoryRepository::archiveExported);
        QTRY_COMPARE(ready.size(), 1); repository.beginSession(session, "Adaptive protocol trace", lease);
        const auto detect = request(lease, "detect"), validation = request(lease, "validate", "validate_plan");
        const QJsonObject model{{"type", "model_request"}, {"model_identity", "protocol-fixture-model"}, {"http_status", 200}, {"duration_ms", 2.5}, {"retry_count", 0}};
        repository.appendAgentEvent(session, model);
        repository.appendAgentEvent(session, {{"type", "tool_call"}, {"call_id", "call-1"}, {"name", "detect"}, {"arguments", QJsonObject{{"system_ids", QJsonArray{}}}}});
        repository.appendAnalysis(session, detect, response(detect));
        repository.appendAgentEvent(session, {{"type", "tool_result"}, {"call_id", "call-1"}, {"name", "detect"}, {"ok", true}, {"analysis_request_id", "detect"}, {"summary", "Synthetic detection fixture retained."}});
        repository.appendAgentEvent(session, model);
        repository.appendAgentEvent(session, {{"type", "tool_call"}, {"call_id", "call-2"}, {"name", "validate_plan"}, {"arguments", QJsonObject{{"plan_id", "protocol-fixture-plan"}}}});
        repository.appendAnalysis(session, validation, response(validation));
        repository.appendAgentEvent(session, {{"type", "tool_result"}, {"call_id", "call-2"}, {"name", "validate_plan"}, {"ok", true}, {"analysis_request_id", "validate"}, {"summary", "Synthetic validation fixture retained."}});
        repository.appendAgentEvent(session, {{"type", "final"}, {"summary", "Interface fixture; no hosted model or numerical execution."},
            {"selected_plan_id", "protocol-fixture-plan"}, {"validation_id", "fixture-validation-record"}, {"event_ids", QJsonArray{"fixture-event"}}});
        repository.finishSession(session); QTRY_COMPARE_WITH_TIMEOUT(saved.size(), 11, 10000); QVERIFY2(failed.isEmpty(), qPrintable(failureText(failed)));
        repository.appendAgentEvent(session, model); QTRY_COMPARE(failed.size(), 1);
        const auto path = QUrl::fromLocalFile(directory.filePath("agent.eog-session.json"));
        repository.exportSession(session, path); QTRY_VERIFY(exported.size() == 1 || failed.size() > 1); QVERIFY2(exported.size() == 1, qPrintable(failureText(failed)));
        const auto archive = QJsonDocument::fromJson(bytes(path.toLocalFile())).object(); const auto events = archive.value("agent_events").toArray();
        QCOMPARE(events.size(), 7); const QStringList order{"model_request", "tool_call", "tool_result", "model_request", "tool_call", "tool_result", "final"};
        for (qsizetype index = 0; index < events.size(); ++index) {
            const auto item = events[index].toObject(); QCOMPARE(item.value("sequence").toInt(), index + 1);
            QCOMPARE(item.value("event").toObject().value("type").toString(), order[index]);
        }
        EogHistoryRepository imported(directory.filePath("agent-import.sqlite")); QSignalSpy importedReady(&imported, &EogHistoryRepository::ready);
        QSignalSpy done(&imported, &EogHistoryRepository::archiveImported), importedFailed(&imported, &EogHistoryRepository::failed), loaded(&imported, &EogHistoryRepository::sessionLoaded);
        QTRY_COMPARE(importedReady.size(), 1);
        auto bad = archive; auto badEvents = events; auto second = badEvents[1].toObject(); second["sequence"] = 1; badEvents[1] = second; bad["agent_events"] = badEvents;
        const auto badPath = QUrl::fromLocalFile(directory.filePath("bad-agent.eog-session.json")); QVERIFY(write(badPath.toLocalFile(), QJsonDocument(bad).toJson(QJsonDocument::Compact)));
        imported.importSession(badPath); QTRY_COMPARE(importedFailed.size(), 1); QCOMPARE(done.size(), 0);
        imported.importSession(path); QTRY_COMPARE(done.size(), 1); imported.loadSession(session); QTRY_COMPARE(loaded.size(), 1);
        QCOMPARE(loaded.first().first().toJsonObject().value("agent_events").toArray(), events);
    }
    void agentTraceBoundsAndPrivateFieldsFailVisibly() {
        QTemporaryDir directory; const auto lease = capture(); const auto session = id();
        EogHistoryRepository repository(directory.filePath("agent-bounds.sqlite")); QSignalSpy ready(&repository, &EogHistoryRepository::ready);
        QSignalSpy saved(&repository, &EogHistoryRepository::sessionSaved), failed(&repository, &EogHistoryRepository::failed);
        QTRY_COMPARE(ready.size(), 1); repository.beginSession(session, "Bounded trace fixture", lease); QTRY_COMPARE(saved.size(), 1);
        const QJsonObject model{{"type", "model_request"}, {"model_identity", "protocol-fixture-model"}, {"duration_ms", 1}, {"retry_count", 0}};
        auto privateEvent = model; privateEvent["reasoning"] = "NOT_A_SECRET_TEST_ONLY";
        repository.appendAgentEvent(session, privateEvent); QTRY_COMPARE(failed.size(), 1);
        repository.appendAgentEvent(session, {{"type", "tool_result"}, {"call_id", "unknown"}, {"name", "detect"}, {"ok", true}}); QTRY_COMPARE(failed.size(), 2);
        repository.appendAgentEvent(session, {{"type", "tool_call"}, {"call_id", "too-large"}, {"name", "detect"}, {"arguments", QJsonObject{{"padding", QString(70000, 'x')}}}}); QTRY_COMPARE(failed.size(), 3);
        repository.appendAgentEvent(session, {{"type", "tool_call"}, {"call_id", "private-path"}, {"name", "detect"}, {"arguments", QJsonObject{{"path", lease->directory()}}}}); QTRY_COMPARE(failed.size(), 4);
        QCOMPARE(saved.size(), 1);
        for (int index = 0; index < EogHistoryRepository::maximumAgentEvents; ++index) repository.appendAgentEvent(session, model);
        QTRY_COMPARE_WITH_TIMEOUT(saved.size(), 129, 15000);
        repository.appendAgentEvent(session, model); QTRY_COMPARE(failed.size(), 5);
        QCOMPARE(sql(directory.filePath("agent-bounds.sqlite"), "SELECT count(*) FROM eog_agent_events").toInt(), 128);
    }
    void malformedTamperedOversizedAndAuthorityImportsAreAtomic() {
        QTemporaryDir directory; const auto lease = capture(); const auto session = id();
        EogHistoryRepository source(directory.filePath("source.sqlite")); QSignalSpy ready(&source, &EogHistoryRepository::ready);
        QSignalSpy saved(&source, &EogHistoryRepository::sessionSaved), exported(&source, &EogHistoryRepository::archiveExported);
        QSignalSpy sourceFailures(&source, &EogHistoryRepository::failed);
        QTRY_COMPARE(ready.size(), 1); source.beginSession(session, "Import fixture", lease); QTRY_COMPARE(saved.size(), 1);
        const auto input = request(lease, "retained"); source.appendAnalysis(session, input, response(input)); source.finishSession(session); QTRY_COMPARE(saved.size(), 3);
        const auto originalPath = QUrl::fromLocalFile(directory.filePath("original.eog-session.json"));
        source.exportSession(session, originalPath); QTRY_VERIFY(exported.size() == 1 || !sourceFailures.isEmpty());
        QVERIFY2(exported.size() == 1, qPrintable(failureText(sourceFailures)));
        const auto original = QJsonDocument::fromJson(bytes(originalPath.toLocalFile())).object();
        EogHistoryRepository destination(directory.filePath("destination.sqlite")); QSignalSpy destinationReady(&destination, &EogHistoryRepository::ready);
        QSignalSpy imported(&destination, &EogHistoryRepository::archiveImported), failures(&destination, &EogHistoryRepository::failed);
        QTRY_COMPARE(destinationReady.size(), 1);
        const auto badPath = QUrl::fromLocalFile(directory.filePath("bad.eog-session.json"));
        for (const auto& change : {QStringLiteral("authority"), QStringLiteral("tamper"), QStringLiteral("bomb-length"), QStringLiteral("runtime-path"), QStringLiteral("unknown-field"), QStringLiteral("metadata"), QStringLiteral("fabricated-evidence")}) {
            auto bad = original;
            if (change == "authority") bad["actionable"] = true;
            else if (change == "runtime-path") {
                auto analyses = bad.value("analyses").toArray(); auto item = analyses[0].toObject(), request = item.value("request").toObject();
                request["capture_directory"] = lease->directory(); item["request"] = request; analyses[0] = item; bad["analyses"] = analyses;
            } else if (change == "unknown-field") bad["approval"] = "applied";
            else if (change == "metadata") {
                auto analyses = bad.value("analyses").toArray(); auto item = analyses[0].toObject();
                item["metadata"] = QJsonObject{{"authorization", "NOT_A_SECRET_TEST_ONLY"}}; analyses[0] = item; bad["analyses"] = analyses;
            } else if (change == "fabricated-evidence") {
                auto analyses = bad.value("analyses").toArray(); auto item = analyses[0].toObject(), response = item.value("response").toObject();
                auto result = response.value("result").toObject(); auto evidence = result.value("evidence").toArray(); auto sample = evidence[0].toObject();
                sample["value"] = 81; evidence[0] = sample; result["evidence"] = evidence; response["result"] = result;
                item["response"] = response; analyses[0] = item; bad["analyses"] = analyses;
            }
            else {
                auto inputs = bad.value("captured_inputs").toObject(), packedInput = inputs.value("telemetry.csv").toObject();
                if (change == "tamper") packedInput["compressed_sha256"] = QString(64, '0');
                else packedInput["raw_bytes"] = 1; // Valid bound, but incompatible MSZIP output length: rejected before allocation.
                inputs["telemetry.csv"] = packedInput; bad["captured_inputs"] = inputs;
            }
            QVERIFY(write(badPath.toLocalFile(), QJsonDocument(bad).toJson(QJsonDocument::Compact)));
            const auto previous = failures.size(); destination.importSession(badPath); QTRY_COMPARE(failures.size(), previous + 1);
            QCOMPARE(imported.size(), 0);
        }
        QVERIFY(write(badPath.toLocalFile(), "{\"eog_session_version\":1,\"eog_session_version\":1}"));
        destination.importSession(badPath); QTRY_COMPARE(failures.size(), 8);
        QFile oversized(badPath.toLocalFile()); QVERIFY(oversized.open(QIODevice::WriteOnly | QIODevice::Truncate));
        QVERIFY(oversized.resize(EogHistoryRepository::maximumArchiveBytes + 1)); oversized.close();
        destination.importSession(badPath); QTRY_COMPARE(failures.size(), 9);
        QCOMPARE(sql(directory.filePath("destination.sqlite"), "SELECT count(*) FROM eog_sessions").toInt(), 0);
        destination.importSession(originalPath); QTRY_COMPARE(imported.size(), 1);
        QCOMPARE(sql(directory.filePath("destination.sqlite"), "SELECT count(*) FROM eog_sessions").toInt(), 1);
    }
    void paginationIsBoundedDeterministicAndLeavesTheUiResponsive() {
        QTemporaryDir directory; const auto lease = capture();
        EogHistoryRepository repository(directory.filePath("pages.sqlite")); QSignalSpy ready(&repository, &EogHistoryRepository::ready);
        QSignalSpy saved(&repository, &EogHistoryRepository::sessionSaved), pages(&repository, &EogHistoryRepository::sessionsListed), failures(&repository, &EogHistoryRepository::failed);
        int beats = 0; QTimer timer; timer.setInterval(2); connect(&timer, &QTimer::timeout, this, [&] { ++beats; }); timer.start();
        QTRY_COMPARE(ready.size(), 1);
        for (int index = 0; index < 102; ++index) repository.beginSession(id(), "Paged fixture", lease);
        QTRY_COMPARE_WITH_TIMEOUT(saved.size(), 102, 15000); QVERIFY(beats > 2);
        repository.listSessions(); QTRY_COMPARE(pages.size(), 1); const auto first = pages[0][0].toJsonArray();
        QCOMPARE(first.size(), 100); QCOMPARE(pages[0][1].toInt(), 100);
        repository.listSessions(100); QTRY_COMPARE(pages.size(), 2); QCOMPARE(pages[1][0].toJsonArray().size(), 2); QCOMPARE(pages[1][1].toInt(), -1);
        repository.listSessions(); QTRY_COMPARE(pages.size(), 3); QCOMPARE(pages[2][0].toJsonArray(), first);
        repository.listSessions(-1); QTRY_COMPARE(failures.size(), 1);
        repository.listSessions(1000001); QTRY_COMPARE(failures.size(), 2);
        QVERIFY(failures[0][3].toString().contains("offset")); QVERIFY(failures[1][3].toString().contains("offset"));
    }
    void exportNeverOverwritesInputsOrExistingArchives() {
        QTemporaryDir directory; const auto lease = capture(); const auto session = id();
        EogHistoryRepository repository(directory.filePath("export.sqlite")); QSignalSpy ready(&repository, &EogHistoryRepository::ready);
        QSignalSpy saved(&repository, &EogHistoryRepository::sessionSaved), exported(&repository, &EogHistoryRepository::archiveExported), failures(&repository, &EogHistoryRepository::failed);
        QTRY_COMPARE(ready.size(), 1); repository.beginSession(session, "Export fixture", lease); repository.finishSession(session); QTRY_COMPARE(saved.size(), 2);
        const auto input = QUrl::fromLocalFile(directory.filePath("network.json")); QVERIFY(write(input.toLocalFile(), "PRESERVE_INPUT"));
        repository.exportSession(session, input); QTRY_COMPARE(failures.size(), 1); QCOMPARE(bytes(input.toLocalFile()), QByteArray("PRESERVE_INPUT"));
        const auto archive = QUrl::fromLocalFile(directory.filePath("existing.eog-session.json")); QVERIFY(write(archive.toLocalFile(), "PRESERVE_ARCHIVE"));
        repository.exportSession(session, archive); QTRY_COMPARE(failures.size(), 2); QCOMPARE(bytes(archive.toLocalFile()), QByteArray("PRESERVE_ARCHIVE"));
        QCOMPARE(exported.size(), 0);
    }
    void foreignDatabaseFailsVisiblyWithoutChangingItsFormat() {
        QTemporaryDir directory; const auto path = directory.filePath("native.sqlite"); sql(path, "CREATE TABLE native_evidence (value TEXT)");
        sql(path, "INSERT INTO native_evidence VALUES ('preserve original native evidence')");
        EogHistoryRepository repository(path); QSignalSpy ready(&repository, &EogHistoryRepository::ready), failed(&repository, &EogHistoryRepository::failed);
        QTRY_COMPARE(failed.size(), 1); QCOMPARE(ready.size(), 0);
        QCOMPARE(sql(path, "SELECT count(*) FROM sqlite_master WHERE name='eog_sessions'").toInt(), 0);
        QCOMPARE(sql(path, "PRAGMA application_id").toInt(), 0);
        QCOMPARE(sql(path, "SELECT value FROM native_evidence").toString(), "preserve original native evidence");
    }
#else
    void unsupportedCodecIsVisibleWithoutAFalsePass() {
        QTemporaryDir directory; EogHistoryRepository repository(directory.filePath("unsupported.sqlite"));
        QSignalSpy ready(&repository, &EogHistoryRepository::ready), failed(&repository, &EogHistoryRepository::failed);
        QTRY_COMPARE(ready.size(), 1); repository.beginSession(id(), "Unsupported platform", capture());
        QTRY_COMPARE(failed.size(), 1); QCOMPARE(failed.first().at(2).toString(), "unsupported_codec");
    }
#endif
};
QTEST_GUILESS_MAIN(EogHistoryTests)
#include "eog_history_tests.moc"
