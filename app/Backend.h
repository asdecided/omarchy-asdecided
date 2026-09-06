#pragma once
#include <QObject>
#include <QProcess>
#include <QTimer>
#include <QVariant>
#include <QSet>

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
    Q_PROPERTY(QVariantList tabs READ tabs NOTIFY tabsChanged)
    Q_PROPERTY(int activeTab READ activeTab NOTIFY selectionChanged)
    Q_PROPERTY(QString editorText READ editorText NOTIFY selectionChanged)
    Q_PROPERTY(bool dirty READ dirty NOTIFY tabsChanged)
    Q_PROPERTY(bool hasDirty READ hasDirty NOTIFY tabsChanged)
    Q_PROPERTY(bool editable READ editable NOTIFY selectionChanged)
    Q_PROPERTY(QVariantList navigation READ navigation NOTIFY documentsChanged)
    Q_PROPERTY(QVariantList links READ links NOTIFY selectionChanged)
    Q_PROPERTY(QVariantMap reviewData READ reviewData NOTIFY reviewChanged)
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
    QVariantList tabs() const;
    int activeTab() const { return currentTab; }
    QString editorText() const { return selectedDocument.value("raw", selectedDocument.value("text")).toString(); }
    bool dirty() const;
    bool hasDirty() const;
    bool editable() const;
    QVariantList navigation() const;
    QVariantList links() const;
    QVariantMap reviewData() const { return currentReview; }
    Q_INVOKABLE void toggleFolder(const QString &folder);
    Q_INVOKABLE void activateTab(int index);
    Q_INVOKABLE void closeTab(int index);
    Q_INVOKABLE void setEditorText(const QString &text);
    Q_INVOKABLE void initializeProject(const QUrl &folder, const QString &key);
    Q_INVOKABLE void newDraft(const QString &kind, const QString &title, const QString &path);
    Q_INVOKABLE void reviewSave();
    Q_INVOKABLE void applySave();
    Q_INVOKABLE void cancelReview();
    Q_INVOKABLE void followLink(int index);
    Q_INVOKABLE void reloadSelected();
    Q_INVOKABLE void requestQuit();
    Q_INVOKABLE void confirmDiscard();
    Q_INVOKABLE void cancelDiscard();
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
    void tabsChanged();
    void reviewChanged();
    void reviewReady();
    void discardRequested();
    void quitReady();
    void completed(const QString &operation, bool success);
private:
    void start(const QString &operation, const QString &project, const QString &query = {}, const QVariantMap &extra = {});
    void finish(int exitCode, QProcess::ExitStatus exitStatus);
    void drain();
    void fail(const QString &message);
    void applyMatches(const QVariantList &matches);
    void openDocument(const QVariantMap &document);
    void addTab(QVariantMap document);
    void publishSelection();
    void persistRecovery();
    void restoreRecovery();
    QString recoveryPath() const;
    QProcess process;
    QTimer deadline, recoveryTimer;
    QByteArray output, diagnostics;
    QString operation;
    QString projectPath, sourceName, errorText, statusText = "Open a project to begin";
    QString reportText, reportName;
    QVariantList allDocuments, visibleDocuments;
    QVariantMap selectedDocument, pendingDocument, currentReview;
    QVariantList sessions, graphEdges;
    QSet<QString> collapsedFolders;
    QString corpusPath, deferredAction, deferredArgument, reviewedText;
    int currentTab = -1;
    bool aborted = false;
    bool recoveryBlocked = false;
};
