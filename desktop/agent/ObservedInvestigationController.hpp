#pragma once
#include "AgentOrchestrator.hpp"
#include "telemetry/TelemetryController.hpp"
#include "telemetry/TelemetryStream.hpp"

namespace breakwater::desktop::history { class SessionRepository; class HistoryController; class SessionRecorder; }
namespace breakwater::desktop::agent {
class NemotronClient;

// Observed investigations have no control-application method. They share the
// immutable native tools/recorder, but acknowledgements are advisory evidence.
class ObservedInvestigationController final:public QObject {
    Q_OBJECT
    Q_PROPERTY(bool running READ running NOTIFY changed)
    Q_PROPERTY(QString status READ status NOTIFY changed)
    Q_PROPERTY(QVariantList events READ events NOTIFY changed)
    Q_PROPERTY(QVariantList runs READ runs NOTIFY changed)
    Q_PROPERTY(QVariantMap recommendation READ recommendation NOTIFY changed)
    Q_PROPERTY(QVariantMap review READ review NOTIFY changed)
    Q_PROPERTY(QVariantMap sourceSummary READ sourceSummary NOTIFY changed)
    Q_PROPERTY(QVariantMap nextSourceSummary READ nextSourceSummary NOTIFY changed)
    Q_PROPERTY(bool nextSourceDiffers READ nextSourceDiffers NOTIFY changed)
    Q_PROPERTY(QString storageStatus READ storageStatus NOTIFY changed)
    Q_PROPERTY(bool localOnly READ localOnly NOTIFY changed)
    Q_PROPERTY(bool localSearchAvailable READ localSearchAvailable NOTIFY changed)
    Q_PROPERTY(bool canApprove READ canApprove CONSTANT)
    Q_PROPERTY(bool canAcknowledge READ canAcknowledge NOTIFY changed)
    Q_PROPERTY(bool advisoryOnly READ advisoryOnly CONSTANT)
public:
    ObservedInvestigationController(ProjectController* project,telemetry::TelemetryController* telemetry,
                                    ModelTransport* transport=nullptr,QObject* parent=nullptr);
    ~ObservedInvestigationController() override;
    void setHistory(history::SessionRepository* repository,history::HistoryController* history);
    bool running() const { return preparing_ || session_->running(); }
    QString status() const { return status_.isEmpty() ? session_->status():status_; }
    QVariantList events() const { return session_->events(); }
    QVariantList runs() const;
    QVariantMap recommendation() const { return acknowledged_.isEmpty() ? session_->result().toVariantMap():acknowledged_.toVariantMap(); }
    QVariantMap review() const;
    QVariantMap sourceSummary() const;
    QVariantMap nextSourceSummary() const;
    bool nextSourceDiffers() const;
    QString storageStatus() const;
    bool localOnly() const { return session_->localOnly(); }
    bool localSearchAvailable() const;
    bool canApprove() const { return false; }
    bool advisoryOnly() const { return true; }
    bool canAcknowledge() const;
    Q_INVOKABLE void investigate(const QString& objective);
    Q_INVOKABLE void searchLocally();
    Q_INVOKABLE void cancel();
    Q_INVOKABLE bool acknowledgePlan();
signals:
    void changed();
private:
    QPointer<ProjectController> project_;
    QPointer<telemetry::TelemetryController> telemetry_;
    QPointer<history::SessionRepository> repository_;
    QPointer<history::HistoryController> history_;
    AgentOrchestrator* session_;
    QPointer<ModelTransport> transport_;
    NemotronClient* owned_client_=nullptr;
    history::SessionRecorder* recorder_=nullptr;
    std::shared_ptr<const telemetry::ObservedEvidence> source_;
    QJsonObject acknowledged_;
    QString configured_model_,status_,storage_status_="History is not attached.";
    bool preparing_=false;
    quint64 generation_=0;
    bool currentSource(bool requireAvailable) const;
    QVariantMap describeSource(const std::shared_ptr<const telemetry::ObservedEvidence>& source) const;
    void start(const QString& objective,bool local);
    void sourceChanged();
};
}
