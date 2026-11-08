#include "science/StorageScienceEvidence.hpp"
#include "science/StorageScienceController.hpp"
#include "history/SessionRepository.hpp"

#include <QFileInfo>
#include <QFile>
#include <QDir>
#include <QJsonArray>
#include <QJsonDocument>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QUrl>
#include <QUuid>
#include <QtTest>
#include <algorithm>
#include <cmath>
#include <functional>
#include <limits>
#include <stdexcept>

using namespace breakwater;
using namespace breakwater::desktop;
using namespace breakwater::desktop::science;
using breakwater::desktop::history::SessionRepository;

#ifndef BREAKWATER_TEST_PYTHON
#define BREAKWATER_TEST_PYTHON ""
#endif

namespace {
QJsonArray numbers(const std::vector<double>& values) {
    QJsonArray result; for (const auto value : values) result.append(value); return result;
}
CapturedForecast capture() {
    auto document = example_project(); document.network = main_pump_failure(document.network);
    document.network = compile(document.network, incident_controls("mixed")).network;
    CapturedForecast result; result.network = document.network; result.config = {0.95, 0.3};
    const auto compiled = compile(result.network);
    result.result = simulate(compiled, result.config); result.validation = validate(compiled, result.result, result.config);
    result.id = QUuid::createUuid().toString(QUuid::WithoutBraces); result.physical_hash = physics_hash(document); result.revision = 7;
    return result;
}
QJsonObject evidence(const CapturedForecast& native, bool derivatives = false, double workerOffset = 0) {
    QJsonArray tankDescriptors, tankErrors, nativeVolumes, workerVolumes, indices;
    for (std::size_t i = 0; i < native.result.time_h.size(); ++i) indices.append(static_cast<qint64>(i));
    for (std::size_t i = 0; i < native.result.tank_ids.size(); ++i) {
        const auto id = QString::fromStdString(native.result.tank_ids[i]);
        const auto found = std::find_if(native.network.assets.begin(), native.network.assets.end(), [&](const Asset& a) { return a.id == native.result.tank_ids[i]; });
        tankDescriptors.append(QJsonObject{{"id", id}, {"label", QString::fromStdString(found->label)}, {"capacity_m3", found->capacity_m3}});
        const auto& values = native.result.volumes_m3[i]; nativeVolumes.append(numbers(values));
        QJsonArray worker; double absoluteMax = 0, scaledMax = 0;
        for (const auto value : values) {
            const auto predicted = value + workerOffset; worker.append(predicted);
            const auto absolute = std::abs(value - predicted);
            absoluteMax = std::max(absoluteMax, absolute);
            scaledMax = std::max(scaledMax, absolute / (1e-9 + 1e-10 * std::max(std::abs(value), std::abs(predicted))));
        }
        workerVolumes.append(worker);
        tankErrors.append(QJsonObject{{"tank_id", id}, {"max_absolute_error_m3", absoluteMax}, {"max_scaled_error", scaledMax}, {"within_tolerance", scaledMax <= 1}});
    }
    QJsonObject result{{"result_version", 1}, {"result_id", QUuid::createUuid().toString(QUuid::WithoutBraces)},
        {"created_at", "2026-11-08T02:35:00.000Z"}, {"method", "storage_rollout"}, {"run_id", native.id}, {"snapshot_id", native.id},
        {"physical_hash", native.physical_hash}, {"numerical_revision", static_cast<qint64>(native.revision)},
        {"horizon_h", native.config.horizon_h}, {"step_h", native.config.step_h},
        {"analysis_mode", derivatives ? "sensitivities" : "forward"}, {"selected_controls", QJsonArray{}},
        {"native_solver_version", QString::fromLatin1(solver_version)}, {"native_cost", native.result.cost},
        {"worker_cost", native.result.cost}, {"cost_difference", 0.0}, {"cost_absolute_error", 0.0}, {"cost_scaled_error", 0.0},
        {"within_tolerance", workerOffset == 0}, {"absolute_tolerance", 1e-9}, {"relative_tolerance", 1e-10},
        {"comparison_scope", "Returned worker preview samples versus their original native indices; not a feasibility verdict"},
        {"native_volume_feasible", native.validation.feasible}, {"tanks", tankDescriptors}, {"tank_errors", tankErrors},
        {"provenance", QJsonObject{{"implementation", "torch_storage_float64"}, {"implementation_version", "test-fixture-1"},
            {"dtype", "float64"}, {"device", "cpu"}, {"execution_status", "completed"}}}, {"warnings", QJsonArray{}},
        {"preview", QJsonObject{{"time_h", numbers(native.result.time_h)}, {"time_indices", indices},
            {"native_volumes_m3", nativeVolumes}, {"worker_volumes_m3", workerVolumes}, {"source_frames", static_cast<qint64>(native.result.time_h.size())}}}};
    if (derivatives) {
        QJsonArray selected, ids;
        const auto catalog = control_catalog(native.network);
        for (const auto& control : catalog) if (control.id == "source:source_a:curtail" || control.id == "route:bypass:flow") {
            selected.append(QJsonObject{{"control_id", QString::fromStdString(control.id)}, {"captured_value", control.value}, {"unit", QString::fromStdString(control.unit)}});
            ids.append(QString::fromStdString(control.id));
        }
        result["selected_controls"] = selected;
        result["derivatives"] = QJsonObject{{"control_ids", ids}, {"terminal_volume_jacobian_h", QJsonArray{QJsonArray{-0.95, -0.95}, QJsonArray{0.0, 0.95}}},
            {"cost_gradient", QJsonArray{237.5, 0.0}}};
    }
    return result;
}
bool rejects(const QJsonObject& result, const QJsonObject& archive) {
    auto containing = archive; containing["scientific_evidence"] = QJsonArray{result};
    QString error; return !validate_storage_science_evidence(result, containing, &error) && !error.isEmpty();
}
}

