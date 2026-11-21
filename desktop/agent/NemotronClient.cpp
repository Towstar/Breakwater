#include "NemotronClient.hpp"
#include <QDateTime>
#include <QRegularExpression>

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QRandomGenerator>
#include <QJsonDocument>
#include <QJsonParseError>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QSet>
#include <QStandardPaths>
#include <QThread>
#include <QUuid>
#include <algorithm>
#include <cmath>
#include <limits>

namespace breakwater::desktop::agent {
namespace {
QString localEnvironmentFile() {
    const auto findFile = [](QDir directory) -> QString {
        // Check the launch directory, then its nearest checkout root. Stop at the
        // first .git boundary so a worktree cannot inherit another checkout's key.
        const auto local = directory.filePath(".env");
        if (QFileInfo(local).isFile()) return local;
        do {
            if (directory.exists(".git")) {
                const auto path = directory.filePath(".env");
                return QFileInfo(path).isFile() ? path : QString{};
            }
        } while (directory.cdUp());
        return {};
    };
    const auto workingFile = findFile(QDir::current());
    if (!workingFile.isEmpty()) return workingFile;
    return findFile(QDir(QCoreApplication::applicationDirPath()));
}

QHash<QString, QString> localEnvironmentValues(const QString& path) {
    QHash<QString, QString> values;
    QFile file(path);
    constexpr qint64 maximumBytes = 64 * 1024;
    if (path.isEmpty() || !file.open(QIODevice::ReadOnly) || file.size() > maximumBytes) return values;
    const auto bytes = file.read(maximumBytes + 1);
    if (bytes.size() > maximumBytes || file.error() != QFileDevice::NoError) return values;
    auto text = QString::fromUtf8(bytes);
    if (text.startsWith(QChar(0xfeff))) text.remove(0, 1);
    for (auto line : text.split('\n')) {
        line = line.trimmed();
        if (line.isEmpty() || line.startsWith('#')) continue;
        if (line.startsWith("export ") || line.startsWith("export\t")) line = line.mid(6).trimmed();
        const auto separator = line.indexOf('=');
        if (separator < 0) continue;
        const auto name = line.left(separator).trimmed();
        if (name != "NVIDIA_API_KEY" && name != "NVIDIA_BASE_URL" && name != "NEMOTRON_MODEL" && name != "BREAKWATER_NEMOTRON_INTERVAL_MS") continue;
        auto value = line.mid(separator + 1).trimmed();
        if (value.startsWith('\'') || value.startsWith('"')) {
            const auto end = value.indexOf(value.front(), 1);
            if (end < 0) return {};
            const auto suffix = value.mid(end + 1).trimmed();
            if (!suffix.isEmpty() && !suffix.startsWith('#')) return {};
            value = value.mid(1, end - 1);
        } else {
            for (qsizetype i = 1; i < value.size(); ++i) {
                if (value[i] == '#' && value[i - 1].isSpace()) {
                    value = value.left(i).trimmed();
                    break;
                }
            }
            if (value.startsWith('#')) value.clear();
        }
        // Treat values literally: never execute shell syntax, expand variables,
        // or publish credentials into the process environment or worker children.
        values.insert(name, value);
    }
    return values;
}

QString modelIdentifier(const QString& value, const QString& secret = {}) {
    if (value.isEmpty() || value.size() > 256 || (!secret.isEmpty() && value.contains(secret))) return {};
    for (const auto character : value) {
        const auto c = character.unicode();
        if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') ||
              c == '/' || c == '.' || c == '_' || c == '-' || c == ':')) return {};
    }
    return value;
}

bool retryable(QNetworkReply::NetworkError error) {
    return error == QNetworkReply::ConnectionRefusedError || error == QNetworkReply::RemoteHostClosedError ||
           error == QNetworkReply::TimeoutError || error == QNetworkReply::TemporaryNetworkFailureError ||
           error == QNetworkReply::NetworkSessionFailedError;
}

