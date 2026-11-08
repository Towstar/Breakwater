#pragma once

#include "breakwater/telemetry.hpp"
#include <QByteArray>
#include <QJsonObject>
#include <QString>

namespace breakwater::desktop::telemetry {

inline constexpr qsizetype maximumDocumentBytes = 4 * 1024 * 1024;

// Pure interchange validation. Context is explicit; it never reads the editor,
// a process, a clock or a database. A missing network context does not establish
// physical attachment compatibility.
QJsonObject decode_document(const QByteArray& bytes, qsizetype maximum = maximumDocumentBytes);
void validate_document(const QJsonObject& document, const QString& kind, const QJsonObject& context = {});
breakwater::telemetry::SensorConfiguration parse_sensor_configuration(const QJsonObject& document, const QJsonObject& context = {});
breakwater::telemetry::StreamFrame parse_stream_frame(const QJsonObject& document, const QJsonObject& context = {});
breakwater::telemetry::StreamCommand parse_stream_command(const QJsonObject& document);
breakwater::telemetry::ObservedSnapshot parse_observed_snapshot(const QJsonObject& document, const QJsonObject& context = {});
breakwater::telemetry::DetectionEvent parse_detection_event(const QJsonObject& document, const QJsonObject& context = {});
QByteArray sensor_semantic_bytes(const QJsonObject& configuration);
QString sensor_semantic_hash(const QJsonObject& configuration);
QString frame_disposition(const QJsonObject& frame, const QJsonObject& context);
QString snapshot_disposition(const QJsonObject& snapshot, const QJsonObject& context = {});

} // namespace breakwater::desktop::telemetry
