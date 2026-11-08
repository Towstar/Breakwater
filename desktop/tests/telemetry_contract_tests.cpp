#include "telemetry/TelemetryContracts.hpp"
#include "ProjectDocument.hpp"
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QtTest>
#include <limits>
#include <stdexcept>

using namespace breakwater::desktop;
using namespace breakwater::desktop::telemetry;

namespace {
QJsonObject fixture(const char* name) {
    QFile file(QStringLiteral(BREAKWATER_SOURCE_DIR "/contracts/telemetry/fixtures/") + QString::fromLatin1(name));
    if (!file.open(QIODevice::ReadOnly)) throw std::runtime_error("Missing shared telemetry fixture.");
    return decode_document(file.readAll());
}
QJsonObject canonicalConfiguration() {
    return fixture("hash-vectors.json").value("vectors").toArray().first().toObject().value("configuration").toObject();
}
QJsonObject namedCase(const char* name) {
    for (const auto& value : fixture("validation-cases.json").value("cases").toArray())
        if (value.toObject().value("name") == QString::fromLatin1(name)) return value.toObject();
    throw std::runtime_error("Missing named telemetry regression fixture.");
}
QJsonObject fixtureContext(const QJsonObject& testCase) {
    auto context = fixture("validation-cases.json").value("contexts").toObject().value(testCase.value("context_id").toString()).toObject();
    const auto overlay = testCase.value("context").toObject(); for (auto it = overlay.begin(); it != overlay.end(); ++it) context[it.key()] = it.value();
    return context;
}
}

