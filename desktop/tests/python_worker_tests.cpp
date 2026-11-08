#include "science/PythonWorkerClient.hpp"

#include <QElapsedTimer>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QSignalSpy>
#include <QtTest>

using namespace breakwater::desktop::science;

#ifndef BREAKWATER_TEST_PYTHON
#define BREAKWATER_TEST_PYTHON ""
#endif

namespace {
class RestoreEnvironment final {
public:
    explicit RestoreEnvironment(const char* name) : name_(name), existed_(qEnvironmentVariableIsSet(name)), value_(qgetenv(name)) {}
    ~RestoreEnvironment() { if (existed_) qputenv(name_.constData(), value_); else qunsetenv(name_.constData()); }
private:
    QByteArray name_;
    bool existed_;
    QByteArray value_;
};
QString python() { return QString::fromUtf8(BREAKWATER_TEST_PYTHON); }
QString realWorker() { return QStringLiteral(BREAKWATER_SOURCE_DIR "/science/worker.py"); }
QString fixture() { return QStringLiteral(BREAKWATER_SOURCE_DIR "/desktop/tests/worker_fixtures/worker_fixture.py"); }
void mode(const char* value) { qputenv("BREAKWATER_WORKER_FIXTURE_MODE", value); }
QString errorCode(const QSignalSpy& failed) { return failed.isEmpty() ? QString{} : failed.front().at(0).toString(); }
} // namespace

class PythonWorkerTests final : public QObject {
    Q_OBJECT
private slots:
    void initTestCase() {
        if (python().isEmpty() || !QFileInfo(python()).isFile())
            QSKIP("A configured Python test interpreter is unavailable; scientific execution remains unavailable.");
    }

    void defaultWorkerPathSupportsConfiguredHealth() {
        RestoreEnvironment pythonRestore("BREAKWATER_PYTHON"); qputenv("BREAKWATER_PYTHON", python().toUtf8());
        PythonWorkerClient client;
        QSignalSpy ready(&client, &PythonWorkerClient::healthReady), failed(&client, &PythonWorkerClient::failed);
        client.checkHealth(); QTRY_COMPARE_WITH_TIMEOUT(ready.size(), 1, 10000);
        QCOMPARE(failed.size(), 0); QCOMPARE(client.readiness()["scientific_execution"].toString(), "not_implemented");
    }

    void existingWorkerHealthUsesFreshMatchingRequestsAndDoesNotClaimScience() {
        QVERIFY(QFileInfo(python()).isFile()); QVERIFY(QFileInfo(realWorker()).isFile());
        PythonWorkerClient client(python(), realWorker());
        QSignalSpy ready(&client, &PythonWorkerClient::healthReady), failed(&client, &PythonWorkerClient::failed);
        client.checkHealth(); QCOMPARE(client.operation(), "health"); QVERIFY(client.busy()); client.checkHealth(); // No second concurrent request.
        QTRY_COMPARE_WITH_TIMEOUT(ready.size(), 1, 10000);
        QCOMPARE(failed.size(), 0); QVERIFY(!client.busy());
        const auto first = ready.front().front().toJsonObject();
        QCOMPARE(first["protocol_version"].toInt(), 1); QCOMPARE(first["status"].toString(), "completed");
        QVERIFY(!first["request_id"].toString().isEmpty());
        QVERIFY(client.readiness()["worker_available"].toBool());
        QCOMPARE(client.readiness()["scientific_execution"].toString(), "not_implemented");
        QCOMPARE(client.readiness()["methods"].toStringList(), QStringList{"health"});
        client.checkHealth(); QTRY_COMPARE_WITH_TIMEOUT(ready.size(), 2, 10000);
        QVERIFY(ready.at(1).at(0).toJsonObject()["request_id"] != first["request_id"]);
    }

