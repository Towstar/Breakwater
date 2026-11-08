#include "eog/EogAnalysisController.hpp"
#include "eog/EogContracts.hpp"
#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTimer>
#include <QtTest>
#include <algorithm>
#include <functional>
#include <memory>
#include <stdexcept>

using namespace breakwater::desktop;
using namespace breakwater::desktop::eog;
namespace {
QString python() {
#ifdef BREAKWATER_TEST_PYTHON
    return QString::fromUtf8(BREAKWATER_TEST_PYTHON);
#else
    return qEnvironmentVariable("BREAKWATER_PYTHON");
#endif
}
QByteArray bytes(const QString& path) {
    QFile file(path); if (!file.open(QIODevice::ReadOnly)) throw std::runtime_error("Cannot read fixture");
    return file.readAll();
}
bool write(const QString& path, const QByteArray& content) {
    QFile file(path); return file.open(QIODevice::WriteOnly | QIODevice::Truncate) && file.write(content) == content.size();
}
QJsonObject parse(const QString& json) { return QJsonDocument::fromJson(json.toUtf8()).object(); }
QJsonObject tool(const QString& id, const QString& name, const QJsonObject& arguments = {}) {
    return {{"role", "assistant"}, {"content", QJsonValue::Null}, {"tool_calls", QJsonArray{
        QJsonObject{{"id", id}, {"type", "function"}, {"function", QJsonObject{{"name", name},
            {"arguments", QString::fromUtf8(QJsonDocument(arguments).toJson(QJsonDocument::Compact))}}}}}}};
}
QJsonObject final(const QString& plan = {}, const QString& validation = {}, const QString& event = {}) {
    return {{"role", "assistant"}, {"content", QString::fromUtf8(QJsonDocument(QJsonObject{
        {"summary", "Protocol-only model handoff; numerical qualification is not exercised."},
        {"selected_plan_id", plan.isEmpty() ? QJsonValue(QJsonValue::Null) : QJsonValue(plan)},
        {"validation_id", validation.isEmpty() ? QJsonValue(QJsonValue::Null) : QJsonValue(validation)},
        {"event_ids", event.isEmpty() ? QJsonArray{} : QJsonArray{event}}}).toJson(QJsonDocument::Compact))}};
}
QJsonObject resultFor(const QJsonArray& messages, const QString& callId) {
    for (const auto& value : messages) {
        const auto row = value.toObject();
        if (row.value("role") == "tool" && row.value("tool_call_id") == callId)
            return parse(row.value("content").toString());
    }
    return {};
}
class FakeModelTransport final : public agent::ModelTransport {
public:
    using ModelTransport::ModelTransport;
    int requests = 0, cancellations = 0, generation = 0;
    QJsonArray messages, definitions;
    std::function<QJsonObject(const QJsonArray&)> respond = [](const QJsonArray&) { return final(); };
    void request(const QJsonArray& history, const QJsonArray& tools) override {
        messages = history; definitions = tools; ++requests; emit requestStarted();
        const auto reply = respond(history); const auto epoch = generation;
        QTimer::singleShot(0, this, [this, epoch, reply] {
            if (generation != epoch) return;
            emit requestCompleted({{"model_identity", "protocol-only-model"}, {"http_status", 200},
                {"duration_ms", 1}, {"retry_count", 0}});
            emit replyReady(reply);
        });
    }
    void cancel() override { ++generation; ++cancellations; }
};
void adaptive(FakeModelTransport& model, bool skipValidation = false, bool wrongFinal = false,
              const QString& preferredPlan = {}, bool incompatibleAlternative = false) {
    // The fake model deliberately chooses observation -> topology -> detection;
    // production code must preserve this model-chosen order, not prescribe it.
    model.respond = [step = 0, selected = QString{}, event = QString{}, skipValidation, wrongFinal,
                    preferredPlan, incompatibleAlternative](const QJsonArray& messages) mutable {
        switch (step++) {
        case 0: return tool("observations", "read_observations", {{"asset_id", "tank-Z"}, {"metric", "level_pct"}, {"window_s", 300}, {"limit", 8}});
        case 1: return tool("topology", "read_topology", {{"offset", 0}, {"limit", 64}});
        case 2: return tool("detection", "detect", {{"system_ids", QJsonArray{}}});
        case 3: {
            const auto result = resultFor(messages, "detection").value("result").toObject().value("result").toObject();
            const auto events = result.value("events").toArray();
            if (!events.isEmpty()) event = events.first().toObject().value("event_id").toString();
            return tool("search", "search_plans", {{"system_ids", QJsonArray{}}, {"candidate_limit", 32}, {"time_limit_s", 5}});
        }
        case 4: {
            const auto result = resultFor(messages, "search").value("result").toObject().value("result").toObject();
            selected = result.value("selected_plan_id").toString();
            if (!preferredPlan.isEmpty() || incompatibleAlternative) {
                QJsonObject alternative;
                for (const auto& value : result.value("evaluations").toArray()) {
                    const auto proposed = value.toObject().value("plan").toObject();
                    const bool baseline = proposed.value("truck_jobs").toArray().isEmpty() &&
                        proposed.value("well_shut_ins").toArray().isEmpty();
                    if ((incompatibleAlternative && baseline) ||
                        (!incompatibleAlternative && proposed.value("plan_id") == selected)) { alternative = proposed; break; }
                }
                if (alternative.isEmpty()) return final("missing-retained-plan", {}, event);
                alternative["plan_id"] = incompatibleAlternative ? QStringLiteral("short-cheaper-incompatible") : preferredPlan;
                if (incompatibleAlternative) {
                    auto deadlines = alternative.value("repair_deadlines").toArray();
                    for (qsizetype i = 0; i < deadlines.size(); ++i) {
                        auto deadline = deadlines[i].toObject(); deadline["repair_elapsed_s"] = 3600; deadlines[i] = deadline;
                    }
                    alternative["repair_deadlines"] = deadlines;
                } else selected = preferredPlan;
                alternative["plan_hash"] = plan_identity(alternative);
                return tool("alternative", "evaluate_plan", {{"plan", alternative}});
            }
            if (skipValidation) return final(selected, {}, event);
            return tool("validation", "validate_plan", {{"plan_id", selected}, {"evaluator_method", "protocol-only-evaluator"}});
        }
        case 5:
            if (!preferredPlan.isEmpty() || incompatibleAlternative) {
                if (skipValidation) return final(selected, {}, event);
                return tool("validation", "validate_plan", {{"plan_id", selected}, {"evaluator_method", "protocol-only-evaluator"}});
            }
            [[fallthrough]];
        default: {
            const auto result = resultFor(messages, "validation").value("result").toObject().value("result").toObject();
            auto validation = result.value("validation").toObject().value("validation_id").toString();
            if (wrongFinal) validation = "invented-validation";
            return final(selected, validation, event);
        }
        }
    };
}
struct Harness {
    QTemporaryDir directory;
    QString folder, calls, gate;
    EogDatasetController dataset;
    EogAnalysisClient client;
    EogHistoryRepository history;
    QSignalSpy historyReady, historyFailures, operatorRecords;
    FakeModelTransport model;
    EogAnalysisController analysis;
    Harness(const QString& fixtures, const QString& worker, const QString& mode = "valid")
        : folder(directory.filePath("dataset")), calls(directory.filePath("provider-calls.ndjson")),
          gate(directory.filePath("provider-gate")), client(python(), worker, {mode, calls, gate}),
          history(directory.filePath("history.sqlite")), historyReady(&history, &EogHistoryRepository::ready),
          historyFailures(&history, &EogHistoryRepository::failed), operatorRecords(&history, &EogHistoryRepository::operatorRecordSaved),
          analysis(&dataset, &client, &history, &model) {
        if (!directory.isValid() || !QDir().mkpath(folder)) throw std::runtime_error("Cannot create integration fixture");
        for (const auto* name : {"network.json", "ops.json", "telemetry.csv"})
            if (!QFile::copy(QDir(fixtures).filePath(name), QDir(folder).filePath(name)))
                throw std::runtime_error("Cannot copy integration fixture");
        auto csv = bytes(QDir(folder).filePath("telemetry.csv"));
        csv.replace(",level_pct,-2", ",level_pct,79"); csv.replace(",level_pct,104", ",level_pct,80");
        if (!write(QDir(folder).filePath("telemetry.csv"), csv)) throw std::runtime_error("Cannot initialize integration observations");
        dataset.loadFolder(QUrl::fromLocalFile(folder));
    }
    int providerCalls() const {
        if (!QFileInfo(calls).exists()) return 0;
        const auto rows = bytes(calls).split('\n');
        return int(std::count_if(rows.cbegin(), rows.cend(), [](const auto& row) { return !row.trimmed().isEmpty(); }));
    }
};
bool drain(EogHistoryRepository& history) {
    QSignalSpy listed(&history, &EogHistoryRepository::sessionsListed);
    history.listSessions(); return !listed.isEmpty() || listed.wait(15000);
}
QString failureText(const QSignalSpy& errors) {
    return errors.isEmpty() ? QStringLiteral("no repository failure") :
        errors.last().at(0).toString() + ": " + errors.last().at(2).toString() + ": " + errors.last().at(3).toString();
}
QJsonObject noAction(const Harness& h, const QString& id, double horizon) {
    QJsonObject result{{"plan_id", id}, {"context", h.dataset.cursorUnix() == h.dataset.nowUnix() ? "snapshot_now" : "historical_cursor"},
        {"start_unix", h.dataset.cursorUnix()}, {"repair_deadlines", QJsonArray{QJsonObject{{"facility_id", "facility-Z"}, {"repair_elapsed_s", horizon}}}},
        {"truck_jobs", QJsonArray{}}, {"well_shut_ins", QJsonArray{}}, {"inspection_assets", QJsonArray{}}};
    result["plan_hash"] = plan_identity(result); return result;
}
}
class EogAnalysisControllerTests final : public QObject {
    Q_OBJECT
    QString fixtures_, worker_;
private slots:
    void initTestCase() {
        QVERIFY2(QFileInfo(python()).isFile(), "Pinned Python is required; controller process checks cannot skip.");
        const auto path = QFINDTESTDATA("eog_fixtures/render/network.json");
        QVERIFY(!path.isEmpty()); fixtures_ = QFileInfo(path).absolutePath();
        worker_ = QFINDTESTDATA("eog_fixtures/controller_worker_fixture.py"); QVERIFY(!worker_.isEmpty());
    }
    void loadingCursorAndEquipmentSelectionMakeNoAutomaticRequests() {
        Harness h(fixtures_, worker_); QTRY_VERIFY_WITH_TIMEOUT(h.dataset.loaded() && h.historyReady.size() == 1, 15000);
        h.dataset.selectAsset("pump-Z"); h.dataset.selectMetric("flow_bpd"); h.dataset.showOverview();
        h.dataset.setCursorUnix(h.dataset.firstUnix()); h.dataset.selectFacility("facility-Z");
        QVERIFY(drain(h.history));
        QCOMPARE(h.model.requests, 0); QCOMPARE(h.providerCalls(), 0); QVERIFY(h.analysis.sessionId().isEmpty());
        QVERIFY(!h.analysis.busy()); QVERIFY(!h.analysis.canReview());
        QSignalSpy done(&h.client, &EogAnalysisClient::completed);
        h.analysis.detect(); QTRY_COMPARE_WITH_TIMEOUT(done.size(), 1, 15000);
        QCOMPARE(h.providerCalls(), 1); QCOMPARE(h.model.requests, 0); QVERIFY(!h.analysis.events().isEmpty());
        QVERIFY(h.analysis.providerVerified()); QVERIFY(drain(h.history));
        QVERIFY2(h.historyFailures.isEmpty(), qPrintable(failureText(h.historyFailures)));
        const auto row = h.analysis.events().first().toMap(); const auto state = row.value("state");
        h.analysis.acknowledge(row.value("event_id").toString(), "Reviewed protocol-only evidence.");
        QTRY_COMPARE_WITH_TIMEOUT(h.operatorRecords.size(), 1, 15000);
        QCOMPARE(h.analysis.events().first().toMap().value("state"), state); // Acknowledge never clears a fault.
    }
    void exactSelectionValidationAndReviewDoNotMutateObservedData() {
        Harness h(fixtures_, worker_); QTRY_VERIFY_WITH_TIMEOUT(h.dataset.loaded() && h.historyReady.size() == 1, 15000);
        const auto source = h.dataset.capturedDataset(); const auto identity = h.dataset.datasetIdentity();
        const auto cutoff = h.dataset.cursorUnix(); const auto raw = bytes(QDir(h.folder).filePath("telemetry.csv"));
        QSignalSpy done(&h.client, &EogAnalysisClient::completed);
        h.analysis.searchPlans(); QTRY_COMPARE_WITH_TIMEOUT(done.size(), 1, 15000);
        QCOMPARE(h.analysis.plans().size(), 2); QVERIFY(h.analysis.comparisonCompatible()); QVERIFY(!h.analysis.canReview());
        const auto candidate = h.analysis.selectedPlanId(), baseline = h.analysis.baselinePlanId();
        QVERIFY(candidate != baseline);
        h.analysis.validateSelected("protocol-only-evaluator"); QTRY_COMPARE_WITH_TIMEOUT(done.size(), 2, 15000);
        QCOMPARE(h.analysis.selectedValidation().value("plan_id").toString(), candidate);
        h.analysis.selectPlan(baseline); QVERIFY(!h.analysis.canReview()); QVERIFY(h.analysis.selectedValidation().isEmpty());
        h.analysis.selectPlan(candidate);
        // Either an explicit finish gate or an enabled review that atomically
        // completes the live session is consistent. An enabled review may not
        // fail merely because its repository still considers the session running.
        if (!h.analysis.canReview()) h.analysis.finishSession();
        QVERIFY(h.analysis.canReview()); h.analysis.recordReview("Advisory protocol-only plan review.");
        QTRY_COMPARE_WITH_TIMEOUT(h.operatorRecords.size(), 1, 15000);
        QVERIFY(drain(h.history)); QVERIFY2(h.historyFailures.isEmpty(), qPrintable(failureText(h.historyFailures)));
        QCOMPARE(h.dataset.capturedDataset(), source); QCOMPARE(h.dataset.datasetIdentity(), identity);
        QCOMPARE(h.dataset.cursorUnix(), cutoff); QCOMPARE(bytes(QDir(h.folder).filePath("telemetry.csv")), raw);
        QCOMPARE(h.model.requests, 0);
    }
    void substitutedValidationCannotGrantReview() {
        Harness h(fixtures_, worker_, "mismatched_validation");
        QTRY_VERIFY_WITH_TIMEOUT(h.dataset.loaded() && h.historyReady.size() == 1, 15000);
        QSignalSpy done(&h.client, &EogAnalysisClient::completed), failed(&h.client, &EogAnalysisClient::failed);
        h.analysis.searchPlans(); QTRY_COMPARE_WITH_TIMEOUT(done.size(), 1, 15000);
        h.analysis.validateSelected("protocol-only-evaluator"); QTRY_COMPARE_WITH_TIMEOUT(failed.size(), 1, 15000);
        QVERIFY(!h.analysis.canReview()); QVERIFY(h.analysis.selectedValidation().isEmpty());
        h.analysis.recordReview(); QVERIFY(drain(h.history)); QCOMPARE(h.operatorRecords.size(), 0);
    }
    void cursorChangeRetiresInflightProviderAndIgnoresItsLateResult() {
        Harness h(fixtures_, worker_, "gated"); QTRY_VERIFY_WITH_TIMEOUT(h.dataset.loaded() && h.historyReady.size() == 1, 15000);
        QSignalSpy done(&h.client, &EogAnalysisClient::completed);
        h.analysis.detect(); QTRY_VERIFY_WITH_TIMEOUT(QFileInfo(h.gate + ".ready").exists(), 15000);
        QVERIFY(h.analysis.busy()); const auto oldSession = h.analysis.sessionId();
        h.dataset.selectAsset("pump-Z"); QVERIFY(h.analysis.busy()); // Selection has no authority effect.
        h.dataset.setCursorUnix(h.dataset.firstUnix());
        QVERIFY(!h.analysis.busy()); QVERIFY(h.analysis.events().isEmpty()); QVERIFY(h.analysis.sessionId().isEmpty());
        QVERIFY(write(h.gate, "release only the next request"));
        h.analysis.detect(); QTRY_COMPARE_WITH_TIMEOUT(done.size(), 1, 15000);
        QVERIFY(h.analysis.sessionId() != oldSession); QCOMPARE(h.providerCalls(), 2);
        QCOMPARE(h.analysis.events().first().toMap().value("updated_unix").toDouble(), h.dataset.firstUnix());
        QVERIFY(drain(h.history)); QVERIFY2(h.historyFailures.isEmpty(), qPrintable(failureText(h.historyFailures)));
    }
    void explicitCancelAndNewSourceLoadRetireEvidence() {
        Harness h(fixtures_, worker_); QTRY_VERIFY_WITH_TIMEOUT(h.dataset.loaded() && h.historyReady.size() == 1, 15000);
        QSignalSpy done(&h.client, &EogAnalysisClient::completed);
        h.analysis.detect(); QTRY_COMPARE_WITH_TIMEOUT(done.size(), 1, 15000);
        const auto oldIdentity = h.dataset.datasetIdentity(); h.analysis.cancel();
        QVERIFY(!h.analysis.canReview()); QVERIFY(h.analysis.sessionId().isEmpty());
        auto csv = bytes(QDir(h.folder).filePath("telemetry.csv")); csv.replace(",level_pct,80", ",level_pct,81");
        QVERIFY(write(QDir(h.folder).filePath("telemetry.csv"), csv));
        h.dataset.loadFolder(QUrl::fromLocalFile(h.folder));
        QVERIFY(h.analysis.events().isEmpty()); // Retire at load start, before its result.
        QTRY_VERIFY_WITH_TIMEOUT(!h.dataset.busy() && h.dataset.datasetIdentity() != oldIdentity, 15000);
        QCOMPARE(h.model.requests, 0); QCOMPARE(h.providerCalls(), 1); QVERIFY(h.analysis.sessionId().isEmpty());
    }
    void incompatibleHorizonsRemainIndividuallyViewable() {
        Harness h(fixtures_, worker_); QTRY_VERIFY_WITH_TIMEOUT(h.dataset.loaded() && h.historyReady.size() == 1, 15000);
        QSignalSpy done(&h.client, &EogAnalysisClient::completed);
        h.analysis.searchPlans(); QTRY_COMPARE_WITH_TIMEOUT(done.size(), 1, 15000);
        const auto candidate = h.analysis.selectedPlanId(), baseline = h.analysis.baselinePlanId();
        const auto different = noAction(h, "different-horizon", 3600);
        h.analysis.evaluatePlan(QString::fromUtf8(QJsonDocument(different).toJson()));
        QTRY_COMPARE_WITH_TIMEOUT(done.size(), 2, 15000);
        QCOMPARE(h.analysis.plans().size(), 3);
        h.analysis.selectPlan(candidate); h.analysis.selectBaseline("different-horizon");
        QVERIFY(!h.analysis.comparisonCompatible()); QVERIFY(!h.analysis.baselineEvaluation().isEmpty());
        h.analysis.selectBaseline(baseline); QVERIFY(h.analysis.comparisonCompatible());
        h.analysis.selectPlan("different-horizon"); QVERIFY(!h.analysis.comparisonCompatible());
        QVERIFY(!h.analysis.selectedEvaluation().isEmpty()); QCOMPARE(h.dataset.cursorUnix(), h.dataset.nowUnix());
    }
    void reopenedAndImportedEvidenceCannotRestoreReview() {
        Harness h(fixtures_, worker_); QTRY_VERIFY_WITH_TIMEOUT(h.dataset.loaded() && h.historyReady.size() == 1, 15000);
        QSignalSpy done(&h.client, &EogAnalysisClient::completed);
        h.analysis.searchPlans(); QTRY_COMPARE_WITH_TIMEOUT(done.size(), 1, 15000);
        h.analysis.validateSelected("protocol-only-evaluator"); QTRY_COMPARE_WITH_TIMEOUT(done.size(), 2, 15000);
        h.analysis.finishSession(); const auto session = h.analysis.sessionId(); QVERIFY(drain(h.history));
        const auto archive = QUrl::fromLocalFile(h.directory.filePath("session.eog-session.json"));
        QSignalSpy exported(&h.history, &EogHistoryRepository::archiveExported), loaded(&h.history, &EogHistoryRepository::sessionLoaded);
        h.analysis.exportHistory(session, archive);
        QTRY_VERIFY_WITH_TIMEOUT(exported.size() == 1 || !h.historyFailures.isEmpty(), 15000);
        QVERIFY2(exported.size() == 1, qPrintable(failureText(h.historyFailures)));
        h.analysis.openHistory(session); QTRY_COMPARE_WITH_TIMEOUT(loaded.size(), 1, 15000);
        QVERIFY(h.analysis.historical()); QVERIFY(!h.analysis.canReview()); QVERIFY(!h.analysis.selectedValidation().isEmpty());
        h.analysis.recordReview("Must not regain eligibility."); QVERIFY(drain(h.history)); QCOMPARE(h.operatorRecords.size(), 0);
        h.analysis.returnToLive(); QVERIFY(!h.analysis.historical()); QVERIFY(!h.analysis.canReview());
        QSignalSpy imported(&h.history, &EogHistoryRepository::archiveImported);
        h.analysis.importHistory(archive);
        QTRY_VERIFY_WITH_TIMEOUT(imported.size() == 1 || !h.historyFailures.isEmpty(), 15000);
        QVERIFY2(imported.size() == 1, qPrintable(failureText(h.historyFailures)));
        QTRY_VERIFY_WITH_TIMEOUT(h.analysis.historical(), 15000);
        QVERIFY(!h.analysis.canReview()); h.analysis.recordReview(); QVERIFY(drain(h.history)); QCOMPARE(h.operatorRecords.size(), 0);
        QVERIFY2(h.historyFailures.isEmpty(), qPrintable(failureText(h.historyFailures)));
    }
    void lateHistoryLoadCannotDisplaceANewerDatasetIntent() {
        Harness h(fixtures_, worker_); QTRY_VERIFY_WITH_TIMEOUT(h.dataset.loaded() && h.historyReady.size() == 1, 15000);
        QSignalSpy done(&h.client, &EogAnalysisClient::completed), loaded(&h.history, &EogHistoryRepository::sessionLoaded);
        h.analysis.detect(); QTRY_COMPARE_WITH_TIMEOUT(done.size(), 1, 15000);
        h.analysis.finishSession(); const auto oldSession = h.analysis.sessionId(); QVERIFY(drain(h.history));
        h.analysis.openHistory(oldSession);
        // No event-loop yield between the history request and newer intent.
        h.dataset.setCursorUnix(h.dataset.firstUnix()); h.analysis.detect();
        QTRY_COMPARE_WITH_TIMEOUT(loaded.size(), 1, 15000);
        QTRY_COMPARE_WITH_TIMEOUT(done.size(), 2, 15000);
        QVERIFY(!h.analysis.historical()); QVERIFY(h.analysis.sessionId() != oldSession);
        QCOMPARE(h.analysis.events().first().toMap().value("updated_unix").toDouble(), h.dataset.firstUnix());
    }
    void modelChosenToolsContinueWithGroundedExactReferences() {
        Harness h(fixtures_, worker_); QTRY_VERIFY_WITH_TIMEOUT(h.dataset.loaded() && h.historyReady.size() == 1, 15000);
        adaptive(h.model, false, false, {}, true);
        h.analysis.investigate("Inspect the actual selected evidence and compare response plans.");
        QTRY_VERIFY_WITH_TIMEOUT(h.model.requests >= 7 && !h.analysis.busy(), 30000);
        QVERIFY2(h.analysis.status().contains("Nemotron completed"), qPrintable(h.analysis.status()));
        QVERIFY(!h.analysis.events().isEmpty());
        QCOMPARE(h.analysis.selectedPlanId(), "protocol-only-truck-plan");
        QVERIFY(!h.analysis.selectedValidation().isEmpty());
        QStringList actual;
        for (const auto& value : h.analysis.trace()) {
            const auto row = value.toMap(); if (row.value("type") == "tool_call") actual.append(row.value("name").toString());
        }
        QCOMPARE(actual, (QStringList{"read_observations", "read_topology", "detect", "search_plans", "evaluate_plan", "validate_plan"}));
        const auto alternative = resultFor(h.model.messages, "alternative").value("result").toObject().value("result").toObject().value("evaluation").toObject();
        QVERIFY(!alternative.isEmpty());
        QCOMPARE(alternative.value("plan").toObject().value("plan_id").toString(), "short-cheaper-incompatible");
        const auto selected = QJsonObject::fromVariantMap(h.analysis.selectedEvaluation());
        QVERIFY(alternative.value("cost").toObject().value("total").toDouble() < selected.value("cost").toObject().value("total").toDouble());
        QVERIFY(alternative.value("plan").toObject().value("repair_deadlines") != selected.value("plan").toObject().value("repair_deadlines"));
        const auto observations = resultFor(h.model.messages, "observations").value("result").toObject();
        for (const auto& value : observations.value("samples").toArray())
            QVERIFY(value.toObject().value("timestamp_unix").toDouble() <= h.dataset.cursorUnix());
        QVERIFY(!QJsonDocument(h.model.messages).toJson().contains(h.folder.toUtf8()));
        QVERIFY(drain(h.history)); QVERIFY2(h.historyFailures.isEmpty(), qPrintable(failureText(h.historyFailures)));
        QVERIFY(h.analysis.canReview()); h.analysis.recordReview("Protocol-only grounded-model exercise.");
        QTRY_COMPARE_WITH_TIMEOUT(h.operatorRecords.size(), 1, 15000);
    }
    void reopenedModelSessionRestoresItsFinalChoiceAndSummary() {
        Harness h(fixtures_, worker_); QTRY_VERIFY_WITH_TIMEOUT(h.dataset.loaded() && h.historyReady.size() == 1, 15000);
        const auto finalChoice = QStringLiteral("protocol-only-alternative");
        // Both retained candidates have the same admissible cost/horizon. The
        // model explicitly chooses/validates B although the search returned A.
        adaptive(h.model, false, false, finalChoice);
        h.analysis.investigate("Compare retained evidence and explain the selected exact plan.");
        QTRY_VERIFY_WITH_TIMEOUT(h.model.requests >= 7 && !h.analysis.busy(), 30000);
        QVERIFY2(h.analysis.status().contains("Nemotron completed"), qPrintable(h.analysis.status()));
        const auto searched = resultFor(h.model.messages, "search").value("result").toObject().value("result").toObject();
        QVERIFY(searched.value("selected_plan_id") != finalChoice);
        QCOMPARE(h.analysis.selectedPlanId(), finalChoice); const auto summary = h.analysis.summary();
        QVERIFY(!summary.isEmpty()); const auto session = h.analysis.sessionId(); QVERIFY(drain(h.history));
        QVERIFY2(h.historyFailures.isEmpty(), qPrintable(failureText(h.historyFailures)));
        QSignalSpy loaded(&h.history, &EogHistoryRepository::sessionLoaded);
        h.analysis.openHistory(session); QTRY_COMPARE_WITH_TIMEOUT(loaded.size(), 1, 15000);
        QVERIFY(h.analysis.historical()); QCOMPARE(h.analysis.selectedPlanId(), finalChoice);
        QCOMPARE(h.analysis.summary(), summary); QVERIFY(!h.analysis.canReview());
    }
    void unvalidatedOrInventedModelFinalIsRejected_data() {
        QTest::addColumn<bool>("skip"); QTest::addColumn<bool>("wrong");
        QTest::newRow("validation-skipped") << true << false;
        QTest::newRow("invented-validation") << false << true;
    }
    void unvalidatedOrInventedModelFinalIsRejected() {
        QFETCH(bool, skip); QFETCH(bool, wrong);
        Harness h(fixtures_, worker_); QTRY_VERIFY_WITH_TIMEOUT(h.dataset.loaded() && h.historyReady.size() == 1, 15000);
        adaptive(h.model, skip, wrong); h.analysis.investigate("Use evidence without inventing references.");
        QTRY_VERIFY_WITH_TIMEOUT(h.model.requests >= (skip ? 5 : 6) && !h.analysis.busy(), 30000);
        QVERIFY2(h.analysis.status().contains("rejected"), qPrintable(h.analysis.status()));
        QVERIFY(!h.analysis.summary().isEmpty()); QVERIFY(!h.analysis.canReview());
        h.analysis.recordReview(); QVERIFY(drain(h.history)); QCOMPARE(h.operatorRecords.size(), 0);
    }
    void tooLongObjectiveAndMissingProviderStayUnavailable() {
        Harness h(fixtures_, worker_); QTRY_VERIFY_WITH_TIMEOUT(h.dataset.loaded() && h.historyReady.size() == 1, 15000);
        h.analysis.investigate(QString(4001, 'a'));
        QVERIFY(!h.analysis.busy()); QCOMPARE(h.model.requests, 0); QCOMPARE(h.providerCalls(), 0);
        QVERIFY(h.analysis.sessionId().isEmpty());
        QTemporaryDir directory; EogDatasetController dataset; EogAnalysisClient absent(python(), worker_ + ".absent");
        EogHistoryRepository history(directory.filePath("history.sqlite")); FakeModelTransport model;
        EogAnalysisController analysis(&dataset, &absent, &history, &model);
        QSignalSpy complete(&dataset, &EogDatasetController::completed);
        dataset.loadFolder(QUrl::fromLocalFile(h.folder)); QTRY_COMPARE_WITH_TIMEOUT(complete.size(), 1, 15000);
        analysis.detect(); QVERIFY(!analysis.busy()); QVERIFY(!analysis.providerConfigured()); QVERIFY(!analysis.providerVerified());
        QVERIFY(analysis.status().contains("unavailable")); QVERIFY(analysis.sessionId().isEmpty()); QCOMPARE(model.requests, 0);
    }
};
QTEST_GUILESS_MAIN(EogAnalysisControllerTests)
#include "eog_analysis_controller_tests.moc"
