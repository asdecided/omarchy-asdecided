#pragma once
#include <QObject>
#include <QProcess>
#include <QTimer>
#include <QVariant>

class Backend : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool busy READ busy NOTIFY changed)
    Q_PROPERTY(QString project READ project NOTIFY changed)
    Q_PROPERTY(QString source READ source NOTIFY changed)
    Q_PROPERTY(QString error READ error NOTIFY changed)
    Q_PROPERTY(QString status READ status NOTIFY changed)
    Q_PROPERTY(QString report READ report NOTIFY changed)
    Q_PROPERTY(QString reportTitle READ reportTitle NOTIFY changed)
    Q_PROPERTY(QVariantList documents READ documents NOTIFY documentsChanged)
    Q_PROPERTY(QVariantMap selected READ selected NOTIFY selectionChanged)
    Q_PROPERTY(QStringList recentProjects READ recentProjects NOTIFY changed)
public:
    explicit Backend(QObject *parent = nullptr);
    ~Backend() override;
    bool busy() const { return process.state() != QProcess::NotRunning; }
    QString project() const { return projectPath; }
    QString source() const { return sourceName; }
    QString error() const { return errorText; }
    QString status() const { return statusText; }
    QString report() const { return reportText; }
    QString reportTitle() const { return reportName; }
    QVariantList documents() const { return visibleDocuments; }
    QVariantMap selected() const { return selectedDocument; }
    QStringList recentProjects() const;
    Q_INVOKABLE void openProject(const QString &path);
    Q_INVOKABLE void openProjectUrl(const QUrl &url);
    Q_INVOKABLE void refresh();
    Q_INVOKABLE void search(const QString &query);
    Q_INVOKABLE void scope(const QString &path);
    Q_INVOKABLE void validate();
    Q_INVOKABLE void federation();
    Q_INVOKABLE void select(int index);
    Q_INVOKABLE void openSelected();
    Q_INVOKABLE void cancel();
    Q_INVOKABLE void dismissReport();
    Q_INVOKABLE void forgetRecent(const QString &path);
signals:
    void changed();
    void documentsChanged();
    void selectionChanged();
    void completed(const QString &operation, bool success);
private:
    void start(const QString &operation, const QString &project, const QString &query = {});
    void finish(int exitCode, QProcess::ExitStatus exitStatus);
    void drain();
    void fail(const QString &message);
    void applyMatches(const QVariantList &matches);
    QProcess process;
    QTimer deadline;
    QByteArray output, diagnostics;
    QString operation;
    QString projectPath, sourceName, errorText, statusText = "Open a project to begin";
    QString reportText, reportName;
    QVariantList allDocuments, visibleDocuments;
    QVariantMap selectedDocument;
    bool aborted = false;
};
