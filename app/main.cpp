#include "Backend.h"
#include "MarkdownView.h"
#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickStyle>
#include <QQuickWindow>
#include <QTimer>

int main(int argc, char *argv[]) {
    QGuiApplication app(argc, argv);
    app.setOrganizationName("AsDecided");
    app.setApplicationName("AsDecided");
    app.setApplicationVersion("0.1.0");
    app.setDesktopFileName("io.github.asdecided.AsDecided");
    QQuickStyle::setStyle("Fusion");
    qmlRegisterType<MarkdownView>("AsDecided",1,0,"MarkdownView");
    qmlRegisterType<MarkdownHighlighter>("AsDecided",1,0,"MarkdownHighlighter");
    Backend backend;
    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty("companion", &backend);
    engine.load(QUrl("qrc:/app/qml/Main.qml"));
    if (engine.rootObjects().isEmpty()) return 1;
    const auto args = app.arguments();
    if (args.contains("--smoke-test")) {
        const int index = args.indexOf("--smoke-test");
        if (index + 1 >= args.size()) return 2;
        QObject::connect(&backend, &Backend::completed, &app, [&](const QString &op, bool ok) {
            if (!ok) { app.exit(1);return; }
            if (op=="open" && !backend.documents().isEmpty()) { backend.select(0);return; }
            if (op=="read" || (op=="open" && backend.documents().isEmpty())) {
                QTimer::singleShot(500,&app,[&,ok]{
                    if(args.contains("--screenshot")) {
                        int i=args.indexOf("--screenshot");auto *window=qobject_cast<QQuickWindow*>(engine.rootObjects().first());
                        if(i+1<args.size() && window)window->grabWindow().save(args[i+1]);
                    }
                    app.exit(ok?0:1);
                });
            }
        });
        QTimer::singleShot(0, &backend, [&] { backend.openProject(args[index + 1]); });
        QTimer::singleShot(35000, &app, [&] { app.exit(1); });
    } else if (args.size() > 1) {
        QTimer::singleShot(0, &backend, [&] { backend.openProject(args[1]); });
    }
    return app.exec();
}
