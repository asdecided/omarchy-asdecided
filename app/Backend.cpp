#include "Backend.h"
#include <QCoreApplication>
#include <QDesktopServices>
#include <QDir>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSettings>
#include <QUrl>

namespace {
constexpr qsizetype OutputLimit = 32 * 1024 * 1024;
constexpr qsizetype DiagnosticLimit = 64 * 1024;
QString validationReport(const QVariantMap &data) {
    const auto summary = data.value("summary").toMap();
    QString text = QString("%1 checked · %2 valid · %3 invalid · %4 skipped\n\n")
        .arg(summary.value("checked").toInt()).arg(summary.value("valid").toInt())
        .arg(summary.value("invalid").toInt()).arg(summary.value("skipped_unknown").toInt());
    int findings = 0;
    for (const auto &entry : data.value("files").toList()) {
        const auto file = entry.toMap();
        const auto issues = file.value("issues").toList();
        if (issues.isEmpty()) continue;
        text += file.value("path").toString() + "\n";
        for (const auto &entryIssue : issues) {
            const auto issue = entryIssue.toMap();
            text += "  " + issue.value("severity").toString() + " [" + issue.value("code").toString() + "] " + issue.value("message").toString() + "\n";
            ++findings;
        }
        text += "\n";
    }
    const auto okf = data.value("okf").toMap();
    for (const auto &entry : okf.value("findings").toList()) {
        text += QString::fromUtf8(QJsonDocument::fromVariant(entry).toJson(QJsonDocument::Indented));
        ++findings;
    }
    if (!findings) text += "No findings.\n";
    text += "\nStructural validation of the current corpus. This does not prove code compliance or agent consumption.";
    return text;
}
QString federationReport(const QVariantMap &data) {
    const auto summary = data.value("summary").toMap();
    QString text = QString("Verified local sources\n\n%1 sources · %2 effective artifacts · %3 overrides\n\n")
        .arg(summary.value("sources").toInt()).arg(summary.value("effective_artifacts").toInt())
        .arg(summary.value("overrides").toInt());
    for (const auto &entry : data.value("sources").toList()) {
        const auto source = entry.toMap();
        text += source.value("source").toString() + "\n";
        text += source.value("writable").toBool() ? "Local source\n" : "Inherited · read-only\n";
        text += QString("%1 artifacts · %2 routes\n").arg(source.value("artifact_count").toInt()).arg(source.value("route_count").toInt());
        if (source.contains("pin")) text += source.value("pin").toString() + "\n";
        text += "\n";
    }
    return text + "Verified from materialised bytes on disk. No sources were downloaded or updated.";
}
QVariantMap provenance(const QVariantMap &doc) {
    return doc.value("metadata").toMap().value("provenance").toMap();
}
}

Backend::Backend(QObject *parent) : QObject(parent) {
    recoveryTimer.setSingleShot(true);recoveryTimer.setInterval(650);
    connect(&recoveryTimer,&QTimer::timeout,this,&Backend::persistRecovery);
    deadline.setSingleShot(true);
    deadline.setInterval(30000);
    connect(&deadline, &QTimer::timeout, this, [this] { fail("The operation took too long. Try a smaller corpus or run the decided CLI for diagnostics."); });
    connect(&process, &QProcess::readyReadStandardOutput, this, &Backend::drain);
    connect(&process, &QProcess::readyReadStandardError, this, &Backend::drain);
    connect(&process, &QProcess::finished, this, &Backend::finish);
    connect(&process, &QProcess::errorOccurred, this, [this](QProcess::ProcessError e) {
        if (e == QProcess::FailedToStart) {
            deadline.stop();
            errorText = "Cannot start the bundled Rust backend: " + process.errorString();
            statusText = "Backend unavailable";
            emit changed();
            emit completed(operation, false);
        }
    });
}

Backend::~Backend() {
    persistRecovery();
    if (busy()) { process.kill(); process.waitForFinished(3000); }
}

QStringList Backend::recentProjects() const {
    return QSettings().value("recentProjects").toStringList();
}

