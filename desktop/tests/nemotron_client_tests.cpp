#include "agent/NemotronClient.hpp"

#include <QBuffer>
#include <QJsonDocument>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QSignalSpy>
#include <QtTest>
#include <algorithm>
#include <cstring>

using namespace breakwater::desktop::agent;

namespace {
struct Response {
    int status = 200;
    QByteArray bytes;
    int delayMs = 0;
    QNetworkReply::NetworkError error = QNetworkReply::NoError;
    QByteArray retryAfter;
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
            if (!response_.bytes.isEmpty()) emit readyRead();
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

    void rejectsMalformedResponses_data() {
        QTest::addColumn<QByteArray>("body");
        QTest::newRow("invalid-json") << QByteArray("{not json");
        QTest::newRow("empty-choices") << QByteArray("{\"choices\":[]}");
        QTest::newRow("empty-message") << completion(QJsonObject{{"role", "assistant"}, {"content", QJsonValue::Null}});
        QTest::newRow("wrong-role") << completion(QJsonObject{{"role", "user"}, {"content", "wrong"}});
        QTest::newRow("duplicate-call-id") << completion(QJsonObject{{"role", "assistant"}, {"content", QJsonValue::Null},
            {"tool_calls", QJsonArray{call("duplicate"), call("duplicate")}}}, "tool_calls");
        auto wrongArguments = call("bad"); auto function = wrongArguments["function"].toObject(); function["arguments"] = QJsonObject{};
        wrongArguments["function"] = function;
        QTest::newRow("non-string-arguments") << completion(QJsonObject{{"role", "assistant"}, {"tool_calls", QJsonArray{wrongArguments}}}, "tool_calls");
        QTest::newRow("unparsed-reasoning") << completion(QJsonObject{{"role", "assistant"}, {"content", "<think>private</think>answer"}});
        QTest::newRow("truncated-generation") << completion(QJsonObject{{"role", "assistant"}, {"content", "incomplete"}}, "length");
    }
    void rejectsMalformedResponses() {
        QFETCH(QByteArray, body); FakeNetwork network; network.responses.append(Response{200, body});
        NemotronClient client(configuration(), nullptr, &network);
        QSignalSpy success(&client, &ModelTransport::replyReady), failure(&client, &ModelTransport::requestFailed);
        client.request(messages(), {}); QTRY_COMPARE(failure.size(), 1); QCOMPARE(success.size(), 0);
        QCOMPARE(failure.front().at(0).toString(), QString("response_invalid"));
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
        QSignalSpy started(&client, &ModelTransport::requestStarted), success(&client, &ModelTransport::replyReady), failure(&client, &ModelTransport::requestFailed);
        client.request(messages(), {}); QTRY_COMPARE(success.size(), 1); QCOMPARE(failure.size(), 0);
        QCOMPARE(started.size(), 1); QCOMPARE(network.requests.size(), 2); QCOMPARE(network.payloads.at(0), network.payloads.at(1));
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
        QSignalSpy failure(&client, &ModelTransport::requestFailed), success(&client, &ModelTransport::replyReady);
        client.request(messages(), {}); QTRY_COMPARE(failure.size(), 1);
        QCOMPARE(failure.front().at(0).toString(), QString("timeout"));
        network.responses.append(Response{200, textCompletion(), 100}); client.request(messages(), {}); client.cancel();
        QCOMPARE(failure.size(), 2); QCOMPARE(failure.at(1).at(0).toString(), QString("cancelled"));
        QTest::qWait(150); QCOMPARE(success.size(), 0); QCOMPARE(failure.size(), 2);
        network.responses.append(Response{200, textCompletion()}); client.request(messages(), {});
        QTRY_COMPARE(success.size(), 1); // Client remains reusable after cleanup.
    }

    void cancellationWhileWaitingForRetryPreventsAnotherPost() {
        FakeNetwork network; network.responses.append(Response{503, {}, 0, QNetworkReply::NoError, "1"});
        auto config = configuration(); config.maxRetries = 1;
        NemotronClient client(config, nullptr, &network); QSignalSpy failure(&client, &ModelTransport::requestFailed);
        client.request(messages(), {}); QTest::qWait(50); client.cancel();
        QTRY_COMPARE(failure.size(), 1); QCOMPARE(failure.front().at(0).toString(), QString("cancelled"));
        QTest::qWait(1100); QCOMPARE(network.requests.size(), 1);
    }

    void rejectsOversizedResponseAndRedirect() {
        FakeNetwork network; network.responses.append(Response{200, QByteArray(2 * 1024 * 1024 + 1, 'x')});
        NemotronClient client(configuration(), nullptr, &network); QSignalSpy failure(&client, &ModelTransport::requestFailed);
        client.request(messages(), {}); QTRY_COMPARE(failure.size(), 1);
        QCOMPARE(failure.front().at(0).toString(), QString("response_too_large"));
        network.responses.append(Response{302, {}}); client.request(messages(), {}); QTRY_COMPARE(failure.size(), 2);
        QCOMPARE(failure.at(1).at(0).toString(), QString("redirect_rejected")); QCOMPARE(network.requests.size(), 2);
    }

    void refusesMissingCredentialsAndInsecureConfiguration() {
        FakeNetwork network; auto config = configuration(); config.apiKey.clear();
        NemotronClient missing(config, nullptr, &network); QSignalSpy missingFailure(&missing, &ModelTransport::requestFailed);
        missing.request(messages(), {}); QCOMPARE(missingFailure.size(), 1);
        QCOMPARE(missingFailure.front().at(0).toString(), QString("credentials_missing"));
        config = configuration(); config.endpoint = QUrl("http://localhost:8080/v1/chat/completions");
        NemotronClient insecure(config, nullptr, &network); QSignalSpy insecureFailure(&insecure, &ModelTransport::requestFailed);
        insecure.request(messages(), {}); QCOMPARE(insecureFailure.size(), 1);
        QCOMPARE(insecureFailure.front().at(0).toString(), QString("configuration_invalid")); QCOMPARE(network.requests.size(), 0);
    }
};

QTEST_GUILESS_MAIN(NemotronClientTests)
#include "nemotron_client_tests.moc"
