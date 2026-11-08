// Real desktop -> protected capture -> PR40 Python/native provider integration.
// Data and expected outcomes reuse science/tests/eog_test_fixtures.py and
// test_eog_worker.py. These checks do not independently qualify the mathematics.
#include "eog/EogAnalysisController.hpp"
#include "eog/EogContracts.hpp"
#include <QCryptographicHash>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QMap>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTimer>
#include <QTimeZone>
#include <QtTest>
#include <array>
#include <memory>
#include <stdexcept>
#include <utility>

using namespace breakwater::desktop;
using namespace breakwater::desktop::eog;

namespace {
QString python() { return QString::fromUtf8(BREAKWATER_TEST_PYTHON); }
QString worker() { return QString::fromUtf8(BREAKWATER_EOG_WORKER_PATH); }
QString nativeLibrary() { return QString::fromUtf8(BREAKWATER_TEST_EOG_NATIVE_LIBRARY); }
QString contracts() { return QDir(QString::fromUtf8(BREAKWATER_SOURCE_DIR)).filePath("contracts/eog"); }
QStringList providerArguments() { return {"--contracts-root", contracts(), "--native-library", nativeLibrary()}; }

QByteArray read(const QString& path) {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) throw std::runtime_error("Cannot read provider integration fixture.");
    return file.readAll();
}
void write(const QString& path, const QByteArray& content) {
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate) || file.write(content) != content.size())
        throw std::runtime_error("Cannot write provider integration fixture.");
}

struct Inputs {
    QTemporaryDir directory;
    QString prefix, system, facility, tank, pump, pipe;
    QMap<QString, QByteArray> hashes;
    explicit Inputs(QString name = QStringLiteral("renamed/\u03b1"), bool future = false)
        : prefix(std::move(name)), system(prefix + "/system"), facility(prefix + "/facility:0"),
          tank(facility + "/tank"), pump(facility + "/pump"), pipe(facility + "/pipe") {
        if (!directory.isValid()) throw std::runtime_error("Cannot create provider integration inputs.");
        const std::array<double, 3> water{450, 700, 650}, oil{560, 170, 390}, gas{2000, 600, 1400};
        QJsonArray wells;
        for (int i = 0; i < 3; ++i) wells.append(QJsonObject{{"id", well(i)}, {"baseline_water_bpd", water[i]},
            {"oil_bpd", oil[i]}, {"gas_mcfd", gas[i]}});
        const QJsonObject f{{"id", facility}, {"lat", 31.9}, {"lon", -102.3}, {"wells", wells},
            {"tank", QJsonObject{{"id", tank}, {"capacity_bbl", 800}}},
            {"pump", QJsonObject{{"id", pump}, {"max_flow_bpd", 2700}, {"start_level_pct", 70}, {"stop_level_pct", 30}}},
            {"pipeline", QJsonObject{{"id", pipe}, {"max_bpd", 3000}, {"max_pressure_psi", 1700}}}};
        const QJsonObject network{
            {"meta", QJsonObject{{"units", QJsonObject{{"volume", "bbl"}, {"rate", "bpd"}, {"gas_rate", "mcfd"}, {"pressure", "psi"}}}, {"timezone", "UTC"}}},
            {"defaults", QJsonObject{{"tank_limits_pct", QJsonObject{{"warning", 85}, {"shutdown", 92}}}}},
            {"systems", QJsonArray{QJsonObject{{"id", system}, {"center", QJsonArray{31.9, -102.3}},
                {"truck_yard", QJsonArray{31.8, -102.2}},
                {"disposal_site", QJsonObject{{"id", prefix + "/disposal"}, {"lat", 31.9}, {"lon", -102.3}, {"pressure_psi", 1050}}},
                {"facilities", QJsonArray{f}}}}}};
        const QJsonObject ops{{"dataset_id", "self-created-analytical-fixture"},
            {"prices", QJsonObject{{"oil_per_bbl", 60}, {"gas_per_mcf", 3}}},
            {"expected_repair_hours", QJsonObject{{"pump_failure", 6}}},
            {"emergency_trucking", QJsonObject{{"truck_capacity_bbl", 150}, {"cost_per_trip", 905}}},
            {"systems", QJsonObject{{system, QJsonObject{{"emergency_fleet_size", 2},
                {"facilities", QJsonObject{{facility, QJsonObject{{"response_min", 75}, {"cycle_min", 230}}}}}}}}}};
        write(directory.filePath("network.json"), QJsonDocument(network).toJson(QJsonDocument::Compact));
        write(directory.filePath("ops.json"), QJsonDocument(ops).toJson(QJsonDocument::Compact));
        QByteArray csv = "timestamp,ts_unix,asset_id,metric,value\n";
        appendFrame(csv, 0, 0, 50);
        appendFrame(csv, 300, 2, 50);
        if (future) appendFrame(csv, 600, 1, 10);
        write(directory.filePath("telemetry.csv"), csv);
        hashes = currentHashes();
    }
    QString well(int i) const { return facility + QStringLiteral("/well:%1").arg(i); }
    void appendFrame(QByteArray& csv, qint64 timestamp, int state, double level) const {
        const auto iso = QDateTime::fromSecsSinceEpoch(timestamp, QTimeZone::UTC).toString("yyyy-MM-dd'T'HH:mm:ss'Z'");
        const auto row = [&](const QString& asset, const char* metric, double value) {
            csv += iso.toUtf8() + ',' + QByteArray::number(timestamp) + ',' + asset.toUtf8() + ',' + metric + ',' + QByteArray::number(value, 'g', 17) + '\n';
        };
        row(facility, "inflow_bpd", 1800); row(tank, "level_pct", level); row(pump, "status", state);
        row(pump, "flow_bpd", state == 1 ? 2700 : 0); row(pump, "discharge_psi", state == 1 ? 1200 : 1050);
        row(pipe, "receive_flow_bpd", state == 1 ? 2700 : 0); row(pipe, "pressure_psi", state == 1 ? 1100 : 1050);
        const std::array<double, 3> rates{450, 700, 650};
        for (int i = 0; i < 3; ++i) { row(well(i), "status", 1); row(well(i), "water_bpd", rates[i]); }
    }
    QMap<QString, QByteArray> currentHashes() const {
        QMap<QString, QByteArray> result;
        for (const auto* name : {"network.json", "ops.json", "telemetry.csv"})
            result[name] = QCryptographicHash::hash(read(directory.filePath(name)), QCryptographicHash::Sha256);
        return result;
    }
    std::shared_ptr<const Capture> capture(qint64 cutoff = 300) const {
        return prepare_capture(load_dataset(directory.path()), cutoff);
    }
};