// Parse only Retry-After's defined seconds/date forms. Raw values never leave
// this function. Excessive valid waits must not turn into an early POST.
std::pair<qint64, QString> retryAfter(const QByteArray& header) {
    const auto value = header.trimmed();
    if (value.isEmpty()) return {-1, "bounded_backoff"};
    if (std::all_of(value.cbegin(), value.cend(), [](char ch) { return ch >= '0' && ch <= '9'; })) {
        bool ok = false; const auto seconds = value.toULongLong(&ok);
        const auto maximum = std::numeric_limits<qint64>::max() / 4;
        return {!ok || seconds > quint64(maximum / 1000) ? maximum : qint64(seconds * 1000), "retry_after_seconds"};
    }
    if (value.size() > 128) return {-1, "invalid_retry_after_backoff"};
    const auto text = QString::fromLatin1(value);
    auto date = QDateTime::fromString(text, Qt::RFC2822Date);
    if (!date.isValid() && text.endsWith(" GMT"))
        date = QDateTime::fromString(text.chopped(4) + " +0000", "ddd, dd MMM yyyy HH:mm:ss tt");
    if (!date.isValid()) {
        static const QRegularExpression obsolete(QStringLiteral("^([A-Za-z]+, [0-9]{2}-[A-Za-z]{3}-)([0-9]{2})( [0-9]{2}:[0-9]{2}:[0-9]{2} GMT)$"));
        const auto match = obsolete.match(text);
        if (match.hasMatch()) {
            const auto nowYear = QDateTime::currentDateTimeUtc().date().year();
            int year = (nowYear / 100) * 100 + match.captured(2).toInt();
            if (year > nowYear + 50) year -= 100;
            const auto expanded = match.captured(1) + QString::number(year) + match.captured(3);
            date = QDateTime::fromString(expanded.chopped(4) + " +0000", "dddd, dd-MMM-yyyy HH:mm:ss tt");
        } else date = QDateTime::fromString(text.simplified() + " +0000", "ddd MMM d HH:mm:ss yyyy tt");
    }
    return date.isValid() ? std::pair<qint64, QString>{std::max<qint64>(0, QDateTime::currentDateTimeUtc().msecsTo(date)), "retry_after_date"}
                          : std::pair<qint64, QString>{-1, "invalid_retry_after_backoff"};
}

void numericHeader(QJsonObject& result, const char* field, const QByteArray& raw) {
    const auto value = raw.trimmed();
    if (value.isEmpty() || value.size() > 13 || !std::all_of(value.cbegin(), value.cend(), [](char ch) { return ch >= '0' && ch <= '9'; })) return;
    bool ok = false; const auto number = value.toULongLong(&ok);
    if (ok && number <= 1000000000000ULL) result[field] = qint64(number);
}

const char* parseAssistant(const QByteArray& bytes, QJsonObject& assistant, QString& model) {
    QJsonParseError error;
    const auto document = QJsonDocument::fromJson(bytes, &error);
    if (error.error != QJsonParseError::NoError) return "response_invalid_json";
    if (!document.isObject()) return "response_invalid_envelope";
    model = modelIdentifier(document.object().value("model").toString());
    const auto choices = document.object().value("choices").toArray();
    if (choices.size() != 1 || !choices.at(0).isObject()) return "response_invalid_choices";
    const auto choice = choices.at(0).toObject();
    const auto reason = choice.value("finish_reason");
    if (reason == "length") return "response_truncated";
    if (!reason.isUndefined() && !reason.isNull() && reason.toString() != "stop" && reason.toString() != "tool_calls") return "response_invalid_finish_reason";
    if (!choice.value("message").isObject()) return "response_invalid_message";
    const auto message = choice.value("message").toObject();
    if (message.value("role").toString() != "assistant") return "response_invalid_role";
    const auto content = message.value("content");
    if (!content.isUndefined() && !content.isNull() && !content.isString()) return "response_invalid_content";
    const auto text = content.toString();
    // A misconfigured reasoning parser must not leak private reasoning through visible content.
    if (text.contains("<think>", Qt::CaseInsensitive) || text.contains("</think>", Qt::CaseInsensitive)) return "response_invalid_private_reasoning";
    assistant = QJsonObject{{"role", "assistant"}, {"content", content.isString() ? content : QJsonValue(QJsonValue::Null)}};
    const auto toolValue = message.value("tool_calls");
    if (!toolValue.isUndefined() && !toolValue.isNull() && !toolValue.isArray()) return "response_invalid_tools";
    const auto calls = toolValue.toArray();
    if (calls.size() > 32) return "response_invalid_tools";
    if (reason.toString() == "tool_calls" && calls.isEmpty()) return "response_invalid_tools";
    QJsonArray cleanCalls;
    QSet<QString> ids;
    for (const auto& value : calls) {
        if (!value.isObject()) return "response_invalid_tools";
        const auto call = value.toObject();
        const auto id = call.value("id").toString();
        const auto function = call.value("function").toObject();
        const auto name = function.value("name").toString();
        if (id.isEmpty() || id.size() > 256 || ids.contains(id) || call.value("type").toString() != "function" ||
            name.isEmpty() || name.size() > 128 || !function.value("arguments").isString() ||
            function.value("arguments").toString().size() > 65536) return "response_invalid_tools";
        ids.insert(id);
        cleanCalls.append(QJsonObject{{"id", id}, {"type", "function"},
            {"function", QJsonObject{{"name", name}, {"arguments", function.value("arguments")}}}});
    }
    // An explicitly stopped empty final is public model output, not a valid
    // recommendation. Let the caller's strict final validator handle it (the
    // EOG loop permits one bounded JSON-format correction). Unknown termination
    // and absent content must not enter that recovery path.
    if (text.trimmed().isEmpty() && cleanCalls.isEmpty() &&
        (reason != "stop" || content.isUndefined())) return "response_invalid_empty";
    if (!cleanCalls.isEmpty()) assistant.insert("tool_calls", cleanCalls);
    // Only the public assistant fields above survive. reasoning/reasoning_content are discarded.
    return nullptr;
}
} // namespace

