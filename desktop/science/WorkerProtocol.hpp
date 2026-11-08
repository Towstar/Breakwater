#pragma once

#include <QJsonObject>
#include <QString>

namespace breakwater::desktop::science::protocol {

// These checks are independent of the child process's own validation. They only
// admit inline forward analysis and explicitly selected interior derivatives,
// never artifacts. The controller separately binds unchanged captured values.
bool storagePayload(const QJsonObject& payload);
bool capabilities(const QJsonObject& result);
bool storage(const QJsonObject& result, const QJsonObject& payload);
QJsonObject storageCapability(const QJsonObject& capabilities);
QString publicErrorMessage(const QString& code);
bool publicErrorCode(const QString& code);

} // namespace breakwater::desktop::science::protocol
