#include "agent/NemotronClient.hpp"
#include "agent/CredentialSettings.hpp"

#include <QBuffer>
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QUuid>
#include <QtTest>
#include <algorithm>
#include <cstring>

using namespace breakwater::desktop::agent;

namespace {
struct ScopedConfiguration {
    QByteArray key = qgetenv("NVIDIA_API_KEY"), endpoint = qgetenv("NVIDIA_BASE_URL"), model = qgetenv("NEMOTRON_MODEL");
    bool hadKey = qEnvironmentVariableIsSet("NVIDIA_API_KEY"), hadEndpoint = qEnvironmentVariableIsSet("NVIDIA_BASE_URL"), hadModel = qEnvironmentVariableIsSet("NEMOTRON_MODEL");
    QString directory = QDir::currentPath();
    ScopedConfiguration() { qunsetenv("NVIDIA_API_KEY"); qunsetenv("NVIDIA_BASE_URL"); qunsetenv("NEMOTRON_MODEL"); }
    ~ScopedConfiguration() {
        if (hadKey) qputenv("NVIDIA_API_KEY", key); else qunsetenv("NVIDIA_API_KEY");
        if (hadEndpoint) qputenv("NVIDIA_BASE_URL", endpoint); else qunsetenv("NVIDIA_BASE_URL");
        if (hadModel) qputenv("NEMOTRON_MODEL", model); else qunsetenv("NEMOTRON_MODEL");
        QDir::setCurrent(directory);
    }
};
bool writeSettings(const QString& path, const QByteArray& bytes) {
    QFile file(path);
    return file.open(QIODevice::WriteOnly) && file.write(bytes) == bytes.size();
}

struct Response {
    int status = 200;
    QByteArray bytes;
    int delayMs = 0;
    QNetworkReply::NetworkError error = QNetworkReply::NoError;
    QByteArray retryAfter;
    bool emitReadyRead = true;
};

class FakeReply final : public QNetworkReply {
public:
    FakeReply(const QNetworkRequest& request, Response response, QObject* parent)
        : QNetworkReply(parent), response_(std::move(response)) {
        setRequest(request);
        setUrl(request.url());
        setOperation(QNetworkAccessManager::PostOperation);
        setAttribute(QNetworkRequest::HttpStatusCodeAttribute, response_.status);
        if (!response_.retryAfter.isEmpty()) setRawHeader("Retry-After", response_.retryAfter);
        open(QIODevice::ReadOnly | QIODevice::Unbuffered);
        if (response_.delayMs >= 0) QTimer::singleShot(response_.delayMs, this, [this] {
            if (isFinished()) return;
            delivered_ = true;
            if (response_.error != QNetworkReply::NoError) setError(response_.error, "private-transport-message");
            if (response_.emitReadyRead && !response_.bytes.isEmpty()) emit readyRead();
            setFinished(true);
            emit finished();
        });
    }
    void abort() override {
        if (isFinished()) return;
        setError(QNetworkReply::OperationCanceledError, "private-abort-message");
        setFinished(true);
        emit finished();
    }
    qint64 bytesAvailable() const override {
        return (delivered_ ? static_cast<qint64>(response_.bytes.size()) - offset_ : 0) + QIODevice::bytesAvailable();
    }
protected:
    qint64 readData(char* data, qint64 maximum) override {
        if (!delivered_) return 0;
        const auto length = std::min(maximum, static_cast<qint64>(response_.bytes.size()) - offset_);
        if (length <= 0) return -1;
        std::memcpy(data, response_.bytes.constData() + offset_, static_cast<std::size_t>(length));
        offset_ += length;
        return length;
    }
private:
    Response response_;
    qint64 offset_ = 0;
    bool delivered_ = false;
};

class FakeNetwork final : public QNetworkAccessManager {
public:
    QList<Response> responses;
    QList<QNetworkRequest> requests;
    QList<QByteArray> payloads;
protected:
    QNetworkReply* createRequest(Operation operation, const QNetworkRequest& request, QIODevice* outgoingData) override {
        Q_ASSERT(operation == PostOperation);
        requests.append(request);
        payloads.append(outgoingData ? outgoingData->readAll() : QByteArray{});
        const auto response = responses.isEmpty() ? Response{500, "unexpected request"} : responses.takeFirst();
        return new FakeReply(request, response, this);
    }
};

QByteArray completion(const QJsonObject& message, const QString& reason = "stop") {
    return QJsonDocument(QJsonObject{{"model", "nvidia/nemotron-3.5-lightning-30b-a3b"},
        {"choices", QJsonArray{QJsonObject{{"finish_reason", reason}, {"message", message}}}}}).toJson(QJsonDocument::Compact);
}
QJsonObject call(const QString& id, const QString& name = "get_network_state") {
    return {{"id", id}, {"type", "function"}, {"function", QJsonObject{{"name", name}, {"arguments", "{}"}}}};
}
QByteArray textCompletion() { return completion(QJsonObject{{"role", "assistant"}, {"content", "Ready."}}); }
QJsonArray messages() { return {QJsonObject{{"role", "user"}, {"content", "Inspect this network."}}}; }
NemotronConfig configuration() { NemotronConfig c; c.apiKey = "test-secret-must-not-leak"; c.maxRetries = 0; return c; }
} // namespace

class NemotronClientTests final : public QObject {
    Q_OBJECT
private slots:
    void finalJsonFallbackKeepsExistingTransportsCompatible() {
        class ExistingTransport final : public ModelTransport {
        public:
            QJsonArray receivedMessages, receivedTools; int requests = 0;
            void request(const QJsonArray& history, const QJsonArray& tools) override {
                receivedMessages = history; receivedTools = tools; ++requests;
            }
            void cancel() override {}
        } transport;
        ModelTransport* base = &transport;
        base->requestFinalJson(messages());
        QCOMPARE(transport.requests, 1); QCOMPARE(transport.receivedMessages, messages());
        QVERIFY(transport.receivedTools.isEmpty());
    }