    void configuredEnvironmentAndPartialOutputAreHandledWithoutBlocking() {
        RestoreEnvironment modeRestore("BREAKWATER_WORKER_FIXTURE_MODE"), pythonRestore("BREAKWATER_PYTHON");
        mode("partial"); qputenv("BREAKWATER_PYTHON", python().toUtf8());
        PythonWorkerClient client({}, fixture()); QSignalSpy ready(&client, &PythonWorkerClient::healthReady);
        int beats = 0; QTimer heartbeat; heartbeat.setInterval(5);
        connect(&heartbeat, &QTimer::timeout, this, [&] { ++beats; }); heartbeat.start();
        client.checkHealth(); QTRY_COMPARE_WITH_TIMEOUT(ready.size(), 1, 10000);
        QVERIFY(beats > 0); QVERIFY(!client.busy());
        QVERIFY(client.readiness()["torch_detected"].toBool()); QVERIFY(client.readiness()["physicsnemo_detected"].toBool());
        QCOMPARE(client.readiness()["scientific_execution"].toString(), "not_implemented");
    }

    void missingConfigurationAndFilesAreExplicitlyUnavailable() {
        RestoreEnvironment pythonRestore("BREAKWATER_PYTHON"); qunsetenv("BREAKWATER_PYTHON");
        PythonWorkerClient missingPython({}, realWorker()); QSignalSpy pythonFailed(&missingPython, &PythonWorkerClient::failed);
        missingPython.checkHealth(); QCOMPARE(errorCode(pythonFailed), "python_unavailable"); QVERIFY(!missingPython.busy());
        QCOMPARE(missingPython.readiness()["health_status"].toString(), "unavailable");
        PythonWorkerClient missingWorker(python(), realWorker() + ".does-not-exist"); QSignalSpy workerFailed(&missingWorker, &PythonWorkerClient::failed);
        missingWorker.checkHealth(); QCOMPARE(errorCode(workerFailed), "worker_unavailable"); QVERIFY(!missingWorker.busy());
    }

    void failedInterpreterStartIsReportedWithoutRawSystemErrors() {
        PythonWorkerClient client(python() + ".does-not-exist", fixture());
        QSignalSpy failed(&client, &PythonWorkerClient::failed), ready(&client, &PythonWorkerClient::healthReady);
        client.checkHealth(); QTRY_COMPARE_WITH_TIMEOUT(failed.size(), 1, 5000);
        QCOMPARE(errorCode(failed), "worker_start_failed"); QCOMPARE(ready.size(), 0); QVERIFY(!client.busy());
        QVERIFY(!failed.front().at(1).toString().contains(python()));
    }

    void malformedForeignAndUnsupportedResponses_data() {
        QTest::addColumn<QByteArray>("fixtureMode"); QTest::addColumn<QString>("expectedCode");
        QTest::newRow("wrong-id") << QByteArray("wrong_id") << QString("worker_response_mismatch");
        QTest::newRow("wrong-protocol") << QByteArray("wrong_protocol") << QString("worker_response_mismatch");
        QTest::newRow("malformed-json") << QByteArray("malformed") << QString("worker_protocol_invalid");
        QTest::newRow("unsupported-status") << QByteArray("wrong_status") << QString("worker_health_invalid");
        QTest::newRow("unimplemented-science") << QByteArray("scientific_claim") << QString("worker_health_invalid");
        QTest::newRow("wrong-boolean") << QByteArray("wrong_boolean") << QString("worker_health_invalid");
        QTest::newRow("explicit-worker-error") << QByteArray("worker_error") << QString("worker_reported_error");
        QTest::newRow("missing-newline") << QByteArray("no_newline") << QString("worker_protocol_invalid");
        QTest::newRow("trailing-noise") << QByteArray("extra_line") << QString("worker_protocol_invalid");
        QTest::newRow("empty-response") << QByteArray("empty") << QString("worker_protocol_invalid");
        QTest::newRow("nonzero-exit") << QByteArray("crash") << QString("worker_crashed");
        QTest::newRow("crash-after-response") << QByteArray("crash_after_reply") << QString("worker_crashed");
        QTest::newRow("oversized-stdout") << QByteArray("oversized_stdout") << QString("worker_stdout_limit");
        QTest::newRow("oversized-stderr") << QByteArray("oversized_stderr") << QString("worker_stderr_limit");
    }
    void malformedForeignAndUnsupportedResponses() {
        QFETCH(QByteArray, fixtureMode); QFETCH(QString, expectedCode);
        RestoreEnvironment modeRestore("BREAKWATER_WORKER_FIXTURE_MODE"); mode(fixtureMode.constData());
        PythonWorkerClient client(python(), fixture());
        QSignalSpy failed(&client, &PythonWorkerClient::failed), ready(&client, &PythonWorkerClient::healthReady);
        client.checkHealth(); QTRY_COMPARE_WITH_TIMEOUT(failed.size(), 1, 10000);
        QCOMPARE(errorCode(failed), expectedCode); QCOMPARE(ready.size(), 0); QVERIFY(!client.busy());
        QVERIFY(!client.readiness()["worker_available"].toBool());
        QVERIFY(!failed.front().at(1).toString().contains("private-secret-marker"));
        QVERIFY(!QJsonDocument::fromVariant(client.readiness()).toJson().contains("private-secret-marker"));
    }

