#include "telemetry/TelemetryContracts.hpp"
#include "ProjectDocument.hpp"
#include <QCoreApplication>
#include <QFile>
#include <QJsonDocument>
#include <cstdio>
#include <iostream>
#include <stdexcept>

// Test-only differential checker. This is not the G2 acquisition adapter.
int main(int argc, char** argv) {
    QCoreApplication application(argc, argv);
    using namespace breakwater::desktop::telemetry;
    QFile output;
    if (!output.open(stdout, QIODevice::WriteOnly)) return 2;
    constexpr qsizetype maximumRequest = 8 * 1024 * 1024;
    for (;;) {
        QByteArray line; char character = 0; bool any = false;
        while (std::cin.get(character)) {
            any = true; if (character == '\n') break;
            if (line.size() <= maximumRequest) line.append(character);
        }
        if (!any) break;
        QJsonObject result{{"valid", false}};
        if (line.size() <= maximumRequest) try {
            const auto request = decode_document(line, maximumRequest);
            const auto kind = request.value("kind").toString();
            if (kind.isEmpty() || (!request.contains("document") && !request.contains("raw_base64"))) throw std::invalid_argument("Invalid checker request.");
            const auto context = request.value("context").toObject();
            QJsonObject document;
            if (request.contains("raw_base64")) {
                const auto encoded = request.value("raw_base64").toString().toLatin1();
                const auto bytes = QByteArray::fromBase64(encoded);
                if (bytes.toBase64() != encoded) throw std::invalid_argument("Invalid checker base64.");
                document = decode_document(bytes, kind == "stream_frame" || kind == "stream_command" ? 256 * 1024 : maximumDocumentBytes);
            } else {
                if (!request.value("document").isObject()) throw std::invalid_argument("Checker document is not an object.");
                document = request.value("document").toObject();
            }
            validate_document(document, kind, context); result["valid"] = true;
            if (kind == "sensor_configuration") {
                result["semantic_utf8"] = QString::fromUtf8(sensor_semantic_bytes(document)); result["sha256"] = sensor_semantic_hash(document);
            }
            if (kind == "project_v2") {
                const auto project = breakwater::desktop::parse_project(document.value("project").toObject());
                result["project_physical_hash"] = breakwater::desktop::physics_hash(project);
                QJsonObject hashes;
                for (auto it = project.pipe_parameters.begin(); it != project.pipe_parameters.end(); ++it)
                    hashes[it.key()] = breakwater::desktop::quality::pipe_parameters_hash(it.value());
                result["project_pipe_parameter_hashes"] = hashes;
            }
            if (kind == "stream_frame" && context.contains("clock")) result["disposition"] = frame_disposition(document, context);
            if (kind == "observed_snapshot") result["snapshot_disposition"] = snapshot_disposition(document, context);
        } catch (const std::exception&) { result = {{"valid", false}}; }
        const auto bytes = QJsonDocument(result).toJson(QJsonDocument::Compact) + '\n';
        if (output.write(bytes) != bytes.size() || !output.flush()) return 3;
    }
    return 0;
}
