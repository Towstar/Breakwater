#pragma once
#include <QDir>
#include <QFileInfo>
#include <QStringList>
#ifdef Q_OS_WIN
#include <qt_windows.h>
#endif

namespace breakwater::desktop::evidencepaths {
inline QString pathKey(QString path) {
    path = QDir::cleanPath(QDir::fromNativeSeparators(path));
#ifdef Q_OS_WIN
    path = path.toCaseFolded();
#endif
    return path;
}

inline QString canonicalExisting(const QString& path) {
    const auto canonical = QFileInfo(path).canonicalFilePath();
    if (canonical.isEmpty()) return {};
#ifdef Q_OS_WIN
    // Qt can preserve an NTFS 8.3 spelling in canonicalFilePath(). Compare
    // existing ancestors using the final handle path so aliases cannot make
    // an evidence destination appear to be outside its input directory.
    const auto native = QDir::toNativeSeparators(canonical);
    const auto handle = CreateFileW(reinterpret_cast<LPCWSTR>(native.utf16()), FILE_READ_ATTRIBUTES,
        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr, OPEN_EXISTING,
        FILE_FLAG_BACKUP_SEMANTICS, nullptr);
    if (handle == INVALID_HANDLE_VALUE) return {};
    constexpr auto flags = FILE_NAME_NORMALIZED | VOLUME_NAME_DOS;
    const auto needed = GetFinalPathNameByHandleW(handle, nullptr, 0, flags);
    if (!needed || needed > 32768) { CloseHandle(handle); return {}; }
    QString resolved(int(needed), QChar(0));
    const auto written = GetFinalPathNameByHandleW(handle, reinterpret_cast<LPWSTR>(resolved.data()), needed, flags);
    CloseHandle(handle);
    if (!written || written >= needed) return {};
    resolved.truncate(int(written));
    if (resolved.startsWith(QStringLiteral("\\\\?\\UNC\\"), Qt::CaseInsensitive))
        resolved = QStringLiteral("\\\\") + resolved.mid(8);
    else if (resolved.startsWith(QStringLiteral("\\\\?\\"))) resolved.remove(0, 4);
    return QDir::cleanPath(QDir::fromNativeSeparators(resolved));
#else
    return canonical;
#endif
}

inline QString resolvedOutput(const QString& path) {
    const QFileInfo target(path);
    if (target.exists() || target.isSymLink()) return canonicalExisting(path);
    QStringList suffix{target.fileName()};
    QFileInfo ancestor(target.absolutePath());
    while (!ancestor.exists()) {
        suffix.prepend(ancestor.fileName());
        const QFileInfo parent(ancestor.absolutePath());
        if (parent.absoluteFilePath() == ancestor.absoluteFilePath()) return {};
        ancestor = parent;
    }
    if (!ancestor.isDir()) return {};
    auto resolved = canonicalExisting(ancestor.absoluteFilePath());
    if (resolved.isEmpty()) return {};
    for (const auto& part : suffix) resolved = QDir(resolved).filePath(part);
    return QDir::cleanPath(resolved);
}

inline bool within(const QString& candidate, const QString& directory) {
    const auto path = pathKey(candidate), root = pathKey(directory);
    return !path.isEmpty() && !root.isEmpty()
        && (path == root || path.startsWith(root.endsWith('/') ? root : root + '/'));
}
} // namespace breakwater::desktop::evidencepaths
