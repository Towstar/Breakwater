#pragma once

#include "ModelTransport.hpp"
#include "NemotronRequestPacer.hpp"
#include <QElapsedTimer>
#include <QPointer>
#include <QTimer>
#include <QUrl>

class QNetworkAccessManager;
class QNetworkReply;

namespace breakwater::desktop::agent {

struct NemotronConfig {
    QString apiKey;
    QUrl endpoint = QUrl(QStringLiteral("https://integrate.api.nvidia.com/v1/chat/completions"));
    QString model = QStringLiteral("nvidia/nemotron-3.5-lightning-30b-a3b");
    QString credentialSource;
    int timeoutMs = 60000;
    int maxRetries = 2;
    int minimumIntervalMs = 6000;
    int backoffBaseMs = 2000;
    int backoffJitterMs = 1000;
    int terminalCooldownMs = 60000;
    // An explicit file is useful for isolated launches; otherwise discover the local .env.
    // Nonempty process variables take precedence. Endpoints follow the selected
    // credential source unless explicitly overridden by the process environment.
    // An explicit local path excludes user settings unless a saved path is supplied.
    static NemotronConfig fromEnvironment(const QString& envFilePath = {}, const QString& savedEnvFilePath = {});
    static QString userEnvironmentFile();
};

class NemotronClient final : public ModelTransport {
    Q_OBJECT
public:
    // An injected manager remains caller-owned and must share this object's thread.
    explicit NemotronClient(NemotronConfig config = NemotronConfig::fromEnvironment(),
                            QObject* parent = nullptr, QNetworkAccessManager* manager = nullptr,
                            std::shared_ptr<NemotronRequestPacer> pacer = {});
    ~NemotronClient() override;
    void request(const QJsonArray& messages, const QJsonArray& tools) override;
    void requestFinalJson(const QJsonArray& messages) override;
    void requestWithBudget(const QJsonArray& messages, const QJsonArray& tools, int budgetMs, bool finalJson) override;
    void cancel() override;
    // Configuration changes are allowed between requests only.
    bool reloadConfiguration();
    const NemotronConfig& config() const { return config_; }
    QString configuredModelIdentifier() const;
    QString lastResponseModel() const;
    qint64 admissionDelayMs() const override;
private:
    static constexpr qsizetype maximumResponseBytes = 2 * 1024 * 1024;
    NemotronConfig config_;
    std::shared_ptr<NemotronRequestPacer> pacer_;
    QPointer<QNetworkAccessManager> manager_;
    QPointer<QNetworkReply> reply_;
    QTimer timeout_;
    QTimer retry_;
    QTimer budget_timeout_;
    QJsonArray retry_delays_;
    QJsonArray attempt_diagnostics_;
    qint64 attempt_started_ms_ = 0;
    qint64 admission_started_ms_ = -1;
    qint64 admission_wait_ms_ = 0;
    qint64 retry_after_ms_ = -1;
    qint64 next_retry_ms_ = 0;
    QString retry_after_policy_;
    bool attempt_pending_ = false;
    QString retry_wait_policy_;
    QByteArray payload_;
    QByteArray response_;
    QString responseModel_;
    QString requestId_;
    QElapsedTimer elapsed_;
    int httpStatus_ = 0;
    int attempts_ = 0;
    int retries_ = 0;
    int request_budget_ms_ = 0;
    bool budget_is_explicit_ = false;
    quint64 requestGeneration_ = 0;
    quint64 deliveringGeneration_ = 0;
    bool active_ = false;
    void requestImpl(const QJsonArray& messages, const QJsonArray& tools, bool jsonFinal, int budgetMs = 0);
    void send();
    void consume();
    void finish();
    void releaseReply(bool abort);
    void fail(const QString& code, const QString& message);
    void complete(const QJsonObject& assistant, const QString& code, const QString& message);
    QJsonObject metadata(const QString& outcome, const QString& code) const;
    void rejectBusyRequest();
    void scheduleRetry();
    void recordAttempt();
    qint64 fallbackDelayMs() const;
};

} // namespace breakwater::desktop::agent
