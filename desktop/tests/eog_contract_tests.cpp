#include "eog/EogContracts.hpp"
#include "telemetry/TelemetryContracts.hpp"
#include <QCoreApplication>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QtTest>
#include <cstdio>
#include <iostream>
#include <stdexcept>

namespace eog = breakwater::desktop::eog;
QJsonObject load(const QString& relative) {
    QFile file(QStringLiteral(BREAKWATER_SOURCE_DIR "/contracts/eog/") + relative);
    if (!file.open(QIODevice::ReadOnly)) throw std::runtime_error("Missing EOG fixture.");
    return breakwater::desktop::telemetry::decode_document(file.readAll(), 8 * 1024 * 1024);
}
bool accepts(const QJsonObject& value, const QString& kind = {}, const QJsonObject& context = {}) {
    try { eog::validate_document(value, kind, context); return true; }
    catch (const std::invalid_argument&) { return false; }
}
class EogContractTests final : public QObject {
    Q_OBJECT
private slots:
    void sharedCorpus_data() {
        QTest::addColumn<QJsonObject>("document");
        QTest::addColumn<QJsonObject>("context");
        QTest::addColumn<QString>("kind");
        QTest::addColumn<bool>("valid");
        const auto corpus = load("fixtures/validation-cases.json");
        const auto contexts = corpus.value("contexts").toObject();
        for (const auto& value : corpus.value("cases").toArray()) {
            const auto item = value.toObject();
            QTest::newRow(item.value("name").toString().toUtf8().constData())
                << item.value("document").toObject()
                << contexts.value(item.value("context_id").toString()).toObject()
                << item.value("kind").toString() << item.value("valid").toBool();
        }
    }
    void sharedCorpus() {
        QFETCH(QJsonObject, document); QFETCH(QJsonObject, context);
        QFETCH(QString, kind); QFETCH(bool, valid);
        QCOMPARE(accepts(document, kind, context), valid);
    }
    void canonicalHashes() {
        const auto vectors = load("fixtures/hash-vectors.json");
        for (const auto& value : vectors.value("dataset_vectors").toArray()) {
            const auto row = value.toObject();
            QCOMPARE(eog::dataset_identity(row.value("source_hashes").toObject()), row.value("sha256").toString());
        }
        for (const auto& value : vectors.value("capture_vectors").toArray()) {
            const auto row = value.toObject(); auto document = row.value("manifest").toObject();
            QCOMPARE(eog::capture_identity(document), row.value("sha256").toString());
            document.remove("capture_id");
            QCOMPARE(QString::fromUtf8(eog::semantic_bytes(document)), row.value("canonical_utf8").toString());
        }
        for (const auto& value : vectors.value("plan_vectors").toArray()) {
            const auto row = value.toObject(); auto document = row.value("plan").toObject();
            QCOMPARE(eog::plan_identity(document), row.value("sha256").toString());
            document.remove("plan_hash");
            QCOMPARE(QString::fromUtf8(eog::semantic_bytes(document)), row.value("canonical_utf8").toString());
        }
        QCOMPARE(eog::semantic_bytes(QJsonObject{{"a", 1.0}, {"b", -0.0}}),
                 eog::semantic_bytes(QJsonObject{{"b", 0}, {"a", 1}}));
    }
    void rawInputRejectsDuplicateKeysNonfiniteAndBadUnicode() {
        const QList<QByteArray> invalid{
            "{\"a\":1,\"a\":2}", "{\"a\":1,\"\\u0061\":2}", "{\"v\":NaN}", "{\"v\":Infinity}",
            "{\"v\":1e9999}", QByteArray("{\"v\":\"") + QByteArray::fromHex("ff") + "\"}", "{\"v\":\"\\ud800\"}",
            "[]", "{}{}", QByteArray("{\"v\":") + QByteArray(65, '[') + '0' + QByteArray(65, ']') + '}'};
        for (const auto& bytes : invalid)
            QVERIFY_THROWS_EXCEPTION(std::invalid_argument, eog::decode_document(bytes));
        auto bytes = QJsonDocument(load("examples/detect-request.json")).toJson(QJsonDocument::Compact);
        bytes += QByteArray(static_cast<qsizetype>(breakwater::eog::maximum_request_bytes) - bytes.size(), ' ');
        QVERIFY(!eog::decode_document(bytes).isEmpty());
        bytes += ' ';
        QVERIFY_THROWS_EXCEPTION(std::invalid_argument, eog::decode_document(bytes));
    }
    void sameIdNumericalEditRetiresPlanValidation() {
        auto request = load("examples/validate-plan-request.json");
        const auto response = load("examples/validate-plan-response.json");
        QVERIFY(accepts(response, {}, {{"request", request}}));
        auto args = request.value("arguments").toObject(); auto plan = args.value("plan").toObject();
        auto jobs = plan.value("truck_jobs").toArray(); auto job = jobs[0].toObject();
        auto trips = job.value("trips").toArray(); auto trip = trips[0].toObject();
        trip["volume_bbl"] = trip.value("volume_bbl").toDouble() + 1;
        trips[0] = trip; job["trips"] = trips; jobs[0] = job; plan["truck_jobs"] = jobs;
        QVERIFY(plan.value("plan_hash") != eog::plan_identity(plan));
        plan["plan_hash"] = eog::plan_identity(plan); args["plan"] = plan; request["arguments"] = args;
        QVERIFY(accepts(request));
        QVERIFY(!accepts(response, {}, {{"request", request}}));
    }
    void selectionMustBeCheapestRetainedFeasible() {
        auto response = load("examples/search-plans-response.json"); auto result = response.value("result").toObject();
        auto evaluations = result.value("evaluations").toArray(); auto cheaper = evaluations[0].toObject();
        auto plan = cheaper.value("plan").toObject(); plan["plan_id"] = "cheaper"; plan["plan_hash"] = eog::plan_identity(plan);
        cheaper["plan"] = plan; cheaper["cost"] = QJsonObject{{"truck_cost", 50}, {"lost_revenue", 0}, {"total", 50}};
        evaluations.append(cheaper); result["evaluations"] = evaluations;
        auto search = result.value("search").toObject(); search["evaluated_candidates"] = 2; result["search"] = search;
        response["result"] = result; QVERIFY(!accepts(response));
        result["selected_plan_id"] = "cheaper"; response["result"] = result; QVERIFY(accepts(response));
    }
    void noImportedActionAuthorityOrStateMutation() {
        auto response = load("examples/detect-response.json"); const auto before = response;
        QVERIFY(accepts(response)); QCOMPARE(response, before);
        response["actionable"] = true; QVERIFY(!accepts(response));
    }
};