QString NemotronClient::lastResponseModel() const { return modelIdentifier(responseModel_, config_.apiKey); }

NemotronConfig NemotronConfig::fromEnvironment(const QString& envFilePath, const QString& savedEnvFilePath) {
    const auto local = localEnvironmentValues(envFilePath.isEmpty() ? localEnvironmentFile() : envFilePath);
    // An explicit path isolates tests/custom launches from saved user settings.
    const auto savedPath = savedEnvFilePath.isEmpty()
        ? (envFilePath.isEmpty() ? userEnvironmentFile() : QString{}) : savedEnvFilePath;
    const auto saved = localEnvironmentValues(savedPath);
    const auto setting = [&local, &saved](const char* name) {
        const auto process = qEnvironmentVariable(name).trimmed();
        const auto file = local.value(QString::fromLatin1(name)).trimmed();
        if (!process.isEmpty()) return process;
        return file.isEmpty() ? saved.value(QString::fromLatin1(name)).trimmed() : file;
    };
    NemotronConfig config;
    config.apiKey = setting("NVIDIA_API_KEY");
    if (!qEnvironmentVariable("NVIDIA_API_KEY").trimmed().isEmpty()) config.credentialSource = "environment";
    else if (!local.value("NVIDIA_API_KEY").trimmed().isEmpty()) config.credentialSource = "local_file";
    else if (!config.apiKey.isEmpty()) config.credentialSource = "user_file";
    auto endpoint = qEnvironmentVariable("NVIDIA_BASE_URL").trimmed();
    if (endpoint.isEmpty()) {
        // An automatically discovered file must never redirect a credential
        // inherited from the process or saved for the user. Custom endpoints
        // are allowed with their own file's key, or an explicit process override.
        if (config.credentialSource == "local_file") endpoint = local.value("NVIDIA_BASE_URL").trimmed();
        else if (config.credentialSource == "user_file") endpoint = saved.value("NVIDIA_BASE_URL").trimmed();
        else if (config.apiKey.isEmpty()) endpoint = setting("NVIDIA_BASE_URL");
    }
    if (!endpoint.isEmpty()) {
        config.endpoint = QUrl(endpoint);
        auto path = config.endpoint.path();
        while (path.endsWith('/')) path.chop(1);
        if (!path.endsWith("/chat/completions")) path += "/chat/completions";
        config.endpoint.setPath(path);
    }
    const auto model = setting("NEMOTRON_MODEL");
    if (!model.isEmpty()) config.model = model;
    const auto interval = setting("BREAKWATER_NEMOTRON_INTERVAL_MS");
    if (!interval.isEmpty()) {
        bool ok = false; const auto value = interval.toInt(&ok);
        config.minimumIntervalMs = ok && value >= 1000 && value <= 60000 ? value : -1;
    }
    return config;
}

QString NemotronConfig::userEnvironmentFile() {
    const auto directory = QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation);
    return directory.isEmpty() ? QString{} : QDir(directory).filePath(".env");
}