void Backend::openProjectUrl(const QUrl &url) { if (url.isLocalFile()) openProject(url.toLocalFile()); }
void Backend::openProject(const QString &path) {
    if(busy())return;
    if(hasDirty()){deferredAction="open";deferredArgument=path;emit discardRequested();return;}
    start("open",path);
}
void Backend::refresh() { if (!projectPath.isEmpty()) openProject(projectPath); }
void Backend::search(const QString &query) {
    if (busy() || projectPath.isEmpty()) return;
    if (query.trimmed().isEmpty()) {
        visibleDocuments = allDocuments;
        statusText = QString::number(visibleDocuments.size()) + " artifacts";
        emit documentsChanged(); emit selectionChanged(); emit changed();
    } else start("search", projectPath, query);
}
void Backend::scope(const QString &path) { if (!projectPath.isEmpty()) start("scope", projectPath, path); }
void Backend::validate() { if (!projectPath.isEmpty()) start("validate", projectPath); }
void Backend::federation() { if (!projectPath.isEmpty()) start("federation", projectPath); }
void Backend::select(int index) {
    if (index >= 0 && index < visibleDocuments.size()) {
        openDocument(visibleDocuments[index].toMap());
    }
}
void Backend::openSelected() {
    if (!selectedDocument.isEmpty() && provenance(selectedDocument).value("layer", "local").toString() == "local")
        start("locate", projectPath, selectedDocument.value("metadata").toMap().value("path").toString());
}
void Backend::cancel() { if (busy() && operation != "save") fail("Operation cancelled."); }
void Backend::dismissReport() { reportText.clear(); reportName.clear(); emit changed(); }
void Backend::forgetRecent(const QString &path) {
    auto recent = recentProjects(); recent.removeAll(path);
    QSettings().setValue("recentProjects", recent); emit changed();
}
void Backend::fail(const QString &message) {
    aborted = true; errorText = message; process.kill(); emit changed();
}

void Backend::start(const QString &op, const QString &project, const QString &query, const QVariantMap &extra) {
    if (busy()) return;
    operation = op;
    output.clear(); diagnostics.clear(); errorText.clear(); aborted = false;
    statusText = "Working…";
    auto backend = qEnvironmentVariable("ASDECIDED_BACKEND");
    if (backend.isEmpty()) backend = QCoreApplication::applicationDirPath() + "/asdecided-desktop-backend";
    process.setProgram(backend);
    process.setArguments({});
    process.start();
    QJsonObject request=QJsonObject::fromVariantMap(extra);
    request.insert("protocol",1);request.insert("operation",op=="refresh_index"?"open":op);request.insert("project",project);request.insert("query",query);
    process.write(QJsonDocument(request).toJson(QJsonDocument::Compact));
    process.closeWriteChannel();
    deadline.start();
    emit changed();
}

void Backend::drain() {
    const auto out = process.readAllStandardOutput();
    const auto err = process.readAllStandardError();
    if (aborted) return;
    if (output.size() + out.size() > OutputLimit || diagnostics.size() + err.size() > DiagnosticLimit) {
        fail("The result exceeds the desktop display limit. Use the decided CLI for this corpus.");
        return;
    }
    output += out; diagnostics += err;
}

void Backend::applyMatches(const QVariantList &matches) {
    QVariantList found;
    int unavailable = 0;
    for (const auto &value : matches) {
        auto match = value.toMap();
        const auto matchSource = match.value("provenance").toMap().value("source").toString();
        bool mapped = false;
        for (const auto &item : allDocuments) {
            auto doc = item.toMap();
            const auto metadata = doc.value("metadata").toMap();
            if (doc.value("id") == match.value("id") &&
                (matchSource.isEmpty() ? metadata.value("path") == match.value("path") : metadata.value("source").toString() == matchSource)) {
                doc.insert("match", match);
                found.append(doc); mapped = true; break;
            }
        }
        if (!mapped) ++unavailable;
    }
    visibleDocuments = found;
    statusText = QString::number(matches.size()) + " results";
    if (unavailable) errorText = "The corpus changed since it was opened. Refresh to load all matching documents.";
}