    void finalJsonRequestIsSeparateAndDoesNotChangeOrdinaryToolRequests() {
        FakeNetwork network;
        network.responses.append(Response{200, textCompletion()});
        network.responses.append(Response{200, completion(QJsonObject{{"role", "assistant"}, {"content", "{}"}})});
        network.responses.append(Response{200, textCompletion()});
        const auto config = configuration(); NemotronClient client(config, nullptr, &network);
        QSignalSpy success(&client, &ModelTransport::replyReady), failure(&client, &ModelTransport::requestFailed);
        const QJsonArray tools{QJsonObject{{"type", "function"}, {"function", QJsonObject{{"name", "protocol_only"},
            {"parameters", QJsonObject{{"type", "object"}}}}}}};
        const QJsonObject ordinary{{"model", config.model}, {"messages", messages()}, {"stream", false},
            {"max_tokens", 4096}, {"reasoning_budget", 1024}, {"temperature", 0.2}, {"tools", tools}, {"tool_choice", "auto"}};
        client.request(messages(), tools); QTRY_COMPARE(success.size(), 1);
        QCOMPARE(QJsonDocument::fromJson(network.payloads[0]).object(), ordinary);
        client.requestFinalJson(messages()); QTRY_COMPARE(success.size(), 2);
        const auto finalBody = QJsonDocument::fromJson(network.payloads[1]).object();
        QCOMPARE(finalBody, (QJsonObject{{"model", config.model}, {"messages", messages()}, {"stream", false},
            {"max_tokens", 4096}, {"reasoning_budget", 1024}, {"temperature", 0.0},
            {"response_format", QJsonObject{{"type", "json_object"}}}}));
        QVERIFY(!finalBody.contains("tools")); QVERIFY(!finalBody.contains("tool_choice"));
        client.request(messages(), tools); QTRY_COMPARE(success.size(), 3);
        QCOMPARE(network.payloads[2], network.payloads[0]); QVERIFY(failure.isEmpty());
    }

    void readsLiteralDotEnvWithoutExportingSecrets() {
        ScopedConfiguration environment;
        QTemporaryDir directory; QVERIFY(directory.isValid());
        const auto path = directory.filePath(".env");
        QVERIFY(writeSettings(path, QByteArray::fromHex("efbbbf") +
            "# Local configuration\r\nexport NVIDIA_API_KEY = 'fixture-secret#literal' # key\r\n"
            "NVIDIA_BASE_URL=\"https://fixture.invalid/v1/\"\r\n"
            "NEMOTRON_MODEL=fixture/nemotron # model\r\nIGNORED_VARIABLE=anything\r\n"));
        const auto config = NemotronConfig::fromEnvironment(path);
        QVERIFY(config.apiKey == "fixture-secret#literal");
        QCOMPARE(config.endpoint, QUrl("https://fixture.invalid/v1/chat/completions"));
        QCOMPARE(config.model, "fixture/nemotron"); QCOMPARE(config.credentialSource, "local_file");
        QVERIFY(!qEnvironmentVariableIsSet("NVIDIA_API_KEY"));
        QVERIFY(writeSettings(path, "NVIDIA_API_KEY=$(literal-command)${LITERAL_VARIABLE}\n"));
        QVERIFY(NemotronConfig::fromEnvironment(path).apiKey == "$(literal-command)${LITERAL_VARIABLE}");
    }

    void processSettingsOverrideFileAndAbsentFilesKeepDefaults() {
        ScopedConfiguration environment;
        QTemporaryDir directory; QVERIFY(directory.isValid());
        const auto path = directory.filePath(".env");
        const auto defaults = NemotronConfig::fromEnvironment(path);
        QVERIFY(defaults.apiKey.isEmpty()); QCOMPARE(defaults.endpoint, NemotronConfig{}.endpoint);
        QCOMPARE(defaults.model, NemotronConfig{}.model);
        QVERIFY(writeSettings(path, "NVIDIA_API_KEY=file-fixture\nNVIDIA_BASE_URL=https://file.invalid/v1\nNEMOTRON_MODEL=file/model\n"));
        qputenv("NVIDIA_API_KEY", "process-fixture"); qputenv("NVIDIA_BASE_URL", "https://process.invalid/v1/chat/completions");
        qputenv("NEMOTRON_MODEL", "process/model");
        const auto config = NemotronConfig::fromEnvironment(path);
        QVERIFY(config.apiKey == "process-fixture"); QCOMPARE(config.credentialSource, "environment");
        QCOMPARE(config.endpoint, QUrl("https://process.invalid/v1/chat/completions")); QCOMPARE(config.model, "process/model");
        qputenv("NVIDIA_API_KEY", "  ");
        QVERIFY(NemotronConfig::fromEnvironment(path).apiKey == "file-fixture");
    }

    void endpointFollowsCredentialSource_data() {
        QTest::addColumn<QByteArray>("localKey");
        QTest::addColumn<QByteArray>("savedKey");
        QTest::addColumn<QByteArray>("processKey");
        QTest::addColumn<QByteArray>("localEndpoint");
        QTest::addColumn<QByteArray>("savedEndpoint");
        QTest::addColumn<QByteArray>("processEndpoint");
        QTest::addColumn<QString>("expectedSource");
        QTest::addColumn<QByteArray>("expectedKey");
        QTest::addColumn<QString>("expectedEndpoint");
        const QByteArray localKey = "local-fixture-key", savedKey = "saved-fixture-key", processKey = "process-fixture-key";
        const QByteArray localEndpoint = "https://project.invalid/v1", savedEndpoint = "https://saved.invalid/v1";
        const QByteArray processEndpoint = "https://process.invalid/v1";
        const auto nvidia = NemotronConfig{}.endpoint.toString();
        QTest::newRow("project-endpoint-only-cannot-redirect-process-key")
            << QByteArray{} << savedKey << processKey << localEndpoint << savedEndpoint << QByteArray{}
            << QString("environment") << processKey << nvidia;
        QTest::newRow("project-key-and-endpoint-cannot-redirect-overriding-process-key")
            << localKey << savedKey << processKey << localEndpoint << savedEndpoint << QByteArray{}
            << QString("environment") << processKey << nvidia;
        QTest::newRow("project-endpoint-only-cannot-redirect-saved-key")
            << QByteArray{} << savedKey << QByteArray{} << localEndpoint << QByteArray{} << QByteArray{}
            << QString("user_file") << savedKey << nvidia;
        QTest::newRow("saved-key-keeps-its-own-endpoint")
            << QByteArray{} << savedKey << QByteArray{} << localEndpoint << savedEndpoint << QByteArray{}
            << QString("user_file") << savedKey << QString("https://saved.invalid/v1/chat/completions");
        QTest::newRow("project-key-and-endpoint-pair-is-supported")
            << localKey << savedKey << QByteArray{} << localEndpoint << savedEndpoint << QByteArray{}
            << QString("local_file") << localKey << QString("https://project.invalid/v1/chat/completions");
        QTest::newRow("project-key-without-endpoint-cannot-inherit-saved-endpoint")
            << localKey << savedKey << QByteArray{} << QByteArray{} << savedEndpoint << QByteArray{}
            << QString("local_file") << localKey << nvidia;
        QTest::newRow("explicit-process-endpoint-with-process-key")
            << QByteArray{} << savedKey << processKey << localEndpoint << savedEndpoint << processEndpoint
            << QString("environment") << processKey << QString("https://process.invalid/v1/chat/completions");
        QTest::newRow("explicit-process-endpoint-with-local-key")
            << localKey << savedKey << QByteArray{} << localEndpoint << savedEndpoint << processEndpoint
            << QString("local_file") << localKey << QString("https://process.invalid/v1/chat/completions");
        QTest::newRow("explicit-process-endpoint-with-saved-key")
            << QByteArray{} << savedKey << QByteArray{} << localEndpoint << savedEndpoint << processEndpoint
            << QString("user_file") << savedKey << QString("https://process.invalid/v1/chat/completions");
    }

