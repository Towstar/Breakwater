#pragma once
#include <QObject>
#include <QSettings>
#include <QVariantMap>

namespace breakwater::desktop {
// Presentation preferences only. Never used in project identity or approval.
class WorkspaceSettings final : public QObject {
    Q_OBJECT
    Q_PROPERTY(QVariantMap values READ values NOTIFY changed)
    Q_PROPERTY(QString status READ status NOTIFY changed)
public:
    explicit WorkspaceSettings(const QString& path = {}, QObject* parent = nullptr);
    QVariantMap values() const { return values_; }
    QString status() const { return status_; }
    Q_INVOKABLE bool setValue(const QString& key, const QVariant& value);
    Q_INVOKABLE void reset();
signals:
    void changed();
private:
    QSettings settings_;
    QVariantMap values_;
    QString status_;
    bool writable_ = true;
    static QVariantMap defaults();
    static bool valid(const QString& key, const QVariant& value);
    void persist();
};
}
