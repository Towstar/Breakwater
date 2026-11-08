#pragma once

#include <QJsonObject>
#include <QString>

namespace breakwater::desktop::science::protocol {

// These checks are independent of the child process's own validation. They only
// admit the adopted inline forward method, never derivatives or artifact paths.
bool forwardPayload(const QJsonObject& payload);
bool capabilities(const QJsonObject& result);
bool storage(const QJsonObject& result, const QJsonObject& payload);
QJsonObject storageCapability(const QJsonObject& capabilities);
QString publicErrorMessage(const QString& code);
bool publicErrorCode(const QString& code);

} // namespace breakwater::desktop::science::protocol