    void endpointFollowsCredentialSource() {
        QFETCH(QByteArray, localKey); QFETCH(QByteArray, savedKey); QFETCH(QByteArray, processKey);
        QFETCH(QByteArray, localEndpoint); QFETCH(QByteArray, savedEndpoint); QFETCH(QByteArray, processEndpoint);
        QFETCH(QString, expectedSource); QFETCH(QByteArray, expectedKey); QFETCH(QString, expectedEndpoint);
        ScopedConfiguration environment;
        QTemporaryDir directory; QVERIFY(directory.isValid());
        const auto localPath = directory.filePath("project.env"), savedPath = directory.filePath("saved-user.env");
        QVERIFY(writeSettings(localPath, "NVIDIA_API_KEY=" + localKey + "\nNVIDIA_BASE_URL=" + localEndpoint + "\n"));
        QVERIFY(writeSettings(savedPath, "NVIDIA_API_KEY=" + savedKey + "\nNVIDIA_BASE_URL=" + savedEndpoint + "\n"));
        qputenv("NVIDIA_API_KEY", processKey); qputenv("NVIDIA_BASE_URL", processEndpoint);
        const auto config = NemotronConfig::fromEnvironment(localPath, savedPath);
        QCOMPARE(config.credentialSource, expectedSource);
        QVERIFY(config.apiKey.toUtf8() == expectedKey);
        QCOMPARE(config.endpoint, QUrl(expectedEndpoint));
        FakeNetwork network; network.responses.append(Response{200, textCompletion()});
        NemotronClient client(config, nullptr, &network);
        QSignalSpy success(&client, &ModelTransport::replyReady);
        client.request(messages(), {}); QTRY_COMPARE(success.size(), 1);
        QCOMPARE(network.requests.size(), 1);
        QCOMPARE(network.requests.front().url(), QUrl(expectedEndpoint));
        QVERIFY(network.requests.front().rawHeader("Authorization") == "Bearer " + expectedKey);
    }

    void discoversCheckoutFileFromNestedBuildDirectoryAndRespectsWorktreeBoundary() {
        QTemporaryDir directory; QVERIFY(directory.isValid());
        ScopedConfiguration environment;
        QDir root(directory.path()); QVERIFY(root.mkpath("out/build")); QVERIFY(root.mkdir(".git"));
        QVERIFY(writeSettings(root.filePath(".env"), "NVIDIA_API_KEY=checkout-fixture\n"));
        QVERIFY(QDir::setCurrent(root.filePath("out/build")));
        QVERIFY(NemotronConfig::fromEnvironment().apiKey == "checkout-fixture");
        QVERIFY(writeSettings(root.filePath("out/build/.env"), "NVIDIA_API_KEY=working-fixture\n"));
        QVERIFY(NemotronConfig::fromEnvironment().apiKey == "working-fixture");
        QVERIFY(root.mkpath(".worktrees/child/out/build"));
        QVERIFY(writeSettings(root.filePath(".worktrees/child/.git"), "gitdir: fixture\n"));
        QVERIFY(QDir::setCurrent(root.filePath(".worktrees/child/out/build")));
        // Saved user configuration or an executable-local file may still apply;
        // the containing checkout's credential must never cross this boundary.
        QVERIFY(NemotronConfig::fromEnvironment().apiKey != "checkout-fixture");
    }

    void rejectsMalformedQuotedAndOversizedFilesWithoutLoggingTheirContents() {
        ScopedConfiguration environment;
        QTemporaryDir directory; QVERIFY(directory.isValid()); const auto path = directory.filePath(".env");
        QVERIFY(writeSettings(path, "NVIDIA_API_KEY=fixture-secret\nNEMOTRON_MODEL=\"unterminated\n"));
        QVERIFY(NemotronConfig::fromEnvironment(path).apiKey.isEmpty());
        QVERIFY(writeSettings(path, "NVIDIA_API_KEY=\"fixture-secret\"unexpected\n"));
        QVERIFY(NemotronConfig::fromEnvironment(path).apiKey.isEmpty());
        QVERIFY(writeSettings(path, "NVIDIA_API_KEY=fixture-secret\n#" + QByteArray(65536, 'x')));
        QVERIFY(NemotronConfig::fromEnvironment(path).apiKey.isEmpty());
    }

    void userKeySettingsSaveReloadReplaceAndRemoveWithoutExposingAKeyProperty() {
        ScopedConfiguration environment;
        QTemporaryDir directory; QVERIFY(directory.isValid());
        const auto path = directory.filePath("settings/.env");
        CredentialSettings settings(nullptr, path);
        QVERIFY(!settings.configured()); QVERIFY(!settings.saved());
        QVERIFY(settings.saveApiKey("  fixture-user-key  "));
        QVERIFY(settings.configured()); QVERIFY(settings.saved());
        QVERIFY(!settings.status().contains("fixture-user-key"));
        QVERIFY(settings.metaObject()->indexOfProperty("apiKey") < 0);
        QVERIFY(settings.metaObject()->indexOfProperty("key") < 0);
        CredentialSettings reopened(nullptr, path); QVERIFY(reopened.configured());
        QVERIFY(NemotronConfig::fromEnvironment(path).apiKey == "fixture-user-key");
        QVERIFY(settings.saveApiKey("fixture-replacement-key"));
        QVERIFY(NemotronConfig::fromEnvironment(path).apiKey == "fixture-replacement-key");
        QVERIFY(settings.forgetApiKey()); QVERIFY(!settings.saved()); QVERIFY(!settings.configured());
        QVERIFY(!QFile::exists(path)); QVERIFY(!qEnvironmentVariableIsSet("NVIDIA_API_KEY"));
    }

