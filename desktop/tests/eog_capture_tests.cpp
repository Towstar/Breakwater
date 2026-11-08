#include "eog/EogCapture.hpp"
#include "eog/EogContracts.hpp"
#include <QCryptographicHash>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QTemporaryDir>
#include <QtTest>
#include <memory>
#include <stdexcept>

using namespace breakwater::desktop::eog;
namespace {
QByteArray bytes(const QString& path) {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) throw std::runtime_error("Cannot read test fixture");
    return file.readAll();
}
bool write(const QString& path, const QByteArray& data) {
    QFile file(path); return file.open(QIODevice::WriteOnly | QIODevice::Truncate) && file.write(data) == data.size();
}
QString hash(const QByteArray& value) {
    return QString::fromLatin1(QCryptographicHash::hash(value, QCryptographicHash::Sha256).toHex());
}
void fixture(const QString& folder, const QString& fixtureFolder) {
    for (const auto* name : {"network.json", "ops.json", "telemetry.csv"})
        if (!QFile::copy(QDir(fixtureFolder).filePath(name), QDir(folder).filePath(name)))
            throw std::runtime_error("Cannot copy test fixture");
}
}
class EogCaptureTests final : public QObject {
    Q_OBJECT
    QString fixtureFolder_;
private slots:
    void initTestCase() {
        const auto path = QFINDTESTDATA("eog_fixtures/render/network.json");
        QVERIFY2(!path.isEmpty(), "The committed tiny EOG fixture must be available.");
        fixtureFolder_ = QFileInfo(path).absolutePath();
    }
    void snapshotCopiesExactlyThreeOriginalFilesAndPublishesBoundManifest() {
        QTemporaryDir source; QVERIFY(source.isValid()); fixture(source.path(), fixtureFolder_);
        QVERIFY(write(source.filePath("answer_key.json"), "POISON: NEVER READ OR COPY"));
        const auto data = load_dataset(source.path()); const auto capture = prepare_capture(data, data->now_unix);
        QVERIFY(QFileInfo(capture->directory()).isDir()); QVERIFY(capture->directory() != source.path());
        QCOMPARE(QDir(capture->directory()).entryList(QDir::Files, QDir::Name),
            (QStringList{"network.json", "ops.json", "telemetry.csv"}));
        const auto manifest = capture->manifest(); QCOMPARE(manifest.value("context").toString(), "snapshot_now");
        QCOMPARE(manifest.value("dataset_identity").toString(), data->identity);
        QCOMPARE(manifest.value("source_hashes").toObject(), data->source_hashes);
        QCOMPARE(manifest.value("source_first_unix").toInteger(), data->first_unix);
        QCOMPARE(manifest.value("source_now_unix").toInteger(), data->now_unix);
        QCOMPARE(manifest.value("cutoff_unix").toInteger(), data->now_unix);
        QCOMPARE(manifest.value("capture_id").toString(), capture_identity(manifest));
        const auto repeated = prepare_capture(data, data->now_unix);
        QVERIFY(repeated->directory() != capture->directory());
        QCOMPARE(repeated->manifest().value("capture_id"), manifest.value("capture_id"));
        for (const auto* name : {"network.json", "ops.json", "telemetry.csv"}) {
            const auto original = bytes(source.filePath(name)), actual = bytes(QDir(capture->directory()).filePath(name));
            QCOMPARE(actual, original);
            const auto metadata = manifest.value("captured_files").toObject().value(name).toObject();
            QCOMPARE(metadata.value("sha256").toString(), hash(actual));
            QCOMPARE(metadata.value("size_bytes").toInteger(), actual.size());
        }
    }
    void historicalPrefixPreservesBomQuotingCrLfAndExcludesFuture() {
        QTemporaryDir source; QVERIFY(source.isValid()); fixture(source.path(), fixtureFolder_);
        auto original = bytes(source.filePath("telemetry.csv")); original.replace("\r\n", "\n");
        auto lines = original.split('\n'); if (lines.last().isEmpty()) lines.removeLast();
        lines[0] = "\"timestamp\",\"ts_unix\",\"asset_id\",\"metric\",\"value\"";
        for (qsizetype i = 1; i < lines.size(); ++i) {
            const auto first = lines[i].indexOf(','), second = lines[i].indexOf(',', first + 1);
            QVERIFY(first > 0 && second > first);
            lines[i] = '"' + lines[i].left(first) + "\",\"" + lines[i].mid(first + 1, second - first - 1) + '"' + lines[i].mid(second);
        }
        original = QByteArray::fromHex("efbbbf") + lines.join("\r\n"); // Deliberately no final newline.
        QVERIFY(write(source.filePath("telemetry.csv"), original));
        const auto data = load_dataset(source.path()); const auto cutoff = data->first_unix + 150;
        const auto capture = prepare_capture(data, cutoff);
        const auto actual = bytes(QDir(capture->directory()).filePath("telemetry.csv"));
        const auto nextTime = '"' + QDateTime::fromSecsSinceEpoch(data->first_unix + 300, Qt::UTC).toString(Qt::ISODate).toUtf8() + '"';
        const auto nextOffset = original.indexOf(nextTime); QVERIFY(nextOffset > 0);
        QCOMPARE(actual, original.left(nextOffset)); QVERIFY(actual.startsWith(QByteArray::fromHex("efbbbf")));
        QVERIFY(!actual.contains(nextTime)); QVERIFY(actual.contains("-2"));
        const auto manifest = capture->manifest(); QCOMPARE(manifest.value("context").toString(), "historical_cursor");
        QCOMPARE(manifest.value("source_hashes").toObject().value("telemetry.csv").toString(), hash(original));
        const auto capturedHash = manifest.value("captured_files").toObject().value("telemetry.csv").toObject().value("sha256").toString();
        QCOMPARE(capturedHash, hash(actual)); QVERIFY(capturedHash != hash(original));
        // Snapshot-now preserves the complete original bytes, including the
        // final physical record without a newline.
        const auto full = prepare_capture(data, data->now_unix);
        QCOMPARE(bytes(QDir(full->directory()).filePath("telemetry.csv")), original);
    }
    void cutoffMustStayInsideTheOriginalWindow() {
        QTemporaryDir source; fixture(source.path(), fixtureFolder_); const auto data = load_dataset(source.path());
        QVERIFY_THROWS_EXCEPTION(std::invalid_argument, prepare_capture(data, data->first_unix - 1));
        QVERIFY_THROWS_EXCEPTION(std::invalid_argument, prepare_capture(data, data->now_unix + 1));
        QVERIFY_THROWS_EXCEPTION(std::invalid_argument, prepare_capture({}, data->now_unix));
        const auto first = prepare_capture(data, data->first_unix);
        QCOMPARE(first->manifest().value("cutoff_unix").toInteger(), data->first_unix);
        QVERIFY(!bytes(QDir(first->directory()).filePath("telemetry.csv")).contains("104"));
    }
    void editedSourceIsRejected_data() {
        QTest::addColumn<QString>("name");
        QTest::newRow("network") << QString("network.json");
        QTest::newRow("ops") << QString("ops.json");
        QTest::newRow("telemetry") << QString("telemetry.csv");
    }
    void editedSourceIsRejected() {
        QFETCH(QString, name);
        QTemporaryDir source; fixture(source.path(), fixtureFolder_); const auto data = load_dataset(source.path());
        QFile changed(source.filePath(name)); QVERIFY(changed.open(QIODevice::WriteOnly | QIODevice::Append));
        QCOMPARE(changed.write(" "), qint64(1)); changed.close();
        QVERIFY_THROWS_EXCEPTION(std::invalid_argument, prepare_capture(data, data->now_unix));
    }
    void missingFileAndMismatchedDatasetIdentityAreRejected() {
        QTemporaryDir source; fixture(source.path(), fixtureFolder_); const auto data = load_dataset(source.path());
        auto mismatched = std::make_shared<Dataset>(*data); mismatched->identity = QString(64, '0');
        QVERIFY_THROWS_EXCEPTION(std::invalid_argument, prepare_capture(mismatched, data->now_unix));
        QVERIFY(QFile::remove(source.filePath("ops.json")));
        QVERIFY_THROWS_EXCEPTION(std::invalid_argument, prepare_capture(data, data->now_unix));
    }
    void sourcePathRejectsAmbiguousRootsBeforeOpeningFiles() {
        QTemporaryDir source; fixture(source.path(), fixtureFolder_); const auto data = load_dataset(source.path());
        QStringList ambiguous{"relative/source", "//server/share/source", "\\\\server\\share\\source"};
#ifdef Q_OS_WIN
        const auto local = QDir::fromNativeSeparators(data->folder);
        ambiguous += QStringList{local.mid(2), local.left(2) + local.mid(3), "/tmp/source", "C:relative", "1:/source"};
#endif
        for (const auto& path : ambiguous) {
            auto changed = std::make_shared<Dataset>(*data); changed->folder = path;
            bool rejected = false;
            try { prepare_capture(changed, data->now_unix); }
            catch (const std::invalid_argument& error) {
                rejected = true;
                QVERIFY2(QString::fromUtf8(error.what()).contains("plain absolute local source paths"), error.what());
            }
            QVERIFY2(rejected, qPrintable(path));
        }
        QCOMPARE(bytes(source.filePath("telemetry.csv")), bytes(QDir(fixtureFolder_).filePath("telemetry.csv")));
    }
    void cancellationDoesNotInvalidateAnExistingCapture() {
        QTemporaryDir source; fixture(source.path(), fixtureFolder_); const auto data = load_dataset(source.path());
        const auto token = std::make_shared<std::atomic_bool>(false);
        const auto held = prepare_capture(data, data->now_unix, token); const auto path = held->directory();
        token->store(true);
        QVERIFY_THROWS_EXCEPTION(std::runtime_error, prepare_capture(data, data->now_unix, token));
        QVERIFY(QFileInfo(path).isDir()); QCOMPARE(bytes(QDir(path).filePath("network.json")), bytes(source.filePath("network.json")));
    }
    void lastLeaseOwnsCleanupAndOutlivesSource() {
        std::shared_ptr<const Capture> held; QString capturePath, sourcePath;
        {
            QTemporaryDir source; fixture(source.path(), fixtureFolder_); sourcePath = source.path();
            const auto data = load_dataset(source.path()); held = prepare_capture(data, data->now_unix);
            capturePath = held->directory(); auto secondLease = held;
            held.reset(); QVERIFY(QFileInfo(capturePath).isDir()); held = secondLease;
        }
        QVERIFY(!QFileInfo(sourcePath).exists()); QVERIFY(QFileInfo(capturePath).isDir());
        QVERIFY(!bytes(QDir(capturePath).filePath("telemetry.csv")).isEmpty());
        held.reset(); QVERIFY(!QFileInfo(capturePath).exists());
    }
};
QTEST_GUILESS_MAIN(EogCaptureTests)
#include "eog_capture_tests.moc"