    void stderrAndUnrecognizedFieldsAreNeverPublishedAndModelKeyIsNotInherited() {
        RestoreEnvironment modeRestore("BREAKWATER_WORKER_FIXTURE_MODE"), keyRestore("NVIDIA_API_KEY");
        mode("extra_private_fields"); PythonWorkerClient client(python(), fixture());
        QSignalSpy ready(&client, &PythonWorkerClient::healthReady), failed(&client, &PythonWorkerClient::failed);
        client.checkHealth(); QTRY_COMPARE_WITH_TIMEOUT(ready.size(), 1, 10000);
        const auto response = ready.front().front().toJsonObject();
        QVERIFY(!QJsonDocument(response).toJson().contains("private-secret-marker"));
        QVERIFY(!QJsonDocument::fromVariant(client.readiness()).toJson().contains("private-secret-marker"));
        qputenv("NVIDIA_API_KEY", "private-secret-marker"); mode("credential_environment");
        client.checkHealth(); QTRY_COMPARE_WITH_TIMEOUT(ready.size(), 2, 10000); QCOMPARE(failed.size(), 0);
    }

    void timeoutCancellationAndFreshRetryIgnoreLateCompletion() {
        RestoreEnvironment modeRestore("BREAKWATER_WORKER_FIXTURE_MODE"); mode("hang");
        PythonWorkerClient client(python(), fixture()); client.setTimeoutMs(150); client.setTerminationGraceMs(10);
        QSignalSpy ready(&client, &PythonWorkerClient::healthReady), failed(&client, &PythonWorkerClient::failed);
        client.checkHealth(); QTRY_COMPARE_WITH_TIMEOUT(failed.size(), 1, 5000);
        QCOMPARE(errorCode(failed), "worker_timeout"); QVERIFY(!client.busy());
        client.setTimeoutMs(10000); client.checkHealth(); client.cancel();
        QCOMPARE(failed.size(), 2); QCOMPARE(failed.at(1).at(0).toString(), "cancelled");
        mode("partial"); client.checkHealth(); QTRY_COMPARE_WITH_TIMEOUT(ready.size(), 1, 10000);
        QTest::qWait(100); QCOMPARE(ready.size(), 1); QCOMPARE(failed.size(), 2);
        QVERIFY(client.readiness()["worker_available"].toBool());
    }

    void destructionAndSynchronousCancellationDoNotStartOrWaitForLateWorkers() {
        RestoreEnvironment modeRestore("BREAKWATER_WORKER_FIXTURE_MODE"); mode("hang");
        auto* client = new PythonWorkerClient(python(), fixture());
        QSignalSpy failed(client, &PythonWorkerClient::failed);
        connect(client, &PythonWorkerClient::changed, client, [client] { if (client->busy()) client->cancel(); });
        client->checkHealth(); QVERIFY(!client->busy()); QCOMPARE(failed.size(), 1);
        delete client;
        auto* active = new PythonWorkerClient(python(), fixture()); active->checkHealth();
        QTest::qWait(50);
        QElapsedTimer elapsed; elapsed.start(); delete active;
        QVERIFY2(elapsed.elapsed() < 500, "Destroying the adapter blocked on a running worker.");
        QTest::qWait(100); // Allow asynchronous process-handle cleanup.
    }
};

QTEST_GUILESS_MAIN(PythonWorkerTests)
#include "python_worker_tests.moc"