    void invalidKeyInputAndWriteFailuresPreserveAnExistingKey() {
        ScopedConfiguration environment;
        QTemporaryDir directory; QVERIFY(directory.isValid()); const auto path = directory.filePath(".env");
        CredentialSettings settings(nullptr, path); QVERIFY(settings.saveApiKey("fixture-kept-key"));
        for (const auto& invalid : {QString{}, QString("key\nNEMOTRON_MODEL=injected"), QString("bad key"), QString(4097, 'x')}) {
            QVERIFY(!settings.saveApiKey(invalid));
            QVERIFY(NemotronConfig::fromEnvironment(path).apiKey == "fixture-kept-key");
            QVERIFY(!settings.status().contains("fixture-kept-key"));
        }
        CredentialSettings unwritable(nullptr, directory.path());
        QVERIFY(!unwritable.saveApiKey("fixture-secret"));
        QVERIFY(!unwritable.status().contains("fixture-secret"));
    }

    void completionMetadataPrecedesThePublicResultAndContainsOnlySafeFields() {
        FakeNetwork network; network.responses.append(Response{200, textCompletion()});
        const auto config = configuration(); NemotronClient client(config, nullptr, &network);
        QSignalSpy completed(&client, &ModelTransport::requestCompleted), success(&client, &ModelTransport::replyReady);
        QStringList order;
        connect(&client, &ModelTransport::requestCompleted, &client, [&](const QJsonObject&) { order.append("metadata"); });
        connect(&client, &ModelTransport::replyReady, &client, [&](const QJsonObject&) { order.append("reply"); });
        client.request(messages(), {}); QTRY_COMPARE(success.size(), 1); QCOMPARE(completed.size(), 1);
        QCOMPARE(order, QStringList({"metadata", "reply"}));
        const auto metadata = completed.front().front().toJsonObject();
        QCOMPARE(metadata.size(), 8); QVERIFY(!QUuid(metadata["model_request_id"].toString()).isNull());
        QCOMPARE(metadata["configured_model"].toString(), config.model); QCOMPARE(metadata["response_model"].toString(), config.model);
        QCOMPARE(metadata["http_status"].toInt(), 200); QVERIFY(metadata["duration_ms"].toDouble() >= 0.0);
        QCOMPARE(metadata["retry_count"].toInt(), 0); QCOMPARE(metadata["outcome"].toString(), "succeeded");
        QVERIFY(metadata["error_code"].toString().isEmpty());
        const auto serialized = QJsonDocument(metadata).toJson();
        QVERIFY(!serialized.contains(config.apiKey.toUtf8())); QVERIFY(!serialized.contains("Authorization"));
        QVERIFY(!serialized.contains("messages")); QVERIFY(!serialized.contains("reasoning")); QVERIFY(!serialized.contains("Ready."));
    }

    void configurationReloadWaitsUntilTheCurrentRequestEnds() {
        ScopedConfiguration environment;
        qputenv("NVIDIA_API_KEY", "fixture-next-key");
        FakeNetwork network; network.responses.append(Response{200, textCompletion(), -1});
        NemotronClient client(configuration(), nullptr, &network);
        client.request(messages(), {});
        QVERIFY(!client.reloadConfiguration());
        QVERIFY(client.config().apiKey == configuration().apiKey);
        client.cancel();
        QVERIFY(client.reloadConfiguration());
        QVERIFY(client.config().apiKey == "fixture-next-key");
    }

    void failureMetadataIsRedactedAndPrecedesTheError() {
        FakeNetwork network; network.responses.append(Response{401, "test-secret-must-not-leak private server reasoning"});
        NemotronClient client(configuration(), nullptr, &network);
        QSignalSpy completed(&client, &ModelTransport::requestCompleted), failure(&client, &ModelTransport::requestFailed);
        QStringList order;
        connect(&client, &ModelTransport::requestCompleted, &client, [&](const QJsonObject&) { order.append("metadata"); });
        connect(&client, &ModelTransport::requestFailed, &client, [&](const QString&, const QString&) { order.append("failure"); });
        client.request(messages(), {}); QTRY_COMPARE(failure.size(), 1); QCOMPARE(completed.size(), 1);
        QCOMPARE(order, QStringList({"metadata", "failure"}));
        const auto metadata = completed.front().front().toJsonObject();
        QCOMPARE(metadata["http_status"].toInt(), 401); QCOMPARE(metadata["outcome"].toString(), "failed");
        QCOMPARE(metadata["error_code"].toString(), "authentication_failed"); QVERIFY(metadata["response_model"].toString().isEmpty());
        const auto serialized = QJsonDocument(metadata).toJson();
        QVERIFY(!serialized.contains("test-secret")); QVERIFY(!serialized.contains("private")); QVERIFY(!serialized.contains("reasoning"));
    }

    void metadataRejectsArbitraryOrCredentialModelText() {
        FakeNetwork network;
        auto response = QJsonDocument::fromJson(textCompletion()).object(); response["model"] = configuration().apiKey;
        network.responses.append(Response{200, QJsonDocument(response).toJson()});
        response["model"] = "private reasoning\nnot a model identifier";
        network.responses.append(Response{200, QJsonDocument(response).toJson()});
        NemotronClient client(configuration(), nullptr, &network);
        QSignalSpy completed(&client, &ModelTransport::requestCompleted), success(&client, &ModelTransport::replyReady);
        client.request(messages(), {}); QTRY_COMPARE(success.size(), 1);
        QVERIFY(client.lastResponseModel().isEmpty());
        client.request(messages(), {}); QTRY_COMPARE(success.size(), 2); QCOMPARE(completed.size(), 2);
        for (const auto& arguments : completed) {
            const auto metadata = arguments.front().toJsonObject(); QVERIFY(metadata["response_model"].toString().isEmpty());
            const auto bytes = QJsonDocument(metadata).toJson(); QVERIFY(!bytes.contains("test-secret")); QVERIFY(!bytes.contains("private"));
        }
        QVERIFY(client.lastResponseModel().isEmpty());
        auto privateModelConfig = configuration(); privateModelConfig.model = privateModelConfig.apiKey;
        NemotronClient privateModel(privateModelConfig, nullptr, &network);
        QVERIFY(privateModel.configuredModelIdentifier().isEmpty());
        QCOMPARE(client.configuredModelIdentifier(), configuration().model);
    }

