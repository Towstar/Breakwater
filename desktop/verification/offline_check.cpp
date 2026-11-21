#include "eog/EogAnalysisController.hpp"
#include "LocalRuntime.hpp"
#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDir>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QSet>
#include <QTemporaryDir>
#include <QTextStream>
#include <QTimer>
#include <functional>
#include <stdexcept>

using namespace breakwater::desktop;
namespace {
class NoModel final : public agent::ModelTransport {
public:
    int requests = 0;
    void request(const QJsonArray&, const QJsonArray&) override { ++requests; emit requestFailed("offline_model_forbidden", "Offline verification never calls a model."); }
    void cancel() override {}
};
bool waitFor(const std::function<bool()>& condition, int milliseconds = 90000) {
    if (condition()) return true;
    QEventLoop loop; QTimer poll, deadline;
    poll.setInterval(10); deadline.setSingleShot(true);
    QObject::connect(&poll, &QTimer::timeout, &loop, [&] { if (condition()) loop.quit(); });
    QObject::connect(&deadline, &QTimer::timeout, &loop, &QEventLoop::quit);
    poll.start(); deadline.start(milliseconds); loop.exec(); return condition();
}
QJsonObject hashes(const QString& directory) {
    QJsonObject result;
    for (const auto* name : {"network.json", "ops.json", "telemetry.csv"}) {
        QFile file(QDir(directory).filePath(name)); if (!file.open(QIODevice::ReadOnly)) return {};
        QCryptographicHash hash(QCryptographicHash::Sha256); if (!hash.addData(&file)) return {};
        result[name] = QString::fromLatin1(hash.result().toHex());
    }
    return result;
}
void require(bool condition, const char* message) { if (!condition) throw std::runtime_error(message); }
void preflight(const QString& input, const QTemporaryDir& artifacts, const QJsonObject& sourceHashes, QJsonObject& report) {
    const auto archivePath = artifacts.filePath("local-analysis.eog-session.json");
    NoModel model;
    QObject::connect(&model, &agent::ModelTransport::requestFailed, &model, [&](const QString&, const QString&) {
        report["model_requests"] = model.requests; report["zero_model_requests"] = model.requests == 0;
    });
    QJsonArray responses, requestIds;
    QString sessionId, storageFailure, providerFailure;
    {
        eog::EogDatasetController dataset; eog::EogAnalysisClient client;
        eog::EogHistoryRepository history(artifacts.filePath("analysis.sqlite"));
        eog::EogAnalysisController analysis(&dataset, &client, &history, &model);
        bool historyReady = false, exported = false;
        QObject::connect(&history, &eog::EogHistoryRepository::ready, &analysis, [&] { historyReady = true; });
        QObject::connect(&history, &eog::EogHistoryRepository::failed, &analysis,
            [&](const QString&, const QString&, const QString& code, const QString&) { storageFailure = code; });
        QObject::connect(&client, &eog::EogAnalysisClient::failed, &analysis,
            [&](const QString& code, const QString&) { providerFailure = code; });
        QObject::connect(&client, &eog::EogAnalysisClient::completed, &analysis, [&](const QJsonObject& response, const QJsonObject&) {
            responses.append(response); requestIds.append(response.value("request_id"));
        });
        dataset.loadFolder(QUrl::fromLocalFile(input));
        require(waitFor([&] { return !dataset.busy() && (historyReady || !storageFailure.isEmpty()); }, 15000) &&
            dataset.loaded() && historyReady && storageFailure.isEmpty(), "Preflight dataset/history setup failed.");
        report["provider_configured"] = client.configured();
        require(client.configured(), "Preflight numerical provider is not configured.");
        const auto run = [&](const QString& operation, const std::function<void()>& start) {
            const auto count = responses.size(); start();
            require(waitFor([&] { return !analysis.busy() || !providerFailure.isEmpty() || !storageFailure.isEmpty(); }),
                "Preflight numerical operation exceeded its deadline.");
            require(providerFailure.isEmpty() && storageFailure.isEmpty(), "Preflight provider or history failed.");
            require(responses.size() == count + 1, "Preflight operation produced no verified provider response.");
            const auto response = responses.last().toObject();
            const auto context = QJsonObject::fromVariantMap(analysis.evidenceContext());
            const auto retained = QJsonObject::fromVariantMap(analysis.lastOperation());
            require(response.value("eog_version") == 1 && response.value("kind") == "analysis_response" &&
                response.value("method_version") == 1 && response.value("operation") == operation &&
                !response.value("request_id").toString().isEmpty() && response.value("request_id") == retained.value("request_id"),
                "Preflight provider response identity is unavailable.");
            for (const auto* key : {"capture_id", "dataset_identity", "cutoff_unix"})
                require(response.value(key) == context.value(key), "Preflight provider response does not bind the current capture.");
            return response;
        };
        const auto detection = run("detect", [&] { analysis.detect(); });
        require(detection.value("status") == "completed", "Preflight detection did not complete.");
        const auto detectedCoverage = detection.value("result").toObject().value("coverage").toString();
        require(detectedCoverage == "available" || detectedCoverage == "partial" || detectedCoverage == "unavailable",
            "Preflight detection has no supported coverage verdict.");
        report["detection"] = QJsonObject::fromVariantMap(analysis.detectionDetails());
        const auto searched = run("search_plans", [&] { analysis.searchPlans(); });
        report["search_response_status"] = searched.value("status");
        report["diagnostics"] = searched.value("diagnostics");
        QString outcome, coverage;
        if (searched.value("status") == "unavailable") {
            const QSet<QString> coverageCodes{"failure_context_incomplete", "failure_state_unavailable", "well_rate_unavailable",
                "well_rate_ambiguous", "inflow_ambiguous"};
            const QSet<QString> noPlanCodes{"no_active_failure", "repair_unavailable"};
            const auto diagnostics = searched.value("diagnostics").toArray();
            require(!diagnostics.isEmpty(), "Unavailable preflight search has no retained reason.");
            bool unavailableCoverage = false, noPlan = false, unavailableHorizon = false;
            for (const auto& value : diagnostics) {
                const auto code = value.toObject().value("code").toString();
                require(coverageCodes.contains(code) || noPlanCodes.contains(code),
                    "Preflight search failed or returned an unsupported refusal; no no-plan verdict is issued.");
                unavailableCoverage |= coverageCodes.contains(code);
                noPlan |= noPlanCodes.contains(code);
                unavailableHorizon |= code == "repair_unavailable";
            }
            require(unavailableCoverage || noPlan, "Preflight search has no supported outcome.");
            outcome = unavailableCoverage ? "coverage_unavailable" : "no_validated_plan";
            coverage = unavailableCoverage || unavailableHorizon ? "unavailable" : "available";
        } else {
            require(searched.value("status") == "completed", "Preflight search did not complete.");
            const auto searchResult = searched.value("result").toObject();
            require(searchResult.value("search").isObject() && searchResult.value("evaluations").isArray(),
                "Preflight search did not retain its bounded search evidence.");
            report["search"] = searchResult.value("search");
            report["baseline"] = QJsonObject::fromVariantMap(analysis.baselineEvaluation());
            const auto selected = searchResult.value("selected_plan_id").toString();
            if (selected.isEmpty()) {
                // A completed bounded search may retain candidates without a
                // completed selected-plan check. This is no global proof.
                outcome = "no_validated_plan"; coverage = detectedCoverage == "available" ? "available" : "unavailable";
                for (const auto& value : searchResult.value("evaluations").toArray())
                    if (value.toObject().value("coverage") == "available") coverage = "available";
            } else {
                require(analysis.selectedPlanId() == selected, "Preflight controller did not retain the provider's selection.");
                const auto candidate = QJsonObject::fromVariantMap(analysis.selectedEvaluation());
                const auto plan = candidate.value("plan").toObject();
                require(plan.value("plan_id") == selected && candidate.value("coverage") == "available" && candidate.value("feasible").toBool(),
                    "Preflight selected candidate has no available numerical assessment.");
                const auto method = analysis.selectedEvaluatorMethod();
                require(!method.isEmpty(), "Preflight selected evaluator identity is unsupported.");
                const auto validated = run("validate_plan", [&] { analysis.validateSelected(method); });
                require(validated.value("status") == "completed", "Preflight independent validation did not complete.");
                const auto validation = QJsonObject::fromVariantMap(analysis.selectedValidation());
                require(!validation.value("validation_id").toString().isEmpty() && validation.value("plan_id") == selected &&
                    validation.value("plan_hash") == plan.value("plan_hash") && validation.value("independent").toBool(),
                    "Preflight independent validation identity is missing or mismatched.");
                report["candidate"] = candidate; report["validation"] = validation;
                outcome = analysis.canReview() ? "validated_advice" : "no_validated_plan";
                coverage = validation.value("coverage") == "available" ? "available" : "unavailable";
                if (outcome == "validated_advice") {
                    report["selected_plan_id"] = selected; report["validation_id"] = validation.value("validation_id");
                }
            }
        }
        sessionId = analysis.sessionId();
        require(!sessionId.isEmpty() && model.requests == 0, "Preflight session is missing or attempted a model request.");
        const auto context = QJsonObject::fromVariantMap(analysis.evidenceContext());
        require(context.value("source_hashes").toObject() == sourceHashes, "Preflight capture does not bind the original input bytes.");
        report["session_id"] = sessionId; report["capture_id"] = context.value("capture_id");
        report["dataset_identity"] = context.value("dataset_identity"); report["cutoff_unix"] = context.value("cutoff_unix");
        report["analysis_request_ids"] = requestIds; report["plans_retained"] = analysis.plans().size();
        report["provider_verified"] = client.executionVerified(); report["provider_status"] = analysis.status();
        // The client truthfully leaves executionVerified false for a typed
        // unavailable response. A verified refusal is an exchange, not a claim
        // that numerical evaluation ran. Actual advice still needs execution.
        require(outcome != "validated_advice" || client.executionVerified(), "Preflight validated advice has no verified numerical execution.");
        QObject::connect(&history, &eog::EogHistoryRepository::archiveExported, &analysis, [&](const QString& id, const QUrl& path) {
            exported = id == sessionId && path.toLocalFile() == archivePath;
        });
        analysis.finishSession(); analysis.exportHistory(sessionId, QUrl::fromLocalFile(archivePath));
        require(waitFor([&] { return exported || !storageFailure.isEmpty(); }, 15000) && exported && storageFailure.isEmpty(),
            "Preflight completed evidence export failed.");
        report["outcome"] = outcome; report["coverage"] = coverage; report["archive_path"] = archivePath;
    }
    {
        eog::EogDatasetController dataset; eog::EogAnalysisClient client;
        eog::EogHistoryRepository history(artifacts.filePath("reopened.sqlite"));
        eog::EogAnalysisController analysis(&dataset, &client, &history, &model);
        QJsonObject normalized;
        QObject::connect(&history, &eog::EogHistoryRepository::failed, &analysis,
            [&](const QString&, const QString&, const QString& code, const QString&) { storageFailure = code; });
        QObject::connect(&history, &eog::EogHistoryRepository::sessionLoaded, &analysis, [&](const QJsonObject& archive) { normalized = archive; });
        analysis.importHistory(QUrl::fromLocalFile(archivePath));
        require(waitFor([&] { return analysis.historical() || !storageFailure.isEmpty(); }, 15000) && storageFailure.isEmpty() && analysis.historical(),
            "Preflight evidence did not reopen through production normalization.");
        require(normalized.value("session_id") == sessionId && normalized.value("status") == "completed" &&
            normalized.value("historical").toBool() && !normalized.value("actionable").toBool() && !analysis.canReview(),
            "Preflight reopened evidence has incorrect status or authority.");
        const auto analyses = normalized.value("analyses").toArray();
        require(analyses.size() == responses.size(), "Preflight reopening lost completed provider evidence.");
        for (qsizetype i = 0; i < responses.size(); ++i) {
            const auto row = analyses[i].toObject(), request = row.value("request").toObject(), response = row.value("response").toObject();
            require(row.value("response") == responses[i], "Preflight reopening changed provider evidence.");
            require(request.value("eog_version") == 1 && request.value("kind") == "analysis_request" && request.value("method_version") == 1 &&
                response.value("eog_version") == 1 && response.value("kind") == "analysis_response" && response.value("method_version") == 1 &&
                request.value("request_id") == requestIds[i] && response.value("request_id") == request.value("request_id") &&
                response.value("operation") == request.value("operation"), "Preflight normalized exchange identity is mismatched.");
            for (const auto* key : {"capture_id", "dataset_identity", "cutoff_unix"})
                require(request.value(key) == normalized.value("capture_manifest").toObject().value(key) && response.value(key) == request.value(key),
                    "Preflight normalized exchange does not bind its retained capture.");
        }
        require(model.requests == 0, "Preflight reopening attempted a model request.");
    }
    // completed() is emitted only after the real client's schema/request/capture
    // checks. Import then validates those exact records again through history.
    // Unknown or capability failures never reach this flag's assignment.
    report["provider_exchange_verified"] = true;
    report["model_requests"] = model.requests; report["zero_model_requests"] = true;
}
}
int main(int argc, char** argv) {
    QCoreApplication app(argc, argv); QCoreApplication::setApplicationName("BREAKWATER"); QCoreApplication::setOrganizationName("Breakwater");
    runtime::configureBundledPython();
    const bool isPreflight = app.arguments().size() == 4 && app.arguments()[3] == "--preflight";
    if (app.arguments().size() != 3 && !isPreflight) { QTextStream(stderr) << "Usage: breakwater_offline_check <dataset-folder> <existing-output-root> [--preflight]\n"; return 2; }
    const auto input = QFileInfo(app.arguments()[1]).canonicalFilePath(), output = QFileInfo(app.arguments()[2]).canonicalFilePath();
    if (!QFileInfo(input).isDir() || !QFileInfo(output).isDir() || output.compare(input, Qt::CaseInsensitive) == 0 || output.startsWith(input + '/', Qt::CaseInsensitive)) return 2;
    QTemporaryDir artifacts(QDir(output).filePath("breakwater-offline-XXXXXX")); if (!artifacts.isValid()) return 2;
    artifacts.setAutoRemove(false);
    const auto archive = artifacts.filePath("local-analysis.eog-session.json");
    QJsonObject report{{"source", "local_numerical_tools"}, {"model_requests", 0}, {"actionable", false}, {"artifact_directory", artifacts.path()}};
    if (isPreflight) {
        report["preflight"] = true; report["zero_model_requests"] = true;
        report["provider_exchange_verified"] = false;
        report["selected_plan_id"] = ""; report["validation_id"] = "";
        report["limitation"] = "Local bounded search and independent validation only; no global feasibility or optimality proof, dispatch or control application.";
    }
    try {
        const auto before = hashes(input); require(before.size() == 3, "Input files are unavailable.");
        if (isPreflight) {
            report["source_hashes"] = before;
            preflight(input, artifacts, before, report);
        } else {
        {
            eog::EogDatasetController dataset; eog::EogAnalysisClient client;
            eog::EogHistoryRepository history(artifacts.filePath("analysis.sqlite")); NoModel model;
            eog::EogAnalysisController analysis(&dataset, &client, &history, &model);
            dataset.loadFolder(QUrl::fromLocalFile(input)); require(waitFor([&] { return !dataset.busy(); }) && dataset.loaded(), "Dataset load failed.");
            report["provider_configured"] = client.configured();
            analysis.searchPlans(); const auto completed = waitFor([&] { return !analysis.busy(); });
            report["provider_status"] = analysis.status();
            require(completed && !analysis.selectedPlanId().isEmpty(), "Local plan search failed.");
            const auto evaluation = analysis.selectedEvaluation(); const auto method = analysis.selectedEvaluatorMethod();
            require(!method.isEmpty(), "No supported numerical evaluator identity was retained.");
            const auto previous = analysis.lastOperation().value("request_id");
            analysis.validateSelected(method);
            require(waitFor([&] { return !analysis.busy() && analysis.lastOperation().value("request_id") != previous; }) && analysis.canReview(), "Independent validation did not establish review eligibility.");
            report["candidate"] = QJsonObject::fromVariantMap(evaluation);
            report["baseline"] = QJsonObject::fromVariantMap(analysis.baselineEvaluation());
            report["validation"] = QJsonObject::fromVariantMap(analysis.selectedValidation());
            report["plans_retained"] = static_cast<int>(analysis.plans().size());
            bool exported = false; QObject::connect(&history, &eog::EogHistoryRepository::archiveExported, &app, [&](const QString&, const QUrl&) { exported = true; });
            analysis.finishSession(); analysis.exportHistory(analysis.sessionId(), QUrl::fromLocalFile(archive));
            require(waitFor([&] { return exported; }, 15000), "Completed evidence export failed.");
            require(model.requests == 0, "An offline check attempted a model request.");
        }
        {
            eog::EogDatasetController dataset; eog::EogAnalysisClient client;
            eog::EogHistoryRepository history(artifacts.filePath("reopened.sqlite")); NoModel model;
            eog::EogAnalysisController analysis(&dataset, &client, &history, &model);
            analysis.importHistory(QUrl::fromLocalFile(archive));
            require(waitFor([&] { return analysis.historical() && !analysis.plans().isEmpty(); }, 15000), "Evidence did not reopen.");
            require(!analysis.canReview() && model.requests == 0, "Restored evidence gained authority or called a model.");
        }
        }
        require(hashes(input) == before, "The input files changed.");
        report["status"] = "passed"; report["inputs_unchanged"] = true; report["history_reopened_nonactionable"] = true;
    } catch (const std::exception& error) { report["status"] = "failed"; report["error"] = QString::fromUtf8(error.what()); }
    if (isPreflight) {
        const auto sourceHashes = report.value("source_hashes").toObject();
        report["inputs_unchanged"] = sourceHashes.size() == 3 && hashes(input) == sourceHashes;
    }
    const auto bytes = QJsonDocument(report).toJson(QJsonDocument::Indented);
    QFile result(artifacts.filePath("verification.json"));
    if (!result.open(QIODevice::WriteOnly | QIODevice::NewOnly) || result.write(bytes) != bytes.size() || !result.flush()) return 3;
    QTextStream(stdout) << QString::fromUtf8(bytes);
    return report.value("status") == "passed" ? 0 : 1;
}
