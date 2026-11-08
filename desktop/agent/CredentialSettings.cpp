#include "CredentialSettings.hpp"
#include "NemotronClient.hpp"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QRegularExpression>
#include <QSaveFile>

namespace breakwater::desktop::agent {

CredentialSettings::CredentialSettings(QObject* parent, const QString& settingsFile)
    : QObject(parent), settings_file_(settingsFile.isEmpty() ? NemotronConfig::userEnvironmentFile() : settingsFile),
      explicit_file_(!settingsFile.isEmpty()) { refresh(); }

void CredentialSettings::refresh(const QString& message) {
    const auto config = explicit_file_ ? NemotronConfig::fromEnvironment(settings_file_) : NemotronConfig::fromEnvironment();
    configured_ = !config.apiKey.isEmpty();
    const auto& path = settings_file_;
    saved_ = !path.isEmpty() && QFileInfo(path).isFile();
    status_ = message;
    if (configured_) {
        if (!status_.isEmpty()) status_ += ' ';
        status_ += (config.credentialSource == "user_file" || (explicit_file_ && config.credentialSource == "local_file")) ? "Using the key saved for this Windows account."
            : "Using a key supplied by the launch configuration.";
    } else if (status_.isEmpty()) status_ = "Add an NVIDIA API key to use Nemotron.";
    emit changed();
}

bool CredentialSettings::saveApiKey(const QString& input) {
    const auto key = input.trimmed();
    static const QRegularExpression validKey(QStringLiteral("\\A[A-Za-z0-9_.-]{1,4096}\\z"));
    if (!validKey.match(key).hasMatch()) {
        refresh("Enter an API key without spaces, quotes or line breaks.");
        return false;
    }
    const auto& path = settings_file_;
    if (path.isEmpty() || !QDir().mkpath(QFileInfo(path).absolutePath())) {
        refresh("Could not create local key settings.");
        return false;
    }
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly) || !file.setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner)) {
        refresh("Could not save the API key locally.");
        return false;
    }
    const auto bytes = QByteArray("# Managed by BREAKWATER. Local to this Windows account.\nNVIDIA_API_KEY=") + key.toUtf8() + '\n';
    if (file.write(bytes) != bytes.size() || !file.commit()) {
        refresh("Could not save the API key locally.");
        return false;
    }
    refresh("API key saved.");
    return true;
}

bool CredentialSettings::forgetApiKey() {
    const auto& path = settings_file_;
    if (!path.isEmpty() && QFile::exists(path) && !QFile::remove(path)) {
        refresh("Could not remove the saved API key.");
        return false;
    }
    refresh("Saved API key removed.");
    return true;
}
}