class StorageScienceEvidenceTests final : public QObject {
    Q_OBJECT
private slots:
    void standaloneArchiveRoundTripsForwardAndSensitivities() {
        const auto native = capture();
        for (const bool derivatives : {false, true}) {
            const auto result = evidence(native, derivatives), archive = storage_science_archive(native, result);
            QString error;
            QVERIFY2(SessionRepository::validateArchive(archive, &error), qPrintable(error));
            QCOMPARE(archive.value("session_id"), result.value("result_id"));
            QCOMPARE(archive.value("scientific_evidence_version").toInt(), 1);
            QCOMPARE(archive.value("scientific_evidence").toArray(), QJsonArray{result});
            QCOMPARE(archive.value("runs").toArray().first().toObject().value("time_h"), numbers(native.result.time_h));
            QVERIFY(archive.value("recommendation").toObject().isEmpty()); QVERIFY(archive.value("approvals").toArray().isEmpty());
            QVERIFY(!archive.value("actionable").toBool(true));
            const auto restored = QJsonDocument::fromJson(QJsonDocument(archive).toJson()).object();
            QVERIFY(validate_storage_science_evidence(result, restored));
            QCOMPARE(restored, archive);
        }
    }
    void mismatchRemainsValidEvidenceWithoutBecomingAgreement() {
        const auto native = capture(); const auto result = evidence(native, false, 1);
        const auto archive = storage_science_archive(native, result);
        QVERIFY(validate_storage_science_evidence(result, archive)); QVERIFY(!result.value("within_tolerance").toBool());
        auto forged = result; forged["within_tolerance"] = true; QVERIFY(rejects(forged, archive));
    }
    void rejectsVersionIdentityTimeAndAuthorityClaims_data() {
        QTest::addColumn<QString>("field"); QTest::addColumn<QJsonValue>("value");
        const QList<QPair<QString, QJsonValue>> rows{
            {"result_version", 2}, {"result_id", "not-a-uuid"}, {"created_at", "bad-time"},
            {"created_at", "2026-11-07T20:35:00-06:00"}, {"run_id", "unknown"}, {"snapshot_id", "unknown"},
            {"physical_hash", QString(64, 'a')}, {"numerical_revision", -1}, {"numerical_revision", 0.5}, {"numerical_revision", 8},
            {"horizon_h", 1.0}, {"step_h", 0.4}, {"native_solver_version", "unsupported"}, {"analysis_mode", "unknown"},
            {"native_cost", 1.0}, {"cost_difference", 1.0}, {"cost_absolute_error", 1.0}, {"cost_scaled_error", 1.0},
            {"within_tolerance", false}, {"absolute_tolerance", 1.0}, {"relative_tolerance", 1.0},
            {"native_volume_feasible", false}, {"comparison_scope", "Approved"}, {"actionable", true},
            {"authorization", "synthetic-test-only"}, {"reasoning", "synthetic-test-only"}, {"artifact", "not-permitted"}};
        for (qsizetype i = 0; i < rows.size(); ++i) QTest::newRow(qPrintable(rows[i].first + QString::number(i))) << rows[i].first << rows[i].second;
    }
    void rejectsVersionIdentityTimeAndAuthorityClaims() {
        QFETCH(QString, field); QFETCH(QJsonValue, value);
        const auto native = capture(); auto result = evidence(native); const auto archive = storage_science_archive(native, result);
        result[field] = value; QVERIFY2(rejects(result, archive), qPrintable(field));
    }
    void rejectsMalformedPreviewAndNativeBinding() {
        const auto native = capture(); const auto original = evidence(native); const auto archive = storage_science_archive(native, original);
        auto result = original; auto preview = result.value("preview").toObject(); preview["source_frames"] = 4; result["preview"] = preview; QVERIFY(rejects(result, archive));
        result = original; preview = result.value("preview").toObject(); auto indices = preview.value("time_indices").toArray(); indices[1] = 0; preview["time_indices"] = indices; result["preview"] = preview; QVERIFY(rejects(result, archive));
        result = original; preview = result.value("preview").toObject(); auto times = preview.value("time_h").toArray(); times[1] = 0.31; preview["time_h"] = times; result["preview"] = preview; QVERIFY(rejects(result, archive));
        result = original; preview = result.value("preview").toObject(); auto volumes = preview.value("native_volumes_m3").toArray(); auto samples = volumes[0].toArray(); samples[1] = samples[1].toDouble() + 1; volumes[0] = samples; preview["native_volumes_m3"] = volumes; result["preview"] = preview; QVERIFY(rejects(result, archive));
        result = original; preview = result.value("preview").toObject(); preview["worker_volumes_m3"] = QJsonArray{}; result["preview"] = preview; QVERIFY(rejects(result, archive));
        result = original; auto errors = result.value("tank_errors").toArray(); auto diagnostic = errors[0].toObject(); diagnostic["max_absolute_error_m3"] = 1; errors[0] = diagnostic; result["tank_errors"] = errors; QVERIFY(rejects(result, archive));
        auto altered = archive; auto runs = altered.value("runs").toArray(); auto details = runs[0].toObject(); auto fullTimes = details.value("time_h").toArray(); fullTimes[1] = 0.31; details["time_h"] = fullTimes; runs[0] = details; altered["runs"] = runs; QVERIFY(rejects(original, altered));
        auto foreign = capture(); QVERIFY(rejects(original, storage_science_archive(foreign, evidence(foreign))));
    }
    void rejectsDerivativeShapeOrderValuesAndBoundaries() {
        const auto native = capture(); const auto original = evidence(native, true); const auto archive = storage_science_archive(native, original);
        auto result = original; result.remove("derivatives"); QVERIFY(rejects(result, archive));
        result = original; auto derivative = result.value("derivatives").toObject(); auto ids = derivative.value("control_ids").toArray(); const QJsonValue firstId = ids[0]; ids[0] = ids[1]; ids[1] = firstId; derivative["control_ids"] = ids; result["derivatives"] = derivative; QVERIFY(rejects(result, archive));
        result = original; derivative = result.value("derivatives").toObject(); derivative["cost_gradient"] = QJsonArray{1}; result["derivatives"] = derivative; QVERIFY(rejects(result, archive));
        result = original; derivative = result.value("derivatives").toObject(); derivative["terminal_volume_jacobian_h"] = QJsonArray{QJsonArray{1, 2}}; result["derivatives"] = derivative; QVERIFY(rejects(result, archive));
        result = original; auto selected = result.value("selected_controls").toArray(); auto column = selected[0].toObject(); column["captured_value"] = 11; selected[0] = column; result["selected_controls"] = selected; QVERIFY(rejects(result, archive));
        result = original; selected = result.value("selected_controls").toArray(); column = selected[0].toObject(); column["unit"] = "wrong"; selected[0] = column; result["selected_controls"] = selected; QVERIFY(rejects(result, archive));
        result = original; selected = result.value("selected_controls").toArray(); const QJsonValue firstColumn = selected[0]; selected[0] = selected[1]; selected[1] = firstColumn; result["selected_controls"] = selected; QVERIFY(rejects(result, archive));
        result = original; result["analysis_mode"] = "forward"; QVERIFY(rejects(result, archive));
        result = original; selected = result.value("selected_controls").toArray(); column = selected[1].toObject(); column["control_id"] = "route:truck_route:flow"; column["captured_value"] = 22; selected[1] = column;
        result["selected_controls"] = selected; derivative = result.value("derivatives").toObject(); ids = derivative.value("control_ids").toArray(); ids[1] = "route:truck_route:flow"; derivative["control_ids"] = ids; result["derivatives"] = derivative; QVERIFY(rejects(result, archive));
    }
    void rejectsProvenanceWarningsNestingAndOversize() {
        const auto native = capture(); const auto original = evidence(native); const auto archive = storage_science_archive(native, original);
        for (const auto field : {"device", "dtype", "execution_status", "implementation"}) {
            auto result = original; auto provenance = result.value("provenance").toObject(); provenance[field] = "wrong"; result["provenance"] = provenance; QVERIFY(rejects(result, archive));
        }
        auto result = original; auto provenance = result.value("provenance").toObject(); provenance["implementation_version"] = "C:/private/path"; result["provenance"] = provenance; QVERIFY(rejects(result, archive));
        result = original; provenance = result.value("provenance").toObject(); provenance["authorization"] = "synthetic-only"; result["provenance"] = provenance; QVERIFY(rejects(result, archive));
        result = original; QJsonArray warnings; for (int i = 0; i < 33; ++i) warnings.append("warning"); result["warnings"] = warnings; QVERIFY(rejects(result, archive));
        result = original; result["warnings"] = QJsonArray{QString(1001, 'x')}; QVERIFY(rejects(result, archive));
        result = original; result["extra"] = QString(maximumStorageScienceEvidenceBytes, 'x'); QVERIFY(rejects(result, archive));
    }
    void archiveFactoryRejectsCrossCapture() {
        const auto first = capture(), second = capture();
        QVERIFY_EXCEPTION_THROWN(storage_science_archive(second, evidence(first)), std::invalid_argument);
    }
    void scientificEnvelopeRemainsNonActionableAndImmutable() {
        const auto native = capture(); const auto original = evidence(native);
        const auto archive = storage_science_archive(native, original);
        for (const auto field : {"session_id", "session_kind", "status", "created_at", "updated_at"}) {
            auto altered = archive; altered[field] = "altered"; QVERIFY2(rejects(original, altered), field);
        }
        auto altered = archive; altered["checkpoint_sequence"] = 2; QVERIFY(rejects(original, altered));
        altered = archive; altered["actionable"] = true; QVERIFY(rejects(original, altered));
        altered = archive; altered["approvals"] = QJsonArray{QJsonObject{{"status", "approved"}}}; QVERIFY(rejects(original, altered));
        altered = archive; altered["recommendation"] = QJsonObject{{"actionable", true}}; QVERIFY(rejects(original, altered));
        altered = archive; altered["historical"] = true; altered["recommendation"] = QJsonObject{{"actionable", false}, {"approval_status", "historical"}};
        QVERIFY(validate_storage_science_evidence(original, altered)); QVERIFY(SessionRepository::validateArchive(altered));
    }
    void exactTerminalTailAndLargeStepRemainReadable() {
        for (const auto configuration : {SimulationConfig{std::nextafter(10.0 * 0.3, std::numeric_limits<double>::infinity()), 0.3},
                                         SimulationConfig{0.95, 100.0}}) {
            auto native = capture(); native.config = configuration; const auto compiled = compile(native.network);
            native.result = simulate(compiled, configuration); native.validation = validate(compiled, native.result, configuration);
            const auto result = evidence(native), archive = storage_science_archive(native, result);
            QVERIFY(validate_storage_science_evidence(result, archive));
            QCOMPARE(native.result.time_h.back(), configuration.horizon_h);
        }
    }
    void realWorkerForwardAndSelectedDerivativesFitArchive() {
        const auto python = QString::fromUtf8(BREAKWATER_TEST_PYTHON);
        if (!QFileInfo(python).isFile()) QSKIP("Configure the actual CPU scientific runtime for archive integration.");
        QTemporaryDir directory; QVERIFY(directory.isValid()); const auto path = directory.filePath("science.sqlite");
        QJsonObject archive, result;
        {
            SessionRepository repository(path); QSignalSpy ready(&repository, &SessionRepository::ready), saved(&repository, &SessionRepository::sessionSaved);
            QTRY_COMPARE_WITH_TIMEOUT(ready.size(), 1, 10000);
            ProjectController project; project.loadExample("mixed"); project.forecastWithConfig({0.95, 0.3});
            QTRY_VERIFY_WITH_TIMEOUT(!project.busy(), 10000); const auto native = project.capturedForecast(); QVERIFY(native);
            PythonWorkerClient worker(python, QStringLiteral(BREAKWATER_SOURCE_DIR "/science/worker.py"));
            StorageScienceController science(&project, &worker); science.setHistoryRepository(&repository);
            QSignalSpy completed(&science, &StorageScienceController::completed);
            science.run(); QTRY_COMPARE_WITH_TIMEOUT(completed.size(), 1, 70000); QTRY_COMPARE_WITH_TIMEOUT(saved.size(), 1, 10000);
            QVERIFY(!science.historyPending()); QVERIFY(science.historyStatus().contains("saved"));
            QVERIFY(science.selectControl("source:source_a:curtail", true)); QVERIFY(science.selectControl("route:bypass:flow", true));
            science.runSensitivities(); QTRY_COMPARE_WITH_TIMEOUT(completed.size(), 2, 70000); QTRY_COMPARE_WITH_TIMEOUT(saved.size(), 2, 10000);
            QVERIFY(!science.historyPending());
            result = completed.last().first().toJsonObject(); archive = storage_science_archive(*native, result);
            QVERIFY(SessionRepository::validateArchive(archive)); QCOMPARE(result.value("analysis_mode").toString(), "sensitivities");
        }
        SessionRepository reopened(path); QSignalSpy ready(&reopened, &SessionRepository::ready), loaded(&reopened, &SessionRepository::sessionLoaded),
            operations(&reopened, &SessionRepository::operationCompleted), errors(&reopened, &SessionRepository::failed);
        QTRY_COMPARE_WITH_TIMEOUT(ready.size(), 1, 10000); const auto id = result.value("result_id").toString();
        reopened.loadSession(id); QTRY_COMPARE_WITH_TIMEOUT(loaded.size(), 1, 10000);
        auto restored = loaded.last().first().toJsonObject();
        QCOMPARE(restored.value("scientific_evidence").toArray(), QJsonArray{result}); QCOMPARE(restored.value("runs"), archive.value("runs"));
        QVERIFY(restored.value("historical").toBool()); QVERIFY(!restored.value("actionable").toBool(true));
        QVERIFY(!restored.value("recommendation").toObject().value("actionable").toBool(true));
        const auto exportPath = directory.filePath("science.json");
        reopened.exportSession(id, QUrl::fromLocalFile(exportPath));
        QTRY_VERIFY_WITH_TIMEOUT(QFileInfo(exportPath).isFile(), 10000);
        QTRY_VERIFY_WITH_TIMEOUT(!operations.isEmpty() && operations.last().at(0).toString() == "export", 10000);
        auto replacement = archive; replacement["checkpoint_sequence"] = 2;
        reopened.saveSession(replacement); QTRY_COMPARE_WITH_TIMEOUT(errors.size(), 1, 10000);
        reopened.loadSession(id); QTRY_COMPARE_WITH_TIMEOUT(loaded.size(), 2, 10000);
        QCOMPARE(loaded.last().first().toJsonObject().value("scientific_evidence"), archive.value("scientific_evidence"));
        SessionRepository imported(directory.filePath("import.sqlite"));
        QSignalSpy importReady(&imported, &SessionRepository::ready), importRead(&imported, &SessionRepository::sessionLoaded), importError(&imported, &SessionRepository::failed);
        QTRY_COMPARE_WITH_TIMEOUT(importReady.size(), 1, 10000);
        imported.importSession(QUrl::fromLocalFile(exportPath)); QTRY_COMPARE_WITH_TIMEOUT(importRead.size(), 1, 10000);
        QCOMPARE(importRead.last().first().toJsonObject().value("scientific_evidence"), archive.value("scientific_evidence"));
        QVERIFY(importRead.last().first().toJsonObject().value("historical").toBool());
        auto malformed = archive; auto badResult = result; badResult["within_tolerance"] = false; malformed["scientific_evidence"] = QJsonArray{badResult};
        const auto malformedPath = directory.filePath("malformed.json"); QFile file(malformedPath); QVERIFY(file.open(QIODevice::WriteOnly));
        const auto bytes = QJsonDocument(malformed).toJson(); QCOMPARE(file.write(bytes), bytes.size()); file.close();
        imported.importSession(QUrl::fromLocalFile(malformedPath)); QTRY_COMPARE_WITH_TIMEOUT(importError.size(), 1, 10000);
        imported.loadSession(id); QTRY_COMPARE_WITH_TIMEOUT(importRead.size(), 2, 10000);
        QCOMPARE(importRead.last().first().toJsonObject().value("scientific_evidence"), archive.value("scientific_evidence"));
        const auto artifactDirectory = qEnvironmentVariable("BREAKWATER_TEST_ARTIFACT_DIR");
        if (!artifactDirectory.isEmpty()) {
            QVERIFY(QDir().mkpath(artifactDirectory));
            const auto artifactPath = QDir(artifactDirectory).filePath("scientific-history.json");
            const auto previousOperations = operations.size();
            reopened.exportSession(id, QUrl::fromLocalFile(artifactPath));
            QTRY_VERIFY_WITH_TIMEOUT(operations.size() > previousOperations && operations.last().at(0).toString() == "export", 10000);
            QFile artifact(artifactPath); QVERIFY(artifact.open(QIODevice::ReadOnly));
            const auto exported = QJsonDocument::fromJson(artifact.readAll()).object();
            QCOMPARE(exported.value("scientific_evidence"), archive.value("scientific_evidence"));
            QCOMPARE(exported.value("runs"), archive.value("runs"));
        }
    }
    void checkpointFailuresAreCorrelatedAndDoNotStopLaterSaves() {
        QTemporaryDir directory; QVERIFY(directory.isValid());
        SessionRepository repository(directory.filePath("science.sqlite"));
        QSignalSpy ready(&repository, &SessionRepository::ready), saved(&repository, &SessionRepository::sessionSaved),
            errors(&repository, &SessionRepository::failed), checkpointErrors(&repository, &SessionRepository::checkpointFailed);
        QTRY_COMPARE_WITH_TIMEOUT(ready.size(), 1, 10000);
        const auto native = capture(); const auto result = evidence(native), archive = storage_science_archive(native, result);
        const auto producerId = QUuid::createUuid().toString(QUuid::WithoutBraces), id = result.value("result_id").toString();
        repository.enqueueCheckpoint([]() -> QJsonObject { throw std::runtime_error("Synthetic capture failure"); }, producerId);
        auto malformed = archive; malformed["checkpoint_sequence"] = 2;
        repository.enqueueCheckpoint([malformed] { return malformed; }, id);
        repository.enqueueCheckpoint([archive] { return archive; }, id);
        QTRY_COMPARE_WITH_TIMEOUT(saved.size(), 1, 10000);
        QCOMPARE(errors.size(), 2); QCOMPARE(checkpointErrors.size(), 2);
        QCOMPARE(checkpointErrors[0][0].toString(), producerId); QCOMPARE(checkpointErrors[1][0].toString(), id);
        QCOMPARE(saved[0][0].toString(), id);
        QCOMPARE(checkpointErrors[0][1].toString(), "storage_failed");
        QCOMPARE(checkpointErrors[1][1].toString(), "invalid_archive");
    }
    void unrelatedImportFailurePreservesScientificSaveAcknowledgement_data() {
        QTest::addColumn<bool>("beforeSave");
        QTest::newRow("import-before-scientific-checkpoint") << true;
        QTest::newRow("import-after-scientific-checkpoint") << false;
    }
    void unrelatedImportFailurePreservesScientificSaveAcknowledgement() {
        QFETCH(bool, beforeSave);
        const auto python = QString::fromUtf8(BREAKWATER_TEST_PYTHON);
        if (!QFileInfo(python).isFile()) QSKIP("Configure the actual CPU scientific runtime for archive integration.");
        QTemporaryDir directory; QVERIFY(directory.isValid());
        const auto malformedPath = directory.filePath("malformed.json");
        QFile file(malformedPath); QVERIFY(file.open(QIODevice::WriteOnly)); QCOMPARE(file.write("{"), 1); file.close();
        SessionRepository repository(directory.filePath("science.sqlite"));
        QSignalSpy ready(&repository, &SessionRepository::ready), saved(&repository, &SessionRepository::sessionSaved),
            errors(&repository, &SessionRepository::failed), checkpointErrors(&repository, &SessionRepository::checkpointFailed);
        QTRY_COMPARE_WITH_TIMEOUT(ready.size(), 1, 10000);
        QStringList order;
        connect(&repository, &SessionRepository::failed, &repository, [&order] { order.append("error"); });
        connect(&repository, &SessionRepository::sessionSaved, &repository, [&order] { order.append("saved"); });
        ProjectController project; project.loadExample("mixed"); project.forecastWithConfig({0.95, 0.3});
        QTRY_VERIFY_WITH_TIMEOUT(!project.busy(), 10000); QVERIFY(project.capturedForecast());
        const auto native = project.capturedForecast(); const auto document = project.document(); const auto canApprove = project.canApprove();
        PythonWorkerClient worker(python, QStringLiteral(BREAKWATER_SOURCE_DIR "/science/worker.py"));
        StorageScienceController science(&project, &worker); science.setHistoryRepository(&repository);
        QSignalSpy completed(&science, &StorageScienceController::completed);
        bool queued = false;
        if (beforeSave) {
            // storageReady starts asynchronous comparison first. Queue the import
            // before its finished event can enqueue the scientific checkpoint.
            connect(&worker, &PythonWorkerClient::storageReady, &science, [&] {
                if (!queued) { queued = true; repository.importSession(QUrl::fromLocalFile(malformedPath)); }
            });
        } else {
            // Accepted evidence is enqueued before changed observers run.
            connect(&science, &StorageScienceController::changed, &science, [&] {
                if (!queued && !science.result().isEmpty() && science.historyPending()) {
                    queued = true; repository.importSession(QUrl::fromLocalFile(malformedPath));
                }
            });
        }
        science.run(); QTRY_COMPARE_WITH_TIMEOUT(completed.size(), 1, 70000);
        QTRY_VERIFY_WITH_TIMEOUT(saved.size() == 1 && errors.size() == 1, 10000);
        QVERIFY(queued); QVERIFY(checkpointErrors.isEmpty());
        QCOMPARE(order, beforeSave ? QStringList({"error", "saved"}) : QStringList({"saved", "error"}));
        QVERIFY(!science.historyPending()); QVERIFY(!science.historyFailed());
        QCOMPARE(science.historySavedResultId(), science.result().value("result_id").toString());
        QVERIFY(science.historyStatus().contains("saved")); QVERIFY(science.canRun());
        QCOMPARE(project.capturedForecast(), native); QCOMPARE(project.document(), document); QCOMPARE(project.canApprove(), canApprove);
    }
    void storageFailureKeepsCompletedScienceAndNativeForecastAvailable() {
        const auto python = QString::fromUtf8(BREAKWATER_TEST_PYTHON);
        if (!QFileInfo(python).isFile()) QSKIP("Configure the actual CPU scientific runtime for archive integration.");
        QTemporaryDir directory; QVERIFY(directory.isValid()); const auto blocked = directory.filePath("not-a-directory");
        QFile file(blocked); QVERIFY(file.open(QIODevice::WriteOnly)); file.write("synthetic fixture"); file.close();
        SessionRepository repository(blocked + "/science.sqlite"); QSignalSpy errors(&repository, &SessionRepository::failed),
            checkpointErrors(&repository, &SessionRepository::checkpointFailed);
        QTRY_VERIFY_WITH_TIMEOUT(!errors.isEmpty(), 10000);
        ProjectController project; project.loadExample("mixed"); project.forecast(); QTRY_VERIFY_WITH_TIMEOUT(!project.busy(), 10000);
        QVERIFY(project.capturedForecast());
        PythonWorkerClient worker(python, QStringLiteral(BREAKWATER_SOURCE_DIR "/science/worker.py"));
        StorageScienceController science(&project, &worker); science.setHistoryRepository(&repository);
        QSignalSpy completed(&science, &StorageScienceController::completed);
        science.run(); QTRY_COMPARE_WITH_TIMEOUT(completed.size(), 1, 70000);
        QTRY_VERIFY_WITH_TIMEOUT(errors.size() >= 2 && !science.historyPending(), 10000);
        QCOMPARE(checkpointErrors.size(), 1);
        QCOMPARE(checkpointErrors[0][0].toString(), science.result().value("result_id").toString());
        QVERIFY(science.historyFailed());
        QVERIFY(!science.result().isEmpty()); QVERIFY(science.result().value("within_tolerance").toBool());
        QVERIFY(science.historyStatus().contains("error")); QVERIFY(science.canRun()); QVERIFY(project.canApprove());
        project.forecast(); QTRY_VERIFY_WITH_TIMEOUT(!project.busy(), 10000); QVERIFY(project.capturedForecast());
    }
};

QTEST_GUILESS_MAIN(StorageScienceEvidenceTests)
#include "storage_science_evidence_tests.moc"
