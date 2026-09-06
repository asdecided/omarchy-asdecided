#include "Backend.h"
#include <QCoreApplication>
#include <QSettings>
#include <QFile>
#include <QDirIterator>
#include <QJsonDocument>
#include <QStandardPaths>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QtTest>

class BridgeTest : public QObject {
    Q_OBJECT
    static void copyFixture(const QString &destination) {
        QDirIterator files(FIXTURE_ROOT,QDir::Files|QDir::Hidden,QDirIterator::Subdirectories);
        while(files.hasNext()) {
            const auto file=files.next();const auto target=destination+"/"+QDir(FIXTURE_ROOT).relativeFilePath(file);
            QDir().mkpath(QFileInfo(target).path());QFile::copy(file,target);
        }
    }
private slots:
    void authoring_tabs_review_save_conflict_and_recovery() {
        QTemporaryDir project;copyFixture(project.path());
        QString draft;
        {
            Backend backend;QSignalSpy done(&backend,&Backend::completed);
            auto count=[&](const QString &op){int n=0;for(const auto &entry:done)if(entry[0].toString()==op)++n;return n;};
            backend.openProject(project.path());QTRY_COMPARE_WITH_TIMEOUT(count("open"),1,30000);
            backend.select(0);QTRY_COMPARE_WITH_TIMEOUT(count("read"),1,30000);
            QVERIFY(backend.editable());
            draft=backend.editorText().replace("Qt for presentation","native Qt for presentation");
            backend.setEditorText(draft);QVERIFY(backend.dirty());
            backend.select(1);QTRY_COMPARE_WITH_TIMEOUT(count("read"),2,30000);
            QCOMPARE(backend.tabs().size(),2);QVERIFY(backend.hasDirty());QVERIFY(!backend.dirty());
            backend.activateTab(0);QCOMPARE(backend.editorText(),draft);
            backend.reviewSave();QTRY_COMPARE_WITH_TIMEOUT(count("review"),1,30000);
            QVERIFY(backend.reviewData().value("valid").toBool());
            backend.applySave();QTRY_COMPARE_WITH_TIMEOUT(count("refresh_index"),1,30000);
            QVERIFY2(backend.error().isEmpty(),qPrintable(backend.error()));QVERIFY(!backend.dirty());
            QFile file(project.filePath("decisions/native.md"));QVERIFY(file.open(QIODevice::ReadOnly));QCOMPARE(QString::fromUtf8(file.readAll()),draft);file.close();
            draft=draft.replace("native Qt", "native Qt Quick");backend.setEditorText(draft);
            QVERIFY(file.open(QIODevice::Append));file.write("\n<!-- External edit -->\n");file.close();
            backend.reviewSave();QTRY_COMPARE_WITH_TIMEOUT(count("review"),2,30000);
            QVERIFY(backend.error().contains("changed on disk"));QCOMPARE(backend.editorText(),draft);QVERIFY(backend.dirty());
            QSignalSpy discard(&backend,&Backend::discardRequested);backend.closeTab(0);QCOMPARE(discard.count(),1);
            backend.cancelDiscard();QCOMPARE(backend.tabs().size(),2);
        }
        // A fresh window recovers the draft without applying it to the repository.
        Backend recovered;QSignalSpy done(&recovered,&Backend::completed);
        recovered.openProject(project.path());QTRY_COMPARE_WITH_TIMEOUT(done.count(),1,30000);
        QVERIFY(recovered.hasDirty());QCOMPARE(recovered.editorText(),draft);
        recovered.closeTab(0);recovered.confirmDiscard();QVERIFY(!recovered.hasDirty());
    }
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
        QTRY_COMPARE_WITH_TIMEOUT(done.count(), 2, 30000);
        QVERIFY(!backend.selected().value("text").toString().isEmpty());
        backend.search("native");
        QTRY_COMPARE_WITH_TIMEOUT(done.count(), 3, 30000);
        QCOMPARE(backend.documents().size(), 1);
        backend.scope("src/example.rs");
        QTRY_COMPARE_WITH_TIMEOUT(done.count(), 4, 30000);
        QCOMPARE(backend.documents().size(), 1);
        backend.select(0);
        QCOMPARE(backend.selected().value("match").toMap().value("matching_entry").toString(), QString("src/"));
        backend.validate();
        QTRY_COMPARE_WITH_TIMEOUT(done.count(), 5, 30000);
        QVERIFY(!backend.report().isEmpty());
        const auto original = backend.project();
        backend.openProject("/does/not/exist");
        QTRY_COMPARE_WITH_TIMEOUT(done.count(), 6, 30000);
        QCOMPARE(backend.project(), original);
        QVERIFY(!backend.error().isEmpty());
        backend.refresh();
        QTRY_COMPARE_WITH_TIMEOUT(done.count(), 7, 30000);
        QCOMPARE(backend.documents().size(), 2);
        QVERIFY(backend.error().isEmpty());
    }
};
QTEST_GUILESS_MAIN(BridgeTest)
#include "bridge_test.moc"
