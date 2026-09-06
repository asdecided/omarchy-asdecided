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
    if (busy()) { process.kill(); process.waitForFinished(3000); }
}

QStringList Backend::recentProjects() const {
    return QSettings().value("recentProjects").toStringList();
}

void Backend::openProjectUrl(const QUrl &url) { if (url.isLocalFile()) openProject(url.toLocalFile()); }
void Backend::openProject(const QString &path) { start("open", path); }
void Backend::refresh() { if (!projectPath.isEmpty()) start("open", projectPath); }
void Backend::search(const QString &query) {
    if (busy() || projectPath.isEmpty()) return;
    if (query.trimmed().isEmpty()) {
        visibleDocuments = allDocuments;
        selectedDocument.clear();
        statusText = QString::number(visibleDocuments.size()) + " artifacts";
        emit documentsChanged(); emit selectionChanged(); emit changed();
    } else start("search", projectPath, query);
}
void Backend::scope(const QString &path) { if (!projectPath.isEmpty()) start("scope", projectPath, path); }
void Backend::validate() { if (!projectPath.isEmpty()) start("validate", projectPath); }
void Backend::federation() { if (!projectPath.isEmpty()) start("federation", projectPath); }
void Backend::select(int index) {
    if (index >= 0 && index < visibleDocuments.size()) {
        selectedDocument = visibleDocuments[index].toMap();
        emit selectionChanged();
    }
}
void Backend::openSelected() {
    if (!selectedDocument.isEmpty() && provenance(selectedDocument).value("layer", "local").toString() == "local")
        start("locate", projectPath, selectedDocument.value("metadata").toMap().value("path").toString());
}
void Backend::cancel() { if (busy()) fail("Operation cancelled."); }
void Backend::dismissReport() { reportText.clear(); reportName.clear(); emit changed(); }
void Backend::forgetRecent(const QString &path) {
    auto recent = recentProjects(); recent.removeAll(path);
    QSettings().setValue("recentProjects", recent); emit changed();
}
void Backend::fail(const QString &message) {
    aborted = true; errorText = message; process.kill(); emit changed();
}

void Backend::start(const QString &op, const QString &project, const QString &query) {
    if (busy()) return;
    operation = op;
    output.clear(); diagnostics.clear(); errorText.clear(); aborted = false;
    statusText = "Working…";
    auto backend = qEnvironmentVariable("ASDECIDED_BACKEND");
    if (backend.isEmpty()) backend = QCoreApplication::applicationDirPath() + "/asdecided-desktop-backend";
    process.setProgram(backend);
    process.setArguments({});
    process.start();
    const QJsonObject request{{"protocol", 1}, {"operation", op}, {"project", project}, {"query", query}};
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
    selectedDocument.clear();
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
    if ((operation == "open" || operation == "locate") ? data.value("protocol").toInt() != 1 : data.value("schema_version").toString() != "1") {
        errorText = "Unsupported backend response version.";
        statusText = "Operation failed";
        emit changed(); emit completed(operation, false); return;
    }
    if (operation == "open") {
        projectPath = data.value("project").toString(); sourceName = data.value("source").toString();
        allDocuments = data.value("documents").toList(); visibleDocuments = allDocuments;
        selectedDocument.clear(); reportText.clear(); reportName.clear();
        statusText = QString::number(allDocuments.size()) + " artifacts · core " + data.value("core_version").toString();
        auto recent = recentProjects(); recent.removeAll(projectPath); recent.prepend(projectPath);
        while (recent.size() > 8) recent.removeLast();
        QSettings().setValue("recentProjects", recent);
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
    if (operation == "open" || operation == "search" || operation == "scope") {
        emit documentsChanged(); emit selectionChanged();
    }
    emit changed(); emit completed(operation, true);
}
