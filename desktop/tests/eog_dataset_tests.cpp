#include "eog/EogDataset.hpp"
#include "eog/EogDatasetController.hpp"
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTextStream>
#include <QtTest>
#include <cmath>
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
        bad = telemetry(); bad.replace(bad.indexOf("tank-Z"), 1, QByteArray::fromHex("ff")); QTest::newRow("invalid-utf8") << bad;
        bad = telemetry(); bad.chop(1); bad += QByteArray::fromHex("e2"); QTest::newRow("truncated-utf8-at-eof") << bad;
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
    void staleStateRetainsRawEvidenceAndSeparatesPumpMeaning() {
        QTemporaryDir directory; fixture(directory.path(), "tank-Z", 3);
        auto text = telemetry("tank-Z", 3);
        for (const auto& line : text.split('\n')) if (line.contains("\"pump-Z\",status,") && !line.endsWith(",status,0"))
            text.replace(line + '\n', "");
        QVERIFY(write(directory.filePath("telemetry.csv"), text));
        EogDatasetController controller; QSignalSpy complete(&controller, &EogDatasetController::completed);
        controller.loadFolder(QUrl::fromLocalFile(directory.path())); QTRY_COMPARE_WITH_TIMEOUT(complete.size(), 1, 10000);
        controller.selectAsset("pump-Z");
        auto observed = controller.selectedValues();
        QVERIFY(observed.value("has_sample").toBool()); QVERIFY(!observed.value("available").toBool());
        QVERIFY(!observed.value("missing").toBool()); QCOMPARE(observed.value("value").toDouble(), 0.0);
        QCOMPARE(observed.value("status_label").toString(), "stopped");
        QCOMPARE(observed.value("freshness_label").toString(), "stale");
        QCOMPARE(observed.value("event_unix").toLongLong(), firstTime); QCOMPARE(observed.value("age_s").toLongLong(), 600);
        controller.setCursorUnix(firstTime + 300); QVERIFY(controller.selectedValues().value("available").toBool());
        controller.setCursorUnix(firstTime + 301); QVERIFY(!controller.selectedValues().value("available").toBool());
        QCOMPARE(controller.selectedValues().value("status_label").toString(), "stopped");
        controller.selectAsset("disposal-Z"); observed = controller.selectedValues();
        QVERIFY(!observed.value("has_sample").toBool()); QVERIFY(observed.value("missing").toBool());
        QCOMPARE(observed.value("freshness_label").toString(), "missing"); QVERIFY(!observed.contains("value"));
    }
    void tankGuidesUseCustomDefaultsAndNeverChangeRawEvidence() {
        QTemporaryDir directory; const auto tankId = QString::fromUtf8("tank,renamed-Δ"); fixture(directory.path(), tankId);
        auto document = network(tankId);
        document["defaults"] = QJsonObject{{"tank_limits_pct", QJsonObject{{"warning", 61.5}, {"shutdown", 87.25}}}};
        QVERIFY(write(directory.filePath("network.json"), QJsonDocument(document).toJson()));
        EogDatasetController controller; QVERIFY(controller.selectedThresholds().isEmpty());
        QSignalSpy complete(&controller, &EogDatasetController::completed);
        controller.loadFolder(QUrl::fromLocalFile(directory.path())); QTRY_COMPARE_WITH_TIMEOUT(complete.size(), 1, 10000);
        QCOMPARE(controller.assetId(), tankId); const auto before = controller.selectedValues();
        const auto identity = controller.datasetIdentity(); const auto guides = controller.selectedThresholds();
        QCOMPARE(guides.size(), 3); QCOMPARE(controller.property("selectedThresholds").toList(), guides);
        QCOMPARE(guides[0].toMap().value("kind").toString(), "warning");
        QCOMPARE(guides[0].toMap().value("value").toDouble(), 61.5);
        QCOMPARE(guides[1].toMap().value("kind").toString(), "shutdown");
        QCOMPARE(guides[1].toMap().value("value").toDouble(), 87.25);
        QCOMPARE(guides[2].toMap().value("kind").toString(), "capacity");
        QCOMPARE(guides[2].toMap().value("value").toDouble(), 100.0);
        for (const auto& guide : guides) {
            QCOMPARE(guide.toMap().value("unit").toString(), "%");
            QVERIFY(!guide.toMap().value("label").toString().isEmpty());
        }
        QCOMPARE(controller.selectedValues(), before); QCOMPARE(controller.datasetIdentity(), identity);
        QCOMPARE(before.value("value").toDouble(), 104.0);
        controller.setCursorUnix(firstTime); QCOMPARE(controller.selectedValues().value("value").toDouble(), -2.0);
        QCOMPARE(controller.selectedThresholds(), guides);
        controller.selectAsset("pump-Z"); QVERIFY(controller.selectedThresholds().isEmpty());
        controller.selectMetric("flow_bpd"); QVERIFY(controller.selectedThresholds().isEmpty());
        controller.selectAsset("well-Z"); QVERIFY(controller.selectedThresholds().isEmpty());
        controller.selectAsset("disposal-Z"); QVERIFY(controller.selectedThresholds().isEmpty());
        controller.selectAsset(tankId); QCOMPARE(controller.selectedThresholds(), guides);
    }
    void overviewUsesAllInputSystemsAndKeepsTravelTimesAuthoritative() {
        QTemporaryDir directory; fixture(directory.path());
        auto n = network(); auto systems = n.value("systems").toArray(); auto second = systems.first().toObject();
        second["id"] = "other:system"; second["truck_yard"] = QJsonArray{31.5, -102.2};
        auto disposal = second.value("disposal_site").toObject(); disposal["id"] = "other:disposal";
        disposal["lat"] = 31.7; second["disposal_site"] = disposal;
        auto facility = second.value("facilities").toArray().first().toObject(); facility["id"] = "other:facility";
        facility["lat"] = 31.6;
        auto wells = facility.value("wells").toArray(); auto well = wells.first().toObject(); well["id"] = "other:well";
        facility["wells"] = QJsonArray{well};
        for (const auto& kind : {QStringLiteral("tank"), QStringLiteral("pump"), QStringLiteral("pipeline")}) {
            auto asset = facility.value(kind).toObject(); asset["id"] = "other:" + kind; facility[kind] = asset;
        }
        second["facilities"] = QJsonArray{facility}; systems.append(second); n["systems"] = systems;
        auto ops = operations(); auto operatingSystems = ops.value("systems").toObject();
        operatingSystems["other:system"] = QJsonObject{{"emergency_fleet_size", 1},
            {"facilities", QJsonObject{{"other:facility", QJsonObject{{"response_min", 17}, {"cycle_min", 28}}}}}};
        ops["systems"] = operatingSystems;
        QVERIFY(write(directory.filePath("network.json"), QJsonDocument(n).toJson()));
        QVERIFY(write(directory.filePath("ops.json"), QJsonDocument(ops).toJson()));
        EogDatasetController controller; QSignalSpy complete(&controller, &EogDatasetController::completed);
        controller.loadFolder(QUrl::fromLocalFile(directory.path())); QTRY_COMPARE_WITH_TIMEOUT(complete.size(), 1, 10000);
        const auto identity = controller.datasetIdentity(); const auto originalNetwork = controller.capturedDataset()->network;
        QCOMPARE(controller.sceneMode(), "overview"); QCOMPARE(controller.sceneNodes().size(), 6);
        QCOMPARE(controller.sceneRoutes().size(), 4); QMap<QString, QVariantMap> nodes;
        for (const auto& entry : controller.sceneNodes()) {
            const auto row = entry.toMap(); nodes.insert(row.value("id").toString(), row);
            QVERIFY(std::isfinite(row.value("x").toDouble())); QVERIFY(std::isfinite(row.value("z").toDouble()));
            QVERIFY(row.value("display_only").toBool());
        }
        bool checkedTravel = false;
        for (const auto& entry : controller.sceneRoutes()) {
            const auto row = entry.toMap(); QVERIFY(nodes.contains(row.value("source").toString()));
            QVERIFY(nodes.contains(row.value("target").toString())); QVERIFY(!row.value("flow_available").toBool());
            if (row.value("route_kind") == "truck_access" && row.value("facility_id") == "other:facility") {
                QCOMPARE(row.value("response_min").toDouble(), 17.0); checkedTravel = true;
            }
        }
        QVERIFY(checkedTravel); QVERIFY(controller.sceneBounds().value("radius").toDouble() > 0);
        controller.pickSceneNode("other:disposal"); QCOMPARE(controller.sceneMode(), "overview");
        QCOMPARE(controller.systemId(), "other:system"); QCOMPARE(controller.facilityId(), "other:facility");
        QCOMPARE(controller.assetId(), "other:disposal");
        controller.pickSceneNode("other:facility"); QCOMPARE(controller.sceneMode(), "facility");
        QCOMPARE(controller.systemId(), "other:system"); QCOMPARE(controller.facilityId(), "other:facility");
        QCOMPARE(controller.assetId(), "other:tank"); QCOMPARE(controller.sceneNodes(), controller.assets());
        controller.showOverview(); QCOMPARE(controller.sceneMode(), "overview");
        const auto selected = controller.assetId();
        for (const auto& row : nodes) if (row.value("kind") == "truck_yard") controller.pickSceneNode(row.value("id").toString());
        QCOMPARE(controller.assetId(), selected);
        controller.selectSystem("CUSTOM-system"); QCOMPARE(controller.sceneMode(), "overview");
        controller.showFacility("facility-Z"); QCOMPARE(controller.sceneMode(), "facility");
        QCOMPARE(controller.datasetIdentity(), identity); QCOMPARE(controller.capturedDataset()->network, originalNetwork);
    }
    void decimationPreservesExtremaAndOnlyBreaksAtRealGaps() {
        QTemporaryDir directory; fixture(directory.path(), "tank-Z", 12000);
        auto text = telemetry("tank-Z", 12000);
        const auto tankLine = [](int interval, const QByteArray& value) {
            const auto timestamp = firstTime + interval * 300;
            return QDateTime::fromSecsSinceEpoch(timestamp, Qt::UTC).toString(Qt::ISODate).toUtf8() + ',' +
                QByteArray::number(timestamp) + ",\"tank-Z\",level_pct," + value + '\n';
        };
        text.replace(tankLine(5001, "104"), tankLine(5001, "1500"));
        text.replace(tankLine(8001, "104"), tankLine(8001, "-25"));
        QVERIFY(write(directory.filePath("telemetry.csv"), text));
        EogDatasetController continuous; QSignalSpy complete(&continuous, &EogDatasetController::completed);
        continuous.loadFolder(QUrl::fromLocalFile(directory.path())); QTRY_COMPARE_WITH_TIMEOUT(complete.size(), 1, 15000);
        const auto points = continuous.selectedSeries(); QVERIFY(points.size() <= 8192); QVERIFY(points.size() < 12000);
        QCOMPARE(points.first().toMap().value("event_time_s").toLongLong(), firstTime);
        QCOMPARE(points.last().toMap().value("event_time_s").toLongLong(), firstTime + 11999 * 300);
        bool peak = false, trough = false, decimated = false;
        for (const auto& entry : points) {
            const auto point = entry.toMap(); QVERIFY(!point.value("break_before").toBool());
            peak |= point.value("value").toDouble() == 1500; trough |= point.value("value").toDouble() == -25;
            decimated |= point.value("omitted_samples_before").toInt() > 0;
        }
        QVERIFY(peak); QVERIFY(trough); QVERIFY(decimated);
        text.replace(tankLine(6002, "104"), ""); QVERIFY(write(directory.filePath("telemetry.csv"), text));
        EogDatasetController gapped; QSignalSpy gapComplete(&gapped, &EogDatasetController::completed);
        gapped.loadFolder(QUrl::fromLocalFile(directory.path())); QTRY_COMPARE_WITH_TIMEOUT(gapComplete.size(), 1, 15000);
        const auto gapPoints = gapped.selectedSeries(); QVERIFY(gapPoints.size() <= 8192); int breaks = 0;
        qint64 previousTime = 0;
        for (const auto& entry : gapPoints) {
            const auto point = entry.toMap(); const auto time = point.value("event_time_s").toLongLong();
            QVERIFY(time > previousTime); previousTime = time;
            if (point.value("break_before").toBool()) { ++breaks; QVERIFY(time > firstTime + 6002 * 300); }
        }
        QCOMPARE(breaks, 1);
        gapped.setCursorUnix(firstTime + 5000 * 300);
        for (const auto& entry : gapped.selectedSeries()) QVERIFY(entry.toMap().value("event_time_s").toLongLong() <= firstTime + 5000 * 300);
        const auto cutoff = gapped.cursorUnix(); gapped.setCursorUnix(gapped.nowUnix() + 1); QCOMPARE(gapped.cursorUnix(), cutoff);
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
                QTextStream(stdout) << "Official input check PASS: " << expected.folder << " rows=" << data->row_count
                    << " systems=" << data->systems.size() << " facilities=" << data->facilities.size()
                    << " series=" << data->series.size() << " first=" << data->first_unix << " last=" << data->now_unix
                    << " identity=" << data->identity << Qt::endl;
            } catch (const std::exception& error) { QTextStream(stderr) << "Official input check FAILED: " << error.what() << Qt::endl; return 2; }
        }
    } else QTextStream(stdout) << "Official input check NOT RUN: BREAKWATER_EOG_OFFICIAL_ROOT not supplied." << Qt::endl;
    EogDatasetTests tests; return QTest::qExec(&tests, argc, argv);
}
#include "eog_dataset_tests.moc"