    void metadataObserversMayRestartWithoutReceivingTheOldTerminalSignal() {
        FakeNetwork network;
        network.responses.append(Response{401, "private first failure"}); network.responses.append(Response{200, textCompletion()});
        NemotronClient client(configuration(), nullptr, &network);
        QSignalSpy completed(&client, &ModelTransport::requestCompleted), success(&client, &ModelTransport::replyReady), failure(&client, &ModelTransport::requestFailed);
        bool restarted = false;
        connect(&client, &ModelTransport::requestCompleted, &client, [&](const QJsonObject&) {
            if (!restarted) { restarted = true; client.request(messages(), {}); }
        });
        client.request(messages(), {}); QTRY_COMPARE(success.size(), 1);
        QCOMPARE(completed.size(), 2); QCOMPARE(failure.size(), 0); QCOMPARE(network.requests.size(), 2);
        QVERIFY(completed.at(0).front().toJsonObject()["model_request_id"] != completed.at(1).front().toJsonObject()["model_request_id"]);
        QCOMPARE(completed.at(0).front().toJsonObject()["outcome"].toString(), "failed");
        QCOMPARE(completed.at(1).front().toJsonObject()["outcome"].toString(), "succeeded");
    }

    void metadataObserversMayCancelOrDestroyBeforeThePublicResult() {
        FakeNetwork network; network.responses.append(Response{200, textCompletion()});
        NemotronClient client(configuration(), nullptr, &network);
        QSignalSpy completed(&client, &ModelTransport::requestCompleted), success(&client, &ModelTransport::replyReady), failure(&client, &ModelTransport::requestFailed);
        connect(&client, &ModelTransport::requestCompleted, &client, [&](const QJsonObject&) { client.cancel(); });
        client.request(messages(), {}); QTRY_COMPARE(completed.size(), 1); QTest::qWait(20);
        QCOMPARE(success.size(), 0); QCOMPARE(failure.size(), 0);
        network.responses.append(Response{200, textCompletion()});
        QPointer<NemotronClient> disposable = new NemotronClient(configuration(), nullptr, &network);
        QSignalSpy deletedCompletion(disposable, &ModelTransport::requestCompleted), deletedSuccess(disposable, &ModelTransport::replyReady);
        connect(disposable, &ModelTransport::requestCompleted, &network, [&](const QJsonObject&) { delete disposable.data(); });
        disposable->request(messages(), {}); QTRY_VERIFY(disposable.isNull());
        QCOMPARE(deletedCompletion.size(), 1); QCOMPARE(deletedSuccess.size(), 0);
    }

    void oversizedFinishMetadataMayDestroyTheClientDuringFinalRead() {
        FakeNetwork network;
        network.responses.append(Response{200, QByteArray(2 * 1024 * 1024 + 1, 'x'), 0, QNetworkReply::NoError, {}, false});
        QPointer<NemotronClient> disposable = new NemotronClient(configuration(), nullptr, &network);
        QSignalSpy completed(disposable, &ModelTransport::requestCompleted), failed(disposable, &ModelTransport::requestFailed);
        connect(disposable, &ModelTransport::requestCompleted, &network, [&](const QJsonObject&) { delete disposable.data(); });
        disposable->request(messages(), {}); QTRY_VERIFY(disposable.isNull());
        QCOMPARE(completed.size(), 1); QCOMPARE(failed.size(), 0);
        QCOMPARE(completed.front().front().toJsonObject()["error_code"].toString(), "response_too_large");
    }

    void serializesToolConversationAndUsesHttpsWithoutRedirects() {
        FakeNetwork network; network.responses.append(Response{200, textCompletion()});
        const auto config = configuration();
        NemotronClient client(config, nullptr, &network);
        QSignalSpy success(&client, &ModelTransport::replyReady), failure(&client, &ModelTransport::requestFailed);
        auto history = messages();
        history.append(QJsonObject{{"role", "assistant"}, {"content", QJsonValue::Null}, {"tool_calls", QJsonArray{call("call-1")}}});
        history.append(QJsonObject{{"role", "tool"}, {"tool_call_id", "call-1"}, {"content", "{\"snapshot_id\":\"actual\"}"}});
        const QJsonArray tools{QJsonObject{{"type", "function"}, {"function", QJsonObject{{"name", "get_network_state"},
            {"parameters", QJsonObject{{"type", "object"}, {"properties", QJsonObject{}}}}}}}};
        client.request(history, tools);
        QTRY_COMPARE(success.size(), 1); QCOMPARE(failure.size(), 0); QCOMPARE(network.requests.size(), 1);
        const auto request = network.requests.front();
        QCOMPARE(request.url(), config.endpoint);
        QCOMPARE(request.rawHeader("Authorization"), QByteArray("Bearer test-secret-must-not-leak"));
        QCOMPARE(request.attribute(QNetworkRequest::RedirectPolicyAttribute).toInt(), int(QNetworkRequest::ManualRedirectPolicy));
        const auto body = QJsonDocument::fromJson(network.payloads.front()).object();
        QCOMPARE(body["messages"].toArray(), history); QCOMPARE(body["tools"].toArray(), tools);
        QCOMPARE(body["tool_choice"].toString(), QString("auto")); QVERIFY(body["stream"].isBool()); QVERIFY(!body["stream"].toBool());
        QCOMPARE(body["model"].toString(), config.model); QVERIFY(!network.payloads.front().contains(config.apiKey.toUtf8()));
        QCOMPARE(client.lastResponseModel(), config.model);
    }

