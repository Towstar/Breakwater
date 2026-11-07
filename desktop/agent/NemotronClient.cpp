#include "NemotronClient.hpp"

#include <QJsonDocument>
#include <QJsonParseError>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QSet>
#include <QThread>
#include <algorithm>

namespace breakwater::desktop::agent {
namespace {
bool retryable(QNetworkReply::NetworkError error) {
    return error == QNetworkReply::ConnectionRefusedError || error == QNetworkReply::RemoteHostClosedError ||
           error == QNetworkReply::TimeoutError || error == QNetworkReply::TemporaryNetworkFailureError ||
           error == QNetworkReply::NetworkSessionFailedError;
}

bool parseAssistant(const QByteArray& bytes, QJsonObject& assistant, QString& model) {
    QJsonParseError error;
    const auto document = QJsonDocument::fromJson(bytes, &error);
    if (error.error != QJsonParseError::NoError || !document.isObject()) return false;
    model = document.object().value("model").toString().left(256);
    const auto choices = document.object().value("choices").toArray();
    if (choices.size() != 1 || !choices.at(0).isObject()) return false;
    const auto choice = choices.at(0).toObject();
    const auto reason = choice.value("finish_reason");
    if (!reason.isUndefined() && !reason.isNull() && reason.toString() != "stop" && reason.toString() != "tool_calls") return false;
    if (!choice.value("message").isObject()) return false;
    const auto message = choice.value("message").toObject();
    if (message.value("role").toString() != "assistant") return false;
    const auto content = message.value("content");
    if (!content.isUndefined() && !content.isNull() && !content.isString()) return false;
    const auto text = content.toString();
    // A misconfigured reasoning parser must not leak private reasoning through visible content.
    if (text.contains("<think>", Qt::CaseInsensitive) || text.contains("</think>", Qt::CaseInsensitive)) return false;
    assistant = QJsonObject{{"role", "assistant"}, {"content", content.isString() ? content : QJsonValue(QJsonValue::Null)}};
    const auto toolValue = message.value("tool_calls");
    if (!toolValue.isUndefined() && !toolValue.isNull() && !toolValue.isArray()) return false;
    const auto calls = toolValue.toArray();
    if (calls.size() > 32) return false;
    if (reason.toString() == "tool_calls" && calls.isEmpty()) return false;
    QJsonArray cleanCalls;
    QSet<QString> ids;
    for (const auto& value : calls) {
        if (!value.isObject()) return false;
        const auto call = value.toObject();
        const auto id = call.value("id").toString();
        const auto function = call.value("function").toObject();
        const auto name = function.value("name").toString();
        if (id.isEmpty() || id.size() > 256 || ids.contains(id) || call.value("type").toString() != "function" ||
            name.isEmpty() || name.size() > 128 || !function.value("arguments").isString() ||
            function.value("arguments").toString().size() > 65536) return false;
        ids.insert(id);
        cleanCalls.append(QJsonObject{{"id", id}, {"type", "function"},
            {"function", QJsonObject{{"name", name}, {"arguments", function.value("arguments")}}}});
    }
    if (text.trimmed().isEmpty() && cleanCalls.isEmpty()) return false;
    if (!cleanCalls.isEmpty()) assistant.insert("tool_calls", cleanCalls);
    // Only the public assistant fields above survive. reasoning/reasoning_content are discarded.
    return true;
}
} // namespace

NemotronConfig NemotronConfig::fromEnvironment() {
    NemotronConfig config;
    config.apiKey = qEnvironmentVariable("NVIDIA_API_KEY").trimmed();
    const auto endpoint = qEnvironmentVariable("NVIDIA_BASE_URL").trimmed();
    if (!endpoint.isEmpty()) {
        config.endpoint = QUrl(endpoint);
        auto path = config.endpoint.path();
        while (path.endsWith('/')) path.chop(1);
        if (!path.endsWith("/chat/completions")) path += "/chat/completions";
        config.endpoint.setPath(path);
    }
    const auto model = qEnvironmentVariable("NEMOTRON_MODEL").trimmed();
    if (!model.isEmpty()) config.model = model;
    return config;
}

NemotronClient::NemotronClient(NemotronConfig config, QObject* parent, QNetworkAccessManager* manager)
    : ModelTransport(parent), config_(std::move(config)),
      manager_(manager ? manager : new QNetworkAccessManager(this)) {
    config_.timeoutMs = std::clamp(config_.timeoutMs, 1, 300000);
    config_.maxRetries = std::clamp(config_.maxRetries, 0, 3);
    timeout_.setParent(this);
    retry_.setParent(this);
    timeout_.setSingleShot(true);
    retry_.setSingleShot(true);
    connect(&timeout_, &QTimer::timeout, this, [this] {
        if (active_) fail("timeout", "The model request exceeded its time limit.");
    });
    connect(&retry_, &QTimer::timeout, this, &NemotronClient::send);
}

NemotronClient::~NemotronClient() {
    active_ = false;
    timeout_.stop();
    retry_.stop();
    releaseReply(true);
}

void NemotronClient::request(const QJsonArray& messages, const QJsonArray& tools) {
    if (active_) { emit requestFailed("request_busy", "A model request is already active."); return; }
    if (config_.apiKey.isEmpty()) { emit requestFailed("credentials_missing", "NVIDIA_API_KEY is not configured."); return; }
    if (config_.apiKey.size() > 4096 || config_.apiKey.contains('\r') || config_.apiKey.contains('\n') || config_.model.trimmed().isEmpty() ||
        config_.model.size() > 256 || !config_.endpoint.isValid() || config_.endpoint.scheme() != "https" ||
        config_.endpoint.host().isEmpty() || !config_.endpoint.userInfo().isEmpty() ||
        config_.endpoint.hasFragment() || !manager_ || manager_->thread() != thread() || QThread::currentThread() != thread()) {
        emit requestFailed("configuration_invalid", "The model configuration is invalid; HTTPS is required."); return;
    }
    if (messages.isEmpty()) { emit requestFailed("request_invalid", "At least one model message is required."); return; }
    QJsonObject body{{"model", config_.model}, {"messages", messages}, {"stream", false},
                     {"max_tokens", 4096}, {"reasoning_budget", 1024}, {"temperature", 0.2}};
    if (!tools.isEmpty()) { body.insert("tools", tools); body.insert("tool_choice", "auto"); }
    payload_ = QJsonDocument(body).toJson(QJsonDocument::Compact);
    if (payload_.size() > maximumResponseBytes) {
        payload_.clear(); emit requestFailed("request_too_large", "The model request exceeds the size limit."); return;
    }
    active_ = true;
    retries_ = 0;
    responseModel_.clear();
    const auto generation = ++requestGeneration_;
    const QPointer<NemotronClient> guard(this);
    emit requestStarted();
    // A direct requestStarted slot may cancel or destroy the session before the POST.
    if (guard && active_ && generation == requestGeneration_) send();
}

void NemotronClient::send() {
    if (!active_) return;
    if (!manager_) { fail("transport_failed", "The model network transport is no longer available."); return; }
    response_.clear();
    QNetworkRequest request(config_.endpoint);
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    request.setRawHeader("Accept", "application/json");
    request.setRawHeader("Authorization", QByteArray("Bearer ") + config_.apiKey.toUtf8());
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::ManualRedirectPolicy);
    request.setTransferTimeout(config_.timeoutMs);
    reply_ = manager_->post(request, payload_);
    reply_->setReadBufferSize(maximumResponseBytes + 1);
    connect(reply_, &QIODevice::readyRead, this, &NemotronClient::consume);
    connect(reply_, &QNetworkReply::finished, this, &NemotronClient::finish);
    timeout_.start(config_.timeoutMs);
}

