#pragma once
#include "SessionRepository.hpp"
#include <QVariantList>
#include <QVariantMap>

namespace breakwater::desktop::history {
class HistoryController final : public QObject {
    Q_OBJECT
    Q_PROPERTY(QVariantList sessions READ sessions NOTIFY changed)
    Q_PROPERTY(QVariantList runs READ runs NOTIFY changed)
    Q_PROPERTY(QString selectedSessionId READ selectedSessionId NOTIFY changed)
    Q_PROPERTY(QString baselineRunId READ baselineRunId NOTIFY changed)
    Q_PROPERTY(QString candidateRunId READ candidateRunId NOTIFY changed)
    Q_PROPERTY(bool historical READ historical NOTIFY changed)
    Q_PROPERTY(bool busy READ busy NOTIFY changed)
    Q_PROPERTY(QString status READ status NOTIFY changed)
    Q_PROPERTY(QVariantMap comparison READ comparison NOTIFY changed)
    Q_PROPERTY(QVariantMap chart READ chart NOTIFY changed)
    Q_PROPERTY(QVariantList tanks READ tanks NOTIFY changed)
    Q_PROPERTY(QString selectedTankId READ selectedTankId NOTIFY changed)
    Q_PROPERTY(QVariantList scientificEvidence READ scientificEvidence NOTIFY changed)
    Q_PROPERTY(QString selectedScientificResultId READ selectedScientificResultId NOTIFY changed)
    Q_PROPERTY(QVariantMap scientificResult READ scientificResult NOTIFY changed)
    Q_PROPERTY(QVariantList scientificTanks READ scientificTanks NOTIFY changed)
    Q_PROPERTY(QString selectedScientificTankId READ selectedScientificTankId NOTIFY changed)
    Q_PROPERTY(QVariantMap scientificChart READ scientificChart NOTIFY changed)
    Q_PROPERTY(QVariantList scientificSensitivityRows READ scientificSensitivityRows NOTIFY changed)
public:
    explicit HistoryController(SessionRepository* repository, QObject* parent = nullptr);
    QVariantList sessions() const { return sessions_; }
    QVariantList runs() const;
    QString selectedSessionId() const { return archive_.value("session_id").toString(); }
    QString baselineRunId() const { return baseline_; }
    QString candidateRunId() const { return candidate_; }
    bool historical() const { return historical_; }
    bool busy() const { return busy_; }
    QString status() const { return status_; }
    QVariantMap comparison() const;
    QVariantMap chart() const;
    QVariantList tanks() const;
    QString selectedTankId() const { return tank_; }
    QVariantList scientificEvidence() const;
    QString selectedScientificResultId() const { return scientific_result_; }
    QVariantMap scientificResult() const;
    QVariantList scientificTanks() const;
    QString selectedScientificTankId() const { return scientific_tank_; }
    QVariantMap scientificChart() const;
    QVariantList scientificSensitivityRows() const;
    void setLiveSession(const QJsonObject& archive);
    QJsonObject selectedArchive() const { return archive_; }
    Q_INVOKABLE void refresh();
    Q_INVOKABLE void loadSession(const QString& id);
    Q_INVOKABLE void showLive();
    Q_INVOKABLE void selectRuns(const QString& baseline, const QString& candidate);
    Q_INVOKABLE void selectTank(const QString& id);
    Q_INVOKABLE void selectScientificResult(const QString& id);
    Q_INVOKABLE void selectScientificTank(const QString& id);
    Q_INVOKABLE void exportSession(const QUrl& destination);
    Q_INVOKABLE void importSession(const QUrl& source);
signals:
    void changed();
private:
    SessionRepository* repository_;
    QJsonObject archive_, live_;
    QVariantList sessions_;
    QString baseline_, candidate_, tank_, status_ = "History ready";
    QString scientific_result_, scientific_tank_;
    bool historical_ = false, busy_ = false, manual_selection_ = false;
    QJsonObject detail(const QString& runId) const;
    void defaultSelection(bool reset);
    QJsonObject scientificDetail(const QString& id) const;
    void defaultScientificSelection(bool reset);
};
}