    void parsesMultipleStructuredCallsWithNullContentAndStripsReasoning() {
        FakeNetwork network;
        const QJsonArray calls{call("call-1"), call("call-2", "get_control_catalog")};
        network.responses.append(Response{200, completion(QJsonObject{{"role", "assistant"}, {"content", QJsonValue::Null},
            {"tool_calls", calls}, {"reasoning", "private internal trace"}, {"reasoning_content", "another private trace"}}, "tool_calls")});
        NemotronClient client(configuration(), nullptr, &network); QSignalSpy success(&client, &ModelTransport::replyReady);
        client.request(messages(), {}); QTRY_COMPARE(success.size(), 1);
        const auto assistant = success.front().front().toJsonObject();
        QVERIFY(assistant["content"].isNull()); QCOMPARE(assistant["tool_calls"].toArray(), calls);
        QVERIFY(!assistant.contains("reasoning")); QVERIFY(!assistant.contains("reasoning_content"));
        QVERIFY(!QJsonDocument(assistant).toJson().contains("private"));
    }

    void leavesArgumentJsonValidationToDispatcher() {
        FakeNetwork network; auto malformed = call("call-invalid-json");
        auto function = malformed["function"].toObject(); function["arguments"] = "{broken"; malformed["function"] = function;
        network.responses.append(Response{200, completion(QJsonObject{{"role", "assistant"},
            {"tool_calls", QJsonArray{malformed}}}, "tool_calls")});
        NemotronClient client(configuration(), nullptr, &network); QSignalSpy success(&client, &ModelTransport::replyReady);
        client.request(messages(), {}); QTRY_COMPARE(success.size(), 1);
        QCOMPARE(success.front().front().toJsonObject()["tool_calls"].toArray().at(0).toObject(), malformed);
    }

    void explicitStoppedEmptyFinalReachesTheCallersStrictValidator_data() {
        QTest::addColumn<QJsonValue>("content");
        QTest::newRow("null") << QJsonValue(QJsonValue::Null);
        QTest::newRow("empty") << QJsonValue(QString{});
        QTest::newRow("whitespace") << QJsonValue(QString(" \t\r\n"));
    }
    void explicitStoppedEmptyFinalReachesTheCallersStrictValidator() {
        QFETCH(QJsonValue, content);
        const auto response = completion(QJsonObject{{"role", "assistant"}, {"content", content},
            {"reasoning", "PRIVATE_STOPPED_EMPTY_CANARY"}, {"reasoning_content", "PRIVATE_STOPPED_EMPTY_CANARY"}});
        FakeNetwork network; network.responses.append(Response{200, response}); network.responses.append(Response{200, response});
        NemotronClient client(configuration(), nullptr, &network);
        QSignalSpy success(&client, &ModelTransport::replyReady), failure(&client, &ModelTransport::requestFailed);
        QSignalSpy metadata(&client, &ModelTransport::requestCompleted);
        client.request(messages(), {}); QTRY_COMPARE(success.size(), 1);
        // The transport preserves the empty public final even in JSON mode;
        // final validity and the single correction cap belong to its caller.
        client.requestFinalJson(messages()); QTRY_COMPARE(success.size(), 2);
        for (const auto& row : success) {
            QCOMPARE(row[0].toJsonObject(), (QJsonObject{{"role", "assistant"}, {"content", content}}));
            QVERIFY(!QJsonDocument(row[0].toJsonObject()).toJson().contains("PRIVATE_STOPPED_EMPTY_CANARY"));
        }
        QCOMPARE(metadata.size(), 2); QCOMPARE(network.requests.size(), 2); QVERIFY(failure.isEmpty());
        for (const auto& row : metadata)
            QVERIFY(!QJsonDocument(row[0].toJsonObject()).toJson().contains("PRIVATE_STOPPED_EMPTY_CANARY"));
        QVERIFY(!QJsonDocument::fromJson(network.payloads[1]).object().contains("tools"));
    }
    void rejectsMalformedResponses_data() {
        QTest::addColumn<QByteArray>("body");
        QTest::addColumn<QString>("code");
        QTest::newRow("invalid-json") << QByteArray("{not json PRIVATE_DIAGNOSTIC_CANARY") << QString("response_invalid_json");
        QTest::newRow("non-object-envelope") << QByteArray("[]") << QString("response_invalid_envelope");
        QTest::newRow("empty-choices") << QByteArray("{\"choices\":[]}") << QString("response_invalid_choices");
        QTest::newRow("non-object-message") << QByteArray("{\"choices\":[{\"message\":[]}]}") << QString("response_invalid_message");
        const QJsonObject emptyMessage{{"role", "assistant"}, {"content", QJsonValue::Null}};
        const auto emptyWithoutReason = QJsonObject{{"message", emptyMessage}};
        auto emptyNullReason = emptyWithoutReason; emptyNullReason["finish_reason"] = QJsonValue::Null;
        QTest::newRow("empty-missing-finish-reason") << QJsonDocument(QJsonObject{{"choices", QJsonArray{emptyWithoutReason}}}).toJson() << QString("response_invalid_empty");
        QTest::newRow("empty-null-finish-reason") << QJsonDocument(QJsonObject{{"choices", QJsonArray{emptyNullReason}}}).toJson() << QString("response_invalid_empty");
        QTest::newRow("empty-tool-finish") << completion(emptyMessage, "tool_calls") << QString("response_invalid_tools");
        QTest::newRow("empty-unknown-finish") << completion(emptyMessage, "PRIVATE_DIAGNOSTIC_CANARY") << QString("response_invalid_finish_reason");
        QTest::newRow("empty-truncated") << completion(emptyMessage, "length") << QString("response_truncated");
        QTest::newRow("stopped-missing-content") << completion(QJsonObject{{"role", "assistant"}}) << QString("response_invalid_empty");
        QTest::newRow("wrong-role") << completion(QJsonObject{{"role", "user"}, {"content", "PRIVATE_DIAGNOSTIC_CANARY"}}) << QString("response_invalid_role");
        QTest::newRow("non-string-content") << completion(QJsonObject{{"role", "assistant"}, {"content", QJsonObject{{"private", "PRIVATE_DIAGNOSTIC_CANARY"}}}}) << QString("response_invalid_content");
        QTest::newRow("duplicate-call-id") << completion(QJsonObject{{"role", "assistant"}, {"content", QJsonValue::Null},
            {"tool_calls", QJsonArray{call("duplicate"), call("duplicate")}}}, "tool_calls") << QString("response_invalid_tools");
        QTest::newRow("non-array-tools") << completion(QJsonObject{{"role", "assistant"}, {"content", "PRIVATE_DIAGNOSTIC_CANARY"},
            {"tool_calls", "PRIVATE_DIAGNOSTIC_CANARY"}}) << QString("response_invalid_tools");
        QTest::newRow("tool-finish-without-tools") << completion(QJsonObject{{"role", "assistant"}, {"content", "PRIVATE_DIAGNOSTIC_CANARY"}}, "tool_calls") << QString("response_invalid_tools");
        auto wrongArguments = call("bad"); auto function = wrongArguments["function"].toObject(); function["arguments"] = QJsonObject{};
        wrongArguments["function"] = function;
        QTest::newRow("non-string-arguments") << completion(QJsonObject{{"role", "assistant"}, {"tool_calls", QJsonArray{wrongArguments}}}, "tool_calls") << QString("response_invalid_tools");
        QTest::newRow("unparsed-reasoning") << completion(QJsonObject{{"role", "assistant"}, {"content", "<think>PRIVATE_DIAGNOSTIC_CANARY</think>answer"}}) << QString("response_invalid_private_reasoning");
        QTest::newRow("unknown-finish-reason") << completion(QJsonObject{{"role", "assistant"}, {"content", "PRIVATE_DIAGNOSTIC_CANARY"}}, "PRIVATE_DIAGNOSTIC_CANARY") << QString("response_invalid_finish_reason");
        QTest::newRow("truncated-generation") << completion(QJsonObject{{"role", "assistant"}, {"content", "PRIVATE_DIAGNOSTIC_CANARY"}}, "length") << QString("response_truncated");
    }
    void rejectsMalformedResponses() {
        QFETCH(QByteArray, body); QFETCH(QString, code); FakeNetwork network; network.responses.append(Response{200, body});
        NemotronClient client(configuration(), nullptr, &network);
        QSignalSpy success(&client, &ModelTransport::replyReady), failure(&client, &ModelTransport::requestFailed), completed(&client, &ModelTransport::requestCompleted);
        client.request(messages(), {}); QTRY_COMPARE(failure.size(), 1); QCOMPARE(success.size(), 0);
        QCOMPARE(failure.front().at(0).toString(), code); QCOMPARE(completed.size(), 1);
        const auto metadata = completed.first().first().toJsonObject(); QCOMPARE(metadata.value("error_code"), QJsonValue(code));
        const auto exposed = QJsonDocument(metadata).toJson() + failure.first().at(1).toString().toUtf8();
        QVERIFY(!exposed.contains("PRIVATE_DIAGNOSTIC_CANARY")); QVERIFY(!exposed.contains(configuration().apiKey.toUtf8()));
        QVERIFY(!exposed.contains("Authorization")); QVERIFY(!exposed.contains("reasoning_content"));
    }

