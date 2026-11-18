#pragma once
#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QUrl>
#include <QVariantMap>
#include <cstdlib>

namespace breakwater::desktop::runtime {
inline void configureBundledPython() {
    // An explicit override, even an invalid one, must not be silently replaced.
    if (!qEnvironmentVariable("BREAKWATER_PYTHON").trimmed().isEmpty()) return;
    const auto path = QDir(QCoreApplication::applicationDirPath()).absoluteFilePath("../runtime/python/python.exe");
    if (QFileInfo(path).isFile()) {
        const auto absolute = QFileInfo(path).absoluteFilePath();
#ifdef Q_OS_WIN
        // qputenv accepts narrow CRT bytes on Windows. A relocated Unicode
        // path must enter both the wide CRT and OS environment without ACP loss.
        _wputenv_s(L"BREAKWATER_PYTHON", reinterpret_cast<const wchar_t*>(absolute.utf16()));
#else
        qputenv("BREAKWATER_PYTHON", absolute.toLocal8Bit());
#endif
    }
}
inline QVariantMap exampleFiles(const QString& developmentRoot, const QString& developmentBin) {
    const auto app = QFileInfo(QCoreApplication::applicationDirPath()).canonicalFilePath();
    auto root = QDir(app).absoluteFilePath("../share/breakwater/examples");
    if (!QFileInfo(root).isDir() && app.compare(QFileInfo(developmentBin).canonicalFilePath(), Qt::CaseInsensitive) == 0) root = developmentRoot;
    QVariantMap files;
    for (const auto* name : {"operations", "renamed-topology"}) {
        const auto path = QFileInfo(QDir(root).filePath(name)).canonicalFilePath();
        if (!path.isEmpty() && QFileInfo(QDir(path).filePath("network.json")).isFile()) {
            files[name] = QUrl::fromLocalFile(path); files[QString(name) + "Path"] = path;
        }
    }
    const auto recorded = QFileInfo(QDir(root).filePath("recordings/nemotron-demo.eog-session.json"));
    if (recorded.isFile()) files["recordedNemotron"] = QUrl::fromLocalFile(recorded.canonicalFilePath());
    return files;
}
}
