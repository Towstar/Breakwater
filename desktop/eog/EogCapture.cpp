#include "EogCapture.hpp"
#include "EogContracts.hpp"
#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSaveFile>
#include <array>
#include <stdexcept>
#ifdef Q_OS_WIN
#include <qt_windows.h>
#include <fcntl.h>
#include <io.h>
#endif

namespace breakwater::desktop::eog {
namespace {
[[noreturn]] void invalid(const QString& message) { throw std::invalid_argument(message.toStdString()); }
void checkCancellation(const std::shared_ptr<std::atomic_bool>& token) {
    if (token && token->load(std::memory_order_relaxed)) throw std::runtime_error("EOG capture cancelled.");
}
bool samePath(const QString& a, const QString& b) {
#ifdef Q_OS_WIN
    return a.compare(b, Qt::CaseInsensitive) == 0;
#else
    return a == b;
#endif
}
void requirePlainPath(const QString& path) {
#ifdef Q_OS_WIN
    const auto drive = path.isEmpty() ? ushort(0) : path[0].unicode();
    if (path.size() < 3 || !((drive >= 'A' && drive <= 'Z') || (drive >= 'a' && drive <= 'z')) ||
        path[1] != ':' || (path[2] != '/' && path[2] != '\\'))
        invalid("EOG capture requires plain absolute local source paths.");
#endif
    if (!QDir::isAbsolutePath(path) || path.startsWith("//") || path.startsWith("\\\\"))
        invalid("EOG capture requires plain absolute local source paths.");
    for (auto cursor = QDir::cleanPath(path); !cursor.isEmpty();) {
        const QFileInfo info(cursor);
        if (!info.exists() || info.isSymLink()) invalid("Missing or linked EOG capture source.");
#ifdef Q_OS_WIN
        const auto attributes = GetFileAttributesW(reinterpret_cast<LPCWSTR>(cursor.utf16()));
        if (attributes == INVALID_FILE_ATTRIBUTES || (attributes & FILE_ATTRIBUTE_REPARSE_POINT))
            invalid("Reparse points are not permitted in EOG capture source paths.");
#endif
        const auto parent = info.absolutePath();
        if (samePath(parent, cursor)) break;
        cursor = parent;
    }
}
// Windows read guards deny writes/deletes through every alias while the three
// files are copied and checked. QFile still provides the bounded byte reader.
class SourceReadGuard final {
public:
    explicit SourceReadGuard(const QString& path) {
#ifdef Q_OS_WIN
        handle_ = CreateFileW(reinterpret_cast<LPCWSTR>(path.utf16()), GENERIC_READ, FILE_SHARE_READ,
            nullptr, OPEN_EXISTING, FILE_FLAG_OPEN_REPARSE_POINT | FILE_FLAG_SEQUENTIAL_SCAN, nullptr);
        if (handle_ == INVALID_HANDLE_VALUE) invalid("EOG source is unavailable or open for modification.");
        BY_HANDLE_FILE_INFORMATION info{};
        if (!GetFileInformationByHandle(handle_, &info) ||
            (info.dwFileAttributes & (FILE_ATTRIBUTE_REPARSE_POINT | FILE_ATTRIBUTE_DIRECTORY))) {
            CloseHandle(handle_); handle_ = INVALID_HANDLE_VALUE;
            invalid("EOG capture source must be a plain file.");
        }
        const auto needed = GetFinalPathNameByHandleW(handle_, nullptr, 0, FILE_NAME_NORMALIZED | VOLUME_NAME_DOS);
        if (!needed || needed > 32768) {
            CloseHandle(handle_); handle_ = INVALID_HANDLE_VALUE;
            invalid("Cannot resolve the bounded EOG source handle path.");
        }
        QString finalPath(int(needed), QChar(0));
        const auto written = GetFinalPathNameByHandleW(handle_, reinterpret_cast<LPWSTR>(finalPath.data()),
            needed, FILE_NAME_NORMALIZED | VOLUME_NAME_DOS);
        finalPath.truncate(int(written));
        if (finalPath.startsWith("\\\\?\\")) finalPath.remove(0, 4);
        if (!written || written >= needed || !samePath(QDir::cleanPath(QDir::fromNativeSeparators(finalPath)),
                QDir::cleanPath(QFileInfo(path).absoluteFilePath()))) {
            CloseHandle(handle_); handle_ = INVALID_HANDLE_VALUE;
            invalid("EOG capture source resolved outside its checked plain path.");
        }
#else
        Q_UNUSED(path);
#endif
    }
    ~SourceReadGuard() {
#ifdef Q_OS_WIN
        if (handle_ != INVALID_HANDLE_VALUE) CloseHandle(handle_);
#endif
    }
    SourceReadGuard(const SourceReadGuard&) = delete;
    SourceReadGuard& operator=(const SourceReadGuard&) = delete;
    bool openReader(QFile& file) const {
#ifdef Q_OS_WIN
        HANDLE duplicate = INVALID_HANDLE_VALUE;
        if (!DuplicateHandle(GetCurrentProcess(), handle_, GetCurrentProcess(), &duplicate,
                0, FALSE, DUPLICATE_SAME_ACCESS)) return false;
        const auto descriptor = _open_osfhandle(reinterpret_cast<intptr_t>(duplicate), _O_RDONLY | _O_BINARY);
        if (descriptor < 0) { CloseHandle(duplicate); return false; }
        if (!file.open(descriptor, QIODevice::ReadOnly, QFileDevice::AutoCloseHandle)) {
            _close(descriptor); return false;
        }
        return true;
#else
        return file.open(QIODevice::ReadOnly);
#endif
    }
private:
#ifdef Q_OS_WIN
    HANDLE handle_ = INVALID_HANDLE_VALUE;
#endif
};
struct SourceFile {
    QString path;
    QFileInfo before;
    SourceReadGuard guard;
    QFile file;
    SourceFile(const QString& folder, const QString& name, qint64 limit)
        : path(QDir(folder).filePath(name)), before(path), guard(checkedPath(folder, name, limit)), file(path) {
        if (!guard.openReader(file)) invalid("Cannot open an EOG capture source.");
    }
    static QString checkedPath(const QString& folder, const QString& name, qint64 limit) {
        const auto path = QDir(folder).filePath(name); requirePlainPath(path);
        const QFileInfo info(path);
        if (!info.isFile() || info.size() <= 0 || info.size() > limit ||
            !samePath(QFileInfo(info.canonicalFilePath()).absolutePath(), folder) ||
            QFileInfo(info.canonicalFilePath()).fileName() != name)
            invalid("EOG capture source is missing, escaped or outside its byte limit.");
        return path;
    }
    void checkUnchanged() const {
        requirePlainPath(path);
        const QFileInfo after(path);
        if (!after.isFile() || after.size() != before.size() || after.lastModified() != before.lastModified() ||
            !samePath(after.canonicalFilePath(), before.canonicalFilePath()))
            invalid("EOG source changed while preparing the capture.");
    }
};
QString rawHash(const QByteArray& data) {
    return QString::fromLatin1(QCryptographicHash::hash(data, QCryptographicHash::Sha256).toHex());
}
void commitFile(QSaveFile& output) {
    if (!output.commit()) invalid("Cannot atomically retain the private EOG capture.");
}
QJsonObject copyJson(SourceFile& source, const QString& target, const QString& expectedHash,
                     const std::shared_ptr<std::atomic_bool>& token) {
    checkCancellation(token);
    const auto bytes = source.file.read(4 * 1024 * 1024 + 1);
    if (source.file.error() != QFileDevice::NoError || !source.file.atEnd() ||
        bytes.size() != source.before.size() || bytes.size() > 4 * 1024 * 1024 || rawHash(bytes) != expectedHash)
        invalid("EOG JSON source differs from the loaded dataset.");
    source.checkUnchanged(); checkCancellation(token);
    QSaveFile output(target);
    if (!output.open(QIODevice::WriteOnly) || output.write(bytes) != bytes.size())
        invalid("Cannot write private EOG JSON capture.");
    commitFile(output);
    return {{"sha256", expectedHash}, {"size_bytes", double(bytes.size())}};
}
QByteArray unquote(QByteArray field) {
    if (field.size() >= 2 && field.front() == '"' && field.back() == '"') {
        field = field.mid(1, field.size() - 2); field.replace("\"\"", "\"");
    }
    return field;
}
void checkHeader(QByteArray header) {
    if (header.startsWith("\xef\xbb\xbf")) header.remove(0, 3);
    if (header.endsWith('\n')) header.chop(1);
    if (header.endsWith('\r')) header.chop(1);
    const auto columns = header.split(',');
    const std::array<QByteArray, 5> names{"timestamp", "ts_unix", "asset_id", "metric", "value"};
    if (columns.size() != qsizetype(names.size())) invalid("EOG CSV header changed.");
    for (qsizetype i = 0; i < columns.size(); ++i) if (unquote(columns[i]) != names[size_t(i)])
        invalid("EOG CSV header changed.");
}
qint64 timestamp(const QByteArray& line) {
    // The already-validated first two fields are ISO UTC and an integer, so neither
    // can contain a comma, including when quoted. Asset-ID quoting is untouched.
    const auto first = line.indexOf(','), second = first < 0 ? -1 : line.indexOf(',', first + 1);
    bool ok = false;
    const auto time = second < 0 ? -1 : unquote(line.mid(first + 1, second - first - 1)).toLongLong(&ok);
    if (!ok || time < 0 || time > 253402300799LL) invalid("Invalid EOG CSV timestamp while capturing.");
    return time;
}
QJsonObject copyCsv(SourceFile& source, const QString& target, const Dataset& dataset, qint64 cutoff,
                    const std::shared_ptr<std::atomic_bool>& token) {
    QSaveFile output(target);
    if (!output.open(QIODevice::WriteOnly)) invalid("Cannot write private EOG telemetry capture.");
    QCryptographicHash original(QCryptographicHash::Sha256), captured(QCryptographicHash::Sha256);
    qint64 totalBytes = 0, capturedBytes = 0, first = -1, previous = -1; qsizetype rows = 0;
    const auto readLine = [&] {
        const auto line = source.file.readLine(16 * 1024 + 1);
        if (source.file.error() != QFileDevice::NoError || line.isEmpty() || line.size() > 16 * 1024 ||
            (!line.endsWith('\n') && !source.file.atEnd()))
            invalid("Invalid or oversized EOG CSV record while capturing.");
        totalBytes += line.size();
        if (totalBytes > maximum_csv_bytes) invalid("EOG CSV capture source exceeds its byte limit.");
        original.addData(line); return line;
    };
    const auto retain = [&](const QByteArray& bytes) {
        checkCancellation(token);
        if (output.write(bytes) != bytes.size()) invalid("Cannot retain private EOG telemetry bytes.");
        captured.addData(bytes); capturedBytes += bytes.size();
    };
    checkCancellation(token); const auto header = readLine(); checkHeader(header); retain(header);
    while (!source.file.atEnd()) {
        if ((rows & 1023) == 0) checkCancellation(token);
        const auto raw = readLine(); const auto time = timestamp(raw);
        if (previous > time || (previous >= 0 && time != previous && (time - previous) % 300 != 0))
            invalid("EOG CSV clock changed while capturing.");
        if (first < 0) first = time;
        previous = time;
        if (++rows > maximum_rows) invalid("EOG CSV capture source exceeds its row limit.");
        if (time <= cutoff) retain(raw);
    }
    if (rows != dataset.row_count || first != dataset.first_unix || previous != dataset.now_unix ||
        totalBytes != source.before.size() ||
        QString::fromLatin1(original.result().toHex()) != dataset.source_hashes.value("telemetry.csv").toString())
        invalid("EOG telemetry source differs from the loaded dataset.");
    source.checkUnchanged(); checkCancellation(token); commitFile(output);
    return {{"sha256", QString::fromLatin1(captured.result().toHex())}, {"size_bytes", double(capturedBytes)}};
}
} // namespace

std::shared_ptr<const Capture> prepare_capture(std::shared_ptr<const Dataset> dataset, qint64 cutoff,
                                               const std::shared_ptr<std::atomic_bool>& cancellation) {
    checkCancellation(cancellation);
    if (!dataset || dataset->row_count <= 0 || dataset->row_count > maximum_rows ||
        dataset->first_unix < 0 || dataset->now_unix > 253402300799LL ||
        dataset->first_unix > cutoff || cutoff > dataset->now_unix ||
        dataset->identity != dataset_identity(dataset->source_hashes))
        invalid("Invalid EOG dataset identity, source window or capture cutoff.");
    requirePlainPath(dataset->folder);
    if (!QFileInfo(dataset->folder).isDir() ||
        !samePath(QFileInfo(dataset->folder).canonicalFilePath(), dataset->folder))
        invalid("The EOG source folder is no longer the loaded plain local directory.");
    SourceFile network(dataset->folder, "network.json", 4 * 1024 * 1024);
    SourceFile operations(dataset->folder, "ops.json", 4 * 1024 * 1024);
    SourceFile telemetry(dataset->folder, "telemetry.csv", maximum_csv_bytes);
    auto directory = std::make_shared<QTemporaryDir>(QDir::tempPath() + "/breakwater-eog-capture-XXXXXX");
    if (!directory->isValid() || samePath(directory->path(), dataset->folder))
        invalid("Cannot create a separate private EOG capture directory.");
    requirePlainPath(directory->path());
    QJsonObject files;
    files["network.json"] = copyJson(network, directory->filePath("network.json"),
        dataset->source_hashes.value("network.json").toString(), cancellation);
    files["ops.json"] = copyJson(operations, directory->filePath("ops.json"),
        dataset->source_hashes.value("ops.json").toString(), cancellation);
    files["telemetry.csv"] = copyCsv(telemetry, directory->filePath("telemetry.csv"), *dataset, cutoff, cancellation);
    network.checkUnchanged(); operations.checkUnchanged(); telemetry.checkUnchanged();
    checkCancellation(cancellation);
    QJsonObject manifest{{"eog_version", 1}, {"kind", "capture_manifest"}, {"dataset_identity", dataset->identity},
        {"source_hashes", dataset->source_hashes}, {"captured_files", files},
        {"source_first_unix", double(dataset->first_unix)}, {"source_now_unix", double(dataset->now_unix)},
        {"cutoff_unix", double(cutoff)}, {"context", cutoff == dataset->now_unix ? "snapshot_now" : "historical_cursor"},
        {"sample_period_s", 300}, {"max_age_s", 300}};
    manifest["capture_id"] = capture_identity(manifest);
    validate_document(manifest, "capture_manifest");
    return std::shared_ptr<const Capture>(new Capture(std::move(directory), std::move(manifest)));
}
} // namespace breakwater::desktop::eog
