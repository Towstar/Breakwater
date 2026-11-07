#include "history/SessionRepository.hpp"
#include "agent/SimulationGateway.hpp"
#include "ProjectDocument.hpp"

#include <QDateTime>
#include <QElapsedTimer>
#include <QFile>
#include <QJsonDocument>
#include <QSignalSpy>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QTemporaryDir>
#include <QUuid>
#include <QtTest>
#include <atomic>
#include <limits>

using namespace breakwater;
using namespace breakwater::desktop;
using namespace breakwater::desktop::agent;
using namespace breakwater::desktop::history;
namespace {
bool received(QSignalSpy& spy, qsizetype count = 1) {
    QElapsedTimer timer; timer.start();
    while (spy.size() < count && timer.elapsed() < 10000) spy.wait(100);
    return spy.size() >= count;
}
QJsonObject fixture(const QString& status = "completed", int sequence = 1) {
    auto project = example_project(); project.network = main_pump_failure(project.network);
    SimulationGateway gateway(project.network, physics_hash(project));
    (void)gateway.execute("get_network_state", {}); (void)gateway.execute("get_control_catalog", {});
    QJsonArray values;
    for (const auto& control : incident_controls("mixed")) values.append(QJsonObject{{"control_id", QString::fromStdString(control.id)}, {"value", control.value}});
    const auto simulated = gateway.execute("simulate_candidates", {{"snapshot_id", gateway.snapshotId()},
        {"candidates", QJsonArray{QJsonObject{{"candidate_id", "mixed"}, {"controls", values}}}}});
    const auto id = simulated["runs"].toArray().first().toObject()["run_id"].toString();
    (void)gateway.execute("validate_run", {{"snapshot_id", gateway.snapshotId()}, {"run_id", id}});
    auto recommendation = gateway.recommendation(id); recommendation["actionable"] = true;
    const auto session = QUuid::createUuid().toString(QUuid::WithoutBraces);
    const auto now = QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs);
    return {{"archive_version", 1}, {"session_id", session}, {"checkpoint_sequence", sequence},
        {"created_at", now}, {"updated_at", now}, {"status", status}, {"objective", "Avoid synthetic shutdown."},
        {"snapshot", QJsonObject{{"snapshot_id", gateway.snapshotId()}, {"physical_hash", gateway.physicalHash()},
            {"horizon_h", 3.0}, {"network", network_json(project.network)}}},
        {"model", QJsonObject{{"configured_model", "fake-test-model"}}}, {"runs", QJsonArray{gateway.runDetails(id)}},
        {"events", QJsonArray{QJsonObject{{"type", "agent_started"}, {"session_id", session}, {"sequence", 1}, {"timestamp", now}, {"snapshot_id", gateway.snapshotId()}}}},
        {"recommendation", recommendation}, {"approvals", QJsonArray{QJsonObject{{"status", "operator_applied"}, {"run_id", id}, {"timestamp", now}}}}};
}
bool writeJson(const QString& path, const QJsonObject& value) {
    QFile file(path); return file.open(QIODevice::WriteOnly) && file.write(QJsonDocument(value).toJson()) > 0;
}
bool sql(const QString& path, const QString& statement) {
    const auto connection = "repository-test-" + QUuid::createUuid().toString();
    bool ok = false;
    {
        auto database = QSqlDatabase::addDatabase("QSQLITE", connection); database.setDatabaseName(path);
        if (database.open()) { QSqlQuery query(database); ok = query.exec(statement); }
        database.close();
    }
    QSqlDatabase::removeDatabase(connection); return ok;
}
QJsonObject loaded(QSignalSpy& spy) { return spy.last().first().value<QJsonObject>(); }
QString failure(QSignalSpy& spy) { return spy.last().first().toString(); }
}

