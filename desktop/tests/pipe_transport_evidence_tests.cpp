#include "quality/PipeTransportEvidence.hpp"
#include "quality/PipeParameters.hpp"

#include <QJsonArray>
#include <QJsonDocument>
#include <QtTest>
#include <limits>
#include <utility>

using namespace breakwater;
using namespace breakwater::desktop::quality;

namespace {
QJsonObject evidence() {
    PipeParameters parameters; parameters.pipe.cells = 2;
    return {{"result_version", 1}, {"result_id", "d8ad63e9-9011-4857-90e4-e2c21d970d2c"},
        {"created_at", "2026-11-07T12:00:00.000Z"}, {"route_id", "renamed:route"}, {"run_id", "run-id"},
        {"volume_run_id", "run-id"}, {"snapshot_id", "snapshot-id"}, {"physical_hash", QString(64, 'a')},
        {"parameter_hash", pipe_parameters_hash(parameters)}, {"numerical_revision", 7},
        {"method", "native_pipe_transport"}, {"method_version", QString::fromLatin1(transport_solver_version)},
        {"quality_status", "native_scalar_reference"}, {"horizon_h", 1.0}, {"flow_m3h", 0.0},
        {"historical", false}, {"volume_feasible", true}, {"parameters", pipe_parameters_json(parameters)},
        {"preview", QJsonObject{{"time_h", QJsonArray{0.0, 1.0}}, {"x_m", QJsonArray{25.0, 75.0}},
            {"time_indices", QJsonArray{0, 1}}, {"cell_indices", QJsonArray{0, 1}},
            {"concentration_mg_l", QJsonArray{QJsonArray{0.0, 0.0}, QJsonArray{0.0, -1e-15}}},
            {"outlet_mg_l", QJsonArray{0.0, -1e-15}}, {"inventory_mg", QJsonArray{0.0, -1e-12}},
            {"cumulative_inlet_mg", QJsonArray{0.0, -1.0}}, {"cumulative_outlet_mg", QJsonArray{0.0, 0.0}},
            {"cumulative_source_mg", QJsonArray{0.0, 0.0}}, {"cumulative_decay_mg", QJsonArray{0.0, 0.0}},
            {"mass_balance_residual_mg", QJsonArray{0.0, -1e-14}}, {"source_frames", 2}, {"source_cells", 2},
            {"sampling", "selected native frames and cell centers; no interpolation"}}},
        {"diagnostics", QJsonObject{{"substeps", 1}, {"max_cfl_number", 0.8},
            {"minimum_concentration_mg_l", -1e-15}, {"max_mass_balance_residual_mg", 1e-14}}}};
}

QJsonObject previewWith(const char* key, const QJsonValue& value) {
    auto result = evidence(), preview = result.value("preview").toObject();
    preview[key] = value; result["preview"] = preview; return result;
}
QJsonObject diagnosticWith(const char* key, const QJsonValue& value) {
    auto result = evidence(), diagnostics = result.value("diagnostics").toObject();
    diagnostics[key] = value; result["diagnostics"] = diagnostics; return result;
}
bool rejects(const QJsonObject& value) {
    QString error;
    return !validate_pipe_transport_evidence(value, &error) && !error.isEmpty();
}
}

