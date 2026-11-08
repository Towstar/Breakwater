#pragma once

#include <QObject>

namespace breakwater::desktop::agent {

// Only configuration state crosses into QML. There is deliberately no key getter.
class CredentialSettings final : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool configured READ configured NOTIFY changed)
    Q_PROPERTY(bool saved READ saved NOTIFY changed)
    Q_PROPERTY(QString status READ status NOTIFY changed)
public:
    explicit CredentialSettings(QObject* parent = nullptr, const QString& settingsFile = {});
    bool configured() const { return configured_; }
    bool saved() const { return saved_; }
    QString status() const { return status_; }
    Q_INVOKABLE bool saveApiKey(const QString& key);
    Q_INVOKABLE bool forgetApiKey();
signals:
    void changed();
private:
    bool configured_ = false;
    bool saved_ = false;
    QString status_;
    QString settings_file_;
    bool explicit_file_ = false;
    void refresh(const QString& message = {});
};
}
