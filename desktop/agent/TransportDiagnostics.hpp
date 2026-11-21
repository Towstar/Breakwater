#pragma once
#include <QJsonArray>
#include <QJsonObject>
#include <QSet>
#include <cmath>
#include <algorithm>

namespace breakwater::desktop::agent {
inline bool diagnosticInteger(const QJsonValue& value, double maximum = 9007199254740991.0) {
    const auto n = value.toDouble(-1);
    return value.isDouble() && std::isfinite(n) && n >= 0 && n <= maximum && std::floor(n) == n;
}
inline bool diagnosticFields(const QJsonObject& value, const QSet<QString>& required, const QSet<QString>& optional = {}) {
    for (const auto& key : required) if (!value.contains(key)) return false;
    for (auto it = value.begin(); it != value.end(); ++it) if (!required.contains(it.key()) && !optional.contains(it.key())) return false;
    return true;
}
inline bool validTransportDiagnostics(const QJsonValue& value) {
    if (!value.isObject()) return false;
    const auto d = value.toObject();
    if (!diagnosticFields(d, {"version", "minimum_interval_ms", "backoff_base_ms", "backoff_jitter_ms", "terminal_cooldown_ms",
                            "admission_wait_ms", "cooldown_remaining_ms", "cooldown_capped", "attempts"}) ||
        !diagnosticInteger(d["version"], 1) || d["version"] != 1 ||
        !diagnosticInteger(d["minimum_interval_ms"], 60000) || !diagnosticInteger(d["backoff_base_ms"], 60000) || d["backoff_base_ms"] == 0 ||
        !diagnosticInteger(d["backoff_jitter_ms"], 5000) || !diagnosticInteger(d["terminal_cooldown_ms"], 86400000) ||
        !diagnosticInteger(d["admission_wait_ms"]) || !diagnosticInteger(d["cooldown_remaining_ms"], 86400000) ||
        !d["cooldown_capped"].isBool() || !d["attempts"].isArray() || d["attempts"].toArray().size() > 4) return false;
    if (d["cooldown_capped"] == true && d["cooldown_remaining_ms"] != 86400000) return false;
    qint64 previousEnd = 0; int index = 0;
    for (const auto& item : d["attempts"].toArray()) {
        if (!item.isObject()) return false;
        const auto a = item.toObject();
        if (!diagnosticFields(a, {"attempt", "started_ms", "duration_ms", "http_status"},
             {"retry_after_ms", "retry_after_capped", "rate_limits", "usage", "provider_error_code"}) ||
            !diagnosticInteger(a["attempt"], 4) || a["attempt"].toInt() != ++index ||
            !diagnosticInteger(a["started_ms"]) || !diagnosticInteger(a["duration_ms"]) ||
            a["started_ms"].toInteger() < previousEnd || !diagnosticInteger(a["http_status"], 599) ||
            (a["http_status"].toInt() != 0 && a["http_status"].toInt() < 100)) return false;
        previousEnd = a["started_ms"].toInteger() + a["duration_ms"].toInteger();
        if (previousEnd > 9007199254740991LL) return false;
        if (a.contains("retry_after_ms") != a.contains("retry_after_capped")) return false;
        if (a.contains("retry_after_ms") && (!diagnosticInteger(a["retry_after_ms"], 86400000) || !a["retry_after_capped"].isBool())) return false;
        if (a["retry_after_capped"] == true && a["retry_after_ms"] != 86400000) return false;
        for (const auto* group : {"rate_limits", "usage"}) {
            if (!a.contains(group)) continue;
            if (!a[group].isObject() || a[group].toObject().isEmpty()) return false;
            const auto values = a[group].toObject();
            const QSet<QString> names = QString::fromLatin1(group) == "usage"
                ? QSet<QString>{"prompt_tokens", "completion_tokens", "total_tokens"}
                : QSet<QString>{"request_limit", "request_remaining", "token_limit", "token_remaining"};
            if (!diagnosticFields(values, {}, names)) return false;
            for (auto it = values.begin(); it != values.end(); ++it) if (!diagnosticInteger(it.value(), 1000000000000.0)) return false;
        }
        if (a.contains("provider_error_code")) {
            const QSet<QString> codes{"rate_limit_exceeded", "insufficient_quota", "too_many_requests", "concurrency_limit_exceeded", "model_unavailable"};
            if (!a["provider_error_code"].isString() || !codes.contains(a["provider_error_code"].toString())) return false;
        }
    }
    return true;
}
inline QJsonObject sanitizedTransportDiagnostics(const QJsonObject& metadata) {
    const auto value = metadata.value("transport_diagnostics");
    return validTransportDiagnostics(value) ? QJsonObject{{"transport_diagnostics", value}} : QJsonObject{};
}
inline bool validTransportDiagnosticsForRequest(const QJsonObject& metadata) {
    const auto value = metadata.value("transport_diagnostics");
    if (!validTransportDiagnostics(value) || !diagnosticInteger(metadata.value("retry_count"), 3) ||
        !diagnosticInteger(metadata.value("http_status"), 599)) return false;
    const auto duration = metadata.value("duration_ms");
    if (!duration.isDouble() || !std::isfinite(duration.toDouble()) || duration.toDouble() < 0 ||
        duration.toDouble() > 9007199254740991.0) return false;
    const auto attempts = value.toObject()["attempts"].toArray();
    if (metadata["retry_count"].toInt() != std::max(0, int(attempts.size()) - 1)) return false;
    if (attempts.isEmpty()) return metadata["http_status"] == 0;
    const auto last = attempts.last().toObject();
    return last["http_status"] == metadata["http_status"] && duration.isDouble() && std::isfinite(duration.toDouble()) &&
        duration.toDouble() >= last["started_ms"].toDouble() + last["duration_ms"].toDouble();
}
}