    void authenticationFailuresAreNotRetriedOrLeaked_data() {
        QTest::addColumn<int>("status"); QTest::newRow("unauthorized") << 401; QTest::newRow("forbidden") << 403;
    }
    void authenticationFailuresAreNotRetriedOrLeaked() {
        QFETCH(int, status); FakeNetwork network;
        network.responses.append(Response{status, "test-secret-must-not-leak private server details"});
        auto config = configuration(); config.maxRetries = 3;
        NemotronClient client(config, nullptr, &network); QSignalSpy failure(&client, &ModelTransport::requestFailed);
        client.request(messages(), {}); QTRY_COMPARE(failure.size(), 1); QCOMPARE(network.requests.size(), 1);
        QCOMPARE(failure.front().at(0).toString(), QString("authentication_failed"));
        QVERIFY(!failure.front().at(1).toString().contains("test-secret")); QVERIFY(!failure.front().at(1).toString().contains("private"));
    }

    void rateLimitRetryIsBoundedAndCanRecover() {
        FakeNetwork network; network.responses.append(Response{429, "private", 0, QNetworkReply::NoError, "0"});
        network.responses.append(Response{200, textCompletion()});
        auto config = configuration(); config.maxRetries = 1;
        NemotronClient client(config, nullptr, &network);
        QSignalSpy started(&client, &ModelTransport::requestStarted), success(&client, &ModelTransport::replyReady), failure(&client, &ModelTransport::requestFailed), completed(&client, &ModelTransport::requestCompleted);
        client.request(messages(), {}); QTRY_COMPARE(success.size(), 1); QCOMPARE(failure.size(), 0);
        QCOMPARE(started.size(), 1); QCOMPARE(network.requests.size(), 2); QCOMPARE(network.payloads.at(0), network.payloads.at(1));
        QCOMPARE(completed.size(), 1); QCOMPARE(completed.front().front().toJsonObject()["retry_count"].toInt(), 1);
        QCOMPARE(completed.front().front().toJsonObject()["http_status"].toInt(), 200);
    }

    void exhaustedRateLimitRetriesFail() {
        FakeNetwork network; for (int i = 0; i < 3; ++i) network.responses.append(Response{429, "private", 0, QNetworkReply::NoError, "0"});
        auto config = configuration(); config.maxRetries = 2;
        NemotronClient client(config, nullptr, &network); QSignalSpy failure(&client, &ModelTransport::requestFailed);
        client.request(messages(), {}); QTRY_COMPARE(failure.size(), 1); QCOMPARE(network.requests.size(), 3);
        QCOMPARE(failure.front().at(0).toString(), QString("rate_limited"));
    }

    void transientNetworkFailureRetries() {
        FakeNetwork network; network.responses.append(Response{0, {}, 0, QNetworkReply::TemporaryNetworkFailureError});
        network.responses.append(Response{200, textCompletion()});
        auto config = configuration(); config.maxRetries = 1;
        NemotronClient client(config, nullptr, &network); QSignalSpy success(&client, &ModelTransport::replyReady);
        client.request(messages(), {}); QTRY_COMPARE_WITH_TIMEOUT(success.size(), 1, 3000); QCOMPARE(network.requests.size(), 2);
    }

