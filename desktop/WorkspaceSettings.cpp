#include "WorkspaceSettings.hpp"
#include <QDir>
#include <QStandardPaths>
#include <cmath>

namespace breakwater::desktop {
namespace {
QString preferencePath(const QString& path) {
    return path.isEmpty() ? QDir(QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation))
        .filePath("workspace-v1.ini") : path;
}
}
QVariantMap WorkspaceSettings::defaults() {
    return {{"navigatorOpen", true}, {"inspectorOpen", true}, {"dockOpen", true},
        {"dockHeight", 270}, {"allLabels", false}, {"quality", 1}, {"compactControls", false}};
}
bool WorkspaceSettings::valid(const QString& key, const QVariant& value) {
    if (key == "dockHeight" || key == "quality") {
        bool ok = false;
        const auto number = value.toDouble(&ok);
        return ok && value.metaType().id() != QMetaType::Bool && std::isfinite(number)
            && std::floor(number) == number && number >= (key == "quality" ? 0 : 160)
            && number <= (key == "quality" ? 2 : 640);
    }
    return defaults().contains(key) && value.metaType().id() == QMetaType::Bool;
}
WorkspaceSettings::WorkspaceSettings(const QString& path, QObject* parent)
    : QObject(parent), settings_(preferencePath(path), QSettings::IniFormat), values_(defaults()) {
    // Unknown versions are left untouched. The UI still has safe defaults.
    const auto version = settings_.value("version", 1);
    bool versionOk = false;
    if (version.metaType().id() == QMetaType::Bool || version.toDouble(&versionOk) != 1.0 || !versionOk) {
        writable_ = false; status_ = "Workspace preferences use an unsupported version. Reset to use local defaults."; return;
    }
    for (auto it = values_.begin(); it != values_.end(); ++it) {
        auto stored = settings_.value("view/" + it.key(), it.value());
        if (it.value().metaType().id() == QMetaType::Bool && stored.metaType().id() == QMetaType::QString
            && (stored.toString() == "true" || stored.toString() == "false")) stored = stored.toString() == "true";
        if (valid(it.key(), stored)) it.value() = stored;
    }
    if (settings_.status() != QSettings::NoError) status_ = "Workspace preferences could not be read; defaults are in use.";
}
bool WorkspaceSettings::setValue(const QString& key, const QVariant& value) {
    if (!writable_ || !valid(key, value)) return false;
    if (values_.value(key) == value) return true;
    values_[key] = value;
    persist(); emit changed(); return settings_.status() == QSettings::NoError;
}
void WorkspaceSettings::reset() {
    writable_ = true; settings_.clear(); values_ = defaults(); persist(); emit changed();
}
void WorkspaceSettings::persist() {
    settings_.setValue("version", 1);
    for (auto it = values_.cbegin(); it != values_.cend(); ++it) settings_.setValue("view/" + it.key(), it.value());
    settings_.sync();
    status_ = settings_.status() == QSettings::NoError ? QString{} : "Workspace preferences could not be saved. Your project is unaffected.";
}
}
