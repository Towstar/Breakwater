#include "AgentOrchestrator.hpp"
#include "NemotronClient.hpp"
#include "ProjectDocument.hpp"
#include <QCoreApplication>
#include <QJsonDocument>
#include <QTextStream>
#include <QTimer>

using namespace breakwater;
using namespace breakwater::desktop;
using namespace breakwater::desktop::agent;

int main(int argc, char** argv) {
    QCoreApplication app(argc, argv);
    QTextStream output(stdout);
    const auto config = NemotronConfig::fromEnvironment();
    if (config.apiKey.isEmpty()) {
        output << "NVIDIA_API_KEY is unavailable. No live request was made.\n";
        return 2;
    }
    NemotronClient client(config);
    auto project = example_project();
    project.network = main_pump_failure(project.network);
    auto gateway = std::make_shared<SimulationGateway>(project.network, physics_hash(project));
    if (app.arguments().contains("--investigate")) {
        AgentOrchestrator session(&client);
        QObject::connect(&session, &AgentOrchestrator::activity, &app, [&](const QJsonObject& event) {
            output << QString::fromUtf8(QJsonDocument(event).toJson(QJsonDocument::Compact)) << Qt::endl;
            if (event["type"] == "recommendation_ready" || event["type"] == "agent_failed") {
                output << "Response model: " << client.lastResponseModel() << Qt::endl;
                app.exit(event["type"] == "recommendation_ready" ? 0 : 1);
            }
        });
        QTimer::singleShot(0, &app, [&] { session.start(gateway, "The main transfer pump failed. Find a validated feasible low-cost response for the next three hours. Stage it for human review if useful."); });
        return app.exec();
    }
    // A deliberate protocol smoke, separate from the adaptive investigation above.
    QJsonArray messages{QJsonObject{{"role", "user"}, {"content", "Call get_network_state once to inspect this synthetic network. After receiving the actual tool result, describe its asset count in one concise sentence."}}};
    QJsonArray tools;
    for (const auto& value : SimulationGateway::toolDefinitions())
        if (value.toObject()["function"].toObject()["name"] == "get_network_state") tools.append(value);
    bool executed = false;
    QTimer deadline; deadline.setSingleShot(true); deadline.start(90000);
    QObject::connect(&deadline, &QTimer::timeout, &app, [&] { client.cancel(); output << "Live smoke deadline exceeded.\n"; app.exit(1); });
    QObject::connect(&client, &ModelTransport::requestFailed, &app, [&](const QString& code, const QString& message) {
        output << code << ": " << message << Qt::endl; app.exit(1);
    });
    QObject::connect(&client, &ModelTransport::replyReady, &app, [&](const QJsonObject& message) {
        if (client.lastResponseModel() != config.model) {
            output << "Model identity did not match configured model.\n"; app.exit(1); return;
        }
        const auto calls = message["tool_calls"].toArray();
        if (!executed) {
            if (calls.size() != 1 || calls.first().toObject()["function"].toObject()["name"] != "get_network_state") {
                output << "No expected model-issued structured tool call.\n"; app.exit(1); return;
            }
            const auto call = calls.first().toObject();
            QJsonParseError parseError;
            const auto args = QJsonDocument::fromJson(call["function"].toObject()["arguments"].toString().toUtf8(), &parseError);
            if (parseError.error != QJsonParseError::NoError || !args.isObject()) { app.exit(1); return; }
            const auto result = gateway->execute("get_network_state", args.object());
            if (!result["ok"].toBool()) { app.exit(1); return; }
            messages.append(message);
            messages.append(QJsonObject{{"role", "tool"}, {"tool_call_id", call["id"]}, {"content", QString::fromUtf8(QJsonDocument(result).toJson(QJsonDocument::Compact))}});
            executed = true;
            output << "Executed actual get_network_state; snapshot " << gateway->snapshotId() << Qt::endl;
            client.request(messages, tools);
        } else {
            if (!calls.isEmpty() || message["content"].toString().trimmed().isEmpty()) { app.exit(1); return; }
            output << "PASS: model " << client.lastResponseModel() << " issued a structured call, received its real result and continued.\n";
            // Do not print model reasoning or raw HTTP data.
            app.exit(0);
        }
    });
    QTimer::singleShot(0, &app, [&] { client.request(messages, tools); });
    return app.exec();
}