QJsonObject request(const std::shared_ptr<const Capture>& capture, const QString& operation,
                    const QJsonObject& arguments, const QString& id) {
    const auto manifest = capture->manifest();
    return {{"eog_version", 1}, {"kind", "analysis_request"}, {"request_id", id}, {"operation", operation},
        {"method_version", 1}, {"dataset_identity", manifest.value("dataset_identity")}, {"capture_id", manifest.value("capture_id")},
        {"cutoff_unix", manifest.value("cutoff_unix")}, {"capture_directory", capture->directory()},
        {"capture_manifest", manifest}, {"arguments", arguments}};
}
QJsonObject noAction(const Inputs& inputs, const QString& context = "snapshot_now") {
    QJsonObject plan{{"plan_id", "integration/no-action"}, {"context", context}, {"start_unix", 300},
        {"repair_deadlines", QJsonArray{QJsonObject{{"facility_id", inputs.facility}, {"repair_elapsed_s", 21600}}}},
        {"truck_jobs", QJsonArray{}}, {"well_shut_ins", QJsonArray{}}, {"inspection_assets", QJsonArray{}}};
    plan["plan_hash"] = plan_identity(plan); return plan;
}
struct EnvironmentRestore {
    QList<QPair<QByteArray, QByteArray>> previous;
    EnvironmentRestore() {
        const QList<QPair<QByteArray, QByteArray>> paths{{"BREAKWATER_PYTHON", python().toUtf8()},
            {"BREAKWATER_EOG_WORKER", worker().toUtf8()}, {"BREAKWATER_EOG_CONTRACTS", contracts().toUtf8()},
            {"BREAKWATER_EOG_NATIVE", nativeLibrary().toUtf8()}};
        for (const auto& path : paths) { previous.append({path.first, qgetenv(path.first.constData())}); qputenv(path.first.constData(), path.second); }
    }
    ~EnvironmentRestore() {
        for (const auto& path : previous) {
            if (path.second.isNull()) qunsetenv(path.first.constData()); else qputenv(path.first.constData(), path.second);
        }
    }
};
class UnusedModel final : public agent::ModelTransport {
public:
    int requests = 0;
    void request(const QJsonArray&, const QJsonArray&) override { ++requests; emit requestFailed("unexpected_model_request", "Manual provider checks make no hosted model request."); }
    void cancel() override {}
};
}