int checker() {
    QFile output;
    if (!output.open(stdout, QIODevice::WriteOnly)) return 2;
    constexpr qsizetype limit = 8 * 1024 * 1024;
    for (;;) {
        QByteArray line; char character = 0; bool any = false;
        while (std::cin.get(character)) {
            any = true; if (character == '\n') break;
            if (line.size() <= limit) line.append(character);
        }
        if (!any) break;
        QJsonObject result{{"valid", false}};
        if (line.size() <= limit) try {
            const auto request = breakwater::desktop::telemetry::decode_document(line, limit);
            const auto kind = request.value("kind").toString();
            if (kind == "dataset_identity") {
                result = {{"valid", true}, {"sha256", eog::dataset_identity(request.value("source_hashes").toObject())}};
            } else if (kind == "plan_identity") {
                const auto plan = request.value("plan").toObject();
                auto projection = plan; projection.remove("plan_hash");
                result = {{"valid", true}, {"sha256", eog::plan_identity(plan)},
                    {"canonical_utf8", QString::fromUtf8(eog::semantic_bytes(projection))}};
            } else {
                QJsonObject document;
                if (request.contains("raw_base64")) {
                    const auto encoded = request.value("raw_base64").toString().toLatin1();
                    const auto bytes = QByteArray::fromBase64(encoded);
                    if (bytes.toBase64() != encoded) throw std::invalid_argument("Invalid base64.");
                    document = eog::decode_document(bytes);
                } else {
                    if (!request.value("document").isObject()) throw std::invalid_argument("Missing document.");
                    document = request.value("document").toObject();
                }
                eog::validate_document(document, kind, request.value("context").toObject());
                result["valid"] = true;
                if (kind == "capture_manifest") {
                    auto projection = document; projection.remove("capture_id");
                    result["sha256"] = eog::capture_identity(document);
                    result["canonical_utf8"] = QString::fromUtf8(eog::semantic_bytes(projection));
                }
            }
        } catch (const std::exception&) { result = {{"valid", false}}; }
        const auto bytes = QJsonDocument(result).toJson(QJsonDocument::Compact) + '\n';
        if (output.write(bytes) != bytes.size() || !output.flush()) return 3;
    }
    return 0;
}
int main(int argc, char** argv) {
    QCoreApplication application(argc, argv);
    if (application.arguments().contains("--contract-check")) return checker();
    EogContractTests test;
    return QTest::qExec(&test, argc, argv);
}
#include "eog_contract_tests.moc"
