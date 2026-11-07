#include "quality/PipeParameters.hpp"

#include <QJsonDocument>
#include <QtTest>
#include <cmath>
#include <limits>
#include <stdexcept>

using namespace breakwater;
using namespace breakwater::desktop::quality;

class PipeParameterTests final : public QObject {
    Q_OBJECT
private slots:
    void completeRoundTripAndStableFingerprint() {
        PipeParameters p;
        p.pipe = {15.0, 0.25, 0.75, 0.05, 128};
        p.initial_mg_l = 2.0; p.inlet_mg_l = 3.5; p.source_mg_l_h = 0.25;
        p.output_step_h = 0.05; p.cfl_safety = 0.8; p.boundary = TransportBoundary::closed_no_flux;
        const auto encoded = pipe_parameters_json(p);
        const auto restored = parse_pipe_parameters(QJsonDocument::fromJson(QJsonDocument(encoded).toJson()).object());
        QCOMPARE(pipe_parameters_json(restored), encoded);
        QCOMPARE(pipe_parameters_hash(restored), pipe_parameters_hash(p));
        QCOMPARE(pipe_parameters_hash(p).size(), 64);
        auto edited = p; edited.pipe.length_m = 16.0;
        QVERIFY(pipe_parameters_hash(edited) != pipe_parameters_hash(p));
        edited = p; edited.initial_mg_l = 2.5;
        QVERIFY(pipe_parameters_hash(edited) != pipe_parameters_hash(p));
        edited = p; edited.output_step_h = 0.025;
        QVERIFY(pipe_parameters_hash(edited) != pipe_parameters_hash(p));
        edited = p; edited.boundary = TransportBoundary::dirichlet_inlet;
        QVERIFY(pipe_parameters_hash(edited) != pipe_parameters_hash(p));
    }
    void missingUnknownAndWrongTypesAreRejected() {
        const auto valid = pipe_parameters_json(PipeParameters{});
        auto malformed = valid; malformed.remove("inlet_mg_l");
        QVERIFY_THROWS_EXCEPTION(std::invalid_argument, parse_pipe_parameters(malformed));
        malformed = valid; malformed["rendered_length"] = 100.0;
        QVERIFY_THROWS_EXCEPTION(std::invalid_argument, parse_pipe_parameters(malformed));
        malformed = valid; malformed["length_m"] = "100";
        QVERIFY_THROWS_EXCEPTION(std::invalid_argument, parse_pipe_parameters(malformed));
        malformed = valid; malformed["cells"] = true;
        QVERIFY_THROWS_EXCEPTION(std::invalid_argument, parse_pipe_parameters(malformed));
        malformed = valid; malformed["boundary"] = "silently_closed";
        QVERIFY_THROWS_EXCEPTION(std::invalid_argument, parse_pipe_parameters(malformed));
    }
    void finiteNumbersAndNativeCellBoundsArePreserved() {
        const auto valid = pipe_parameters_json(PipeParameters{});
        for (const auto field : {"length_m", "area_m2", "dispersion_m2_h", "decay_per_h", "initial_mg_l",
                 "inlet_mg_l", "source_mg_l_h", "output_step_h", "cfl_safety"}) {
            auto malformed = valid; malformed[field] = QJsonValue::Null;
            QVERIFY_THROWS_EXCEPTION(std::invalid_argument, parse_pipe_parameters(malformed));
            malformed[field] = -1.0;
            QVERIFY_THROWS_EXCEPTION(std::invalid_argument, parse_pipe_parameters(malformed));
        }
        for (const double cells : {0.0, 1.0, 2.5, 4097.0, 1e100}) {
            auto malformed = valid; malformed["cells"] = cells;
            QVERIFY_THROWS_EXCEPTION(std::invalid_argument, parse_pipe_parameters(malformed));
        }
        for (const double cells : {2.0, 4096.0}) {
            auto boundary = valid; boundary["cells"] = cells;
            QCOMPARE(parse_pipe_parameters(boundary).pipe.cells, static_cast<std::size_t>(cells));
        }
        for (const auto field : {"length_m", "area_m2", "output_step_h", "cfl_safety"}) {
            auto malformed = valid; malformed[field] = 0.0;
            QVERIFY_THROWS_EXCEPTION(std::invalid_argument, parse_pipe_parameters(malformed));
        }
        auto malformed = valid; malformed["cfl_safety"] = 1.01;
        QVERIFY_THROWS_EXCEPTION(std::invalid_argument, parse_pipe_parameters(malformed));
    }
    void nonfiniteCppInputsCannotSerializeOrHashAsNull() {
        for (const auto value : {std::numeric_limits<double>::quiet_NaN(), std::numeric_limits<double>::infinity()}) {
            PipeParameters p; p.pipe.length_m = value;
            QVERIFY_THROWS_EXCEPTION(std::invalid_argument, pipe_parameters_json(p));
            QVERIFY_THROWS_EXCEPTION(std::invalid_argument, pipe_parameters_hash(p));
            p = {}; p.initial_mg_l = value;
            QVERIFY_THROWS_EXCEPTION(std::invalid_argument, pipe_parameters_json(p));
        }
        PipeParameters p; p.boundary = static_cast<TransportBoundary>(100);
        QVERIFY_THROWS_EXCEPTION(std::invalid_argument, pipe_parameters_json(p));
    }
    void actualNativeExecutionConsumesTheSerializedPhysicalInputs() {
        PipeParameters p;
        p.pipe = {2.0, 0.5, 0.0, 0.0, 8};
        p.initial_mg_l = 2.0; p.inlet_mg_l = 2.0; p.output_step_h = 0.1;
        const auto restored = parse_pipe_parameters(pipe_parameters_json(p));
        const std::vector<double> initial(restored.pipe.cells, restored.initial_mg_l);
        const std::vector<double> source(restored.pipe.cells, restored.source_mg_l_h);
        const std::vector<FlowKnot> flow{{0.0, 1.0}};
        const std::vector<InletKnot> inlet{{0.0, restored.inlet_mg_l}};
        const auto result = transport(restored.pipe, initial, flow, inlet,
            {0.3, restored.output_step_h, restored.cfl_safety, restored.boundary}, source);
        QCOMPARE(result.time_h.front(), 0.0); QCOMPARE(result.time_h.back(), 0.3);
        for (const auto& frame : result.concentration_mg_l)
            for (const auto concentration : frame) QVERIFY(std::abs(concentration - 2.0) < 1e-12);
        QVERIFY(std::abs(result.inventory_mg.front() - 2000.0) < 1e-12);
        QVERIFY(result.diagnostics.max_mass_balance_residual_mg < 1e-9);
    }
};

QTEST_GUILESS_MAIN(PipeParameterTests)
#include "pipe_parameters_tests.moc"
