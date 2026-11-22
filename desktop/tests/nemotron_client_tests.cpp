#include "agent/NemotronClient.hpp"
#include "agent/CredentialSettings.hpp"
#include "agent/TransportDiagnostics.hpp"

#include <QBuffer>
#include <QDateTime>
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
    QByteArray interval = qgetenv("BREAKWATER_NEMOTRON_INTERVAL_MS");
    bool hadInterval = qEnvironmentVariableIsSet("BREAKWATER_NEMOTRON_INTERVAL_MS");
    QString directory = QDir::currentPath();
    ScopedConfiguration() { qunsetenv("NVIDIA_API_KEY"); qunsetenv("NVIDIA_BASE_URL"); qunsetenv("NEMOTRON_MODEL"); qunsetenv("BREAKWATER_NEMOTRON_INTERVAL_MS"); }
    ~ScopedConfiguration() {
        if (hadKey) qputenv("NVIDIA_API_KEY", key); else qunsetenv("NVIDIA_API_KEY");
        if (hadEndpoint) qputenv("NVIDIA_BASE_URL", endpoint); else qunsetenv("NVIDIA_BASE_URL");
        if (hadModel) qputenv("NEMOTRON_MODEL", model); else qunsetenv("NEMOTRON_MODEL");
        if (hadInterval) qputenv("BREAKWATER_NEMOTRON_INTERVAL_MS", interval); else qunsetenv("BREAKWATER_NEMOTRON_INTERVAL_MS");
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
    QList<QByteArray> chunks;
    QList<QPair<QByteArray, QByteArray>> headers;
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
        for (const auto& header : response_.headers) setRawHeader(header.first, header.second);
        open(QIODevice::ReadOnly | QIODevice::Unbuffered);
        if (!response_.chunks.isEmpty()) {
            response_.bytes.clear();
            for (const auto& chunk : response_.chunks) response_.bytes.append(chunk);
        }
        if (response_.delayMs >= 0) QTimer::singleShot(response_.delayMs, this, [this] { publish(); });
    }
    void abort() override {
        if (isFinished()) return;
        setError(QNetworkReply::OperationCanceledError, "private-abort-message");
        setFinished(true);
        emit finished();
    }
    qint64 bytesAvailable() const override {
        return available_ - offset_ + QIODevice::bytesAvailable();
    }
protected:
    qint64 readData(char* data, qint64 maximum) override {
        const auto length = std::min(maximum, available_ - offset_);
        if (length <= 0) return isFinished() ? -1 : 0;
        std::memcpy(data, response_.bytes.constData() + offset_, static_cast<std::size_t>(length));
        offset_ += length;
        return length;
    }
private:
    Response response_;
    qint64 offset_ = 0, available_ = 0;
    qsizetype chunk_ = 0;
    void publish() {
        if (isFinished()) return;
        const bool fragmented = !response_.chunks.isEmpty();
        available_ = fragmented ? available_ + response_.chunks[chunk_++].size() : response_.bytes.size();
        const bool last = !fragmented || chunk_ == response_.chunks.size();
        if (last && response_.error != QNetworkReply::NoError) setError(response_.error, "private-transport-message");
        if (response_.emitReadyRead && available_ > offset_) emit readyRead();
        if (isFinished()) return; // A size-limit rejection may synchronously abort.
        if (!last) { QTimer::singleShot(1, this, [this] { publish(); }); return; }
        setFinished(true); emit finished();
    }
};