class TelemetryContractTests final : public QObject {
    Q_OBJECT
private slots:
    void sharedContractFixtures_data() {
        QTest::addColumn<QJsonObject>("testCase"); QTest::addColumn<QJsonObject>("context");
        const auto file = fixture("validation-cases.json"), contexts = file.value("contexts").toObject();
        for (const auto& value : file.value("cases").toArray()) {
            const auto testCase = value.toObject(); auto context = contexts.value(testCase.value("context_id").toString()).toObject();
            const auto overlay = testCase.value("context").toObject(); for (auto it = overlay.begin(); it != overlay.end(); ++it) context[it.key()] = it.value();
            QTest::newRow(testCase.value("name").toString().toUtf8().constData()) << testCase << context;
        }
    }
    void sharedContractFixtures() {
        QFETCH(QJsonObject, testCase); QFETCH(QJsonObject, context);
        const auto kind = testCase.value("kind").toString(); auto document = testCase.value("document").toObject();
        bool accepted = false;
        try {
            if (testCase.contains("raw_base64")) document = decode_document(QByteArray::fromBase64(testCase.value("raw_base64").toString().toLatin1()),
                kind == "stream_frame" || kind == "stream_command" ? 256 * 1024 : maximumDocumentBytes);
            validate_document(document, kind, context); accepted = true;
        } catch (const std::exception&) {}
        QCOMPARE(accepted, testCase.value("valid").toBool());
        if (accepted && testCase.contains("disposition")) QCOMPARE(frame_disposition(document, context), testCase.value("disposition").toString());
        if (accepted && testCase.contains("snapshot_disposition")) QCOMPARE(snapshot_disposition(document, context), testCase.value("snapshot_disposition").toString());
    }
    void sharedHashVectors_data() {
        QTest::addColumn<QJsonObject>("vector");
        for (const auto& value : fixture("hash-vectors.json").value("vectors").toArray()) {
            const auto item = value.toObject(); QTest::newRow(item.value("name").toString().toUtf8().constData()) << item;
        }
    }
    void sharedHashVectors() {
        QFETCH(QJsonObject, vector); const auto configuration = vector.value("configuration").toObject();
        QCOMPARE(QString::fromUtf8(sensor_semantic_bytes(configuration)), vector.value("semantic_utf8").toString());
        QCOMPARE(sensor_semantic_hash(configuration), vector.value("sha256").toString());
    }
    void metadataAndLayoutDoNotChangeSensorSemantics() {
        auto original = canonicalConfiguration(); const auto hashBefore = sensor_semantic_hash(original);
        original["configuration_id"] = "a-new-export-id"; original["revision"] = 200;
        auto sensors = original.value("sensors").toArray(); auto sensor = sensors.first().toObject(); sensor["label"] = QString::fromUtf8("Repositioned \xf0\x9f\x8c\x8a"); sensors[0] = sensor; original["sensors"] = sensors;
        QCOMPARE(sensor_semantic_hash(original), hashBefore);
        sensor["max_age_s"] = sensor.value("max_age_s").toDouble() + 1; sensors[0] = sensor; original["sensors"] = sensors;
        QVERIFY(sensor_semantic_hash(original) != hashBefore);
    }
    void duplicateKeyFastPathPreservesEscapedAndUnicodeAliases_data() {
        QTest::addColumn<QByteArray>("raw"); QTest::addColumn<bool>("valid");
        const auto pump = QByteArray::fromHex("e6b3b5");
        QTest::newRow("plain-unique") << QByteArray("{\"alpha\":1,\"beta\":2}") << true;
        QTest::newRow("plain-duplicate") << QByteArray("{\"alpha\":1,\"alpha\":2}") << false;
        QTest::newRow("escaped-unique") << QByteArray("{\"\\u0061lpha\":1,\"beta\":2}") << true;
        QTest::newRow("escaped-duplicate") << QByteArray("{\"alpha\":1,\"\\u0061lpha\":2}") << false;
        QTest::newRow("unicode-unique") << QByteArray("{\"") + pump + "\":1,\"other\":2}" << true;
        QTest::newRow("unicode-duplicate") << QByteArray("{\"") + pump + "\":1,\"" + pump + "\":2}" << false;
        QTest::newRow("unicode-escaped-alias") << QByteArray("{\"") + pump + "\":1,\"\\u6cf5\":2}" << false;
        QTest::newRow("escaped-solidus-alias") << QByteArray("{\"a/b\":1,\"a\\/b\":2}") << false;
        QTest::newRow("escaped-backslash-alias") << QByteArray("{\"a\\\\b\":1,\"a\\u005cb\":2}") << false;
        QTest::newRow("invalid-utf8-key") << QByteArray("{\"") + QByteArray::fromHex("ff") + "\":1}" << false;
        QTest::newRow("unpaired-surrogate-key") << QByteArray("{\"\\ud800\":1}") << false;
    }
    void duplicateKeyFastPathPreservesEscapedAndUnicodeAliases() {
        QFETCH(QByteArray, raw); QFETCH(bool, valid);
        bool accepted = false;
        try { (void)decode_document(raw); accepted = true; } catch (const std::invalid_argument&) {}
        QCOMPARE(accepted, valid);
    }
    void duplicateKeyFastPathDoesNotChangeDepthBoundary() {
        const auto atBound = QByteArray("{\"x\":") + QByteArray(63, '[') + '0' + QByteArray(63, ']') + '}';
        QVERIFY(!decode_document(atBound).isEmpty());
        const auto beyond = QByteArray("{\"x\":") + QByteArray(64, '[') + '0' + QByteArray(64, ']') + '}';
        QVERIFY_THROWS_EXCEPTION(std::invalid_argument, decode_document(beyond));
    }
    void malformedBytesCannotBeNormalizedIntoEvidence() {
        for (const auto& bytes : {QByteArray("{\"a\":1,\"\\u0061\":2}"), QByteArray("{\"n\":NaN}"), QByteArray("{\"n\":1e999}"), QByteArray("{\"s\":\"\\ud800\"}"), QByteArray("{\"\\ud800\":1}"), QByteArray("{\"s\":\"\xff\"}"), QByteArray("\xef\xbb\xbf{}")})
            QVERIFY_THROWS_EXCEPTION(std::invalid_argument, decode_document(bytes));
        QVERIFY_THROWS_EXCEPTION(std::invalid_argument, decode_document(QByteArray(256 * 1024 + 1, ' '), 256 * 1024));
        QJsonObject frame;
        for (const auto& entry : fixture("validation-cases.json").value("cases").toArray())
            if (entry.toObject().value("name") == "canonical-readings") frame = entry.toObject().value("document").toObject();
        QVERIFY(!frame.isEmpty()); auto paddedFrame = QJsonDocument(frame).toJson(QJsonDocument::Compact);
        paddedFrame.prepend(QByteArray(256 * 1024 + 1 - paddedFrame.size(), ' '));
        QVERIFY_THROWS_EXCEPTION(std::invalid_argument, decode_document(paddedFrame));
        QByteArray nested("{\"x\":"); nested += QByteArray(65, '['); nested += '0'; nested += QByteArray(65, ']'); nested += '}';
        QVERIFY_THROWS_EXCEPTION(std::invalid_argument, decode_document(nested));
    }
    void projectV2CannotBeSilentlyDowngradedByTheLegacyParser() {
        const auto legacy = project_json(example_project());
        QCOMPARE(project_json(parse_project(legacy)), legacy);
        const QJsonObject wrapper{{"version", 2}, {"project", legacy}, {"sensor_configuration", canonicalConfiguration()}, {"sensor_layout", QJsonObject{}}};
        QVERIFY_THROWS_EXCEPTION(std::invalid_argument, parse_project(wrapper));
    }
    void importedEvidenceHasNoControlAuthority() {
        const auto shared = fixture("validation-cases.json"), contexts = shared.value("contexts").toObject();
        for (const auto& value : shared.value("cases").toArray()) {
            const auto item = value.toObject(); if (!item.value("valid").toBool()) continue;
            const auto kind = item.value("kind").toString(); if (kind != "observed_snapshot" && kind != "detection_event") continue;
            auto context = contexts.value(item.value("context_id").toString()).toObject(); const auto overlay = item.value("context").toObject();
            for (auto it = overlay.begin(); it != overlay.end(); ++it) context[it.key()] = it.value();
            auto document = item.value("document").toObject(); validate_document(document, kind, context); document["actionable"] = true;
            QVERIFY_THROWS_EXCEPTION(std::invalid_argument, validate_document(document, kind, context));
        }
    }
    void liveClaimsRequireTheReceiverClockAndDoNotMutateEvidence() {
        const auto shared = fixture("validation-cases.json"); QJsonObject document;
        for (const auto& item : shared.value("cases").toArray())
            if (item.toObject().value("name") == "canonical-snapshot") document = item.toObject().value("document").toObject();
        QVERIFY(!document.isEmpty()); const auto original = document;
        QCOMPARE(snapshot_disposition(document), QString("unavailable"));
        auto context = shared.value("contexts").toObject().value("canonical").toObject();
        QCOMPARE(snapshot_disposition(document, context), QString("unavailable"));
        context["clock"] = QJsonObject{{"stream_id", document.value("stream_id")}, {"epoch_id", document.value("epoch_id")},
            {"now_event_time_s", document.value("captured_event_time_s")}, {"watermark_event_time_s", document.value("captured_event_time_s")}, {"mode", "replay"}};
        QCOMPARE(snapshot_disposition(document, context), QString("historical"));
        QCOMPARE(document, original);
        document["mode"] = "historical";
        QCOMPARE(snapshot_disposition(document), QString("historical"));
    }
    void directMeasurementsCannotMoveEquipmentOrChangeFeedback() {
        const auto testCase = namedCase("canonical-snapshot"), original = testCase.value("document").toObject(), context = fixtureContext(testCase);
        (void)parse_observed_snapshot(original, context);
        auto changed = original; auto states = changed.value("derived_state").toArray(); auto inventory = states[0].toObject();
        inventory["attachment_id"] = "tank_b"; states[0] = inventory; changed["derived_state"] = states;
        QVERIFY_THROWS_EXCEPTION(std::invalid_argument, parse_observed_snapshot(changed, context));
        changed = original; states = changed.value("derived_state").toArray(); inventory = states[0].toObject();
        inventory["source_sensor_ids"] = QJsonArray{"main-pump-feedback"}; states[0] = inventory; changed["derived_state"] = states;
        QVERIFY_THROWS_EXCEPTION(std::invalid_argument, parse_observed_snapshot(changed, context));
        changed = original; states = changed.value("derived_state").toArray(); auto feedback = states[1].toObject();
        feedback["value"] = "running"; states[1] = feedback; changed["derived_state"] = states;
        QVERIFY_THROWS_EXCEPTION(std::invalid_argument, parse_observed_snapshot(changed, context));
    }
    void actualAcquisitionWindowsAreRetainedAndRequired() {
        const auto testCase = namedCase("canonical-snapshot"), context = fixtureContext(testCase); auto document = testCase.value("document").toObject();
        auto samples = document.value("samples").toArray(); auto sample = samples[0].toObject(); const auto time = sample.value("event_time_s").toDouble();
        QVERIFY(time >= 0.5); sample["acquisition_start_s"] = time - 0.5; sample["acquisition_end_s"] = time - 0.1;
        samples[0] = sample; document["samples"] = samples;
        const auto decoded = parse_observed_snapshot(document, context);
        QCOMPARE(decoded.samples[0].acquisition_start_s, time - 0.5); QCOMPARE(decoded.samples[0].acquisition_end_s, time - 0.1);
        sample.remove("acquisition_start_s"); samples[0] = sample; document["samples"] = samples;
        QVERIFY_THROWS_EXCEPTION(std::invalid_argument, parse_observed_snapshot(document, context));
    }
    void futureCapturesAndMalformedReceiverClocksCannotBecomeAvailable() {
        const auto testCase = namedCase("canonical-snapshot"), document = testCase.value("document").toObject(); auto context = fixtureContext(testCase);
        const auto time = document.value("captured_event_time_s").toDouble();
        QJsonObject clock{{"stream_id", document.value("stream_id")}, {"epoch_id", document.value("epoch_id")},
            {"now_event_time_s", time - 1}, {"watermark_event_time_s", time - 1}, {"mode", "synthetic_live"}};
        context["clock"] = clock; QCOMPARE(snapshot_disposition(document, context), QString("unavailable"));
        for (const auto& invalid : {QJsonValue(true), QJsonValue(-1.0)}) {
            clock["now_event_time_s"] = invalid; context["clock"] = clock;
            QVERIFY_THROWS_EXCEPTION(std::invalid_argument, snapshot_disposition(document, context));
        }
    }
    void contradictoryContextAndNormalizedMidnightAreRejected() {
        const auto testCase = namedCase("canonical-snapshot"), document = testCase.value("document").toObject(); const auto base = fixtureContext(testCase);
        auto context = base; auto project = namedCase("project-v2-wrapper").value("document").toObject().value("project").toObject();
        auto network = project.value("network").toObject(); auto assets = network.value("assets").toArray(); auto tank = assets[0].toObject();
        tank["capacity_m3"] = tank.value("capacity_m3").toDouble() + 1; assets[0] = tank; network["assets"] = assets; project["network"] = network; context["project"] = project;
        QVERIFY_THROWS_EXCEPTION(std::invalid_argument, parse_observed_snapshot(document, context));
        auto changed = document; auto samples = changed.value("samples").toArray(); auto sample = samples[0].toObject();
        sample["received_utc"] = "2026-11-08T24:00:00Z"; samples[0] = sample; changed["samples"] = samples;
        QVERIFY_THROWS_EXCEPTION(std::invalid_argument, parse_observed_snapshot(changed, base));
    }
};

QTEST_GUILESS_MAIN(TelemetryContractTests)
#include "telemetry_contract_tests.moc"