bool NemotronClient::reloadConfiguration() {
    if (active_) return false;
    config_ = NemotronConfig::fromEnvironment();
    return true;
}

NemotronClient::NemotronClient(NemotronConfig config, QObject* parent, QNetworkAccessManager* manager,
                               std::shared_ptr<NemotronRequestPacer> pacer)
    : ModelTransport(parent), config_(std::move(config)),
      pacer_(pacer ? std::move(pacer) : NemotronRequestPacer::shared()),
      manager_(manager ? manager : new QNetworkAccessManager(this)) {
    config_.timeoutMs = std::clamp(config_.timeoutMs, 1, 300000);
    config_.maxRetries = std::clamp(config_.maxRetries, 0, 3);
    config_.backoffBaseMs = std::clamp(config_.backoffBaseMs, 1, 60000);
    config_.backoffJitterMs = std::clamp(config_.backoffJitterMs, 0, 5000);
    config_.terminalCooldownMs = std::clamp(config_.terminalCooldownMs, 0, 86400000);
    timeout_.setParent(this);
    retry_.setParent(this);
    budget_timeout_.setParent(this); budget_timeout_.setSingleShot(true);
    budget_timeout_.setTimerType(Qt::PreciseTimer); retry_.setTimerType(Qt::PreciseTimer);
    timeout_.setSingleShot(true);
    retry_.setSingleShot(true);
    connect(&timeout_, &QTimer::timeout, this, [this] {
        if (active_) fail("timeout", "The model request exceeded its time limit.");
    });
    connect(&retry_, &QTimer::timeout, this, &NemotronClient::send);
    connect(&budget_timeout_, &QTimer::timeout, this, [this] {
        if (active_) fail(budget_is_explicit_ ? "request_budget_exceeded" : "timeout", "The model request reached its overall time limit.");
    });
}

NemotronClient::~NemotronClient() {
    if (active_) { recordAttempt(); retainTerminalCooldown(); }
    active_ = false;
    timeout_.stop();
    retry_.stop();
    budget_timeout_.stop();
    releaseReply(true);
}

QString NemotronClient::configuredModelIdentifier() const { return modelIdentifier(config_.model, config_.apiKey); }
qint64 NemotronClient::admissionDelayMs() const { return pacer_->admissionDelayMs(); }

void NemotronClient::request(const QJsonArray& messages, const QJsonArray& tools) {
    requestImpl(messages, tools, false);
}

void NemotronClient::requestFinalJson(const QJsonArray& messages) {
    requestImpl(messages, {}, true);
}

void NemotronClient::requestWithBudget(const QJsonArray& messages, const QJsonArray& tools, int budgetMs, bool finalJson) {
    requestImpl(messages, tools, finalJson, budgetMs > 0 ? budgetMs : -1);
}

