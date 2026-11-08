#include "eog/EogDataset.hpp"
#include "eog/EogDatasetController.hpp"
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QtTest>
#include <stdexcept>

using namespace breakwater::desktop::eog;
namespace {
const qint64 firstTime = 1791158400;
bool write(const QString& path, const QByteArray& bytes) {
    QFile file(path); return file.open(QIODevice::WriteOnly | QIODevice::Truncate) && file.write(bytes) == bytes.size();
}
QJsonObject network(const QString& tankId = "tank-Z") {
    return {{"meta", QJsonObject{{"timezone", "UTC"}, {"units", QJsonObject{{"volume", "bbl"}, {"rate", "bpd"}, {"gas_rate", "mcfd"}, {"pressure", "psi"}}}}},
        {"defaults", QJsonObject{{"tank_limits_pct", QJsonObject{{"warning", 83}, {"shutdown", 94}}}}},
        {"systems", QJsonArray{QJsonObject{{"id", "CUSTOM-system"}, {"center", QJsonArray{31.0, -102.0}}, {"truck_yard", QJsonArray{31.0, -102.0}},
            {"disposal_site", QJsonObject{{"id", "disposal-Z"}, {"lat", 31.0}, {"lon", -102.0}, {"pressure_psi", 1123}}},
            {"facilities", QJsonArray{QJsonObject{{"id", "facility-Z"}, {"lat", 31.2}, {"lon", -102.1},
                {"wells", QJsonArray{QJsonObject{{"id", "well-Z"}, {"baseline_water_bpd", 17}, {"oil_bpd", 20}, {"gas_mcfd", 30}}}},
                {"tank", QJsonObject{{"id", tankId}, {"capacity_bbl", 725}}},
                {"pump", QJsonObject{{"id", "pump-Z"}, {"max_flow_bpd", 40}, {"start_level_pct", 71}, {"stop_level_pct", 31}}},
                {"pipeline", QJsonObject{{"id", "pipe-Z"}, {"max_bpd", 45}, {"max_pressure_psi", 1600}}}}}}}}}};
}
QJsonObject operations() {
    return {{"dataset_id", "toy-contract-only"}, {"prices", QJsonObject{{"oil_per_bbl", 61}, {"gas_per_mcf", 2.5}}},
        {"expected_repair_hours", QJsonObject{{"pump_failure", 7}}}, {"emergency_trucking", QJsonObject{{"truck_capacity_bbl", 137}, {"cost_per_trip", 919}}},
        {"systems", QJsonObject{{"CUSTOM-system", QJsonObject{{"emergency_fleet_size", 3},
            {"facilities", QJsonObject{{"facility-Z", QJsonObject{{"response_min", 84}, {"cycle_min", 219}}}}}}}}}};
}
QByteArray quoted(const QString& value) { auto s = value; s.replace('"', "\"\""); return '"' + s.toUtf8() + '"'; }
QByteArray telemetry(const QString& tank = "tank-Z", int intervals = 2) {
    QByteArray result("timestamp,ts_unix,asset_id,metric,value\n");
    for (int i = 0; i < intervals; ++i) {
        const auto t = firstTime + i * 300;
        const auto prefix = QDateTime::fromSecsSinceEpoch(t, Qt::UTC).toString(Qt::ISODate).toUtf8() + ',' + QByteArray::number(t) + ',';
        const auto add = [&](const QString& asset, const char* metric, double value) {
            result += prefix + quoted(asset) + ',' + metric + ',' + QByteArray::number(value, 'g', 17) + '\n';
        };
        add("facility-Z", "inflow_bpd", 17.5); add("well-Z", "water_bpd", 17.5); add("well-Z", "status", 1);
        add(tank, "level_pct", i == 0 ? -2.0 : 104.0);
        add("pump-Z", "status", i == 0 ? 0 : 2); add("pump-Z", "flow_bpd", 0); add("pump-Z", "discharge_psi", 1123);
        add("pipe-Z", "receive_flow_bpd", 0); add("pipe-Z", "pressure_psi", 1123);
    }
    return result;
}
void fixture(const QString& folder, const QString& tank = "tank-Z", int count = 2) {
    if (!write(QDir(folder).filePath("network.json"), QJsonDocument(network(tank)).toJson()) ||
        !write(QDir(folder).filePath("ops.json"), QJsonDocument(operations()).toJson()) ||
        !write(QDir(folder).filePath("telemetry.csv"), telemetry(tank, count))) throw std::runtime_error("Cannot write local fixture");
}
}
class EogDatasetTests final : public QObject {
    Q_OBJECT
private slots:
    void threeFilesPreserveRawUnitsAndIgnoreAnswerKey() {
        QTemporaryDir directory; QVERIFY(directory.isValid()); fixture(directory.path());
        QVERIFY(write(directory.filePath("answer_key.json"), "THIS IS NOT JSON AND MUST NEVER BE LOADED"));
        const auto data = load_dataset(directory.path());
        QCOMPARE(data->row_count, 18); QCOMPARE(data->first_unix, firstTime); QCOMPARE(data->now_unix, firstTime + 300);
        QCOMPARE(data->systems.first().id, "CUSTOM-system"); QCOMPARE(data->source_hashes.size(), 3); QVERIFY(data->warnings.isEmpty());
        const auto tank = data->series.value(series_key("tank-Z", "level_pct"));
        QCOMPARE(tank.unit, "%"); QCOMPARE(tank.samples[0].value, -2.0); QCOMPARE(tank.samples[1].value, 104.0);
        const auto identity = data->identity;
        QVERIFY(write(directory.filePath("answer_key.json"), "DIFFERENT POISON")); QCOMPARE(load_dataset(directory.path())->identity, identity);
        QCOMPARE(data->ops.value("emergency_trucking").toObject().value("cost_per_trip").toDouble(), 919.0);
    }
    void quotedRenamedIdsAndReorderedRowsWork() {
        QTemporaryDir directory; fixture(directory.path(), "tank,renamed\"west");
        auto lines = telemetry("tank,renamed\"west").split('\n'); std::swap(lines[1], lines[4]);
        QVERIFY(write(directory.filePath("telemetry.csv"), lines.join('\n')));
        const auto data = load_dataset(directory.path());
        QVERIFY(data->assets.contains("tank,renamed\"west"));
        QCOMPARE(data->series.value(series_key("tank,renamed\"west", "level_pct")).samples.size(), 2);
    }
    void missingReadingsAreNotFilledWithZero() {
        QTemporaryDir directory; fixture(directory.path()); auto text = telemetry();
        const auto line = text.split('\n')[4]; text.replace(line + '\n', "");
        QVERIFY(write(directory.filePath("telemetry.csv"), text));
        const auto data = load_dataset(directory.path()); QVERIFY(!data->warnings.isEmpty());
        QCOMPARE(data->series.value(series_key("tank-Z", "level_pct")).samples.size(), 1);
    }
    void malformedTelemetry_data() {
        QTest::addColumn<QByteArray>("replacement");
        QTest::newRow("duplicate") << telemetry() + telemetry().split('\n').value(10) + '\n';
        auto bad = telemetry(); bad.replace("pump-Z\",status,2", "pump-Z\",status,3"); QTest::newRow("bad-state") << bad;
        bad = telemetry(); bad.replace("pipe-Z\",pressure_psi,1123", "pipe-Z\",pressure_psi,nan"); QTest::newRow("nonfinite") << bad;
        bad = telemetry(); bad.replace("\"tank-Z\"", "\"unknown\""); QTest::newRow("unknown-asset") << bad;
        bad = telemetry(); bad.replace("level_pct", "height_m"); QTest::newRow("unknown-metric") << bad;
        bad = telemetry(); bad.replace("2026-10-05T00:00:00Z", "2026-10-05T01:00:00Z"); QTest::newRow("clock-mismatch") << bad;
        bad = telemetry(); bad.replace("2026-10-05T00:05:00Z,1791158700", "2026-10-05T00:04:00Z,1791158640"); QTest::newRow("not-five-minutes") << bad;
        QTest::newRow("no-rows") << QByteArray("timestamp,ts_unix,asset_id,metric,value\n");
        QTest::newRow("bad-quote") << QByteArray("timestamp,ts_unix,asset_id,metric,value\n\"unterminated\n");
        bad = telemetry(); bad[bad.indexOf("tank-Z")] = char(0xff); QTest::newRow("invalid-utf8") << bad;
    }
    void malformedTelemetry() {
        QFETCH(QByteArray, replacement); QTemporaryDir directory; fixture(directory.path());
        QVERIFY(write(directory.filePath("telemetry.csv"), replacement));
        QVERIFY_THROWS_EXCEPTION(std::invalid_argument, load_dataset(directory.path()));
    }
    void duplicateJsonAndMismatchedOpsFail() {
        QTemporaryDir directory; fixture(directory.path());
        auto bytes = QJsonDocument(network()).toJson(QJsonDocument::Compact); bytes.replace("\"timezone\":\"UTC\"", "\"timezone\":\"UTC\",\"timezone\":\"UTC\"");
        QVERIFY(write(directory.filePath("network.json"), bytes));
        QVERIFY_THROWS_EXCEPTION(std::invalid_argument, load_dataset(directory.path()));
        fixture(directory.path()); auto ops = operations(); ops["systems"] = QJsonObject{};
        QVERIFY(write(directory.filePath("ops.json"), QJsonDocument(ops).toJson()));
        QVERIFY_THROWS_EXCEPTION(std::invalid_argument, load_dataset(directory.path()));
    }
    void explicitCancellationHasNoPublishedData() {
        QTemporaryDir directory; fixture(directory.path());
        const auto token = std::make_shared<std::atomic_bool>(true);
        QVERIFY_THROWS_EXCEPTION(std::runtime_error, load_dataset(directory.path(), token));
    }
    void controllerIsAtomicAndCursorDoesNotLeakFuture() {
        QTemporaryDir good, bad; fixture(good.path()); EogDatasetController controller;
        QSignalSpy completed(&controller, &EogDatasetController::completed);
        controller.loadFolder(QUrl::fromLocalFile(good.path())); QTRY_COMPARE_WITH_TIMEOUT(completed.size(), 1, 10000);
        QVERIFY(controller.loaded()); const auto identity = controller.datasetIdentity();
        QCOMPARE(controller.selectedValues().value("value").toDouble(), 104.0);
        controller.setCursorUnix(double(firstTime));
        QCOMPARE(controller.selectedSeries().size(), 1); QCOMPARE(controller.selectedValues().value("value").toDouble(), -2.0);
        QVERIFY(controller.nowUnix() > controller.cursorUnix()); QCOMPARE(controller.unit(), "%");
        controller.selectAsset("pump-Z"); QCOMPARE(controller.selectedValues().value("status_label").toString(), "stopped");
        controller.setCursorUnix(double(firstTime + 300)); QCOMPARE(controller.selectedValues().value("status_label").toString(), "fault feedback");
        QCOMPARE(controller.opsSummary().value("fleet_size").toDouble(), 3.0);
        QVERIFY(!controller.assets().isEmpty()); QVERIFY(!controller.routes().isEmpty());
        QSignalSpy failed(&controller, &EogDatasetController::failed);
        controller.loadFolder(QUrl::fromLocalFile(bad.path())); QTRY_COMPARE_WITH_TIMEOUT(failed.size(), 1, 10000);
        QCOMPARE(controller.datasetIdentity(), identity); QVERIFY(!controller.error().isEmpty());
    }
    void cancellationAndNewerLoadWinWithoutUiBlocking() {
        QTemporaryDir large, small; fixture(large.path(), "tank-Z", 10000); fixture(small.path(), "new-tank");
        EogDatasetController controller; controller.loadFolder(QUrl::fromLocalFile(large.path())); QVERIFY(controller.busy());
        controller.cancel(); QVERIFY(!controller.busy()); QVERIFY(!controller.loaded());
        QSignalSpy complete(&controller, &EogDatasetController::completed);
        controller.loadFolder(QUrl::fromLocalFile(small.path())); QTRY_COMPARE_WITH_TIMEOUT(complete.size(), 1, 10000);
        QCOMPARE(controller.assetId(), "new-tank");
        QTest::qWait(100); QCOMPARE(controller.assetId(), "new-tank");
    }
};
int main(int argc, char** argv) {
    QCoreApplication app(argc, argv);
    // Explicit optional integration, separate from counted local contract cases.
    // CI without this public checkout reports NOT RUN, never an invented dataset pass.
    const auto official = qEnvironmentVariable("BREAKWATER_EOG_OFFICIAL_ROOT");
    if (!official.isEmpty()) {
        struct PublicFixture { QString folder; qsizetype rows; qint64 last; QString csv_sha256; };
        // CSV-only independent counts/hashes from official revision135ba4c. No event answers.
        const PublicFixture fixtures[] = {
            {"full_week", 393120, 1791762900, "4e5ffc0669ba3bc4fadfe50c0fcf2668bf6a380ca9d8a80a541f76a891dc013a"},
            {"mock_test_2026-10-10T1530", 317265, 1791646200, "e747ff3b81dfa4c9d4b03a6fc6e7b09f944db674a29a5cbd847d6d5f6df051f5"}
        };
        for (const auto& expected : fixtures) {
            try {
                const auto data = load_dataset(QDir(official).filePath("history/" + expected.folder));
                if (data->row_count != expected.rows || data->now_unix != expected.last || data->first_unix != firstTime ||
                    data->systems.size() != 5 || data->facilities.size() != 15 || data->series.size() != 195 ||
                    data->source_hashes.value("telemetry.csv").toString() != expected.csv_sha256 || !data->warnings.isEmpty())
                    throw std::runtime_error("Parsed public dataset differs from independent CSV-only oracle.");
                qInfo().noquote() << "Official input check PASS:" << expected.folder << "rows=" << data->row_count << "identity=" << data->identity;
            } catch (const std::exception& error) { qCritical() << "Official input check FAILED:" << error.what(); return 2; }
        }
    } else qInfo("Official input check NOT RUN: BREAKWATER_EOG_OFFICIAL_ROOT not supplied.");
    EogDatasetTests tests; return QTest::qExec(&tests, argc, argv);
}
#include "eog_dataset_tests.moc"