void Backend::finish(int exitCode, QProcess::ExitStatus exitStatus) {
    deadline.stop(); drain();
    if (aborted) { statusText = "Stopped"; emit changed(); emit completed(operation, false); return; }
    QJsonParseError parseError;
    auto json = QJsonDocument::fromJson(output, &parseError);
    const bool validationFindings = operation == "validate" && exitCode == 1 && json.isObject();
    if (exitStatus != QProcess::NormalExit || (exitCode != 0 && !validationFindings) || parseError.error != QJsonParseError::NoError || !json.isObject()) {
        errorText = QString::fromUtf8(diagnostics).trimmed();
        if (errorText.isEmpty()) errorText = "The backend could not complete the operation or returned an invalid response.";
        statusText = "Operation failed";
        emit changed(); emit completed(operation, false); return;
    }
    const auto data = json.object().toVariantMap();
    if ((operation != "search" && operation != "scope" && operation != "validate" && operation != "federation") ? data.value("protocol").toInt() != 1 : data.value("schema_version").toString() != "1") {
        errorText = "Unsupported backend response version.";
        statusText = "Operation failed";
        emit changed(); emit completed(operation, false); return;
    }
    if (operation == "open" || operation == "refresh_index") {
        projectPath = data.value("project").toString(); sourceName = data.value("source").toString();
        allDocuments = data.value("documents").toList(); visibleDocuments = allDocuments;
        corpusPath=data.value("corpus").toString();graphEdges=data.value("graph").toMap().value("edges").toList();
        if(operation=="open"){sessions.clear();currentTab=-1;restoreRecovery();publishSelection();}
        else if(currentTab>=0){
            for(const auto &item:allDocuments){
                const auto doc=item.toMap();
                if(doc.value("metadata").toMap().value("path")==selectedDocument.value("metadata").toMap().value("path")){
                    selectedDocument.insert("title",doc.value("title"));selectedDocument.insert("text",doc.value("text"));
                    sessions[currentTab]=selectedDocument;break;
                }
            }
            publishSelection();
        }
        reportText.clear(); reportName.clear();
        statusText = hasDirty() ? "Recovered unsaved drafts · review before saving" : QString::number(allDocuments.size()) + " artifacts · core " + data.value("core_version").toString();
        auto recent = recentProjects(); recent.removeAll(projectPath); recent.prepend(projectPath);
        while (recent.size() > 8) recent.removeLast();
        QSettings().setValue("recentProjects", recent);
    } else if(operation=="initialize") {
        statusText="Project initialized";
        const auto newProject=data.value("project").toString();
        QTimer::singleShot(0,this,[this,newProject]{openProject(newProject);});
    } else if(operation=="read") {
        auto doc=pendingDocument;
        doc.insert("raw",data.value("text"));doc.insert("original",data.value("text"));doc.insert("hash",data.value("hash"));
        doc.insert("new_file",false);doc.insert("read_only",false);addTab(doc);statusText="Document opened";
    } else if(operation=="template") {
        QVariantMap doc{{"id",data.value("id")},{"title",data.value("title")},{"type",data.value("type")},{"status","Draft"},
            {"metadata",QVariantMap{{"path",data.value("path")},{"source",sourceName}}},
            {"raw",data.value("text")},{"original",""},{"hash",""},{"new_file",true},{"read_only",false}};
        addTab(doc);statusText="New draft · not yet saved";
    } else if(operation=="review") {
        currentReview=data;
        QString findings;
        const auto validation=data.value("validation").toMap();
        for(const auto &key:{"errors","warnings","relationship_issues"}) {
            const auto items=validation.value(key).toList();
            for(const auto &entry:items) {
                const auto issue=entry.toMap();
                findings+=QString("%1 [%2]\n%3\n\n").arg(QString(key),issue.value("code").toString(),issue.value("message").toString());
            }
        }
        currentReview.insert("findingsText",findings.isEmpty()?"No validation findings.":findings);
        emit reviewChanged();emit reviewReady();statusText=data.value("valid").toBool()?"Review changes before saving":"Draft has validation errors";
    } else if(operation=="save") {
        selectedDocument.insert("original",data.value("text"));selectedDocument.insert("raw",data.value("text"));
        selectedDocument.insert("hash",data.value("hash"));selectedDocument.insert("new_file",false);sessions[currentTab]=selectedDocument;
        publishSelection();persistRecovery();statusText="Saved to your repository";errorText=data.value("warning").toString();
        QTimer::singleShot(0,this,[this]{start("refresh_index",projectPath);});
    } else if (operation == "search") {
        applyMatches(data.value("matches").toList());
    } else if (operation == "scope") {
        applyMatches(data.value("decisions").toList());
        if (!data.value("in_repository").toBool()) statusText = "Path is outside this repository";
        else if (visibleDocuments.isEmpty()) statusText = "No accepted decisions declare scope for this path";
    } else if (operation == "locate") {
        if (!QDesktopServices::openUrl(QUrl::fromLocalFile(data.value("path").toString()))) {
            errorText = "No application could open this Markdown file. Set your default Markdown editor.";
            statusText = "File could not be opened";
        } else statusText = "File handed to your default application";
    } else {
        reportName = operation == "validate" ? "Validation" : "Federation";
        reportText = operation == "validate" ? validationReport(data) : federationReport(data);
        statusText = operation == "validate" ? (exitCode == 0 ? "Validation passed; review the report for warnings" : "Validation found errors") : "Federation verified";
    }
    if (operation == "open" || operation == "refresh_index" || operation == "search" || operation == "scope") {
        emit documentsChanged(); emit selectionChanged();
    }
    const QString finishedOperation=operation;
    emit changed(); emit completed(finishedOperation, true);
}