void NemotronClient::requestImpl(const QJsonArray& messages, const QJsonArray& tools, bool jsonFinal, int budgetMs) {
    if (active_) { rejectBusyRequest(); return; }
    active_ = true;
    retries_ = attempts_ = httpStatus_ = 0;
    budget_is_explicit_ = budgetMs != 0;
    retry_delays_ = {}; retry_wait_policy_.clear();
    attempt_diagnostics_ = {}; attempt_pending_ = false;
    admission_wait_ms_ = 0; admission_started_ms_ = -1;
    retry_after_ms_ = -1; retry_after_policy_.clear();
    request_budget_ms_ = budgetMs == 0 ? config_.timeoutMs * (config_.maxRetries + 1) + 5000 * config_.maxRetries : budgetMs;
    responseModel_.clear();
    requestId_ = QUuid::createUuid().toString(QUuid::WithoutBraces);
    elapsed_.start();
    const auto generation = ++requestGeneration_;
    if (request_budget_ms_ <= 0 || request_budget_ms_ > 1215000) {
        fail("request_invalid", "A positive bounded remaining request budget is required."); return;
    }
    budget_timeout_.start(request_budget_ms_);
    if (config_.apiKey.isEmpty()) { fail("credentials_missing", "NVIDIA_API_KEY is not configured."); return; }
    if (config_.apiKey.size() > 4096 || config_.apiKey.contains('\r') || config_.apiKey.contains('\n') || config_.model.trimmed().isEmpty() ||
        config_.model.size() > 256 || !config_.endpoint.isValid() || config_.endpoint.scheme() != "https" ||
        config_.endpoint.host().isEmpty() || !config_.endpoint.userInfo().isEmpty() ||
        config_.endpoint.hasFragment() || config_.minimumIntervalMs < 0 || config_.minimumIntervalMs > 60000 ||
        !manager_ || manager_->thread() != thread() || QThread::currentThread() != thread()) {
        fail("configuration_invalid", "The model configuration requires HTTPS and bounded request settings."); return;
    }
    if (messages.isEmpty()) { fail("request_invalid", "At least one model message is required."); return; }
    QJsonObject body{{"model", config_.model}, {"messages", messages}, {"stream", false},
                     {"max_tokens", 4096}, {"temperature", jsonFinal ? 0.0 : 0.2},
                     {"chat_template_kwargs", QJsonObject{{"enable_thinking", !jsonFinal}}}};
    // Evaluate bounded reasoning for ordinary tool planning, within the same
    // output cap. Explicit final/correction requests only format retained
    // evidence and keep reasoning off. parseAssistant discards private fields.
    // The NVIDIA hosted endpoint uses reasoning_budget (the self-hosted NIM
    // interface has a different name). Keep the existing bounded allowance.
    if (!jsonFinal) body.insert("reasoning_budget", 1024);
    // JSON-only output is requested separately from tool calling; no combined
    // tools/response_format behavior is assumed for the hosted endpoint.
    if (jsonFinal) {
        body.insert("response_format", QJsonObject{{"type", "json_object"}});
        // Final/correction turns can only format retained evidence. NVIDIA's
        // concise/structured mode avoids spending this turn on more reasoning.
    }
    else if (!tools.isEmpty()) { body.insert("tools", tools); body.insert("tool_choice", "auto"); }
    payload_ = QJsonDocument(body).toJson(QJsonDocument::Compact);
    if (payload_.size() > maximumResponseBytes) {
        fail("request_too_large", "The model request exceeds the size limit."); return;
    }
    const QPointer<NemotronClient> guard(this);
    emit requestStarted();
    // A direct requestStarted slot may cancel or destroy the session before the POST.
    if (guard && active_ && generation == requestGeneration_) send();
}

void NemotronClient::send() {
    if (!active_) return;
    if (elapsed_.elapsed() >= request_budget_ms_) {
        fail(budget_is_explicit_ ? "request_budget_exceeded" : "timeout", "The model request reached its overall time limit."); return;
    }
    if (!manager_) { fail("transport_failed", "The model network transport is no longer available."); return; }
    const auto wait = pacer_->acquire(this, config_.minimumIntervalMs);
    if (wait != 0) {
        if (admission_started_ms_ < 0) admission_started_ms_ = elapsed_.elapsed();
        const auto remaining = request_budget_ms_ - elapsed_.elapsed();
        if (wait > 0 && wait >= remaining) {
            fail(pacer_->cooldownRemainingMs() > 0 ? "rate_limit_wait_exceeds_budget" : "request_budget_exceeded",
                 "Provider admission exceeds the remaining investigation deadline. Completed evidence is retained."); return;
        }
        retry_.start(int(wait < 0 ? std::min<qint64>(50, std::max<qint64>(1, remaining)) : wait)); return;
    }
    if (admission_started_ms_ >= 0) {
        admission_wait_ms_ += elapsed_.elapsed() - admission_started_ms_; admission_started_ms_ = -1;
    }
    response_.clear();
    httpStatus_ = 0;
    QNetworkRequest request(config_.endpoint);
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    request.setRawHeader("Accept", "application/json");
    request.setRawHeader("Authorization", QByteArray("Bearer ") + config_.apiKey.toUtf8());
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::ManualRedirectPolicy);
    request.setTransferTimeout(config_.timeoutMs);
    ++attempts_;
    attempt_started_ms_ = elapsed_.elapsed(); attempt_pending_ = true;
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
    if (reply_) {
        auto* reply = reply_.data();
        reply_.clear();
        disconnect(reply, nullptr, this, nullptr);
        if (abort && reply->isRunning()) reply->abort();
        reply->deleteLater();
    }
    pacer_->release(this);
}

void NemotronClient::fail(const QString& code, const QString& message) {
    if (!active_) return;
    complete({}, code, message);
}