class EogProviderIntegrationTests final : public QObject {
    Q_OBJECT
private slots:
    void initTestCase() {
        QVERIFY2(QFileInfo(python()).isFile(), "Pinned Python is mandatory; provider integration cannot skip.");
        QVERIFY2(QFileInfo(worker()).isFile(), "The real EOG provider is required, not a protocol fixture.");
        QVERIFY2(QFileInfo(nativeLibrary()).isFile(), "The matching native response DLL is required.");
        QVERIFY(QFileInfo(QDir(contracts()).filePath("validate.py")).isFile());
    }
    void actualDetectionBindsItsCaptureAndLeavesSourcesUnchanged_data() {
        QTest::addColumn<QString>("prefix");
        QTest::newRow("custom-ascii") << QStringLiteral("integration/custom");
        QTest::newRow("renamed-unicode") << QStringLiteral("renamed/\u03b1");
    }
    void actualDetectionBindsItsCaptureAndLeavesSourcesUnchanged() {
        QFETCH(QString, prefix); Inputs inputs(prefix); const auto capture = inputs.capture();
        EogAnalysisClient client(python(), worker(), providerArguments());
        QSignalSpy done(&client, &EogAnalysisClient::completed), failures(&client, &EogAnalysisClient::failed);
        int beats = 0; QTimer timer; timer.setInterval(2); connect(&timer, &QTimer::timeout, this, [&] { ++beats; }); timer.start();
        const auto input = request(capture, "detect", {{"system_ids", QJsonArray{}}}, "real/detect");
        QVERIFY(!client.executionVerified()); QVERIFY(client.start(input, capture));
        QTRY_VERIFY_WITH_TIMEOUT(done.size() == 1 || !failures.isEmpty(), 30000);
        QVERIFY2(failures.isEmpty(), failures.isEmpty() ? "" : qPrintable(failures.first().at(0).toString()));
        QCOMPARE(done.size(), 1); QVERIFY(beats > 2); QVERIFY(client.executionVerified());
        const auto output = done.first().at(0).toJsonObject(), metadata = done.first().at(1).toJsonObject();
        validate_document(output, "analysis_response", {{"request", input}});
        for (const auto* key : {"request_id", "operation", "method_version", "dataset_identity", "capture_id", "cutoff_unix"})
            QCOMPARE(output.value(key), input.value(key));
        QCOMPARE(output.value("status"), QJsonValue("completed"));
        const auto result = output.value("result").toObject(); const auto events = result.value("events").toArray();
        QCOMPARE(events.size(), 1); const auto event = events.first().toObject();
        QCOMPARE(event.value("category"), QJsonValue("pump_failure")); QCOMPARE(event.value("facility_id"), QJsonValue(inputs.facility));
        QCOMPARE(event.value("countdown_hours").toDouble(), 4.48); QCOMPARE(event.value("lost_revenue_per_hour").toDouble(), 3300.0);
        for (const auto& value : result.value("evidence").toArray()) QVERIFY(value.toObject().value("timestamp_unix").toInteger() <= 300);
        QCOMPARE(metadata.keys(), (QStringList{"duration_ms", "method", "process_exit_code", "retry_count", "status"}));
        QCOMPARE(metadata.value("process_exit_code").toInt(), 0); QCOMPARE(metadata.value("method"), QJsonValue("detect"));
        QVERIFY(!QJsonDocument(metadata).toJson().contains(capture->directory().toUtf8()));
        QCOMPARE(inputs.currentHashes(), inputs.hashes);
    }
    void defaultClientUsesTrustedLocalProviderPathsForRealEvaluation_data() {
        QTest::addColumn<bool>("pathOverrides");
        QTest::newRow("trusted-environment-paths") << true;
        QTest::newRow("exact-development-bin-fallback") << false;
    }
    void defaultClientUsesTrustedLocalProviderPathsForRealEvaluation() {
        QFETCH(bool, pathOverrides);
        Inputs inputs; const auto capture = inputs.capture(); EnvironmentRestore environment;
        if (!pathOverrides) for (const auto* name : {"BREAKWATER_EOG_WORKER", "BREAKWATER_EOG_CONTRACTS", "BREAKWATER_EOG_NATIVE"}) qunsetenv(name);
        EogAnalysisClient client; QSignalSpy done(&client, &EogAnalysisClient::completed), failures(&client, &EogAnalysisClient::failed);
        const auto plan = noAction(inputs), input = request(capture, "evaluate_plan", {{"plan", plan}}, "real/default-evaluate");
        QVERIFY(client.configured()); QVERIFY(client.start(input, capture));
        QTRY_VERIFY_WITH_TIMEOUT(done.size() == 1 || !failures.isEmpty(), 30000);
        QVERIFY2(failures.isEmpty(), failures.isEmpty() ? "" : qPrintable(failures.first().at(0).toString()));
        QCOMPARE(done.size(), 1); const auto response = done.first().at(0).toJsonObject();
        QCOMPARE(response.value("status"), QJsonValue("completed"));
        const auto evaluation = response.value("result").toObject().value("evaluation").toObject();
        QCOMPARE(evaluation.value("plan").toObject(), plan); QVERIFY(evaluation.value("feasible").toBool());
        QCOMPARE(evaluation.value("coverage"), QJsonValue("available")); QCOMPARE(evaluation.value("outcome"), QJsonValue("automatic_shutdown"));
        QVERIFY(qAbs(evaluation.value("cost").toObject().value("total").toDouble() - 5016.0) < 1e-6);
        const auto trajectory = evaluation.value("trajectories").toArray().first().toObject();
        QCOMPARE(trajectory.value("elapsed_s").toArray().last().toDouble(), 21600.0);
        QVERIFY(!evaluation.value("shutdowns").toArray().isEmpty()); QCOMPARE(inputs.currentHashes(), inputs.hashes);
    }
    void defaultStartupPathsMustBeAbsoluteAndLocal_data() {
        QTest::addColumn<QByteArray>("variable"); QTest::addColumn<QByteArray>("value");
        QTest::newRow("relative-contracts") << QByteArray("BREAKWATER_EOG_CONTRACTS") << QByteArray("contracts/eog");
        QTest::newRow("relative-library") << QByteArray("BREAKWATER_EOG_NATIVE") << QByteArray("out/provider.dll");
        QTest::newRow("remote-library") << QByteArray("BREAKWATER_EOG_NATIVE") << QByteArray("\\\\server\\provider.dll");
    }
    void defaultStartupPathsMustBeAbsoluteAndLocal() {
        QFETCH(QByteArray, variable); QFETCH(QByteArray, value); EnvironmentRestore environment;
        qputenv(variable.constData(), value); EogAnalysisClient invalid;
        QVERIFY(!invalid.configured()); QVERIFY(!invalid.executionVerified());
        EogAnalysisClient explicitCommand(python(), worker(), providerArguments());
        QVERIFY(explicitCommand.configured()); // Injected commands do not acquire default flags.
    }
    void historicalCaptureExcludesFutureObservationsWithoutChangingTheCurrentSource() {
        Inputs inputs(QStringLiteral("historical/\u03b1"), true); const auto capture = inputs.capture();
        QCOMPARE(capture->manifest().value("context"), QJsonValue("historical_cursor"));
        QCOMPARE(capture->manifest().value("source_now_unix").toInteger(), 600);
        const auto capturedCsv = read(QDir(capture->directory()).filePath("telemetry.csv"));
        QVERIFY(!capturedCsv.contains(",600,")); QVERIFY(read(inputs.directory.filePath("telemetry.csv")).contains(",600,"));
        EogAnalysisClient client(python(), worker(), providerArguments());
        QSignalSpy done(&client, &EogAnalysisClient::completed), failures(&client, &EogAnalysisClient::failed);
        QVERIFY(client.start(request(capture, "detect", {{"system_ids", QJsonArray{}}}, "real/historical-detect"), capture));
        QTRY_VERIFY_WITH_TIMEOUT(done.size() == 1 || !failures.isEmpty(), 30000); QVERIFY(failures.isEmpty()); QCOMPARE(done.size(), 1);
        const auto result = done.first().at(0).toJsonObject().value("result").toObject();
        QCOMPARE(result.value("events").toArray().size(), 1);
        for (const auto& value : result.value("evidence").toArray()) QVERIFY(value.toObject().value("timestamp_unix").toInteger() <= 300);
        const auto plan = noAction(inputs, "historical_cursor");
        QVERIFY(client.start(request(capture, "evaluate_plan", {{"plan", plan}}, "real/historical-evaluate"), capture));
        QTRY_VERIFY_WITH_TIMEOUT(done.size() == 2 || !failures.isEmpty(), 30000); QVERIFY(failures.isEmpty()); QCOMPARE(done.size(), 2);
        const auto evaluation = done.last().at(0).toJsonObject().value("result").toObject().value("evaluation").toObject();
        QCOMPARE(evaluation.value("plan").toObject(), plan); QCOMPARE(evaluation.value("outcome"), QJsonValue("automatic_shutdown"));
        QVERIFY(qAbs(evaluation.value("cost").toObject().value("total").toDouble() - 5016.0) < 1e-6);
        QCOMPARE(inputs.currentHashes(), inputs.hashes);
    }
    void unavailableSearchAndValidationCannotAuthorizeReviewAndRemainInHistory() {
        Inputs inputs; QTemporaryDir storage; QVERIFY(storage.isValid());
        EogDatasetController dataset; EogAnalysisClient client(python(), worker(), providerArguments());
        EogHistoryRepository history(storage.filePath("provider.sqlite")); UnusedModel model;
        QSignalSpy ready(&history, &EogHistoryRepository::ready), storageFailures(&history, &EogHistoryRepository::failed);
        QSignalSpy loaded(&history, &EogHistoryRepository::sessionLoaded), operatorRecords(&history, &EogHistoryRepository::operatorRecordSaved);
        EogAnalysisController controller(&dataset, &client, &history, &model);
        QSignalSpy done(&client, &EogAnalysisClient::completed), failures(&client, &EogAnalysisClient::failed);
        dataset.loadFolder(QUrl::fromLocalFile(inputs.directory.path()));
        QTRY_VERIFY_WITH_TIMEOUT(dataset.loaded() && ready.size() == 1, 15000);
        controller.detect(); QTRY_VERIFY_WITH_TIMEOUT(done.size() == 1 || !failures.isEmpty(), 30000);
        QVERIFY(failures.isEmpty()); QCOMPARE(done.size(), 1); QCOMPARE(controller.events().size(), 1);
        controller.evaluatePlan(QString::fromUtf8(QJsonDocument(noAction(inputs)).toJson(QJsonDocument::Compact)));
        QTRY_VERIFY_WITH_TIMEOUT(done.size() == 2 || !failures.isEmpty(), 30000); QVERIFY(failures.isEmpty()); QCOMPARE(done.size(), 2);
        QVERIFY(controller.selectedEvaluation().value("feasible").toBool()); QVERIFY(!controller.canReview());
        controller.searchPlans(); QTRY_VERIFY_WITH_TIMEOUT(done.size() == 3 || !failures.isEmpty(), 30000);
        QVERIFY(failures.isEmpty()); QCOMPARE(done.size(), 3); QCOMPARE(controller.lastOperation().value("status").toString(), "unavailable");
        QVERIFY(!controller.canReview());
        controller.validateSelected("eog-response-0.1.0"); QTRY_VERIFY_WITH_TIMEOUT(done.size() == 4 || !failures.isEmpty(), 30000);
        QVERIFY(failures.isEmpty()); QCOMPARE(done.size(), 4); QCOMPARE(controller.lastOperation().value("status").toString(), "unavailable");
        QVERIFY(controller.selectedValidation().isEmpty()); QVERIFY(!controller.canReview());
        for (int i : {2, 3}) {
            const auto response = done[i].at(0).toJsonObject();
            QCOMPARE(response.value("status"), QJsonValue("unavailable")); QVERIFY(response.value("result").isNull());
            QCOMPARE(response.value("diagnostics").toArray().first().toObject().value("code"), QJsonValue("operation_unavailable"));
        }
        controller.recordReview("Unavailable independent validation cannot authorize this advice.");
        QCOMPARE(operatorRecords.size(), 0); const auto session = controller.sessionId(); QVERIFY(!session.isEmpty());
        controller.finishSession(); controller.openHistory(session);
        QTRY_VERIFY_WITH_TIMEOUT(loaded.size() == 1 || !storageFailures.isEmpty(), 15000);
        QVERIFY(storageFailures.isEmpty()); QCOMPARE(loaded.size(), 1); QVERIFY(controller.historical()); QVERIFY(!controller.canReview());
        const auto archive = loaded.first().first().toJsonObject(); const auto analyses = archive.value("analyses").toArray();
        QCOMPARE(analyses.size(), 4); QVERIFY(archive.value("historical").toBool()); QVERIFY(!archive.value("actionable").toBool());
        QVERIFY(archive.value("operator_records").toArray().isEmpty());
        for (const auto& value : analyses) {
            const auto row = value.toObject(), input = row.value("request").toObject(), response = row.value("response").toObject();
            QCOMPARE(response.value("request_id"), input.value("request_id"));
            QCOMPARE(response.value("capture_id"), archive.value("capture_manifest").toObject().value("capture_id"));
        }
        QCOMPARE(model.requests, 0); QCOMPARE(inputs.currentHashes(), inputs.hashes);
    }
};
QTEST_GUILESS_MAIN(EogProviderIntegrationTests)
#include "eog_provider_integration_tests.moc"
