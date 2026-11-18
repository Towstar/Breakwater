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
}
int main(int argc, char** argv) {
    QCoreApplication app(argc, argv); QCoreApplication::setApplicationName("BREAKWATER"); QCoreApplication::setOrganizationName("Breakwater");
    runtime::configureBundledPython();
    if (app.arguments().size() != 3) { QTextStream(stderr) << "Usage: breakwater_offline_check <dataset-folder> <existing-output-root>\n"; return 2; }
    const auto input = QFileInfo(app.arguments()[1]).canonicalFilePath(), output = QFileInfo(app.arguments()[2]).canonicalFilePath();
    if (!QFileInfo(input).isDir() || !QFileInfo(output).isDir() || output.compare(input, Qt::CaseInsensitive) == 0 || output.startsWith(input + '/', Qt::CaseInsensitive)) return 2;
    QTemporaryDir artifacts(QDir(output).filePath("breakwater-offline-XXXXXX")); if (!artifacts.isValid()) return 2;
    artifacts.setAutoRemove(false);
    const auto archive = artifacts.filePath("local-analysis.eog-session.json");
    QJsonObject report{{"source", "local_numerical_tools"}, {"model_requests", 0}, {"actionable", false}, {"artifact_directory", artifacts.path()}};
    try {
        const auto before = hashes(input); require(before.size() == 3, "Input files are unavailable.");
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
        require(hashes(input) == before, "The input files changed.");
        report["status"] = "passed"; report["inputs_unchanged"] = true; report["history_reopened_nonactionable"] = true;
    } catch (const std::exception& error) { report["status"] = "failed"; report["error"] = QString::fromUtf8(error.what()); }
    const auto bytes = QJsonDocument(report).toJson(QJsonDocument::Indented);
    QFile result(artifacts.filePath("verification.json"));
    if (!result.open(QIODevice::WriteOnly | QIODevice::NewOnly) || result.write(bytes) != bytes.size() || !result.flush()) return 3;
    QTextStream(stdout) << QString::fromUtf8(bytes);
    return report.value("status") == "passed" ? 0 : 1;
}