    void timeoutAndCancellationDiscardLateReplies() {
        FakeNetwork network; network.responses.append(Response{200, textCompletion(), 100});
        auto config = configuration(); config.timeoutMs = 10;
        NemotronClient client(config, nullptr, &network);
        QSignalSpy failure(&client, &ModelTransport::requestFailed), success(&client, &ModelTransport::replyReady), completed(&client, &ModelTransport::requestCompleted);
        client.request(messages(), {}); QTRY_COMPARE(failure.size(), 1);
        QCOMPARE(failure.front().at(0).toString(), QString("timeout"));
        network.responses.append(Response{200, textCompletion(), 100}); client.request(messages(), {}); client.cancel();
        QCOMPARE(failure.size(), 2); QCOMPARE(failure.at(1).at(0).toString(), QString("cancelled"));
        QTest::qWait(150); QCOMPARE(success.size(), 0); QCOMPARE(failure.size(), 2);
        network.responses.append(Response{200, textCompletion()}); client.request(messages(), {});
        QTRY_COMPARE(success.size(), 1); // Client remains reusable after cleanup.
        QCOMPARE(completed.size(), 3);
        QCOMPARE(completed.at(0).front().toJsonObject()["error_code"].toString(), "timeout");
        QCOMPARE(completed.at(1).front().toJsonObject()["outcome"].toString(), "cancelled");
        QCOMPARE(completed.at(1).front().toJsonObject()["error_code"].toString(), "cancelled");
        QVERIFY(completed.at(0).front().toJsonObject()["model_request_id"] != completed.at(1).front().toJsonObject()["model_request_id"]);
    }

    void cancellationWhileWaitingForRetryPreventsAnotherPost() {
        FakeNetwork network; network.responses.append(Response{503, {}, 0, QNetworkReply::NoError, "1"});
        auto config = configuration(); config.maxRetries = 1;
        NemotronClient client(config, nullptr, &network); QSignalSpy failure(&client, &ModelTransport::requestFailed), completed(&client, &ModelTransport::requestCompleted);
        client.request(messages(), {}); QTest::qWait(50); client.cancel();
        QTRY_COMPARE(failure.size(), 1); QCOMPARE(failure.front().at(0).toString(), QString("cancelled"));
        QTest::qWait(1100); QCOMPARE(network.requests.size(), 1);
        QCOMPARE(completed.size(), 1); QCOMPARE(completed.front().front().toJsonObject()["retry_count"].toInt(), 0);
        QCOMPARE(completed.front().front().toJsonObject()["http_status"].toInt(), 503);
    }

    void rejectsOversizedResponseAndRedirect() {
        FakeNetwork network; network.responses.append(Response{200, QByteArray(2 * 1024 * 1024 + 1, 'x')});
        NemotronClient client(configuration(), nullptr, &network); QSignalSpy failure(&client, &ModelTransport::requestFailed);
        client.request(messages(), {}); QTRY_COMPARE(failure.size(), 1);
        QCOMPARE(failure.front().at(0).toString(), QString("response_too_large"));
        network.responses.append(Response{302, {}}); client.request(messages(), {}); QTRY_COMPARE(failure.size(), 2);
        QCOMPARE(failure.at(1).at(0).toString(), QString("redirect_rejected")); QCOMPARE(network.requests.size(), 2);
    }

    void oversizedFinalBodyCannotFinishReplacementRequest() {
        FakeNetwork network;
        // A reply may still contain unread final bytes when finished is emitted.
        network.responses.append(Response{200, QByteArray(2 * 1024 * 1024 + 1, 'x'),
            0, QNetworkReply::NoError, {}, false});
        network.responses.append(Response{200, textCompletion()});
        NemotronClient client(configuration(), nullptr, &network);
        QSignalSpy failure(&client, &ModelTransport::requestFailed), success(&client, &ModelTransport::replyReady);
        bool restarted = false;
        connect(&client, &ModelTransport::requestFailed, &network, [&](const QString& code, const QString&) {
            if (code == "response_too_large" && !restarted) {
                restarted = true;
                client.request(messages(), {});
            }
        });
        client.request(messages(), {});
        QTRY_COMPARE(success.size(), 1);
        QVERIFY(restarted); QCOMPARE(network.requests.size(), 2); QCOMPARE(failure.size(), 1);
        QCOMPARE(failure.front().at(0).toString(), QString("response_too_large"));
        QCOMPARE(success.front().front().toJsonObject()["content"].toString(), QString("Ready."));
    }

    void oversizedFinalBodyFailureMayDestroyClient() {
        FakeNetwork network;
        network.responses.append(Response{200, QByteArray(2 * 1024 * 1024 + 1, 'x'),
            0, QNetworkReply::NoError, {}, false});
        auto* client = new NemotronClient(configuration(), nullptr, &network);
        QPointer<NemotronClient> guard(client);
        QSignalSpy failure(client, &ModelTransport::requestFailed), success(client, &ModelTransport::replyReady);
        connect(client, &ModelTransport::requestFailed, &network, [&](const QString& code, const QString&) {
            if (code == "response_too_large") { delete client; client = nullptr; }
        });
        client->request(messages(), {});
        QTRY_VERIFY(guard.isNull());
        QCOMPARE(network.requests.size(), 1); QCOMPARE(failure.size(), 1); QCOMPARE(success.size(), 0);
        QCOMPARE(failure.front().at(0).toString(), QString("response_too_large"));
    }

    void refusesMissingCredentialsAndInsecureConfiguration() {
        FakeNetwork network; auto config = configuration(); config.apiKey.clear();
        NemotronClient missing(config, nullptr, &network); QSignalSpy missingFailure(&missing, &ModelTransport::requestFailed), completed(&missing, &ModelTransport::requestCompleted);
        missing.request(messages(), {}); QCOMPARE(missingFailure.size(), 1);
        QCOMPARE(missingFailure.front().at(0).toString(), QString("credentials_missing"));
        QCOMPARE(completed.size(), 1); QCOMPARE(completed.front().front().toJsonObject()["http_status"].toInt(), 0);
        QCOMPARE(completed.front().front().toJsonObject()["retry_count"].toInt(), 0);
        QCOMPARE(completed.front().front().toJsonObject()["error_code"].toString(), "credentials_missing");
        config = configuration(); config.endpoint = QUrl("http://localhost:8080/v1/chat/completions");
        NemotronClient insecure(config, nullptr, &network); QSignalSpy insecureFailure(&insecure, &ModelTransport::requestFailed);
        insecure.request(messages(), {}); QCOMPARE(insecureFailure.size(), 1);
        QCOMPARE(insecureFailure.front().at(0).toString(), QString("configuration_invalid")); QCOMPARE(network.requests.size(), 0);
    }
};

QTEST_GUILESS_MAIN(NemotronClientTests)
#include "nemotron_client_tests.moc"
