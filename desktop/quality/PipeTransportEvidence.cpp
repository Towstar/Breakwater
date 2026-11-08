#include "PipeTransportEvidence.hpp"
#include "PipeParameters.hpp"

#include <QDateTime>
#include <QJsonArray>
#include <QJsonDocument>
#include <QRegularExpression>
#include <QSet>
#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>

namespace breakwater::desktop::quality {
namespace {
void require(bool condition, const char* message) {
    if (!condition) throw std::invalid_argument(message);
}

void keys(const QJsonObject& object, const QSet<QString>& required, const QSet<QString>& optional = {}) {
    for (const auto& key : required)
        require(object.contains(key), "Transport evidence is missing a required field.");
    for (auto it = object.begin(); it != object.end(); ++it)
        require(required.contains(it.key()) || optional.contains(it.key()), "Transport evidence contains an unsupported field.");
}

double number(const QJsonValue& value) {
    require(value.isDouble() && std::isfinite(value.toDouble()), "Transport evidence requires finite numeric values.");
    return value.toDouble();
}

qint64 integer(const QJsonValue& value, qint64 low, qint64 high) {
    const auto n = number(value);
    require(n >= static_cast<double>(low) && n <= static_cast<double>(high) && std::floor(n) == n,
        "Transport evidence contains an invalid integer or budget.");
    return static_cast<qint64>(n);
}

void identifier(const QJsonValue& value) {
    require(value.isString(), "Transport evidence identifiers must be strings.");
    const auto text = value.toString();
    require(!text.trimmed().isEmpty() && text.size() <= 256, "Transport evidence contains an invalid identifier.");
    for (const auto character : text)
        require(character.category() != QChar::Other_Control, "Transport evidence contains an invalid identifier.");
}

void hash(const QJsonValue& value) {
    static const QRegularExpression pattern(QStringLiteral("^[0-9a-f]{64}$"));
    require(value.isString() && pattern.match(value.toString()).hasMatch(), "Transport evidence contains an invalid hash.");
}

QJsonArray array(const QJsonValue& value, qsizetype low, qsizetype high) {
    require(value.isArray(), "Transport evidence requires numeric arrays.");
    const auto result = value.toArray();
    require(result.size() >= low && result.size() <= high, "Transport evidence array dimensions exceed the display contract.");
    return result;
}

QJsonArray numericArray(const QJsonValue& value, qsizetype size) {
    const auto result = array(value, size, size);
    for (const auto& element : result) (void)number(element);
    return result;
}

QJsonArray indices(const QJsonValue& value, qsizetype size, qint64 sourceSize) {
    const auto result = array(value, size, size);
    qint64 previous = -1;
    for (const auto& element : result) {
        const auto index = integer(element, 0, sourceSize - 1);
        require(index > previous, "Transport evidence sample indices must increase strictly.");
        previous = index;
    }
    require(result.first().toInteger() == 0 && result.last().toInteger() == sourceSize - 1,
        "Transport evidence must retain its native sample endpoints.");
    return result;
}

bool sameCellCenter(double actual, double expected) {
    const auto tolerance = 32.0 * std::numeric_limits<double>::epsilon() * std::max(std::abs(actual), std::abs(expected));
    return std::isfinite(expected) && std::abs(actual - expected) <= tolerance;
}
} // namespace

bool validate_pipe_transport_evidence(const QJsonObject& evidence, QString* error) {
    if (error) error->clear();
    try {
        require(QJsonDocument(evidence).toJson(QJsonDocument::Compact).size() <= maximumPipeEvidenceBytes,
            "Transport evidence exceeds the 1 MiB limit.");
        keys(evidence, {"result_version", "result_id", "created_at", "route_id", "run_id", "volume_run_id", "snapshot_id",
            "physical_hash", "parameter_hash", "numerical_revision", "method", "method_version", "quality_status",
            "horizon_h", "flow_m3h", "historical", "parameters", "preview", "diagnostics"}, {"volume_feasible"});
        require(integer(evidence.value("result_version"), 1, 1) == 1 &&
            evidence.value("method").toString() == "native_pipe_transport" &&
            evidence.value("method_version").toString() == QString::fromLatin1(transport_solver_version) &&
            evidence.value("quality_status").toString() == "native_scalar_reference",
            "Transport evidence has an unsupported version or method.");
        for (const auto key : {"result_id", "route_id", "run_id", "volume_run_id", "snapshot_id"}) identifier(evidence.value(key));
        require(evidence.value("run_id") == evidence.value("volume_run_id"), "Transport evidence volume-run identities disagree.");
        hash(evidence.value("physical_hash")); hash(evidence.value("parameter_hash"));
        (void)integer(evidence.value("numerical_revision"), 0, 9007199254740991LL);
        const auto timestamp = evidence.value("created_at");
        require(timestamp.isString() && timestamp.toString().size() <= 40 && timestamp.toString().endsWith('Z') &&
            QDateTime::fromString(timestamp.toString(), Qt::ISODateWithMs).isValid(),
            "Transport evidence requires a valid UTC creation time.");
        require(evidence.value("historical").isBool() &&
            (!evidence.contains("volume_feasible") || evidence.value("volume_feasible").isBool()),
            "Transport evidence context flags must be Boolean values.");
        require(evidence.value("parameters").isObject(), "Transport evidence requires complete physical parameters.");
        PipeParameters parameters;
        try { parameters = parse_pipe_parameters(evidence.value("parameters").toObject()); }
        catch (const std::invalid_argument&) { throw std::invalid_argument("Transport evidence contains invalid physical parameters."); }
        require(evidence.value("parameter_hash").toString() == pipe_parameters_hash(parameters),
            "Transport evidence parameters do not match their fingerprint.");
        const auto horizon = number(evidence.value("horizon_h")), flow = number(evidence.value("flow_m3h"));
        require(horizon > 0.0 && horizon <= 24.0 && flow >= 0.0 &&
            (parameters.boundary != TransportBoundary::closed_no_flux || flow == 0.0),
            "Transport evidence has an unsupported horizon, flow or boundary combination.");

        require(evidence.value("preview").isObject(), "Transport evidence requires a bounded preview.");
        const auto preview = evidence.value("preview").toObject();
        keys(preview, {"time_h", "x_m", "time_indices", "cell_indices", "concentration_mg_l", "outlet_mg_l", "inventory_mg",
            "cumulative_inlet_mg", "cumulative_outlet_mg", "cumulative_source_mg", "cumulative_decay_mg",
            "mass_balance_residual_mg", "source_frames", "source_cells", "sampling"});
        require(preview.value("sampling").toString() == "selected native frames and cell centers; no interpolation",
            "Transport evidence uses an unsupported preview sampling format.");
        const auto sourceFrames = integer(preview.value("source_frames"), 2, static_cast<qint64>(max_transport_frames));
        const auto sourceCells = integer(preview.value("source_cells"), 2, static_cast<qint64>(max_transport_cells));
        require(sourceCells == static_cast<qint64>(parameters.pipe.cells) && sourceFrames * sourceCells <= static_cast<qint64>(max_transport_values),
            "Transport evidence native dimensions disagree with its parameters or budget.");
        const auto times = array(preview.value("time_h"), 2, std::min<qint64>(256, sourceFrames));
        const auto positions = array(preview.value("x_m"), 2, std::min<qint64>(128, sourceCells));
        (void)indices(preview.value("time_indices"), times.size(), sourceFrames);
        const auto cellIndices = indices(preview.value("cell_indices"), positions.size(), sourceCells);
        double previous = -1.0;
        for (const auto& value : times) {
            const auto time = number(value);
            require(time > previous && time >= 0.0 && time <= horizon, "Transport evidence times must increase within the captured horizon.");
            previous = time;
        }
        require(times.first().toDouble() == 0.0 && times.last().toDouble() == horizon,
            "Transport evidence must include time zero and the exact horizon.");
        const auto dx = parameters.pipe.length_m / static_cast<double>(sourceCells);
        require(std::isfinite(dx) && dx > 0.0, "Transport evidence cell geometry is invalid.");
        previous = 0.0;
        for (qsizetype i = 0; i < positions.size(); ++i) {
            const auto position = number(positions[i]);
            const auto expected = (static_cast<double>(cellIndices[i].toInteger()) + 0.5) * dx;
            require(position > previous && position < parameters.pipe.length_m && sameCellCenter(position, expected),
                "Transport evidence positions must match ordered native cell centers.");
            previous = position;
        }
        const auto field = array(preview.value("concentration_mg_l"), times.size(), times.size());
        const auto outlet = numericArray(preview.value("outlet_mg_l"), times.size());
        for (qsizetype i = 0; i < field.size(); ++i) {
            const auto row = numericArray(field[i], positions.size());
            require(row.last() == outlet[i], "Transport evidence outlet values disagree with the retained outlet cell.");
        }
        for (const auto key : {"inventory_mg", "cumulative_inlet_mg", "cumulative_outlet_mg", "cumulative_source_mg",
                 "cumulative_decay_mg", "mass_balance_residual_mg"})
            (void)numericArray(preview.value(key), times.size());

        require(evidence.value("diagnostics").isObject(), "Transport evidence requires native diagnostics.");
        const auto diagnostics = evidence.value("diagnostics").toObject();
        keys(diagnostics, {"substeps", "max_cfl_number", "minimum_concentration_mg_l", "max_mass_balance_residual_mg"});
        const auto substeps = integer(diagnostics.value("substeps"), 1, static_cast<qint64>(max_transport_substeps));
        require(substeps <= static_cast<qint64>(max_transport_cell_updates) / sourceCells,
            "Transport evidence exceeds the native work budget.");
        const auto cfl = number(diagnostics.value("max_cfl_number"));
        require(cfl >= 0.0 && cfl <= parameters.cfl_safety + 64.0 * std::numeric_limits<double>::epsilon(),
            "Transport evidence CFL diagnostics exceed the captured safety bound.");
        (void)number(diagnostics.value("minimum_concentration_mg_l"));
        require(number(diagnostics.value("max_mass_balance_residual_mg")) >= 0.0,
            "Transport evidence requires a nonnegative peak residual diagnostic.");
        return true;
    } catch (const std::invalid_argument& problem) {
        if (error) *error = QString::fromLatin1(problem.what());
        return false;
    }
}

} // namespace breakwater::desktop::quality
