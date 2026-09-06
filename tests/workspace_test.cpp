#include "Backend.h"
#include "MarkdownView.h"
#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickStyle>
#include <QDirIterator>
#include <QFile>
#include <QSignalSpy>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QtTest>

class WorkspaceTest : public QObject {
    Q_OBJECT
private slots:
    void actual_qml_editor_review_and_save() {
        QTemporaryDir project;
        QDirIterator files(FIXTURE_ROOT,QDir::Files|QDir::Hidden,QDirIterator::Subdirectories);
        while(files.hasNext()) {
            const auto file=files.next();const auto target=project.path()+"/"+QDir(FIXTURE_ROOT).relativeFilePath(file);
            QVERIFY(QDir().mkpath(QFileInfo(target).path()));QVERIFY(QFile::copy(file,target));
        }
        Backend backend;
        QQmlApplicationEngine engine;
        QSignalSpy warnings(&engine,&QQmlApplicationEngine::warnings);
        engine.rootContext()->setContextProperty("companion",&backend);
        engine.load(QUrl("qrc:/app/qml/Main.qml"));
        QVERIFY(!engine.rootObjects().isEmpty());
        auto *root=engine.rootObjects().first();
        auto *editor=root->findChild<QObject*>("sourceEditor");
        auto *preview=root->findChild<MarkdownView*>("markdownPreview");
        QVERIFY(editor);QVERIFY(preview);
        QSignalSpy done(&backend,&Backend::completed);
        auto count=[&](const QString &op){int n=0;for(const auto &entry:done)if(entry[0].toString()==op)++n;return n;};
        backend.openProject(project.path());QTRY_COMPARE(count("open"),1);
        backend.select(0);QTRY_COMPARE(count("read"),1);
        QCOMPARE(editor->property("text").toString(),backend.editorText());
        auto draft=backend.editorText().replace("Qt for presentation","Qt Quick for presentation");
        QVERIFY(editor->setProperty("text",draft));
        QCOMPARE(backend.editorText(),draft);QVERIFY(backend.dirty());
        QTRY_COMPARE(preview->property("markdown").toString(),draft);
        backend.select(1);QTRY_COMPARE(count("read"),2);
        backend.activateTab(0);QCOMPARE(editor->property("text").toString(),draft);
        backend.reviewSave();QTRY_COMPARE(count("review"),1);
        QVERIFY(backend.reviewData().value("valid").toBool());
        backend.applySave();QTRY_COMPARE(count("refresh_index"),1);
        QVERIFY2(backend.error().isEmpty(),qPrintable(backend.error()));
        QFile file(project.filePath("decisions/native.md"));QVERIFY(file.open(QIODevice::ReadOnly));
        QCOMPARE(QString::fromUtf8(file.readAll()),draft);QVERIFY(!backend.dirty());
        root->setProperty("width",980);QTest::qWait(200);
        QCOMPARE(warnings.count(),0);
    }
};
int main(int argc,char **argv) {
    QGuiApplication app(argc,argv);
    QStandardPaths::setTestModeEnabled(true);
    app.setOrganizationName("AsDecidedTest");app.setApplicationName("WorkspaceTest");
    QQuickStyle::setStyle("Fusion");
    qmlRegisterType<MarkdownView>("AsDecided",1,0,"MarkdownView");
    qmlRegisterType<MarkdownHighlighter>("AsDecided",1,0,"MarkdownHighlighter");
    WorkspaceTest test;return QTest::qExec(&test,argc,argv);
}
#include "workspace_test.moc"