class FakeNetwork final : public QNetworkAccessManager {
public:
    FakeNetwork() { clock.start(); }
    QElapsedTimer clock;
    QList<qint64> postedMs;
    QList<Response> responses;
    QList<QNetworkRequest> requests;
    QList<QByteArray> payloads;
protected:
    QNetworkReply* createRequest(Operation operation, const QNetworkRequest& request, QIODevice* outgoingData) override {
        Q_ASSERT(operation == PostOperation);
        requests.append(request);
        postedMs.append(clock.elapsed());
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
NemotronConfig configuration() { NemotronConfig c; c.apiKey = "test-secret-must-not-leak"; c.maxRetries = 0; c.minimumIntervalMs = 0; c.backoffBaseMs = 250; c.backoffJitterMs = 0; c.terminalCooldownMs = 0; return c; }
} // namespace

class NemotronClientTests final : public QObject {
    Q_OBJECT
private:
    std::shared_ptr<NemotronRequestPacer> pacer_;
private slots:
    void init() { pacer_ = std::make_shared<NemotronRequestPacer>(); }
    void defaultClientsUseTheSameProcessCooldown() {
        FakeNetwork network; network.responses = {Response{429, "private"}, Response{200, textCompletion()}};
        auto config = configuration(); config.backoffBaseMs = 1; config.terminalCooldownMs = 200;
        NemotronClient first(config, nullptr, &network), second(config, nullptr, &network);
        QSignalSpy failed(&first, &ModelTransport::requestFailed), blocked(&second, &ModelTransport::requestFailed);
        QSignalSpy ready(&second, &ModelTransport::replyReady);
        first.request(messages(), {}); QTRY_COMPARE(failed.size(), 1);
        second.requestWithBudget(messages(), {}, 20, false); QTRY_COMPARE(blocked.size(), 1);
        QCOMPARE(blocked[0][0].toString(), QString("rate_limit_wait_exceeds_budget")); QCOMPARE(network.requests.size(), 1);
        second.request(messages(), {}); QTRY_COMPARE_WITH_TIMEOUT(ready.size(), 1, 1000);
        QVERIFY(network.postedMs[1] - network.postedMs[0] >= 199);
    }
    void pacingEnvironmentIsBoundedAndInvalidSettingsCannotPost() {
        ScopedConfiguration environment; QTemporaryDir directory;
        const auto file = directory.filePath("absent.env");
        QCOMPARE(NemotronConfig::fromEnvironment(file).minimumIntervalMs, 6000);
        qputenv("NVIDIA_API_KEY", "fixture-key");
        qputenv("BREAKWATER_NEMOTRON_INTERVAL_MS", "9000");
        QCOMPARE(NemotronConfig::fromEnvironment(file).minimumIntervalMs, 9000);
        for (const auto value : {"0", "999", "60001", "1.5", "not-a-number"}) {
            qputenv("BREAKWATER_NEMOTRON_INTERVAL_MS", value);
            FakeNetwork network; NemotronClient client(NemotronConfig::fromEnvironment(file), nullptr, &network, pacer_);
            QSignalSpy failed(&client, &ModelTransport::requestFailed);
            client.request(messages(), {}); QTRY_COMPARE(failed.size(), 1);
            QCOMPARE(failed[0][0].toString(), QString("configuration_invalid")); QVERIFY(network.requests.isEmpty());
        }
    }
    void sharedClientsSerializePostsAndKeepFifoAcrossContinuations() {
        FakeNetwork network;
        network.responses = {Response{200, textCompletion(), 130}, Response{200, textCompletion()}, Response{200, textCompletion()}};
        auto config = configuration(); config.minimumIntervalMs = 80;
        NemotronClient first(config, nullptr, &network, pacer_), second(config, nullptr, &network, pacer_);
        QSignalSpy firstDone(&first, &ModelTransport::replyReady), secondDone(&second, &ModelTransport::replyReady);
        QSignalSpy metadata(&second, &ModelTransport::requestCompleted);
        const QJsonArray secondMessages{QJsonObject{{"role", "user"}, {"content", "Second client"}}};
        connect(&first, &ModelTransport::replyReady, &first, [&] { if (firstDone.size() == 1) first.request(messages(), {}); });
        first.request(messages(), {}); second.request(secondMessages, {});
        QCOMPARE(network.requests.size(), 1); QTest::qWait(60); QCOMPARE(network.requests.size(), 1);
        QTRY_COMPARE_WITH_TIMEOUT(firstDone.size(), 2, 3000); QCOMPARE(secondDone.size(), 1);
        QCOMPARE(QJsonDocument::fromJson(network.payloads[1]).object()["messages"].toArray(), secondMessages);
        QVERIFY(network.postedMs[1] - network.postedMs[0] >= 129);
        QVERIFY(network.postedMs[2] - network.postedMs[1] >= 79);
        const auto d = metadata.first().first().toJsonObject()["transport_diagnostics"];
        QVERIFY(validTransportDiagnostics(d)); QVERIFY(d.toObject()["admission_wait_ms"].toInteger() >= 100);
    }
    void cancellingQueuedClientNeverPostsAndDoesNotBlockOthers() {
        FakeNetwork network;
        network.responses = {Response{200, textCompletion(), 150}, Response{200, textCompletion()}};
        auto config = configuration();
        NemotronClient first(config, nullptr, &network, pacer_), cancelled(config, nullptr, &network, pacer_), third(config, nullptr, &network, pacer_);
        QSignalSpy cancelledMetadata(&cancelled, &ModelTransport::requestCompleted), failures(&cancelled, &ModelTransport::requestFailed);
        QSignalSpy success(&third, &ModelTransport::replyReady);
        first.request(messages(), {}); cancelled.request(messages(), {}); third.request(messages(), {});
        cancelled.cancel(); QTRY_COMPARE_WITH_TIMEOUT(success.size(), 1, 2000);
        QCOMPARE(failures.size(), 1); QCOMPARE(failures[0][0].toString(), QString("cancelled"));
        QCOMPARE(network.requests.size(), 2);
        const auto metadata = cancelledMetadata[0][0].toJsonObject();
        QCOMPARE(metadata["retry_count"].toInt(), 0);
        QVERIFY(metadata["transport_diagnostics"].toObject()["attempts"].toArray().isEmpty());
        QVERIFY(validTransportDiagnostics(metadata["transport_diagnostics"]));
        QVERIFY(validTransportDiagnosticsForRequest(metadata));
    }
    void queuedRequestKeepsItsOriginalDeadline() {
        FakeNetwork network; network.responses = {Response{200, textCompletion(), 200}};
        NemotronClient first(configuration(), nullptr, &network, pacer_), waiting(configuration(), nullptr, &network, pacer_);
        QSignalSpy failures(&waiting, &ModelTransport::requestFailed);
        first.request(messages(), {}); waiting.requestWithBudget(messages(), {}, 70, false);
        QTRY_COMPARE_WITH_TIMEOUT(failures.size(), 1, 1000);
        QCOMPARE(failures[0][0].toString(), QString("request_budget_exceeded"));
        QTest::qWait(210); QCOMPARE(network.requests.size(), 1);
    }
    void terminalCooldownSurvivesClientDestructionAndBlocksOtherClients() {
        FakeNetwork network; network.responses = {Response{429, "private"}, Response{200, textCompletion()}};
        auto config = configuration(); config.backoffBaseMs = 1; config.terminalCooldownMs = 250;
        auto* first = new NemotronClient(config, nullptr, &network, pacer_);
        QSignalSpy failed(first, &ModelTransport::requestFailed), meta(first, &ModelTransport::requestCompleted);
        first->request(messages(), {}); QTRY_COMPARE(failed.size(), 1);
        QVERIFY(meta[0][0].toJsonObject()["transport_diagnostics"].toObject()["cooldown_remaining_ms"].toInteger() >= 200);
        delete first;
        NemotronClient second(config, nullptr, &network, pacer_);
        QSignalSpy blocked(&second, &ModelTransport::requestFailed), ready(&second, &ModelTransport::replyReady);
        second.requestWithBudget(messages(), {}, 30, false);
        QTRY_COMPARE(blocked.size(), 1); QCOMPARE(blocked[0][0].toString(), QString("rate_limit_wait_exceeds_budget"));
        QCOMPARE(network.requests.size(), 1);
        second.request(messages(), {}); QTest::qWait(90); QCOMPARE(network.requests.size(), 1);
        QTRY_COMPARE_WITH_TIMEOUT(ready.size(), 1, 1000);
        QVERIFY(network.postedMs[1] - network.postedMs[0] >= 249);
    }
    void terminalCooldownAppliesBeforeRetryExhaustion_data() {
        QTest::addColumn<int>("status"); QTest::addColumn<bool>("cancel");
        QTest::newRow("429-budget") << 429 << false;
        QTest::newRow("503-budget") << 503 << false;
        QTest::newRow("429-cancel") << 429 << true;
        QTest::newRow("503-cancel") << 503 << true;
    }
    void terminalCooldownAppliesBeforeRetryExhaustion() {
        QFETCH(int, status); QFETCH(bool, cancel);
        FakeNetwork network; network.responses = {Response{status, "private"}, Response{200, textCompletion()}};
        auto config = configuration(); config.maxRetries = 2; config.backoffBaseMs = 150; config.terminalCooldownMs = 600;
        NemotronClient first(config, nullptr, &network, pacer_), second(config, nullptr, &network, pacer_);
        QSignalSpy failed(&first, &ModelTransport::requestFailed), metadata(&first, &ModelTransport::requestCompleted);
        first.requestWithBudget(messages(), {}, cancel ? 1000 : 50, false);
        if (cancel) { QTest::qWait(30); first.cancel(); }
        QTRY_COMPARE(failed.size(), 1); QCOMPARE(network.requests.size(), 1);
        QCOMPARE(failed[0][0].toString(), cancel ? QString("cancelled") :
            status == 429 ? QString("rate_limit_wait_exceeds_budget") : QString("retry_wait_exceeds_budget"));
        const auto retained = metadata[0][0].toJsonObject();
        QCOMPARE(retained["retry_count"].toInt(), 0);
        QVERIFY(validTransportDiagnosticsForRequest(retained));
        QVERIFY(retained["transport_diagnostics"].toObject()["cooldown_remaining_ms"].toInteger() >= 500);
        QSignalSpy blocked(&second, &ModelTransport::requestFailed);
        second.requestWithBudget(messages(), {}, 200, false); QTRY_COMPARE(blocked.size(), 1);
        QCOMPARE(blocked[0][0].toString(), QString("rate_limit_wait_exceeds_budget"));
        QCOMPARE(network.requests.size(), 1);
    }
    void destroyingAClientDuringThrottledBackoffKeepsTerminalCooldown() {
        FakeNetwork network; network.responses = {Response{429, "private"}, Response{200, textCompletion()}};
        auto config = configuration(); config.maxRetries = 2; config.backoffBaseMs = 150; config.terminalCooldownMs = 600;
        auto* first = new NemotronClient(config, nullptr, &network, pacer_);
        first->request(messages(), {}); QTest::qWait(30); QCOMPARE(network.requests.size(), 1);
        delete first;
        NemotronClient second(config, nullptr, &network, pacer_);
        QSignalSpy blocked(&second, &ModelTransport::requestFailed);
        second.requestWithBudget(messages(), {}, 200, false); QTRY_COMPARE(blocked.size(), 1);
        QCOMPARE(blocked[0][0].toString(), QString("rate_limit_wait_exceeds_budget"));
        QCOMPARE(network.requests.size(), 1);
    }
    void longerFallbackBackoffHasBoundedJitterAndCountsActualAttempts() {
        FakeNetwork network;
        network.responses = {Response{429, "private"}, Response{503, "private", 0, QNetworkReply::NoError, "invalid"}, Response{200, textCompletion()}};
        auto config = configuration(); config.maxRetries = 2; config.backoffBaseMs = 2000; config.backoffJitterMs = 1000;
        NemotronClient client(config, nullptr, &network, pacer_);
        QSignalSpy success(&client, &ModelTransport::replyReady), meta(&client, &ModelTransport::requestCompleted);
        client.requestWithBudget(messages(), {}, 12000, false);
        QTRY_COMPARE_WITH_TIMEOUT(success.size(), 1, 12000);
        const auto metadata = meta[0][0].toJsonObject(); const auto waits = metadata["retry_delays_ms"].toArray();
        QCOMPARE(waits.size(), 2); QVERIFY(waits[0].toInt() >= 2000 && waits[0].toInt() <= 3000);
        QVERIFY(waits[1].toInt() >= 4000 && waits[1].toInt() <= 5000);
        const auto d = metadata["transport_diagnostics"]; QVERIFY(validTransportDiagnostics(d));
        const auto attempts = d.toObject()["attempts"].toArray(); QCOMPARE(attempts.size(), 3);
        QCOMPARE(metadata["retry_count"].toInt(), 2); QCOMPARE(attempts[0].toObject()["http_status"].toInt(), 429);
        QCOMPARE(attempts[1].toObject()["http_status"].toInt(), 503);
        QVERIFY(network.postedMs[1] - network.postedMs[0] >= waits[0].toInt() - 1);
        QVERIFY(network.postedMs[2] - network.postedMs[1] >= waits[1].toInt() - 1);
    }
    void attemptDiagnosticsKeepOnlyClosedNumericGuidanceAndUsage() {
        FakeNetwork network;
        Response error{429, R"({"error":{"code":"insufficient_quota","message":"PRIVATE_CANARY"},"reasoning_content":"PRIVATE_CANARY"})", 0, QNetworkReply::NoError, "0"};
        error.headers = {{"x-ratelimit-limit-requests", "40"}, {"x-ratelimit-remaining-tokens", "PRIVATE_CANARY"}, {"Authorization", "PRIVATE_CANARY"}};
        auto successBody = QJsonDocument::fromJson(textCompletion()).object();
        successBody["usage"] = QJsonObject{{"prompt_tokens", 12}, {"completion_tokens", 0.5}, {"total_tokens", "PRIVATE_CANARY"}, {"private", "PRIVATE_CANARY"}};
        network.responses = {error, Response{200, QJsonDocument(successBody).toJson()}};
        auto config = configuration(); config.maxRetries = 1;
        NemotronClient client(config, nullptr, &network, pacer_);
        QSignalSpy meta(&client, &ModelTransport::requestCompleted);
        client.request(messages(), {}); QTRY_COMPARE(meta.size(), 1);
        const auto metadata = meta[0][0].toJsonObject(); const auto d = metadata["transport_diagnostics"];
        QVERIFY(validTransportDiagnosticsForRequest(metadata));
        auto inconsistent = metadata; inconsistent["retry_count"] = metadata["retry_count"].toInt() + 1;
        QVERIFY(!validTransportDiagnosticsForRequest(inconsistent));
        inconsistent = metadata; inconsistent["http_status"] = 503; QVERIFY(!validTransportDiagnosticsForRequest(inconsistent));
        inconsistent = metadata; inconsistent["duration_ms"] = -1; QVERIFY(!validTransportDiagnosticsForRequest(inconsistent));
        QVERIFY(validTransportDiagnostics(d)); const auto attempts = d.toObject()["attempts"].toArray();
        QCOMPARE(d.toObject()["request_bytes"].toInteger(), network.payloads[0].size());
        for (const auto bad : {QJsonValue(0), QJsonValue(-1), QJsonValue(0.5), QJsonValue(2 * 1024 * 1024 + 1), QJsonValue("raw"), QJsonValue(true)}) {
            auto invalidSize = d.toObject(); invalidSize["request_bytes"] = bad; QVERIFY(!validTransportDiagnostics(invalidSize));
        }
        QCOMPARE(attempts[0].toObject()["provider_error_code"].toString(), QString("insufficient_quota"));
        QCOMPARE(attempts[0].toObject()["rate_limits"].toObject(), (QJsonObject{{"request_limit", 40}}));
        QCOMPARE(attempts[1].toObject()["usage"].toObject(), (QJsonObject{{"prompt_tokens", 12}}));
        const auto bytes = QJsonDocument(metadata).toJson();
        QVERIFY(!bytes.contains("PRIVATE_CANARY")); QVERIFY(!bytes.contains("Authorization"));
        QVERIFY(!bytes.contains(config.apiKey.toUtf8()));
        auto invalid = d.toObject(); invalid["headers"] = QJsonObject{}; QVERIFY(!validTransportDiagnostics(invalid));
        invalid = d.toObject(); auto modified = attempts; auto attempt = modified[0].toObject(); attempt["raw_body"] = "private";
        modified[0] = attempt; invalid["attempts"] = modified; QVERIFY(!validTransportDiagnostics(invalid));
    }
    void oversizedRequestRetainsValidFailureWithoutPosting() {
        FakeNetwork network; NemotronClient client(configuration(), nullptr, &network, pacer_);
        QSignalSpy failed(&client, &ModelTransport::requestFailed), metadata(&client, &ModelTransport::requestCompleted);
        client.request(QJsonArray{QJsonObject{{"role", "user"}, {"content", QString(3 * 1024 * 1024, 'x')}}}, {});
        QTRY_COMPARE(failed.size(), 1); QCOMPARE(failed[0][0].toString(), QString("request_too_large"));
        QVERIFY(network.requests.isEmpty()); const auto request = metadata[0][0].toJsonObject();
        QVERIFY(validTransportDiagnosticsForRequest(request));
        QVERIFY(!request["transport_diagnostics"].toObject().contains("request_bytes"));
        QVERIFY(request["transport_diagnostics"].toObject()["attempts"].toArray().isEmpty());
        network.responses = {Response{200, textCompletion()}}; QSignalSpy ready(&client, &ModelTransport::replyReady);
        client.request(messages(), {}); QTRY_COMPARE(ready.size(), 1); QCOMPARE(network.requests.size(), 1);
        QVERIFY(validTransportDiagnosticsForRequest(metadata.last()[0].toJsonObject()));
    }
    void defaultBackoffPreservesTheOriginalBudget() {
        FakeNetwork network; network.responses = {Response{429, "private"}};
        auto config = configuration(); config.maxRetries = 2; config.backoffBaseMs = NemotronConfig{}.backoffBaseMs;
        QCOMPARE(config.backoffBaseMs, 8000);
        NemotronClient client(config, nullptr, &network, pacer_);
        QSignalSpy failed(&client, &ModelTransport::requestFailed), metadata(&client, &ModelTransport::requestCompleted);
        client.requestWithBudget(messages(), {}, 5000, false); QTRY_COMPARE(failed.size(), 1);
        QCOMPARE(network.requests.size(), 1); QCOMPARE(failed[0][0].toString(), QString("rate_limit_wait_exceeds_budget"));
        const auto request = metadata[0][0].toJsonObject(); QCOMPARE(request["retry_count"].toInt(), 0);
        QVERIFY(request["retry_delays_ms"].toArray().isEmpty());
        QCOMPARE(request["transport_diagnostics"].toObject()["backoff_base_ms"].toInt(), 8000);
        QVERIFY(request["duration_ms"].toDouble() < 5000); QVERIFY(validTransportDiagnosticsForRequest(request));
    }
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
        const auto config = configuration(); NemotronClient client(config, nullptr, &network, pacer_);
        QSignalSpy success(&client, &ModelTransport::replyReady), failure(&client, &ModelTransport::requestFailed);
        const QJsonArray tools{QJsonObject{{"type", "function"}, {"function", QJsonObject{{"name", "protocol_only"},
            {"parameters", QJsonObject{{"type", "object"}}}}}}};
        const QJsonObject ordinary{{"model", config.model}, {"messages", messages()}, {"stream", false},
            {"max_tokens", 4096}, {"reasoning_budget", 1024}, {"temperature", 0.2},
            {"chat_template_kwargs",QJsonObject{{"enable_thinking",true}}}, {"tools", tools}, {"tool_choice", "auto"}};
        client.request(messages(), tools); QTRY_COMPARE(success.size(), 1);
        QCOMPARE(QJsonDocument::fromJson(network.payloads[0]).object(), ordinary);
        client.requestFinalJson(messages()); QTRY_COMPARE(success.size(), 2);
        const auto finalBody = QJsonDocument::fromJson(network.payloads[1]).object();
        QCOMPARE(finalBody, (QJsonObject{{"model", config.model}, {"messages", messages()}, {"stream", false},
            {"max_tokens", 4096}, {"temperature", 0.0},
            {"chat_template_kwargs",QJsonObject{{"enable_thinking",false}}},
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
        NemotronClient client(config, nullptr, &network, pacer_);
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
        const auto config = configuration(); NemotronClient client(config, nullptr, &network, pacer_);
        QSignalSpy completed(&client, &ModelTransport::requestCompleted), success(&client, &ModelTransport::replyReady);
        QStringList order;
        connect(&client, &ModelTransport::requestCompleted, &client, [&](const QJsonObject&) { order.append("metadata"); });
        connect(&client, &ModelTransport::replyReady, &client, [&](const QJsonObject&) { order.append("reply"); });
        client.request(messages(), {}); QTRY_COMPARE(success.size(), 1); QCOMPARE(completed.size(), 1);
        QCOMPARE(order, QStringList({"metadata", "reply"}));
        const auto metadata = completed.front().front().toJsonObject();
        QCOMPARE(metadata.size(), 9); QVERIFY(validTransportDiagnostics(metadata["transport_diagnostics"]));
        QVERIFY(!QUuid(metadata["model_request_id"].toString()).isNull());
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
        NemotronClient client(configuration(), nullptr, &network, pacer_);
        client.request(messages(), {});
        QVERIFY(!client.reloadConfiguration());
        QVERIFY(client.config().apiKey == configuration().apiKey);
        client.cancel();
        QVERIFY(client.reloadConfiguration());
        QVERIFY(client.config().apiKey == "fixture-next-key");
    }

    void failureMetadataIsRedactedAndPrecedesTheError() {
        FakeNetwork network; network.responses.append(Response{401, "test-secret-must-not-leak private server reasoning"});
        NemotronClient client(configuration(), nullptr, &network, pacer_);
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
        NemotronClient client(configuration(), nullptr, &network, pacer_);
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
        NemotronClient privateModel(privateModelConfig, nullptr, &network, pacer_);
        QVERIFY(privateModel.configuredModelIdentifier().isEmpty());
        QCOMPARE(client.configuredModelIdentifier(), configuration().model);
    }

    void metadataObserversMayRestartWithoutReceivingTheOldTerminalSignal() {
        FakeNetwork network;
        network.responses.append(Response{401, "private first failure"}); network.responses.append(Response{200, textCompletion()});
        NemotronClient client(configuration(), nullptr, &network, pacer_);
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
        NemotronClient client(configuration(), nullptr, &network, pacer_);
        QSignalSpy completed(&client, &ModelTransport::requestCompleted), success(&client, &ModelTransport::replyReady), failure(&client, &ModelTransport::requestFailed);
        connect(&client, &ModelTransport::requestCompleted, &client, [&](const QJsonObject&) { client.cancel(); });
        client.request(messages(), {}); QTRY_COMPARE(completed.size(), 1); QTest::qWait(20);
        QCOMPARE(success.size(), 0); QCOMPARE(failure.size(), 0);
        network.responses.append(Response{200, textCompletion()});
        QPointer<NemotronClient> disposable = new NemotronClient(configuration(), nullptr, &network, pacer_);
        QSignalSpy deletedCompletion(disposable, &ModelTransport::requestCompleted), deletedSuccess(disposable, &ModelTransport::replyReady);
        connect(disposable, &ModelTransport::requestCompleted, &network, [&](const QJsonObject&) { delete disposable.data(); });
        disposable->request(messages(), {}); QTRY_VERIFY(disposable.isNull());
        QCOMPARE(deletedCompletion.size(), 1); QCOMPARE(deletedSuccess.size(), 0);
    }

    void oversizedFinishMetadataMayDestroyTheClientDuringFinalRead() {
        FakeNetwork network;
        network.responses.append(Response{200, QByteArray(2 * 1024 * 1024 + 1, 'x'), 0, QNetworkReply::NoError, {}, false});
        QPointer<NemotronClient> disposable = new NemotronClient(configuration(), nullptr, &network, pacer_);
        QSignalSpy completed(disposable, &ModelTransport::requestCompleted), failed(disposable, &ModelTransport::requestFailed);
        connect(disposable, &ModelTransport::requestCompleted, &network, [&](const QJsonObject&) { delete disposable.data(); });
        disposable->request(messages(), {}); QTRY_VERIFY(disposable.isNull());
        QCOMPARE(completed.size(), 1); QCOMPARE(failed.size(), 0);
        QCOMPARE(completed.front().front().toJsonObject()["error_code"].toString(), "response_too_large");
    }

    void serializesToolConversationAndUsesHttpsWithoutRedirects() {
        FakeNetwork network; network.responses.append(Response{200, textCompletion()});
        const auto config = configuration();
        NemotronClient client(config, nullptr, &network, pacer_);
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
            {"tool_calls", calls}, {"reasoning", "PRIVATE_REASONING_CANARY"},
            {"reasoning_content", "PRIVATE_REASONING_CONTENT_CANARY"}}, "tool_calls")});
        network.responses.append(Response{200, textCompletion()});
        NemotronClient client(configuration(), nullptr, &network, pacer_);
        QSignalSpy success(&client, &ModelTransport::replyReady), failure(&client, &ModelTransport::requestFailed);
        QSignalSpy metadata(&client, &ModelTransport::requestCompleted);
        client.request(messages(), {}); QTRY_COMPARE(success.size(), 1);
        const auto assistant = success.front().front().toJsonObject();
        QVERIFY(assistant["content"].isNull()); QCOMPARE(assistant["tool_calls"].toArray(), calls);
        QVERIFY(!assistant.contains("reasoning")); QVERIFY(!assistant.contains("reasoning_content"));
        const auto publicReply = QJsonDocument(assistant).toJson();
        QVERIFY(!publicReply.contains("PRIVATE_REASONING_CANARY"));
        QVERIFY(!publicReply.contains("PRIVATE_REASONING_CONTENT_CANARY"));
        auto history = messages(); history.append(assistant);
        for (const auto& value : calls) {
            history.append(QJsonObject{{"role", "tool"}, {"tool_call_id", value.toObject()["id"]},
                {"content", "{\"ok\":true,\"offline_fixture\":true}"}});
        }
        client.request(history, {}); QTRY_COMPARE(success.size(), 2);
        QCOMPARE(network.requests.size(), 2); QCOMPARE(metadata.size(), 2); QVERIFY(failure.isEmpty());
        const auto outgoing = QJsonDocument::fromJson(network.payloads[1]).object();
        QCOMPARE(outgoing["messages"].toArray(), history);
        const auto continued = outgoing["messages"].toArray();
        for (qsizetype index = 0; index < calls.size(); ++index) {
            const auto result = continued[index + 2].toObject();
            QCOMPARE(result["role"], QJsonValue("tool"));
            QCOMPARE(result["tool_call_id"], calls[index].toObject()["id"]);
        }
        QVERIFY(!network.payloads[1].contains("PRIVATE_REASONING_CANARY"));
        QVERIFY(!network.payloads[1].contains("PRIVATE_REASONING_CONTENT_CANARY"));
        QVERIFY(!network.payloads[1].contains("\"reasoning\""));
        QVERIFY(!network.payloads[1].contains("\"reasoning_content\""));
        for (const auto& row : metadata) {
            const auto retained = row.first().toJsonObject();
            QVERIFY(!retained.contains("reasoning")); QVERIFY(!retained.contains("reasoning_content"));
            const auto bytes = QJsonDocument(retained).toJson();
            QVERIFY(!bytes.contains("PRIVATE_REASONING_CANARY"));
            QVERIFY(!bytes.contains("PRIVATE_REASONING_CONTENT_CANARY"));
        }
    }

    void leavesArgumentJsonValidationToDispatcher() {
        FakeNetwork network; auto malformed = call("call-invalid-json");
        auto function = malformed["function"].toObject(); function["arguments"] = "{broken"; malformed["function"] = function;
        network.responses.append(Response{200, completion(QJsonObject{{"role", "assistant"},
            {"tool_calls", QJsonArray{malformed}}}, "tool_calls")});
        NemotronClient client(configuration(), nullptr, &network, pacer_); QSignalSpy success(&client, &ModelTransport::replyReady);
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
        NemotronClient client(configuration(), nullptr, &network, pacer_);
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
        QTest::newRow("unexpected-sse") << QByteArray("data: {\"choices\":[]}\n\ndata: [DONE]\n\nPRIVATE_DIAGNOSTIC_CANARY") << QString("response_invalid_json");
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
    void fragmentedJsonAndUtf8AreBufferedUntilComplete_data() {
        QTest::addColumn<bool>("insideUtf8");
        QTest::newRow("split-json") << false;
        QTest::newRow("split-utf8") << true;
    }
    void fragmentedJsonAndUtf8AreBufferedUntilComplete() {
        QFETCH(bool, insideUtf8);
        const auto content = QString::fromUtf8(QByteArray::fromHex("ceb1f09f8cb5"));
        const auto body = completion(QJsonObject{{"role", "assistant"}, {"content", content}});
        const auto split = insideUtf8 ? body.indexOf(QByteArray::fromHex("f09f8cb5")) + 1 : 7;
        QVERIFY(split > 0 && split + 1 < body.size());
        Response response{200, {}}; response.chunks = {body.left(split), body.mid(split, 1), body.mid(split + 1)};
        FakeNetwork network; network.responses.append(response);
        NemotronClient client(configuration(), nullptr, &network, pacer_);
        QSignalSpy success(&client, &ModelTransport::replyReady), failure(&client, &ModelTransport::requestFailed);
        QSignalSpy completed(&client, &ModelTransport::requestCompleted);
        client.request(messages(), {});
        auto* reply = network.findChild<QNetworkReply*>(); QVERIFY(reply);
        int partialReads = 0; bool publishedBeforeFinished = false;
        connect(reply, &QIODevice::readyRead, &client, [&] {
            ++partialReads;
            publishedBeforeFinished |= !reply->isFinished() && (!success.isEmpty() || !completed.isEmpty());
        });
        QTRY_COMPARE(success.size(), 1); QCOMPARE(partialReads, 3); QVERIFY(!publishedBeforeFinished);
        QCOMPARE(success.first().first().toJsonObject()["content"].toString(), content);
        QCOMPARE(completed.size(), 1); QVERIFY(failure.isEmpty());
        QCOMPARE(network.requests.size(), 1);
        QCOMPARE(QJsonDocument::fromJson(network.payloads.first()).object()["stream"], QJsonValue(false));
    }
    void fragmentedToolCallPreservesExactArguments() {
        auto tool = call("fragmented-call", "validate_run");
        auto function = tool["function"].toObject();
        function["arguments"] = QString::fromUtf8(QJsonDocument(QJsonObject{{"snapshot_id", "snapshot-fixture"},
            {"run_id", "run-fixture"}}).toJson(QJsonDocument::Compact));
        tool["function"] = function;
        const auto body = completion(QJsonObject{{"role", "assistant"}, {"content", QJsonValue::Null},
            {"tool_calls", QJsonArray{tool}}}, "tool_calls");
        Response response{200, {}};
        const auto split = body.indexOf("\\\"run_id\\\"") + 5;
        QVERIFY(split > 5 && split + 1 < body.size());
        response.chunks = {body.left(split), body.mid(split, body.size() - split - 1), body.right(1)};
        FakeNetwork network; network.responses.append(response);
        NemotronClient client(configuration(), nullptr, &network, pacer_);
        QSignalSpy success(&client, &ModelTransport::replyReady), failure(&client, &ModelTransport::requestFailed);
        QSignalSpy completed(&client, &ModelTransport::requestCompleted);
        client.request(messages(), {});
        auto* reply = network.findChild<QNetworkReply*>(); QVERIFY(reply);
        int partialReads = 0; bool publishedBeforeFinished = false;
        connect(reply, &QIODevice::readyRead, &client, [&] {
            ++partialReads;
            publishedBeforeFinished |= !reply->isFinished() && (!success.isEmpty() || !completed.isEmpty());
        });
        QTRY_COMPARE(success.size(), 1); QCOMPARE(partialReads, 3); QVERIFY(!publishedBeforeFinished);
        QCOMPARE(success.first().first().toJsonObject()["tool_calls"].toArray(), QJsonArray{tool});
        QCOMPARE(completed.size(), 1); QVERIFY(failure.isEmpty()); QCOMPARE(network.requests.size(), 1);
    }
    void rejectsMalformedResponses() {
        QFETCH(QByteArray, body); QFETCH(QString, code); FakeNetwork network; network.responses.append(Response{200, body});
        NemotronClient client(configuration(), nullptr, &network, pacer_);
        QSignalSpy success(&client, &ModelTransport::replyReady), failure(&client, &ModelTransport::requestFailed), completed(&client, &ModelTransport::requestCompleted);
        client.request(messages(), {}); QTRY_COMPARE(failure.size(), 1); QCOMPARE(success.size(), 0);
        QCOMPARE(failure.front().at(0).toString(), code); QCOMPARE(completed.size(), 1);
        const auto metadata = completed.first().first().toJsonObject(); QCOMPARE(metadata.value("error_code"), QJsonValue(code));
        const auto exposed = QJsonDocument(metadata).toJson() + failure.first().at(1).toString().toUtf8();
        QVERIFY(!exposed.contains("PRIVATE_DIAGNOSTIC_CANARY")); QVERIFY(!exposed.contains(configuration().apiKey.toUtf8()));
        QVERIFY(!exposed.contains("Authorization")); QVERIFY(!exposed.contains("reasoning_content"));
    }

    void authenticationFailuresAreNotRetriedOrLeaked_data() {
        QTest::addColumn<int>("status"); QTest::addColumn<bool>("afterRetry");
        QTest::newRow("unauthorized") << 401 << false;
        QTest::newRow("forbidden") << 403 << false;
        QTest::newRow("unauthorized-after-service-retry") << 401 << true;
        QTest::newRow("forbidden-after-service-retry") << 403 << true;
    }
    void authenticationFailuresAreNotRetriedOrLeaked() {
        QFETCH(int, status); QFETCH(bool, afterRetry); FakeNetwork network;
        if (afterRetry) network.responses.append(Response{503, "private service failure", 0, QNetworkReply::NoError, "0"});
        network.responses.append(Response{status, "test-secret-must-not-leak private server details", 0, QNetworkReply::NoError, "0"});
        network.responses.append(Response{200, textCompletion()});
        auto config = configuration(); config.maxRetries = 3;
        NemotronClient client(config, nullptr, &network, pacer_);
        QSignalSpy started(&client, &ModelTransport::requestStarted), failure(&client, &ModelTransport::requestFailed);
        QSignalSpy success(&client, &ModelTransport::replyReady), completed(&client, &ModelTransport::requestCompleted);
        client.request(messages(), {}); QTRY_COMPARE(failure.size(), 1); QTest::qWait(30);
        QCOMPARE(network.requests.size(), afterRetry ? 2 : 1); QCOMPARE(network.responses.size(), 1);
        QCOMPARE(started.size(), 1); QCOMPARE(completed.size(), 1); QCOMPARE(failure.size(), 1); QVERIFY(success.isEmpty());
        QCOMPARE(failure.front().at(0).toString(), QString("authentication_failed"));
        QVERIFY(!failure.front().at(1).toString().contains("test-secret")); QVERIFY(!failure.front().at(1).toString().contains("private"));
        const auto metadata = completed.front().front().toJsonObject();
        QVERIFY(!QUuid(metadata["model_request_id"].toString()).isNull());
        QCOMPARE(metadata["http_status"].toInt(), status); QCOMPARE(metadata["retry_count"].toInt(), afterRetry ? 1 : 0);
        QCOMPARE(metadata["outcome"].toString(), "failed"); QCOMPARE(metadata["error_code"].toString(), "authentication_failed");
        QVERIFY(metadata["response_model"].toString().isEmpty());
        const auto exposed = QJsonDocument(metadata).toJson();
        QVERIFY(!exposed.contains(config.apiKey.toUtf8())); QVERIFY(!exposed.contains("private"));
    }

    void serverFailuresRecoverWithinRetryBudget_data() {
        QTest::addColumn<int>("status");
        QTest::newRow("internal-error") << 500; QTest::newRow("bad-gateway") << 502;
        QTest::newRow("unavailable") << 503; QTest::newRow("gateway-timeout") << 504;
    }
    void serverFailuresRecoverWithinRetryBudget() {
        QFETCH(int, status); FakeNetwork network;
        network.responses.append(Response{status, "PRIVATE_HTTP_FAILURE_CANARY", 0, QNetworkReply::NoError, "0"});
        network.responses.append(Response{200, textCompletion()});
        auto config = configuration(); config.maxRetries = 1;
        NemotronClient client(config, nullptr, &network, pacer_);
        QSignalSpy started(&client, &ModelTransport::requestStarted), success(&client, &ModelTransport::replyReady);
        QSignalSpy failure(&client, &ModelTransport::requestFailed), completed(&client, &ModelTransport::requestCompleted);
        client.request(messages(), {}); QTRY_COMPARE(success.size(), 1); QTest::qWait(30);
        QCOMPARE(started.size(), 1); QCOMPARE(completed.size(), 1); QCOMPARE(success.size(), 1); QVERIFY(failure.isEmpty());
        QCOMPARE(network.requests.size(), 2); QCOMPARE(network.payloads[0], network.payloads[1]);
        const auto metadata = completed.front().front().toJsonObject();
        QVERIFY(!QUuid(metadata["model_request_id"].toString()).isNull());
        QCOMPARE(metadata["http_status"].toInt(), 200); QCOMPARE(metadata["retry_count"].toInt(), 1);
        QCOMPARE(metadata["outcome"].toString(), "succeeded"); QVERIFY(metadata["error_code"].toString().isEmpty());
        QCOMPARE(metadata["response_model"].toString(), config.model);
        QVERIFY(!QJsonDocument(metadata).toJson().contains("PRIVATE_HTTP_FAILURE_CANARY"));

        // A retry belongs to the original logical request; a later request gets
        // a fresh correlation ID and its own complete retry allowance.
        network.responses.append(Response{status, "PRIVATE_HTTP_FAILURE_CANARY", 0, QNetworkReply::NoError, "0"});
        network.responses.append(Response{200, textCompletion()});
        client.request(messages(), {}); QTRY_COMPARE(success.size(), 2);
        QCOMPARE(started.size(), 2); QCOMPARE(completed.size(), 2); QCOMPARE(network.requests.size(), 4);
        QCOMPARE(network.payloads[2], network.payloads[3]);
        const auto nextMetadata = completed.at(1).front().toJsonObject();
        QVERIFY(metadata["model_request_id"] != nextMetadata["model_request_id"]);
        QCOMPARE(nextMetadata["retry_count"].toInt(), 1);
    }

    void serverFailuresStopAtRetryBudget_data() {
        QTest::addColumn<int>("status");
        QTest::newRow("internal-error") << 500; QTest::newRow("bad-gateway") << 502;
        QTest::newRow("unavailable") << 503; QTest::newRow("gateway-timeout") << 504;
    }
    void serverFailuresStopAtRetryBudget() {
        QFETCH(int, status); FakeNetwork network;
        for (int i = 0; i < 3; ++i)
            network.responses.append(Response{status, "PRIVATE_HTTP_FAILURE_CANARY", 0, QNetworkReply::NoError, "0"});
        network.responses.append(Response{200, textCompletion()});
        auto config = configuration(); config.maxRetries = 2;
        NemotronClient client(config, nullptr, &network, pacer_);
        QSignalSpy started(&client, &ModelTransport::requestStarted), success(&client, &ModelTransport::replyReady);
        QSignalSpy failure(&client, &ModelTransport::requestFailed), completed(&client, &ModelTransport::requestCompleted);
        client.request(messages(), {}); QTRY_COMPARE(failure.size(), 1); QTest::qWait(30);
        QCOMPARE(network.requests.size(), 3); QCOMPARE(network.responses.size(), 1);
        QCOMPARE(network.payloads[0], network.payloads[1]); QCOMPARE(network.payloads[1], network.payloads[2]);
        QCOMPARE(started.size(), 1); QCOMPARE(completed.size(), 1); QCOMPARE(failure.size(), 1); QVERIFY(success.isEmpty());
        QCOMPARE(failure.front().at(0).toString(), "transport_failed");
        const auto metadata = completed.front().front().toJsonObject();
        QVERIFY(!QUuid(metadata["model_request_id"].toString()).isNull());
        QCOMPARE(metadata["http_status"].toInt(), status); QCOMPARE(metadata["retry_count"].toInt(), 2);
        QCOMPARE(metadata["outcome"].toString(), "failed"); QCOMPARE(metadata["error_code"].toString(), "transport_failed");
        QVERIFY(metadata["response_model"].toString().isEmpty());
        const auto exposed = QJsonDocument(metadata).toJson() + failure.front().at(1).toString().toUtf8();
        QVERIFY(!exposed.contains("PRIVATE_HTTP_FAILURE_CANARY")); QVERIFY(!exposed.contains(config.apiKey.toUtf8()));
    }

    void retryAfterValuesKeepWaitingBounded_data() {
        QTest::addColumn<QByteArray>("retryAfter"); QTest::addColumn<int>("minimumDelayMs");
        QTest::addColumn<int>("completionDeadlineMs");
        QTest::newRow("zero") << QByteArray("0") << 0 << 2000;
        QTest::newRow("missing") << QByteArray{} << 100 << 3000;
        QTest::newRow("negative-seconds") << QByteArray("-1") << 100 << 3000;
        QTest::newRow("fractional-seconds") << QByteArray("0.5") << 100 << 3000;
        QTest::newRow("not-numeric") << QByteArray("PRIVATE_RETRY_AFTER_CANARY") << 100 << 3000;
        QTest::newRow("past-http-date") << QByteArray("Sun, 06 Nov 1994 08:49:37 GMT") << 0 << 2000;
        QTest::newRow("past-rfc850-date") << QByteArray("Sunday, 06-Nov-94 08:49:37 GMT") << 0 << 2000;
        QTest::newRow("past-asctime-date") << QByteArray("Sun Nov  6 08:49:37 1994") << 0 << 2000;
    }
    void retryAfterValuesKeepWaitingBounded() {
        QFETCH(QByteArray, retryAfter); QFETCH(int, minimumDelayMs); QFETCH(int, completionDeadlineMs);
        FakeNetwork network;
        network.responses.append(Response{503, "PRIVATE_HTTP_FAILURE_CANARY", 0, QNetworkReply::NoError, retryAfter});
        network.responses.append(Response{200, textCompletion()});
        // The per-attempt deadline must not fire while no POST is in flight.
        auto config = configuration(); config.maxRetries = 1; config.timeoutMs = 100;
        NemotronClient client(config, nullptr, &network, pacer_);
        QSignalSpy started(&client, &ModelTransport::requestStarted), success(&client, &ModelTransport::replyReady);
        QSignalSpy failure(&client, &ModelTransport::requestFailed), completed(&client, &ModelTransport::requestCompleted);
        QElapsedTimer elapsed; elapsed.start();
        client.request(messages(), {}); QTRY_COMPARE_WITH_TIMEOUT(success.size(), 1, completionDeadlineMs);
        QVERIFY(elapsed.elapsed() >= minimumDelayMs);
        QCOMPARE(network.requests.size(), 2); QCOMPARE(network.payloads[0], network.payloads[1]);
        QCOMPARE(started.size(), 1); QCOMPARE(completed.size(), 1); QVERIFY(failure.isEmpty());
        const auto metadata = completed.front().front().toJsonObject();
        QVERIFY(!QUuid(metadata["model_request_id"].toString()).isNull());
        QCOMPARE(metadata["http_status"].toInt(), 200); QCOMPARE(metadata["retry_count"].toInt(), 1);
        QCOMPARE(metadata["outcome"].toString(), "succeeded"); QVERIFY(metadata["error_code"].toString().isEmpty());
        QVERIFY(metadata["duration_ms"].toDouble() >= minimumDelayMs);
        if (retryAfter.contains("1994") || retryAfter.contains("-94"))
            QCOMPARE(metadata.value("retry_wait_policy").toString(), QString("retry_after_date"));
        const auto exposed = QJsonDocument(metadata).toJson();
        QVERIFY(!exposed.contains("PRIVATE_RETRY_AFTER_CANARY")); QVERIFY(!exposed.contains("PRIVATE_HTTP_FAILURE_CANARY"));
    }

    void rateLimitRetryIsBoundedAndCanRecover() {
        FakeNetwork network; network.responses.append(Response{429, "private", 0, QNetworkReply::NoError, "0"});
        network.responses.append(Response{200, textCompletion()});
        auto config = configuration(); config.maxRetries = 1;
        NemotronClient client(config, nullptr, &network, pacer_);
        QSignalSpy started(&client, &ModelTransport::requestStarted), success(&client, &ModelTransport::replyReady), failure(&client, &ModelTransport::requestFailed), completed(&client, &ModelTransport::requestCompleted);
        client.request(messages(), {}); QTRY_COMPARE(success.size(), 1); QCOMPARE(failure.size(), 0);
        QCOMPARE(started.size(), 1); QCOMPARE(network.requests.size(), 2); QCOMPARE(network.payloads.at(0), network.payloads.at(1));
        QCOMPARE(completed.size(), 1); QCOMPARE(completed.front().front().toJsonObject()["retry_count"].toInt(), 1);
        QCOMPARE(completed.front().front().toJsonObject()["http_status"].toInt(), 200);
    }

    void retryAfterCannotBeClampedIntoAnEarlyPost() {
        FakeNetwork network; network.responses.append(Response{429, {}, 0, QNetworkReply::NoError, "6"});
        network.responses.append(Response{200, textCompletion()});
        auto config = configuration(); config.maxRetries = 1; config.timeoutMs = 1000;
        NemotronClient client(config, nullptr, &network, pacer_);
        QSignalSpy success(&client, &ModelTransport::replyReady), failed(&client, &ModelTransport::requestFailed),
            completed(&client, &ModelTransport::requestCompleted);
        QElapsedTimer elapsed; elapsed.start(); client.requestWithBudget(messages(), {}, 10000, false);
        QTRY_COMPARE_WITH_TIMEOUT(success.size(), 1, 11000);
        QVERIFY(elapsed.elapsed() >= 5900); QVERIFY(failed.isEmpty()); QCOMPARE(network.requests.size(), 2);
        const auto metadata = completed.first().first().toJsonObject();
        QCOMPARE(metadata.value("retry_wait_policy").toString(), QString("retry_after_seconds"));
        QCOMPARE(metadata.value("retry_delays_ms").toArray(), QJsonArray({6000}));
    }
    void overBudgetServerWaitIsRetainedWithoutAnotherPost_data() {
        QTest::addColumn<QByteArray>("header"); QTest::addColumn<int>("status");
        QTest::newRow("seconds") << QByteArray("30") << 429;
        QTest::newRow("overflow") << QByteArray("9999999999999999999999999999999999999999") << 429;
        QTest::newRow("date") << QByteArray("Thu, 31 Dec 2099 23:59:59 GMT") << 429;
        QTest::newRow("service-unavailable") << QByteArray("30") << 503;
    }
    void overBudgetServerWaitIsRetainedWithoutAnotherPost() {
        QFETCH(QByteArray, header); QFETCH(int, status);
        FakeNetwork network; network.responses.append(Response{status, "PRIVATE_RESPONSE", 0, QNetworkReply::NoError, header});
        auto config = configuration(); config.maxRetries = 2;
        NemotronClient client(config, nullptr, &network, pacer_);
        QSignalSpy failed(&client, &ModelTransport::requestFailed), completed(&client, &ModelTransport::requestCompleted);
        client.requestWithBudget(messages(), {}, 500, false); QTRY_COMPARE(failed.size(), 1);
        QCOMPARE(network.requests.size(), 1);
        QCOMPARE(failed.first().first().toString(), status == 429 ? QString("rate_limit_wait_exceeds_budget") : QString("retry_wait_exceeds_budget"));
        const auto metadata = completed.first().first().toJsonObject();
        QCOMPARE(metadata.value("retry_count").toInt(), 0);
        QCOMPARE(metadata.value("retry_wait_policy").toString(), QString("retry_after_exceeds_budget"));
        QVERIFY(metadata.value("retry_delays_ms").toArray().isEmpty());
        const auto publicBytes = QJsonDocument(metadata).toJson();
        if (header.size() > 8) QVERIFY(!publicBytes.contains(header));
        QVERIFY(!publicBytes.contains("PRIVATE_RESPONSE"));
        network.responses.append(Response{200, textCompletion()}); QSignalSpy success(&client, &ModelTransport::replyReady);
        // A new logical request cannot erase the provider's unexpired wait.
        client.requestWithBudget(messages(), {}, 500, false); QTRY_COMPARE(failed.size(), 2);
        QCOMPARE(network.requests.size(), 1); QVERIFY(success.isEmpty());
        QVERIFY(!completed.last().first().toJsonObject().contains("retry_wait_policy"));
    }
    void delayedRetryTimerHonorsOriginalMonotonicBudget() {
        FakeNetwork network; network.responses.append(Response{503, {}});
        network.responses.append(Response{200, textCompletion()});
        auto config = configuration(); config.maxRetries = 1;
        NemotronClient client(config, nullptr, &network, pacer_);
        QSignalSpy failed(&client, &ModelTransport::requestFailed), success(&client, &ModelTransport::replyReady);
        client.requestWithBudget(messages(), {}, 1000, false);
        const auto replies = network.findChildren<QNetworkReply*>(QString{}, Qt::FindDirectChildrenOnly);
        QCOMPARE(replies.size(), 1);
        // Client's earlier finished slot schedules backoff first. Block timer
        // delivery afterward, without allowing an early second POST.
        connect(replies.first(), &QNetworkReply::finished, &network, [] { QTest::qSleep(1100); });
        QTRY_COMPARE_WITH_TIMEOUT(failed.size(), 1, 3000);
        QCOMPARE(failed.first().first().toString(), QString("request_budget_exceeded"));
        QCOMPARE(network.requests.size(), 1); QVERIFY(success.isEmpty());
    }

    void exhaustedRateLimitRetriesFail() {
        FakeNetwork network; for (int i = 0; i < 3; ++i) network.responses.append(Response{429, "private", 0, QNetworkReply::NoError, "0"});
        auto config = configuration(); config.maxRetries = 2;
        NemotronClient client(config, nullptr, &network, pacer_); QSignalSpy failure(&client, &ModelTransport::requestFailed);
        client.request(messages(), {}); QTRY_COMPARE(failure.size(), 1); QCOMPARE(network.requests.size(), 3);
        QCOMPARE(failure.front().at(0).toString(), QString("rate_limited"));
    }

    void transientNetworkFailureRetries() {
        FakeNetwork network; network.responses.append(Response{0, {}, 0, QNetworkReply::TemporaryNetworkFailureError});
        network.responses.append(Response{200, textCompletion()});
        auto config = configuration(); config.maxRetries = 1;
        NemotronClient client(config, nullptr, &network, pacer_); QSignalSpy success(&client, &ModelTransport::replyReady);
        client.request(messages(), {}); QTRY_COMPARE_WITH_TIMEOUT(success.size(), 1, 3000); QCOMPARE(network.requests.size(), 2);
    }

    void timeoutAndCancellationDiscardLateReplies() {
        FakeNetwork network; network.responses.append(Response{200, textCompletion(), 100});
        auto config = configuration(); config.timeoutMs = 10;
        NemotronClient client(config, nullptr, &network, pacer_);
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
        network.responses.append(Response{200, textCompletion()});
        auto config = configuration(); config.maxRetries = 1;
        NemotronClient client(config, nullptr, &network, pacer_);
        QSignalSpy failure(&client, &ModelTransport::requestFailed), completed(&client, &ModelTransport::requestCompleted);
        QSignalSpy success(&client, &ModelTransport::replyReady), started(&client, &ModelTransport::requestStarted);
        client.request(messages(), {}); QTest::qWait(50); client.cancel();
        QTRY_COMPARE(failure.size(), 1); QCOMPARE(failure.front().at(0).toString(), QString("cancelled"));
        QTest::qWait(1100); QCOMPARE(network.requests.size(), 1);
        QCOMPARE(completed.size(), 1); QCOMPARE(failure.size(), 1); QVERIFY(success.isEmpty()); QCOMPARE(started.size(), 1);
        const auto metadata = completed.front().front().toJsonObject();
        QCOMPARE(metadata["retry_count"].toInt(), 0); QCOMPARE(metadata["http_status"].toInt(), 503);
        QCOMPARE(metadata["outcome"].toString(), "cancelled"); QCOMPARE(metadata["error_code"].toString(), "cancelled");
        QCOMPARE(metadata.value("retry_wait_policy"), QJsonValue("retry_after_seconds"));
        QCOMPARE(metadata.value("retry_delays_ms").toArray(), QJsonArray{1000});
        client.request(messages(), {}); QTRY_COMPARE(success.size(), 1);
        QCOMPARE(network.requests.size(), 2); QCOMPARE(completed.size(), 2); QCOMPARE(started.size(), 2);
        QVERIFY(metadata["model_request_id"] != completed.at(1).front().toJsonObject()["model_request_id"]);
        QCOMPARE(completed.at(1).front().toJsonObject()["retry_count"].toInt(), 0);
        QVERIFY(!completed.at(1).front().toJsonObject().contains("retry_delays_ms"));
    }

    void timeoutOnRetriedPostCompletesOnceAndAllowsAnotherRequest() {
        FakeNetwork network;
        network.responses.append(Response{503, "PRIVATE_HTTP_FAILURE_CANARY", 0, QNetworkReply::NoError, "0"});
        network.responses.append(Response{200, textCompletion(), 100});
        auto config = configuration(); config.maxRetries = 2; config.timeoutMs = 10;
        NemotronClient client(config, nullptr, &network, pacer_);
        QSignalSpy started(&client, &ModelTransport::requestStarted), success(&client, &ModelTransport::replyReady);
        QSignalSpy failure(&client, &ModelTransport::requestFailed), completed(&client, &ModelTransport::requestCompleted);
        client.request(messages(), {}); QTRY_COMPARE(failure.size(), 1); QTest::qWait(150);
        QCOMPARE(network.requests.size(), 2); QCOMPARE(network.payloads[0], network.payloads[1]);
        QCOMPARE(started.size(), 1); QCOMPARE(completed.size(), 1); QCOMPARE(failure.size(), 1); QVERIFY(success.isEmpty());
        QCOMPARE(failure.front().at(0).toString(), "timeout");
        const auto metadata = completed.front().front().toJsonObject();
        QCOMPARE(metadata["http_status"].toInt(), 200); QCOMPARE(metadata["retry_count"].toInt(), 1);
        QCOMPARE(metadata["outcome"].toString(), "failed"); QCOMPARE(metadata["error_code"].toString(), "timeout");
        QVERIFY(!QUuid(metadata["model_request_id"].toString()).isNull());
        network.responses.append(Response{200, textCompletion()});
        client.request(messages(), {}); QTRY_COMPARE(success.size(), 1);
        QCOMPARE(network.requests.size(), 3); QCOMPARE(completed.size(), 2); QCOMPARE(started.size(), 2); QCOMPARE(failure.size(), 1);
        QVERIFY(metadata["model_request_id"] != completed.at(1).front().toJsonObject()["model_request_id"]);
        QCOMPARE(completed.at(1).front().toJsonObject()["retry_count"].toInt(), 0);
    }

    void rejectsOversizedResponseAndRedirect() {
        FakeNetwork network; network.responses.append(Response{200, QByteArray(2 * 1024 * 1024 + 1, 'x')});
        NemotronClient client(configuration(), nullptr, &network, pacer_); QSignalSpy failure(&client, &ModelTransport::requestFailed);
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
        NemotronClient client(configuration(), nullptr, &network, pacer_);
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
        auto* client = new NemotronClient(configuration(), nullptr, &network, pacer_);
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
        NemotronClient missing(config, nullptr, &network, pacer_); QSignalSpy missingFailure(&missing, &ModelTransport::requestFailed), completed(&missing, &ModelTransport::requestCompleted);
        missing.request(messages(), {}); QCOMPARE(missingFailure.size(), 1);
        QCOMPARE(missingFailure.front().at(0).toString(), QString("credentials_missing"));
        QCOMPARE(completed.size(), 1); QCOMPARE(completed.front().front().toJsonObject()["http_status"].toInt(), 0);
        QCOMPARE(completed.front().front().toJsonObject()["retry_count"].toInt(), 0);
        QCOMPARE(completed.front().front().toJsonObject()["error_code"].toString(), "credentials_missing");
        config = configuration(); config.endpoint = QUrl("http://localhost:8080/v1/chat/completions");
        NemotronClient insecure(config, nullptr, &network, pacer_); QSignalSpy insecureFailure(&insecure, &ModelTransport::requestFailed);
        insecure.request(messages(), {}); QCOMPARE(insecureFailure.size(), 1);
        QCOMPARE(insecureFailure.front().at(0).toString(), QString("configuration_invalid")); QCOMPARE(network.requests.size(), 0);
    }
};

QTEST_GUILESS_MAIN(NemotronClientTests)
#include "nemotron_client_tests.moc"
