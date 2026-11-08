#include "eog/EogAnalysisClient.hpp"
#include "eog/EogContracts.hpp"
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTimer>
#include <QtTest>
#include <memory>
#include <stdexcept>

using namespace breakwater::desktop::eog;
namespace {
QString python() {
#ifdef BREAKWATER_TEST_PYTHON
    return QString::fromUtf8(BREAKWATER_TEST_PYTHON);
#else
    return qEnvironmentVariable("BREAKWATER_PYTHON");
#endif
}
bool write(const QString& path, const QByteArray& content) {
    QFile file(path); return file.open(QIODevice::WriteOnly | QIODevice::Truncate) && file.write(content) == content.size();
}
QJsonObject request(const std::shared_ptr<const Capture>& capture, const QString& id = "protocol-request") {
    const auto manifest = capture->manifest();
    return {{"eog_version", 1}, {"kind", "analysis_request"}, {"request_id", id},
        {"operation", "detect"}, {"method_version", 1}, {"dataset_identity", manifest.value("dataset_identity")},
        {"capture_id", manifest.value("capture_id")}, {"cutoff_unix", manifest.value("cutoff_unix")},
        {"capture_directory", capture->directory()}, {"capture_manifest", manifest},
        {"arguments", QJsonObject{{"system_ids", QJsonArray{}}}}};
}
QJsonObject validationRequest(const std::shared_ptr<const Capture>& capture) {
    auto result = request(capture);
    QJsonObject plan{{"plan_id", "protocol-only-plan"}, {"context", capture->manifest().value("context")},
        {"start_unix", result.value("cutoff_unix")}, {"repair_deadlines", QJsonArray{QJsonObject{{"facility_id", "facility-Z"}, {"repair_elapsed_s", 7200}}}},
        {"truck_jobs", QJsonArray{}}, {"well_shut_ins", QJsonArray{}}, {"inspection_assets", QJsonArray{}}};
    plan["plan_hash"] = plan_identity(plan);
    result["operation"] = "validate_plan";
    result["arguments"] = QJsonObject{{"plan", plan}, {"evaluator_method", "protocol-only-evaluator"}};
    return result;
}
struct CanaryEnvironment {
    QByteArray previous = qgetenv("BREAKWATER_EOG_TEST_SECRET_CANARY");
    CanaryEnvironment() { qputenv("BREAKWATER_EOG_TEST_SECRET_CANARY", "harmless-test-only"); }
    ~CanaryEnvironment() {
        if (previous.isNull()) qunsetenv("BREAKWATER_EOG_TEST_SECRET_CANARY");
        else qputenv("BREAKWATER_EOG_TEST_SECRET_CANARY", previous);
    }
};
}
class EogAnalysisClientTests final : public QObject {
    Q_OBJECT
    QString fixtureFolder_, worker_;
    std::shared_ptr<const Capture> capture(bool omitChannel = false, bool extendClock = false) const {
        QTemporaryDir source;
        for (const auto* name : {"network.json", "ops.json", "telemetry.csv"})
            if (!QFile::copy(QDir(fixtureFolder_).filePath(name), source.filePath(name)))
                throw std::runtime_error("Cannot copy EOG fixture");
        if (omitChannel || extendClock) {
            QFile file(source.filePath("telemetry.csv"));
            if (!file.open(QIODevice::ReadOnly)) throw std::runtime_error("Cannot read grounding fixture");
            const auto original = file.readAll(); file.close(); QByteArray telemetry;
            for (const auto& line : original.split('\n'))
                if (!line.isEmpty() && (!omitChannel || !line.contains(",pump-Z,discharge_psi,"))) telemetry += line + '\n';
            if (extendClock) telemetry += "2026-10-05T00:15:00Z,1791159300,facility-Z,inflow_bpd,17.5\n";
            if (!write(file.fileName(), telemetry)) throw std::runtime_error("Cannot write grounding fixture");
        }
        const auto data = load_dataset(source.path());
        return prepare_capture(data, data->now_unix);
    }
private slots:
    void initTestCase() {
        QVERIFY2(QFileInfo(python()).isFile(), "The pinned Python executable is required; no skipped process tests.");
        const auto path = QFINDTESTDATA("eog_fixtures/render/network.json");
        QVERIFY(!path.isEmpty()); fixtureFolder_ = QFileInfo(path).absolutePath();
        worker_ = QFINDTESTDATA("eog_fixtures/analysis_worker_fixture.py");
        QVERIFY(!worker_.isEmpty());
    }
    void genuineProcessFragmentsAndValidationLeaveTheUiResponsive() {
        const auto lease = capture(); EogAnalysisClient client(python(), worker_, {"fragmented"});
        QVERIFY(client.configured()); QVERIFY(!client.executionVerified());
        QSignalSpy done(&client, &EogAnalysisClient::completed), failure(&client, &EogAnalysisClient::failed);
        int beats = 0; QTimer heartbeat; heartbeat.setInterval(2);
        connect(&heartbeat, &QTimer::timeout, this, [&] { ++beats; }); heartbeat.start();
        const auto input = request(lease);
        QVERIFY(client.start(input, lease, 15000)); QVERIFY(client.busy());
        QVERIFY(!client.start(input, lease)); // No second concurrent job.
        QTRY_COMPARE_WITH_TIMEOUT(done.size(), 1, 15000);
        QCOMPARE(failure.size(), 0); QVERIFY(beats > 2); QVERIFY(!client.busy()); QVERIFY(client.executionVerified());
        const auto response = done.front().at(0).toJsonObject(), metadata = done.front().at(1).toJsonObject();
        QCOMPARE(response.value("request_id"), input.value("request_id"));
        QCOMPARE(response.value("capture_id"), lease->manifest().value("capture_id"));
        QCOMPARE(response.value("result").toObject().value("analysis_id").toString(), "protocol-only-detection");
        QCOMPARE(metadata.keys(), (QStringList{"duration_ms", "method", "process_exit_code", "retry_count", "status"}));
        QVERIFY(metadata.value("duration_ms").toDouble() >= 0);
        QCOMPARE(metadata.value("method").toString(), "detect"); QCOMPARE(metadata.value("retry_count").toInt(), 0);
        QVERIFY(!QJsonDocument(metadata).toJson().contains(lease->directory().toUtf8()));
    }
    void injectedSearchExchangeDoesNotInventImplementationIdentity() {
        // This real process is deliberately a protocol fixture, not search.
        // Wire method_version and the operation name cannot identify its code.
        const auto lease = capture(); auto input = request(lease, "protocol-only-search");
        input["operation"] = "search_plans";
        input["arguments"] = QJsonObject{{"system_ids", QJsonArray{}}, {"candidate_limit", 17}, {"time_limit_s", 1}};
        EogAnalysisClient client(python(), worker_, {"unavailable"});
        QSignalSpy done(&client, &EogAnalysisClient::completed), failed(&client, &EogAnalysisClient::failed);
        QVERIFY(client.start(input, lease, 15000)); QTRY_COMPARE_WITH_TIMEOUT(done.size(), 1, 15000);
        QVERIFY(failed.isEmpty()); QVERIFY(!client.executionVerified());
        const auto response = done.first().at(0).toJsonObject(), metadata = done.first().at(1).toJsonObject();
        QCOMPARE(response.value("method_version"), QJsonValue(1));
        QCOMPARE(response.value("status"), QJsonValue("unavailable"));
        QCOMPARE(metadata.value("method"), QJsonValue("search_plans"));
        QCOMPARE(metadata.keys(), (QStringList{"duration_ms", "method", "process_exit_code", "retry_count", "status"}));
        for (const auto* field : {"worker_identity", "worker_method", "search_method", "implementation_version"})
            QVERIFY(!metadata.contains(QLatin1String(field)));
        QVERIFY(!QJsonDocument(metadata).toJson().contains(lease->directory().toUtf8()));
    }
    void failures_data() {
        QTest::addColumn<QString>("mode"); QTest::addColumn<QString>("code");
        const QList<QPair<QString, QString>> cases{
            {"wrong_id", "worker_response_invalid"}, {"wrong_cutoff", "worker_response_invalid"},
            {"wrong_capture", "worker_response_invalid"}, {"unknown_asset", "worker_response_invalid"},
            {"future_evidence", "worker_response_invalid"}, {"duplicate_key", "worker_response_invalid"},
            {"malformed", "worker_response_invalid"}, {"invalid_utf8", "worker_response_invalid"},
            {"extra_output", "worker_protocol_invalid"}, {"no_newline", "worker_protocol_invalid"},
            {"oversized_stdout", "worker_stdout_limit"}, {"oversized_stderr", "worker_stderr_limit"},
            {"nonzero_after_valid", "worker_exit_failed"}, {"crash", "worker_crashed"},
            {"tamper_capture", "capture_changed"}};
        for (const auto& entry : cases) QTest::newRow(qPrintable(entry.first)) << entry.first << entry.second;
    }
    void failures() {
        QFETCH(QString, mode); QFETCH(QString, code);
        const auto lease = capture(); EogAnalysisClient client(python(), worker_, {mode});
        QSignalSpy done(&client, &EogAnalysisClient::completed), failure(&client, &EogAnalysisClient::failed);
        QVERIFY(client.start(request(lease), lease, 15000));
        QTRY_COMPARE_WITH_TIMEOUT(failure.size(), 1, 15000);
        QCOMPARE(done.size(), 0); QCOMPARE(failure.front().at(0).toString(), code);
        QVERIFY(!client.busy()); QVERIFY(!client.executionVerified());
        const auto message = failure.front().at(1).toString();
        QVERIFY(!message.contains(lease->directory())); QVERIFY(!message.contains("PRIVATE_PROVIDER_TRACE"));
    }
    void planHashBindingIsCheckedAgainstTheExactRequest() {
        const auto lease = capture(); const auto input = validationRequest(lease);
        validate_document(input, "analysis_request");
        EogAnalysisClient good(python(), worker_); QSignalSpy done(&good, &EogAnalysisClient::completed);
        QSignalSpy goodFailed(&good, &EogAnalysisClient::failed);
        QVERIFY(good.start(input, lease, 15000)); QTRY_COMPARE_WITH_TIMEOUT(done.size(), 1, 15000);
        QCOMPARE(goodFailed.size(), 0);
        EogAnalysisClient bad(python(), worker_, {"wrong_plan_hash"});
        QSignalSpy failure(&bad, &EogAnalysisClient::failed), badDone(&bad, &EogAnalysisClient::completed);
        QVERIFY(bad.start(input, lease, 15000)); QTRY_COMPARE_WITH_TIMEOUT(failure.size(), 1, 15000);
        QCOMPARE(failure.front().at(0).toString(), "worker_response_invalid"); QCOMPARE(badDone.size(), 0);
    }
    void originalSourceReferencesAreCheckedBeforePublication_data() {
        QTest::addColumn<QString>("mode"); QTest::addColumn<bool>("validation");
        QTest::addColumn<bool>("omitChannel"); QTest::addColumn<bool>("extendClock");
        QTest::addColumn<bool>("valid"); QTest::addColumn<double>("expected"); QTest::addColumn<QString>("quality");
        for (const bool validation : {false, true}) {
            const QString prefix = validation ? "validation_" : "";
            for (const auto& mode : {QStringLiteral("wrong_source_value"), QStringLiteral("wrong_source_time"), QStringLiteral("absent_source_channel")})
                QTest::newRow(qPrintable(prefix + mode)) << prefix + mode << validation << (mode == "absent_source_channel") << false << false << 0.0 << QString{};
            QTest::newRow(qPrintable(prefix + "actual_negative")) << prefix + "actual_negative" << validation << false << false << true << -2.0 << QString("good");
            QTest::newRow(qPrintable(prefix + "actual_overflow")) << prefix + "actual_overflow" << validation << false << false << true << 104.0 << QString("good");
            QTest::newRow(qPrintable(prefix + "actual_stale")) << prefix + "actual_stale" << validation << false << true << true << -2.0 << QString("stale");
            QTest::newRow(qPrintable(prefix + "missing_sample")) << prefix + "missing_sample" << validation << true << false << true << 0.0 << QString("missing");
        }
    }
    void originalSourceReferencesAreCheckedBeforePublication() {
        QFETCH(QString, mode); QFETCH(bool, validation); QFETCH(bool, omitChannel); QFETCH(bool, extendClock);
        QFETCH(bool, valid); QFETCH(double, expected); QFETCH(QString, quality);
        const auto lease = capture(omitChannel, extendClock);
        EogAnalysisClient client(python(), worker_, {mode});
        QSignalSpy done(&client, &EogAnalysisClient::completed), failure(&client, &EogAnalysisClient::failed);
        const auto input = validation ? validationRequest(lease) : request(lease);
        QVERIFY(client.start(input, lease, 15000));
        QTRY_VERIFY_WITH_TIMEOUT(!done.isEmpty() || !failure.isEmpty(), 15000);
        if (!valid) {
            QCOMPARE(done.size(), 0); QCOMPARE(failure.size(), 1);
            QCOMPARE(failure.front().at(0).toString(), "worker_response_invalid");
            QVERIFY(!client.executionVerified()); return;
        }
        QCOMPARE(failure.size(), 0); QCOMPARE(done.size(), 1); QVERIFY(client.executionVerified());
        auto result = done.front().at(0).toJsonObject().value("result").toObject();
        if (validation) result = result.value("validation").toObject();
        const auto evidence = result.value("evidence").toArray().first().toObject();
        QCOMPARE(evidence.value("quality").toString(), quality);
        if (quality == "missing") QVERIFY(evidence.value("value").isNull());
        else QCOMPARE(evidence.value("value").toDouble(), expected);
        if (extendClock) QVERIFY(!load_dataset(lease->directory())->warnings.isEmpty());
    }
    void sourceEvidenceLookupHonorsCancellation() {
        const auto lease = capture(); const auto data = load_dataset(lease->directory());
        const auto token = std::make_shared<std::atomic_bool>(true);
        QVERIFY_EXCEPTION_THROWN(validate_source_evidence({}, *data, token), std::runtime_error);
    }
    void cleanExitIsRequiredAndCancelRetiresOldCallbacks() {
        QTemporaryDir control; const auto gate = control.filePath("exit-gate");
        const auto lease = capture(); EogAnalysisClient client(python(), worker_, {"gated_exit", gate});
        QSignalSpy done(&client, &EogAnalysisClient::completed), failure(&client, &EogAnalysisClient::failed);
        QVERIFY(client.start(request(lease, "first-request"), lease, 15000));
        QTRY_VERIFY_WITH_TIMEOUT(QFileInfo(gate + ".ready").exists(), 15000);
        QVERIFY(client.busy()); QCOMPARE(done.size(), 0);
        client.cancel(); QCOMPARE(failure.size(), 1); QCOMPARE(failure.front().at(0).toString(), "cancelled");
        QVERIFY(write(gate, "allow new process exit"));
        QVERIFY(client.start(request(lease, "second-request"), lease, 15000));
        QTRY_COMPARE_WITH_TIMEOUT(done.size(), 1, 15000);
        QCOMPARE(done.front().front().toJsonObject().value("request_id").toString(), "second-request");
        QCOMPARE(failure.size(), 1); QVERIFY(client.executionVerified());
    }
    void timeoutKillsWithoutBlockingAndCanRecoverAfterCrash() {
        const auto lease = capture();
        QTemporaryDir timeoutControl; const auto timeoutGate = timeoutControl.filePath("never-released");
        EogAnalysisClient stalled(python(), worker_, {"gated_exit", timeoutGate});
        QSignalSpy failed(&stalled, &EogAnalysisClient::failed);
        int beats = 0; QTimer heartbeat; heartbeat.setInterval(2);
        connect(&heartbeat, &QTimer::timeout, this, [&] { ++beats; }); heartbeat.start();
        QVERIFY(stalled.start(request(lease), lease, 3000));
        QTRY_VERIFY_WITH_TIMEOUT(QFileInfo(timeoutGate + ".ready").exists(), 15000);
        QTRY_COMPARE_WITH_TIMEOUT(failed.size(), 1, 10000);
        QCOMPARE(failed.front().at(0).toString(), "worker_timeout"); QVERIFY(beats > 1);
        QTemporaryDir control; EogAnalysisClient recovering(python(), worker_, {"crash_once", control.filePath("first-attempt")});
        QSignalSpy crash(&recovering, &EogAnalysisClient::failed), done(&recovering, &EogAnalysisClient::completed);
        QVERIFY(recovering.start(request(lease, "crashed-request"), lease, 15000));
        QTRY_COMPARE_WITH_TIMEOUT(crash.size(), 1, 15000);
        QCOMPARE(crash.front().at(0).toString(), "worker_crashed");
        QVERIFY(recovering.start(request(lease, "recovered-request"), lease, 15000));
        QTRY_COMPARE_WITH_TIMEOUT(done.size(), 1, 15000);
        QCOMPARE(done.front().front().toJsonObject().value("request_id").toString(), "recovered-request");
    }
    void unavailableIsNotExecutionAndDiagnosticPathsStayPrivate() {
        const auto lease = capture(); EogAnalysisClient unavailable(python(), worker_, {"unavailable"});
        QSignalSpy done(&unavailable, &EogAnalysisClient::completed);
        QVERIFY(unavailable.start(request(lease), lease, 15000)); QTRY_COMPARE_WITH_TIMEOUT(done.size(), 1, 15000);
        QCOMPARE(done.front().front().toJsonObject().value("status").toString(), "unavailable");
        QVERIFY(!unavailable.executionVerified());
        EogAnalysisClient stderrClient(python(), worker_, {"stderr_private"});
        QSignalSpy clean(&stderrClient, &EogAnalysisClient::completed);
        QVERIFY(stderrClient.start(request(lease), lease, 15000)); QTRY_COMPARE_WITH_TIMEOUT(clean.size(), 1, 15000);
        for (const auto& argument : clean.front())
            QVERIFY(!QJsonDocument(argument.toJsonObject()).toJson().contains("PRIVATE_PROVIDER_TRACE"));
    }
    void inheritedCredentialsAndArbitraryEnvironmentAreOmitted() {
        const CanaryEnvironment environment;
        const auto lease = capture(); EogAnalysisClient client(python(), worker_);
        QSignalSpy done(&client, &EogAnalysisClient::completed), failure(&client, &EogAnalysisClient::failed);
        QVERIFY(client.start(request(lease), lease, 15000));
        QTRY_COMPARE_WITH_TIMEOUT(done.size(), 1, 15000); QCOMPARE(failure.size(), 0);
    }
    void malformedAndOversizedRequestsFailOffline() {
        const auto lease = capture();
        for (const auto& mode : {QStringLiteral("version"), QStringLiteral("oversized")}) {
            EogAnalysisClient client(python(), worker_);
            QSignalSpy failure(&client, &EogAnalysisClient::failed), done(&client, &EogAnalysisClient::completed);
            auto input = request(lease);
            if (mode == "version") input["eog_version"] = 99;
            else input["unexpected_large_field"] = QString(1024 * 1024, 'x');
            QVERIFY(client.start(input, lease, 15000)); QTRY_COMPARE_WITH_TIMEOUT(failure.size(), 1, 15000);
            QCOMPARE(failure.front().at(0).toString(), "worker_request_invalid"); QCOMPARE(done.size(), 0);
        }
    }
    void invalidRequestOrEditedCaptureFailsBeforeProviderExecution() {
        const auto lease = capture();
        EogAnalysisClient mismatched(python(), worker_); QSignalSpy failure(&mismatched, &EogAnalysisClient::failed);
        auto input = request(lease); input["capture_directory"] = lease->directory() + "-different";
        QVERIFY(mismatched.start(input, lease, 15000)); QTRY_COMPARE_WITH_TIMEOUT(failure.size(), 1, 15000);
        QCOMPARE(failure.front().at(0).toString(), "worker_request_mismatch");
        QFile file(QDir(lease->directory()).filePath("network.json")); QVERIFY(file.open(QIODevice::Append));
        QCOMPARE(file.write(" "), qint64(1)); file.close();
        EogAnalysisClient changed(python(), worker_); QSignalSpy modified(&changed, &EogAnalysisClient::failed);
        QVERIFY(changed.start(request(lease), lease, 15000)); QTRY_COMPARE_WITH_TIMEOUT(modified.size(), 1, 15000);
        QCOMPARE(modified.front().at(0).toString(), "capture_changed");
    }
    void missingWorkerRemainsUnconfigured() {
        const auto lease = capture();
        EogAnalysisClient missing(python(), worker_ + ".missing"); QVERIFY(!missing.configured());
        QSignalSpy failure(&missing, &EogAnalysisClient::failed);
        QVERIFY(!missing.start(request(lease), lease)); QCOMPARE(failure.size(), 1);
        QCOMPARE(failure.front().at(0).toString(), "worker_unavailable");
        QVERIFY(!missing.busy()); QVERIFY(!missing.executionVerified());
    }
    void processAndValidationHoldCaptureUntilRetired() {
        QTemporaryDir control; const auto gate = control.filePath("exit-gate");
        auto lease = capture(); const auto directory = lease->directory();
        auto client = std::make_unique<EogAnalysisClient>(python(), worker_, QStringList{"gated_exit", gate});
        QVERIFY(client->start(request(lease), lease, 15000)); lease.reset();
        QTRY_VERIFY_WITH_TIMEOUT(QFileInfo(gate + ".ready").exists(), 15000);
        QVERIFY(QFileInfo(directory).isDir());
        client.reset();
        QTRY_VERIFY_WITH_TIMEOUT(!QFileInfo(directory).exists(), 10000);
    }
};
QTEST_GUILESS_MAIN(EogAnalysisClientTests)
#include "eog_analysis_client_tests.moc"