QJsonObject NemotronClient::metadata(const QString& outcome, const QString& code) const {
    QJsonObject result{{"model_request_id", requestId_}, {"configured_model", modelIdentifier(config_.model, config_.apiKey)},
        {"response_model", modelIdentifier(responseModel_, config_.apiKey)}, {"http_status", httpStatus_},
        {"duration_ms", elapsed_.isValid() ? elapsed_.elapsed() : qint64{0}},
        {"retry_count", std::max(0, attempts_ - 1)}, {"outcome", outcome}, {"error_code", code}};
    if (!retry_wait_policy_.isEmpty()) {
        result["retry_wait_policy"] = retry_wait_policy_; result["retry_delays_ms"] = retry_delays_;
    }
    const auto cooldown = pacer_->cooldownRemainingMs();
    result["transport_diagnostics"] = QJsonObject{{"version", 1}, {"minimum_interval_ms", std::clamp(config_.minimumIntervalMs, 0, 60000)},
        {"backoff_base_ms", config_.backoffBaseMs}, {"backoff_jitter_ms", config_.backoffJitterMs},
        {"terminal_cooldown_ms", config_.terminalCooldownMs},
        {"admission_wait_ms", admission_wait_ms_ + (admission_started_ms_ < 0 ? 0 : elapsed_.elapsed() - admission_started_ms_)},
        {"cooldown_remaining_ms", std::min<qint64>(86400000, cooldown)}, {"cooldown_capped", cooldown > 86400000},
        {"attempts", attempt_diagnostics_}};
    return result;
}

void NemotronClient::rejectBusyRequest() {
    const auto generation = requestGeneration_;
    auto rejected = metadata("failed", "request_busy");
    rejected["model_request_id"] = QUuid::createUuid().toString(QUuid::WithoutBraces);
    rejected["response_model"] = QString{};
    rejected["http_status"] = 0;
    rejected["duration_ms"] = 0;
    rejected["retry_count"] = 0;
    rejected.remove("retry_wait_policy"); rejected.remove("retry_delays_ms");
    rejected.remove("transport_diagnostics");
    const QPointer<NemotronClient> guard(this);
    emit requestCompleted(rejected);
    if (guard && requestGeneration_ == generation && active_)
        emit requestFailed("request_busy", "A model request is already active.");
}

void NemotronClient::complete(const QJsonObject& assistant, const QString& code, const QString& message) {
    if (!active_) return;
    recordAttempt();
    responseModel_ = modelIdentifier(responseModel_, config_.apiKey);
    if (reply_) httpStatus_ = reply_->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    if (httpStatus_ < 100 || httpStatus_ > 599) httpStatus_ = 0;
    if (!code.isEmpty()) retainTerminalCooldown();
    const auto outcome = code.isEmpty() ? QStringLiteral("succeeded") :
        (code == "cancelled" ? QStringLiteral("cancelled") : QStringLiteral("failed"));
    const auto completed = metadata(outcome, code);
    active_ = false;
    timeout_.stop();
    retry_.stop();
    budget_timeout_.stop();
    releaseReply(!code.isEmpty());
    payload_.clear();
    response_.clear();
    const auto generation = requestGeneration_;
    deliveringGeneration_ = generation;
    const QPointer<NemotronClient> guard(this);
    emit requestCompleted(completed);
    if (!guard) return;
    if (deliveringGeneration_ == generation) deliveringGeneration_ = 0;
    // Metadata observers may synchronously cancel, start another request or destroy us.
    if (requestGeneration_ != generation) return;
    if (code.isEmpty()) emit replyReady(assistant);
    else emit requestFailed(code, message);
}

void NemotronClient::scheduleRetry() {
    const auto delay = next_retry_ms_;
    retry_wait_policy_ = retry_after_policy_;
    const auto remaining = request_budget_ms_ - elapsed_.elapsed();
    if (remaining <= 0 || delay >= remaining) {
        retry_wait_policy_ = "retry_after_exceeds_budget";
        fail(httpStatus_ == 429 ? "rate_limit_wait_exceeds_budget" : "retry_wait_exceeds_budget",
            "The service retry wait exceeds the remaining investigation deadline. Completed evidence is retained."); return;
    }
    retry_delays_.append(delay);
    ++retries_;
    timeout_.stop();
    releaseReply(false);
    response_.clear();
    retry_.start(int(delay));
}

