#pragma once

#include "ModelTransport.hpp"
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
                            QObject* parent = nullptr, QNetworkAccessManager* manager = nullptr);
    ~NemotronClient() override;
    void request(const QJsonArray& messages, const QJsonArray& tools) override;
    void requestFinalJson(const QJsonArray& messages) override;
    void cancel() override;
    // Configuration changes are allowed between requests only.
    bool reloadConfiguration();
    const NemotronConfig& config() const { return config_; }
    QString configuredModelIdentifier() const;
    QString lastResponseModel() const;
private:
    static constexpr qsizetype maximumResponseBytes = 2 * 1024 * 1024;
    NemotronConfig config_;
    QPointer<QNetworkAccessManager> manager_;
    QPointer<QNetworkReply> reply_;
    QTimer timeout_;
    QTimer retry_;
    QByteArray payload_;
    QByteArray response_;
    QString responseModel_;
    QString requestId_;
    QElapsedTimer elapsed_;
    int httpStatus_ = 0;
    int attempts_ = 0;
    int retries_ = 0;
    quint64 requestGeneration_ = 0;
    quint64 deliveringGeneration_ = 0;
    bool active_ = false;
    void requestImpl(const QJsonArray& messages, const QJsonArray& tools, bool jsonFinal);
    void send();
    void consume();
    void finish();
    void releaseReply(bool abort);
    void fail(const QString& code, const QString& message);
    void complete(const QJsonObject& assistant, const QString& code, const QString& message);
    QJsonObject metadata(const QString& outcome, const QString& code) const;
    void rejectBusyRequest();
    void scheduleRetry();
};

} // namespace breakwater::desktop::agent
