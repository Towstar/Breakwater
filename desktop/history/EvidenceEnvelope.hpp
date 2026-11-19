#pragma once
#include <QJsonObject>
#include <QString>

namespace breakwater::desktop::history {
// Version 2 adds one bounded typed envelope beside the unchanged native session
// fields. Existing v1 interchange remains readable and is exported unchanged.
QJsonObject evidence_archive(const QString& kind,const QJsonObject& payload,const QJsonObject& snapshot,
                             const QString& objective,const QString& id={},const QString& created={});
QJsonObject envelope(const QString& kind,const QJsonObject& payload);
bool validate_evidence_envelope(const QJsonObject& archive,QString* error=nullptr);
}