qint64 NemotronClient::fallbackDelayMs() const {
    return qint64(config_.backoffBaseMs) * (qint64{1} << retries_) +
        (config_.backoffJitterMs ? QRandomGenerator::global()->bounded(config_.backoffJitterMs + 1) : 0);
}

void NemotronClient::retainTerminalCooldown() {
    const auto status = reply_ ? reply_->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt() : httpStatus_;
    // A budget refusal, cancellation or destruction can end a throttled request
    // before retries are exhausted. Ending it must not shorten shared cooldown.
    if (status == 429 || status == 503) pacer_->defer(config_.terminalCooldownMs);
}

void NemotronClient::recordAttempt() {
    if (!attempt_pending_) return;
    attempt_pending_ = false;
    auto status = reply_ ? reply_->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt() : 0;
    if (status < 100 || status > 599) status = 0;
    const auto hint = retryAfter(reply_ ? reply_->rawHeader("Retry-After") : QByteArray{});
    retry_after_ms_ = hint.first; retry_after_policy_ = hint.second;
    QJsonObject diagnostic{{"attempt", attempts_}, {"started_ms", attempt_started_ms_},
        {"duration_ms", elapsed_.elapsed() - attempt_started_ms_}, {"http_status", status}};
    if (retry_after_ms_ >= 0) {
        diagnostic["retry_after_ms"] = std::min<qint64>(86400000, retry_after_ms_);
        diagnostic["retry_after_capped"] = retry_after_ms_ > 86400000;
    }
    QJsonObject limits;
    if (reply_) {
        numericHeader(limits, "request_limit", reply_->rawHeader("x-ratelimit-limit-requests"));
        numericHeader(limits, "request_remaining", reply_->rawHeader("x-ratelimit-remaining-requests"));
        numericHeader(limits, "token_limit", reply_->rawHeader("x-ratelimit-limit-tokens"));
        numericHeader(limits, "token_remaining", reply_->rawHeader("x-ratelimit-remaining-tokens"));
    }
    if (!limits.isEmpty()) diagnostic["rate_limits"] = limits;
    const auto envelope = QJsonDocument::fromJson(response_).object();
    if (status == 200) {
        QJsonObject usage;
        const auto raw = envelope.value("usage").toObject();
        for (const auto* key : {"prompt_tokens", "completion_tokens", "total_tokens"}) {
            const auto value = raw.value(QLatin1String(key)); const auto n = value.toDouble(-1);
            if (value.isDouble() && std::isfinite(n) && n >= 0 && n <= 1000000000000.0 && std::floor(n) == n) usage[key] = value;
        }
        if (!usage.isEmpty()) diagnostic["usage"] = usage;
    } else {
        const auto code = envelope.value("error").toObject().value("code").toString();
        static const QSet<QString> codes{"rate_limit_exceeded", "insufficient_quota", "too_many_requests", "concurrency_limit_exceeded", "model_unavailable"};
        if (codes.contains(code)) diagnostic["provider_error_code"] = code;
    }
    attempt_diagnostics_.append(diagnostic);
    next_retry_ms_ = retry_after_ms_ >= 0 ? retry_after_ms_ : fallbackDelayMs();
    const bool serviceRetry = status == 429 || status == 500 || status == 502 || status == 503 || status == 504;
    if (status == 429 || status == 503 || (serviceRetry && retry_after_ms_ >= 0)) {
        pacer_->defer(next_retry_ms_);
    }
}

void NemotronClient::finish() {
    if (!active_ || !reply_) return;
    if (elapsed_.elapsed() >= request_budget_ms_) {
        fail(budget_is_explicit_ ? "request_budget_exceeded" : "timeout", "The model request reached its overall time limit."); return;
    }
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
    httpStatus_ = status;
    const auto error = reply_->error();
    recordAttempt();
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
    // Only literal classifications leave the parser. No response body, unknown
    // finish reason, supplied identifier or private reasoning enters diagnostics.
    if (const auto* code = parseAssistant(response_, assistant, responseModel_)) {
        fail(QString::fromLatin1(code), "The model returned an invalid or unusable response."); return;
    }
    complete(assistant, {}, {});
}

void NemotronClient::cancel() {
    if (active_) fail("cancelled", "The model request was cancelled.");
    else if (deliveringGeneration_ != 0 && deliveringGeneration_ == requestGeneration_) {
        ++requestGeneration_;
        deliveringGeneration_ = 0;
    }
}

} // namespace breakwater::desktop::agent
