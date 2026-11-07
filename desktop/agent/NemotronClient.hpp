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
    int timeoutMs = 30000;
    int maxRetries = 2;
    static NemotronConfig fromEnvironment();
};

class NemotronClient final : public ModelTransport {
    Q_OBJECT
public:
    // An injected manager remains caller-owned and must share this object's thread.
    explicit NemotronClient(NemotronConfig config = NemotronConfig::fromEnvironment(),
                            QObject* parent = nullptr, QNetworkAccessManager* manager = nullptr);
    ~NemotronClient() override;
    void request(const QJsonArray& messages, const QJsonArray& tools) override;
    void cancel() override;
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