class PipeTransportEvidenceTests final : public QObject {
    Q_OBJECT
private slots:
    void acceptsSignedValuesWithoutModifyingTheEvidence() {
        const auto original = evidence(); auto value = original;
        QString error = "old error";
        QVERIFY2(validate_pipe_transport_evidence(value, &error), qPrintable(error));
        QVERIFY(error.isEmpty()); QCOMPARE(value, original);
        value.remove("volume_feasible"); QVERIFY(validate_pipe_transport_evidence(value));
        value["volume_feasible"] = false; value["historical"] = true;
        QVERIFY(validate_pipe_transport_evidence(value));
        const auto restored = QJsonDocument::fromJson(QJsonDocument(value).toJson()).object();
        QVERIFY(validate_pipe_transport_evidence(restored)); QCOMPARE(restored, value);
    }
    void rejectsMissingUnknownAndMistypedFields() {
        const auto original = evidence();
        for (const auto& key : original.keys()) {
            if (key == "volume_feasible") continue;
            auto value = original; value.remove(key); QVERIFY2(rejects(value), qPrintable(key));
        }
        auto value = original; value["approval"] = "approved"; QVERIFY(rejects(value));
        for (const auto key : {"preview", "diagnostics"}) {
            const auto child = original.value(key).toObject();
            for (const auto& field : child.keys()) {
                auto changed = child; changed.remove(field); value = original; value[key] = changed;
                QVERIFY2(rejects(value), qPrintable(field));
            }
            auto changed = child; changed["unknown"] = 0; value = original; value[key] = changed; QVERIFY(rejects(value));
            value = original; value[key] = QJsonArray{}; QVERIFY(rejects(value));
        }
        value = original; value["parameters"] = "{}"; QVERIFY(rejects(value));
        value = original; value["historical"] = "false"; QVERIFY(rejects(value));
        value = original; value["volume_feasible"] = QJsonValue::Null; QVERIFY(rejects(value));
    }
    void rejectsUnsupportedVersionsAndMethods() {
        for (const auto& [key, invalid] : {std::pair<const char*, QJsonValue>{"result_version", 2},
                 {"result_version", "1"}, {"method", "learned_pipe_transport"}, {"method_version", "future-solver"},
                 {"quality_status", "qualified"}}) {
            auto value = evidence(); value[key] = invalid; QVERIFY2(rejects(value), key);
        }
        QVERIFY(rejects(previewWith("sampling", "interpolated")));
    }
    void rejectsMalformedIdentityAndTimestamp() {
        for (const auto key : {"result_id", "route_id", "run_id", "volume_run_id", "snapshot_id"})
            for (const auto& invalid : {QJsonValue(""), QJsonValue(QString(257, 'x')), QJsonValue("invalid\nidentifier"), QJsonValue(7)}) {
                auto value = evidence(); value[key] = invalid; QVERIFY2(rejects(value), key);
            }
        auto value = evidence(); value["run_id"] = "different-run"; QVERIFY(rejects(value));
        for (const auto key : {"physical_hash", "parameter_hash"})
            for (const auto& invalid : {QString(63, 'a'), QString(64, 'G'), QString(64, 'A')}) {
                value = evidence(); value[key] = invalid; QVERIFY2(rejects(value), key);
            }
        value = evidence(); value["parameter_hash"] = QString(64, 'b'); QVERIFY(rejects(value));
        for (const auto invalid : {"not a date", "2026-11-07T12:00:00.000", "2026-99-99T12:00:00.000Z"}) {
            value = evidence(); value["created_at"] = invalid; QVERIFY(rejects(value));
        }
    }
    void rejectsInvalidInputsAndNonNumericMetadata() {
        for (const auto& [key, invalid] : {std::pair<const char*, QJsonValue>{"horizon_h", 0}, {"horizon_h", 25},
                 {"horizon_h", "1"}, {"horizon_h", true}, {"flow_m3h", -1}, {"flow_m3h", QJsonValue::Null},
                 {"numerical_revision", -1}, {"numerical_revision", 0.5}, {"numerical_revision", 9007199254740992.0}}) {
            auto value = evidence(); value[key] = invalid; QVERIFY2(rejects(value), key);
        }
        auto value = evidence(), parameters = value.value("parameters").toObject();
        parameters["length_m"] = -1; value["parameters"] = parameters; QVERIFY(rejects(value));
        auto closed = PipeParameters{}; closed.pipe.cells = 2; closed.boundary = TransportBoundary::closed_no_flux;
        value = evidence(); value["parameters"] = pipe_parameters_json(closed); value["parameter_hash"] = pipe_parameters_hash(closed);
        QVERIFY(validate_pipe_transport_evidence(value));
        value["flow_m3h"] = 1.0; QVERIFY(rejects(value));
    }
    void rejectsInvalidNativeBudgetsAndPreviewDimensions() {
        for (const auto& [key, invalid] : {std::pair<const char*, QJsonValue>{"source_frames", 1}, {"source_frames", 10002},
                 {"source_frames", 2.5}, {"source_cells", 1}, {"source_cells", 4097}, {"source_cells", 3}})
            QVERIFY2(rejects(previewWith(key, invalid)), key);
        QVERIFY(rejects(previewWith("time_h", QJsonArray{0.0})));
        QVERIFY(rejects(previewWith("x_m", QJsonArray{})));
        QVERIFY(rejects(previewWith("concentration_mg_l", QJsonArray{QJsonArray{0.0, 0.0}})));
        QVERIFY(rejects(previewWith("concentration_mg_l", QJsonArray{QJsonArray{0.0}, QJsonArray{0.0}})));
        QJsonArray tooMany; for (int i = 0; i < 257; ++i) tooMany.append(i);
        auto value = previewWith("time_h", tooMany), preview = value.value("preview").toObject();
        preview["source_frames"] = 1000; value["preview"] = preview; QVERIFY(rejects(value));
        tooMany = {}; for (int i = 0; i < 129; ++i) tooMany.append(i);
        QVERIFY(rejects(previewWith("x_m", tooMany)));
        value = evidence(); auto parameters = PipeParameters{}; parameters.pipe.cells = 4096;
        value["parameters"] = pipe_parameters_json(parameters); value["parameter_hash"] = pipe_parameters_hash(parameters);
        preview = value.value("preview").toObject(); preview["source_cells"] = 4096; preview["source_frames"] = 10001;
        value["preview"] = preview; QVERIFY(rejects(value));
    }
    void rejectsOutOfOrderOrUnretainedNativeIndices() {
        for (const auto key : {"time_indices", "cell_indices"})
            for (const auto& invalid : {QJsonArray{0, 0}, QJsonArray{1, 0}, QJsonArray{0, 2}, QJsonArray{0, 0.5}, QJsonArray{0}})
                QVERIFY2(rejects(previewWith(key, invalid)), key);
        auto value = previewWith("time_indices", QJsonArray{0, 1}), preview = value.value("preview").toObject();
        preview["source_frames"] = 3; value["preview"] = preview; QVERIFY(rejects(value));
    }
    void rejectsInvalidTimesAndCellCenters() {
        for (const auto& times : {QJsonArray{0.1, 1.0}, QJsonArray{0.0, 0.9}, QJsonArray{0.0, 0.0},
                 QJsonArray{-1.0, 1.0}, QJsonArray{0.0, 2.0}, QJsonArray{0.0, "1"}})
            QVERIFY(rejects(previewWith("time_h", times)));
        for (const auto& positions : {QJsonArray{25.0, 25.0}, QJsonArray{75.0, 25.0}, QJsonArray{0.0, 75.0},
                 QJsonArray{25.0, 100.0}, QJsonArray{24.0, 75.0}, QJsonArray{"25", 75.0}})
            QVERIFY(rejects(previewWith("x_m", positions)));
    }
    void rejectsMissingNonfiniteAndInconsistentNumericalSamples() {
        for (const auto key : {"outlet_mg_l", "inventory_mg", "cumulative_inlet_mg", "cumulative_outlet_mg",
                 "cumulative_source_mg", "cumulative_decay_mg", "mass_balance_residual_mg"}) {
            QVERIFY2(rejects(previewWith(key, QJsonArray{0.0})), key);
            QVERIFY2(rejects(previewWith(key, QJsonArray{0.0, QJsonValue::Null})), key);
            QVERIFY2(rejects(previewWith(key, QJsonArray{0.0, "0"})), key);
            QVERIFY2(rejects(previewWith(key, QJsonArray{0.0, std::numeric_limits<double>::infinity()})), key);
        }
        QVERIFY(rejects(previewWith("concentration_mg_l", QJsonArray{QJsonArray{0.0, 0.0}, QJsonArray{QJsonValue::Null, -1e-15}})));
        QVERIFY(rejects(previewWith("outlet_mg_l", QJsonArray{0.0, 1.0})));
    }
    void rejectsInvalidDiagnosticsWithoutClippingNegativeMinimum() {
        for (const auto& [key, invalid] : {std::pair<const char*, QJsonValue>{"substeps", 0}, {"substeps", 1.5},
                 {"substeps", 1000001}, {"max_cfl_number", -1.0}, {"max_cfl_number", 0.91},
                 {"max_mass_balance_residual_mg", -1.0}, {"minimum_concentration_mg_l", QJsonValue::Null}})
            QVERIFY2(rejects(diagnosticWith(key, invalid)), key);
        auto value = diagnosticWith("minimum_concentration_mg_l", -1e-15);
        QVERIFY(validate_pipe_transport_evidence(value));
        QCOMPARE(value.value("diagnostics").toObject().value("minimum_concentration_mg_l").toDouble(), -1e-15);
    }
    void rejectsAnOversizedInMemoryDocument() {
        auto value = evidence(); value["unrecognized_payload"] = QString(maximumPipeEvidenceBytes, 'x');
        QString error;
        QVERIFY(!validate_pipe_transport_evidence(value, &error)); QVERIFY(error.contains("1 MiB"));
    }
};

QTEST_GUILESS_MAIN(PipeTransportEvidenceTests)
#include "pipe_transport_evidence_tests.moc"