class SessionRepositoryTests : public QObject {
    Q_OBJECT
private slots:
    void fullEvidenceRoundTripAndHistoricalRead() {
        QTemporaryDir directory; QVERIFY(directory.isValid());
        SessionRepository repository(directory.filePath("sessions.sqlite"));
        QSignalSpy ready(&repository, &SessionRepository::ready), saved(&repository, &SessionRepository::sessionSaved),
            read(&repository, &SessionRepository::sessionLoaded), listed(&repository, &SessionRepository::sessionsListed), errors(&repository, &SessionRepository::failed);
        QVERIFY(received(ready));
        const auto archive = fixture();
        QVERIFY(SessionRepository::validateArchive(archive));
        repository.saveSession(archive); QVERIFY(received(saved));
        repository.loadSession(archive["session_id"].toString()); QVERIFY(received(read));
        const auto result = loaded(read);
        QCOMPARE(result["runs"], archive["runs"]); QCOMPARE(result["events"], archive["events"]); QCOMPARE(result["approvals"], archive["approvals"]);
        QVERIFY(result["historical"].toBool()); QVERIFY(!result["actionable"].toBool(true));
        QVERIFY(!result["recommendation"].toObject()["actionable"].toBool(true));
        QCOMPARE(result["recommendation"].toObject()["approval_status"].toString(), "historical");
        repository.listSessions(); QVERIFY(received(listed));
        const auto rows = listed.first().first().value<QJsonArray>(); QCOMPARE(rows.size(), 1);
        QCOMPARE(rows.first().toObject()["session_id"], archive["session_id"]);
        QVERIFY(errors.isEmpty());
    }
    void destructorDrainsWritesAndRestartRecoversRunningCheckpoint() {
        QTemporaryDir directory; QVERIFY(directory.isValid()); const auto path = directory.filePath("history.sqlite");
        auto archive = fixture("running");
        {
            SessionRepository repository(path);
            repository.saveSession(archive);
            archive["checkpoint_sequence"] = 2;
            repository.saveSession(archive);
            // No event-loop wait: destruction must drain the queued writes itself.
        }
        SessionRepository reopened(path);
        QSignalSpy ready(&reopened, &SessionRepository::ready), read(&reopened, &SessionRepository::sessionLoaded), errors(&reopened, &SessionRepository::failed);
        QVERIFY(received(ready)); reopened.loadSession(archive["session_id"].toString()); QVERIFY(received(read));
        const auto result = loaded(read);
        QCOMPARE(result["status"].toString(), "interrupted");
        QCOMPARE(result["checkpoint_sequence"].toInt(), 3);
        QCOMPARE(result["runs"], archive["runs"]); QCOMPARE(result["events"], archive["events"]);
        QVERIFY(result["historical"].toBool()); QVERIFY(errors.isEmpty());
    }
    void queuedCaptureRunsOffTheCallerAndIsDrainedOnDestruction() {
        QTemporaryDir directory; QVERIFY(directory.isValid());
        const auto path = directory.filePath("captured.sqlite");
        const auto archive = fixture();
        const auto caller = QThread::currentThread();
        std::atomic<bool> captured{false}, different_thread{false};
        {
            SessionRepository repository(path);
            repository.enqueueCheckpoint([archive, caller, &captured, &different_thread] {
                different_thread.store(QThread::currentThread() != caller);
                captured.store(true);
                return archive;
            });
            // No event-loop wait: shutdown must drain capture, validation and write.
        }
        QVERIFY(captured.load()); QVERIFY(different_thread.load());
        SessionRepository reopened(path);
        QSignalSpy ready(&reopened, &SessionRepository::ready), read(&reopened, &SessionRepository::sessionLoaded), errors(&reopened, &SessionRepository::failed);
        QVERIFY(received(ready)); reopened.loadSession(archive["session_id"].toString()); QVERIFY(received(read));
        QCOMPARE(loaded(read)["status"].toString(), "completed");
        QCOMPARE(loaded(read)["runs"], archive["runs"]);
        QVERIFY(errors.isEmpty());
    }
    void checkpointsIgnoreOlderEvidenceAndRejectConflicts() {
        QTemporaryDir directory; SessionRepository repository(directory.filePath("history.sqlite"));
        QSignalSpy ready(&repository, &SessionRepository::ready), saved(&repository, &SessionRepository::sessionSaved),
            read(&repository, &SessionRepository::sessionLoaded), errors(&repository, &SessionRepository::failed);
        QVERIFY(received(ready));
        auto latest = fixture("completed", 3); repository.saveSession(latest); QVERIFY(received(saved));
        auto older = latest; older["checkpoint_sequence"] = 2; older["status"] = "running";
        repository.saveSession(older); QVERIFY(received(saved, 2));
        repository.loadSession(latest["session_id"].toString()); QVERIFY(received(read));
        QCOMPARE(loaded(read)["checkpoint_sequence"].toInt(), 3); QCOMPARE(loaded(read)["status"].toString(), "completed");
        auto conflict = latest; conflict["objective"] = "Different evidence under the same checkpoint.";
        repository.saveSession(conflict); QVERIFY(received(errors)); QCOMPARE(failure(errors), "checkpoint_conflict");
        conflict["checkpoint_sequence"] = 4;
        repository.saveSession(conflict); QVERIFY(received(errors, 2)); QCOMPARE(failure(errors), "checkpoint_conflict");
        auto altered = latest; altered["checkpoint_sequence"] = 4;
        auto runs = altered["runs"].toArray(); auto details = runs[0].toObject(); auto volumes = details["volumes_m3"].toArray();
        auto trajectory = volumes[0].toArray(); trajectory[1] = trajectory[1].toDouble() + 1.0; volumes[0] = trajectory;
        details["volumes_m3"] = volumes; runs[0] = details; altered["runs"] = runs;
        repository.saveSession(altered); QVERIFY(received(errors, 3)); QCOMPARE(failure(errors), "checkpoint_conflict");
    }
    void exportImportIsAtomicIdempotentAndCannotReplaceEvidence() {
        QTemporaryDir directory;
        const auto archive = fixture();
        const auto id = archive["session_id"].toString();
        const auto destination = QUrl::fromLocalFile(directory.filePath("evidence.json"));
        SessionRepository source(directory.filePath("source.sqlite"));
        QSignalSpy ready(&source, &SessionRepository::ready), saved(&source, &SessionRepository::sessionSaved), done(&source, &SessionRepository::operationCompleted), errors(&source, &SessionRepository::failed);
        QVERIFY(received(ready)); source.saveSession(archive); QVERIFY(received(saved));
        source.exportSession(id, destination); QVERIFY(received(done, 2));
        QFile file(destination.toLocalFile()); QVERIFY(file.open(QIODevice::ReadOnly));
        QCOMPARE(QJsonDocument::fromJson(file.readAll()).object(), archive);
        SessionRepository target(directory.filePath("target.sqlite"));
        QSignalSpy target_ready(&target, &SessionRepository::ready), imported(&target, &SessionRepository::sessionLoaded), target_errors(&target, &SessionRepository::failed);
        QVERIFY(received(target_ready)); target.importSession(destination); QVERIFY(received(imported));
        QVERIFY(loaded(imported)["historical"].toBool()); QCOMPARE(loaded(imported)["runs"], archive["runs"]);
        target.importSession(destination); QVERIFY(received(imported, 2)); QVERIFY(target_errors.isEmpty());
        auto collision = archive; collision["objective"] = "Different archive.";
        QVERIFY(writeJson(destination.toLocalFile(), collision)); target.importSession(destination); QVERIFY(received(target_errors));
        QCOMPARE(failure(target_errors), "session_collision");
        source.exportSession(id, QUrl::fromLocalFile(directory.filePath("source.sqlite"))); QVERIFY(received(errors));
        QCOMPARE(failure(errors), "file_invalid");
    }
    void malformedImportMakesNoDatabaseChanges() {
        QTemporaryDir directory; SessionRepository repository(directory.filePath("history.sqlite"));
        QSignalSpy ready(&repository, &SessionRepository::ready), errors(&repository, &SessionRepository::failed), listed(&repository, &SessionRepository::sessionsListed);
        QVERIFY(received(ready));
        auto archive = fixture(); archive["archive_version"] = 2;
        const auto path = directory.filePath("invalid.json"); QVERIFY(writeJson(path, archive));
        repository.importSession(QUrl::fromLocalFile(path)); QVERIFY(received(errors)); QCOMPARE(failure(errors), "invalid_archive");
        repository.listSessions(); QVERIFY(received(listed)); QVERIFY(listed.last().first().value<QJsonArray>().isEmpty());
        QFile invalid(path); QVERIFY(invalid.open(QIODevice::WriteOnly)); invalid.write("{broken"); invalid.close();
        repository.importSession(QUrl::fromLocalFile(path)); QVERIFY(received(errors, 2)); QCOMPARE(failure(errors), "invalid_archive");
        repository.importSession(QUrl("https://example.invalid/archive.json")); QVERIFY(received(errors, 3)); QCOMPARE(failure(errors), "file_invalid");
    }
    void validationRejectsIncoherentIdentityDimensionsAndControls() {
        const auto valid = fixture(); QString error;
        auto invalid = valid; invalid["session_id"] = "not-a-uuid"; QVERIFY(!SessionRepository::validateArchive(invalid, &error)); QVERIFY(!error.isEmpty());
        invalid = valid; auto snapshot = invalid["snapshot"].toObject(); snapshot["physical_hash"] = "fabricated"; invalid["snapshot"] = snapshot;
        QVERIFY(!SessionRepository::validateArchive(invalid));
        for (const auto* field : {"snapshot_id", "physical_hash", "horizon_h"}) {
            invalid = valid; auto runs = invalid["runs"].toArray(); auto details = runs[0].toObject(), run = details["run"].toObject();
            run[QLatin1String(field)] = QStringLiteral("wrong"); details["run"] = run; runs[0] = details; invalid["runs"] = runs;
            QVERIFY(!SessionRepository::validateArchive(invalid));
        }
        invalid = valid; auto runs = invalid["runs"].toArray(); runs.append(runs.first()); invalid["runs"] = runs;
        QVERIFY(!SessionRepository::validateArchive(invalid));
        invalid = valid; runs = invalid["runs"].toArray(); auto details = runs[0].toObject();
        details["time_h"] = QJsonArray{0, 1, 1, 3}; runs[0] = details; invalid["runs"] = runs;
        QVERIFY(!SessionRepository::validateArchive(invalid));
        invalid = valid; runs = invalid["runs"].toArray(); details = runs[0].toObject(); auto run = details["run"].toObject();
        run["controls"] = QJsonArray{QJsonObject{{"control_id", "invented"}, {"value", 1}}}; details["run"] = run; runs[0] = details; invalid["runs"] = runs;
        QVERIFY(!SessionRepository::validateArchive(invalid));
    }
    void archivesRejectSecretsReasoningAndResourceAbuse() {
        const auto valid = fixture();
        for (const auto* field : {"apiKey", "api_key", "Authorization", "reasoning", "reasoning_content"}) {
            auto invalid = valid; invalid["model"] = QJsonObject{{QLatin1String(field), "must-not-persist"}};
            QVERIFY(!SessionRepository::validateArchive(invalid));
        }
        auto allowed = valid; auto snapshot = allowed["snapshot"].toObject(), network = snapshot["network"].toObject(); auto assets = network["assets"].toArray();
        auto asset = assets[0].toObject(); asset["label"] = "Untrusted label mentions api_key and reasoning_content"; assets[0] = asset; network["assets"] = assets;
        const auto parsed = parse_project({{"version", 1}, {"network", network}});
        snapshot["network"] = network; snapshot["physical_hash"] = physics_hash(parsed); allowed["snapshot"] = snapshot;
        auto runs = allowed["runs"].toArray(); auto details = runs[0].toObject(), run = details["run"].toObject();
        run["physical_hash"] = snapshot["physical_hash"]; details["run"] = run; runs[0] = details; allowed["runs"] = runs;
        QVERIFY(SessionRepository::validateArchive(allowed));
        auto invalid = valid; invalid["objective"] = QString(4001, 'x'); QVERIFY(!SessionRepository::validateArchive(invalid));
        invalid = valid; invalid["checkpoint_sequence"] = 1.5; QVERIFY(!SessionRepository::validateArchive(invalid));
        invalid = valid; invalid["checkpoint_sequence"] = std::numeric_limits<double>::infinity(); QVERIFY(!SessionRepository::validateArchive(invalid));
        invalid = valid; QJsonArray excessive; for (int i = 0; i < 129; ++i) excessive.append(valid["runs"].toArray().first()); invalid["runs"] = excessive;
        QVERIFY(!SessionRepository::validateArchive(invalid));
    }
    void importedDerivedControlIdsPreserveMaximumLengthAssetIds() {
        auto archive = fixture();
        auto snapshot = archive["snapshot"].toObject();
        auto project = parse_project({{"version", 1}, {"network", snapshot["network"]}});
        const auto source_id = std::string(256, 's'), route_id = std::string(256, 'r');
        for (auto& asset : project.network.assets) if (asset.id == "source_a") asset.id = source_id;
        for (auto& route : project.network.routes) {
            if (route.source_id == "source_a") route.source_id = source_id;
            if (route.id == "bypass") route.id = route_id;
        }
        auto overrides = incident_controls("mixed");
        for (auto& control : overrides) {
            if (control.id == "source:source_a:curtail") control.id = "source:" + source_id + ":curtail";
            if (control.id == "route:bypass:flow") control.id = "route:" + route_id + ":flow";
        }
        // Renaming stable IDs changes no numerical parameters. Check the renamed
        // candidate against the actual native simulator and validator as well.
        const auto compiled = compile(project.network, overrides);
        const auto native_run = simulate(compiled);
        QVERIFY(validate(compiled, native_run).feasible);
        snapshot["network"] = network_json(project.network);
        snapshot["physical_hash"] = physics_hash(project); archive["snapshot"] = snapshot;
        auto runs = archive["runs"].toArray(); auto details = runs[0].toObject(), summary = details["run"].toObject();
        QCOMPARE(summary["cost"].toDouble(), native_run.cost);
        for (qsizetype tank = 0; tank < details["volumes_m3"].toArray().size(); ++tank)
            QCOMPARE(details["volumes_m3"].toArray()[tank].toArray().last().toDouble(), native_run.volumes_m3[static_cast<std::size_t>(tank)].back());
        QJsonArray controls;
        for (const auto& control : overrides) controls.append(QJsonObject{{"control_id", QString::fromStdString(control.id)}, {"value", control.value}});
        summary["physical_hash"] = snapshot["physical_hash"]; summary["controls"] = controls;
        details["run"] = summary; runs[0] = details; archive["runs"] = runs;
        auto recommendation = archive["recommendation"].toObject();
        recommendation["physical_hash"] = snapshot["physical_hash"]; recommendation["controls"] = controls; archive["recommendation"] = recommendation;
        QString error; QVERIFY2(SessionRepository::validateArchive(archive, &error), qPrintable(error));
        QTemporaryDir directory; QVERIFY(directory.isValid());
        const auto path = directory.filePath("long-control-ids.json"); QVERIFY(writeJson(path, archive));
        SessionRepository repository(directory.filePath("history.sqlite"));
        QSignalSpy ready(&repository, &SessionRepository::ready), imported(&repository, &SessionRepository::sessionLoaded), errors(&repository, &SessionRepository::failed);
        QVERIFY(received(ready)); repository.importSession(QUrl::fromLocalFile(path)); QVERIFY(received(imported));
        QCOMPARE(loaded(imported)["runs"], archive["runs"]); QVERIFY(errors.isEmpty());
        QVERIFY(controls.first().toObject()["control_id"].toString().size() > 256);
        auto invalid = archive; runs = invalid["runs"].toArray(); details = runs[0].toObject(); summary = details["run"].toObject();
        controls[0] = QJsonObject{{"control_id", QString(513, 'x')}, {"value", 1}};
        summary["controls"] = controls; details["run"] = summary; runs[0] = details; invalid["runs"] = runs;
        QVERIFY(!SessionRepository::validateArchive(invalid));
    }
    void transactionFailureLeavesPreviousCheckpointIntact() {
        QTemporaryDir directory; const auto path = directory.filePath("history.sqlite"); SessionRepository repository(path);
        QSignalSpy ready(&repository, &SessionRepository::ready), saved(&repository, &SessionRepository::sessionSaved),
            errors(&repository, &SessionRepository::failed), read(&repository, &SessionRepository::sessionLoaded);
        QVERIFY(received(ready)); auto archive = fixture(); repository.saveSession(archive); QVERIFY(received(saved));
        QVERIFY(sql(path, "CREATE TRIGGER reject_update BEFORE UPDATE ON sessions BEGIN SELECT RAISE(ABORT, 'test transaction failure'); END"));
        archive["checkpoint_sequence"] = 2; repository.saveSession(archive); QVERIFY(received(errors)); QCOMPARE(failure(errors), "storage_failed");
        repository.loadSession(archive["session_id"].toString()); QVERIFY(received(read)); QCOMPARE(loaded(read)["checkpoint_sequence"].toInt(), 1);
    }
    void corruptAndFutureDatabasesAreReportedWithoutReset() {
        QTemporaryDir directory; const auto corrupt = directory.filePath("corrupt.sqlite");
        QFile file(corrupt); QVERIFY(file.open(QIODevice::WriteOnly)); const QByteArray bytes("not a database; preserve these bytes");
        QCOMPARE(file.write(bytes), bytes.size()); file.close();
        {
            SessionRepository repository(corrupt); QSignalSpy errors(&repository, &SessionRepository::failed), ready(&repository, &SessionRepository::ready);
            QVERIFY(received(errors)); QVERIFY(ready.isEmpty());
        }
        QVERIFY(file.open(QIODevice::ReadOnly)); QCOMPARE(file.readAll(), bytes); file.close();
        const auto future = directory.filePath("future.sqlite"); QVERIFY(sql(future, "PRAGMA user_version=99"));
        SessionRepository repository(future); QSignalSpy errors(&repository, &SessionRepository::failed), ready(&repository, &SessionRepository::ready);
        QVERIFY(received(errors)); QCOMPARE(failure(errors), "unsupported_database_version"); QVERIFY(ready.isEmpty());
    }
    void unavailableStorageRemainsVisibleAndNoReadyIsClaimed() {
        QTemporaryDir directory;
        SessionRepository repository(directory.path()); // A directory cannot be an SQLite file.
        QSignalSpy errors(&repository, &SessionRepository::failed), ready(&repository, &SessionRepository::ready);
        QVERIFY(received(errors)); QCOMPARE(failure(errors), "storage_unavailable"); QVERIFY(ready.isEmpty());
        repository.saveSession(fixture()); QVERIFY(received(errors, 2)); QCOMPARE(failure(errors), "storage_unavailable");
    }
};

QTEST_GUILESS_MAIN(SessionRepositoryTests)
#include "session_repository_tests.moc"
