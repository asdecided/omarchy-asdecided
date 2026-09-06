#include "Backend.h"
#include <QCoreApplication>
#include <QSettings>
#include <QFile>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QtTest>

class BridgeTest : public QObject {
    Q_OBJECT
private slots:
    void cancellation_and_missing_backend_recover() {
        const auto realBackend = qgetenv("ASDECIDED_BACKEND");
        QTemporaryDir dir;
        QFile script(dir.filePath("slow"));
        QVERIFY(script.open(QIODevice::WriteOnly));
        script.write("#!/bin/sh\nexec sleep 20\n"); script.close();
        QVERIFY(script.setPermissions(QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner));
        qputenv("ASDECIDED_BACKEND", script.fileName().toUtf8());
        Backend backend;
        QSignalSpy done(&backend, &Backend::completed);
        backend.openProject(FIXTURE_ROOT);
        QTRY_VERIFY(backend.busy());
        backend.cancel();
        QTRY_COMPARE_WITH_TIMEOUT(done.count(), 1, 5000);
        QVERIFY(!backend.busy());
        QVERIFY(backend.error().contains("cancelled"));
        qputenv("ASDECIDED_BACKEND", "/nonexistent/asdecided-backend");
        backend.openProject(FIXTURE_ROOT);
        QTRY_COMPARE_WITH_TIMEOUT(done.count(), 2, 5000);
        QVERIFY(!backend.busy());
        QVERIFY(backend.error().contains("Cannot start"));
        qputenv("ASDECIDED_BACKEND", realBackend);
        backend.openProject(FIXTURE_ROOT);
        QTRY_COMPARE_WITH_TIMEOUT(done.count(), 3, 30000);
        QVERIFY2(backend.error().isEmpty(), qPrintable(backend.error()));
        QCOMPARE(backend.documents().size(), 2);
    }
    void workflow() {
        QTemporaryDir settings;
        QSettings::setDefaultFormat(QSettings::IniFormat);
        QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, settings.path());
        Backend backend;
        QSignalSpy done(&backend, &Backend::completed);
        backend.openProject(FIXTURE_ROOT);
        QTRY_COMPARE_WITH_TIMEOUT(done.count(), 1, 30000);
        QVERIFY2(backend.error().isEmpty(), qPrintable(backend.error()));
        QCOMPARE(backend.documents().size(), 2);
        QVERIFY(backend.recentProjects().contains(backend.project()));
        backend.select(0);
        QVERIFY(!backend.selected().value("text").toString().isEmpty());
        backend.search("native");
        QTRY_COMPARE_WITH_TIMEOUT(done.count(), 2, 30000);
        QCOMPARE(backend.documents().size(), 1);
        backend.scope("src/example.rs");
        QTRY_COMPARE_WITH_TIMEOUT(done.count(), 3, 30000);
        QCOMPARE(backend.documents().size(), 1);
        backend.select(0);
        QCOMPARE(backend.selected().value("match").toMap().value("matching_entry").toString(), QString("src/"));
        backend.validate();
        QTRY_COMPARE_WITH_TIMEOUT(done.count(), 4, 30000);
        QVERIFY(!backend.report().isEmpty());
        const auto original = backend.project();
        backend.openProject("/does/not/exist");
        QTRY_COMPARE_WITH_TIMEOUT(done.count(), 5, 30000);
        QCOMPARE(backend.project(), original);
        QVERIFY(!backend.error().isEmpty());
        backend.refresh();
        QTRY_COMPARE_WITH_TIMEOUT(done.count(), 6, 30000);
        QCOMPARE(backend.documents().size(), 2);
        QVERIFY(backend.error().isEmpty());
    }
};
QTEST_GUILESS_MAIN(BridgeTest)
#include "bridge_test.moc"
