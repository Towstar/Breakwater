#pragma once
#include <QJsonArray>
#include <QJsonObject>
#include <QSet>
#include <cmath>

namespace breakwater::desktop::agent {
// Closed, bounded telemetry only. Never retain header values or error bodies.
inline QJsonObject sanitizedRetryMetadata(const QJsonObject& source) {
    static const QSet<QString> policies{"bounded_backoff", "invalid_retry_after_backoff",
        "retry_after_seconds", "retry_after_date", "retry_after_exceeds_budget"};
    const auto policy = source.value("retry_wait_policy"), delays = source.value("retry_delays_ms");
    if (!policy.isString() || !policies.contains(policy.toString()) || !delays.isArray() || delays.toArray().size() > 3) return {};
    for (const auto& value : delays.toArray()) {
        const auto delay = value.toDouble(-1);
        if (!value.isDouble() || !std::isfinite(delay) || delay < 0 || delay > 1215000 || std::floor(delay) != delay) return {};
    }
    return {{"retry_wait_policy", policy}, {"retry_delays_ms", delays}};
}
} // namespace breakwater::desktop::agent