void NemotronClient::consume() {
    if (!active_ || !reply_) return;
    const auto remaining = maximumResponseBytes - response_.size();
    response_.append(reply_->read(remaining + 1));
    if (response_.size() > maximumResponseBytes || reply_->bytesAvailable() > 0)
        fail("response_too_large", "The model response exceeds the size limit.");
}

void NemotronClient::releaseReply(bool abort) {
    if (!reply_) return;
    auto* reply = reply_.data();
    reply_.clear();
    disconnect(reply, nullptr, this, nullptr);
    if (abort && reply->isRunning()) reply->abort();
    reply->deleteLater();
}

void NemotronClient::fail(const QString& code, const QString& message) {
    if (!active_) return;
    active_ = false;
    timeout_.stop();
    retry_.stop();
    releaseReply(true);
    payload_.clear();
    response_.clear();
    emit requestFailed(code, message);
}

void NemotronClient::scheduleRetry() {
    int delay = 250 * (1 << retries_);
    const auto retryAfter = reply_->rawHeader("Retry-After");
    bool ok = false;
    const auto seconds = retryAfter.toLongLong(&ok);
    if (ok && seconds >= 0) delay = static_cast<int>(std::min<qint64>(seconds, 5) * 1000);
    ++retries_;
    timeout_.stop();
    releaseReply(false);
    response_.clear();
    retry_.start(delay);
}

void NemotronClient::finish() {
    if (!active_ || !reply_) return;
    const QPointer<NemotronClient> guard(this);
    const auto generation = requestGeneration_;
    const QPointer<QNetworkReply> completedReply = reply_;
    consume();
    // Consuming the final bytes can fail and synchronously notify a slot that
    // destroys this client or starts a replacement request. The old completion
    // must not inspect the replacement reply or stop its timeout.
    if (!guard || generation != requestGeneration_ || !active_ || !reply_ || reply_ != completedReply) return;
    timeout_.stop();
    const auto status = reply_->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    const auto error = reply_->error();
    if ((status == 429 || status == 500 || status == 502 || status == 503 || status == 504 ||
         (status == 0 && retryable(error))) && retries_ < config_.maxRetries) {
        scheduleRetry(); return;
    }
    if (status == 401 || status == 403) { fail("authentication_failed", "NVIDIA rejected the credentials or model access."); return; }
    if (status == 429) { fail("rate_limited", "NVIDIA rate limits prevented the model request."); return; }
    if (status >= 300 && status < 400) { fail("redirect_rejected", "The model endpoint returned a redirect; no credentials were forwarded."); return; }
    if (status < 200 || status >= 300 || error != QNetworkReply::NoError) {
        fail(status == 404 ? "model_unavailable" : "transport_failed",
             status == 404 ? "The configured model endpoint is unavailable." : "The model service request failed."); return;
    }
    QJsonObject assistant;
    if (!parseAssistant(response_, assistant, responseModel_)) { fail("response_invalid", "The model returned an invalid or unusable response."); return; }
    active_ = false;
    releaseReply(false);
    payload_.clear();
    response_.clear();
    emit replyReady(assistant);
}

void NemotronClient::cancel() {
    if (active_) fail("cancelled", "The model request was cancelled.");
}

} // namespace breakwater::desktop::agent
