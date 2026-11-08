#include "agent/NemotronClient.hpp"
#include <QCoreApplication>
#include <QJsonDocument>
#include <QJsonParseError>
#include <QSet>
#include <QTextStream>
#include <QTimer>

using namespace breakwater::desktop::agent;
namespace {
bool exactJson(const QString& text) {
    const auto raw = text.toUtf8();
    if (raw.size() > 4096 || QString::fromUtf8(raw) != text) return false;
    QJsonParseError error; const auto document = QJsonDocument::fromJson(raw, &error);
    const QJsonObject expected{{"summary", "JSON-only protocol smoke"}, {"selected_plan_id", QJsonValue::Null},
        {"validation_id", QJsonValue::Null}, {"event_ids", QJsonArray{}}};
    if (error.error != QJsonParseError::NoError || !document.isObject() || document.object() != expected) return false;
    // QJsonDocument validates syntax; additionally reject duplicate/escaped-alias
    // root keys rather than accepting Qt's last-value representation.
    QSet<QString> keys; int depth = 0;
    for (qsizetype i = 0; i < raw.size(); ++i) {
        const auto c = raw[i];
        if (c == '{' || c == '[') ++depth;
        else if (c == '}' || c == ']') --depth;
        else if (c == '"') {
            const auto begin = i++;
            for (; i < raw.size(); ++i) { if (raw[i] == '\\') ++i; else if (raw[i] == '"') break; }
            auto after = i + 1; while (after < raw.size() && (raw[after] == ' ' || raw[after] == '\n' || raw[after] == '\r' || raw[after] == '\t')) ++after;
            if (depth == 1 && after < raw.size() && raw[after] == ':') {
                const auto key = QJsonDocument::fromJson('[' + raw.mid(begin, i - begin + 1) + ']').array().first().toString();
                if (keys.contains(key)) return false; keys.insert(key);
            }
        }
    }
    return keys.size() == 4;
}
}
int main(int argc, char** argv) {
    QCoreApplication app(argc, argv); app.setApplicationName("BREAKWATER"); app.setOrganizationName("Breakwater");
    if (app.arguments() != QStringList{app.arguments().first(), "--live"}) {
        QTextStream(stdout) << "{\"passed\":false,\"code\":\"live_not_enabled\",\"exit_code\":3}\n"; return 3;
    }
    const QString expectedModel("nvidia/nemotron-3.5-lightning-30b-a3b");
    NemotronClient client; bool finished = false; int requests = 0; QJsonObject metadata;
    const auto finish = [&](bool passed, const char* code) {
        if (finished) return; finished = true;
        const QJsonObject result{{"passed", passed}, {"code", code}, {"exit_code", passed ? 0 : 3},
            {"request_count", requests}, {"model_metadata", metadata}, {"scope", "Hosted JSON-only protocol; no EOG numerical result or control authority."}};
        QTextStream stream(stdout); stream << QJsonDocument(result).toJson(QJsonDocument::Compact) << '\n'; stream.flush(); app.exit(passed ? 0 : 3);
    };
    QObject::connect(&client, &ModelTransport::requestStarted, &app, [&] { ++requests; });
    QObject::connect(&client, &ModelTransport::requestCompleted, &app, [&](const QJsonObject& source) {
        metadata = {{"model_identity", client.lastResponseModel() == expectedModel ? expectedModel : QString{}},
            {"http_status", source.value("http_status")}, {"duration_ms", source.value("duration_ms")}, {"retry_count", source.value("retry_count")}};
    });
    QObject::connect(&client, &ModelTransport::requestFailed, &app, [&](const QString&, const QString&) { finish(false, "transport_failed"); });
    QObject::connect(&client, &ModelTransport::replyReady, &app, [&](const QJsonObject& reply) {
        if (client.lastResponseModel() != expectedModel) { finish(false, "model_identity_mismatch"); return; }
        if (reply.value("role") != "assistant" || !reply.value("content").isString() ||
            !reply.value("tool_calls").toArray().isEmpty() || !exactJson(reply.value("content").toString())) {
            finish(false, "json_contract_rejected"); return;
        }
        finish(true, "validated_json_only_completion");
    });
    QTimer::singleShot(0, &app, [&] {
        if (client.configuredModelIdentifier() != expectedModel) { finish(false, "configured_model_mismatch"); return; }
        client.requestFinalJson(QJsonArray{
            QJsonObject{{"role", "system"}, {"content", "This is a JSON protocol smoke only. Return no reasoning or tool calls."}},
            QJsonObject{{"role", "user"}, {"content", "Return exactly this JSON object: {\"summary\":\"JSON-only protocol smoke\",\"selected_plan_id\":null,\"validation_id\":null,\"event_ids\":[]}"}}});
    });
    QTimer::singleShot(65000, &app, [&] { finish(false, "overall_timeout"); client.cancel(); });
    return app.exec();
}
