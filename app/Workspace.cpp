#include "Backend.h"
#include <QDir>
#include <QFileInfo>
#include <QJsonDocument>
#include <QUrl>
#include <QCryptographicHash>
#include <QSaveFile>
#include <QStandardPaths>
#include <QJsonObject>
#include <QJsonArray>

namespace {
bool modified(const QVariantMap &doc) { return doc.value("new_file").toBool() || doc.value("raw") != doc.value("original"); }
QString pathOf(const QVariantMap &doc) { return doc.value("metadata").toMap().value("path").toString(); }
QString sourceOf(const QVariantMap &doc) { return doc.value("metadata").toMap().value("source").toString(); }
}
QVariantList Backend::tabs() const {
    QVariantList result;
    for (const auto &entry : sessions) {
        const auto doc = entry.toMap();
        result.append(QVariantMap{{"title",doc.value("title")},{"dirty",modified(doc)},{"path",pathOf(doc)}});
    }
    return result;
}
bool Backend::dirty() const { return !selectedDocument.isEmpty() && modified(selectedDocument); }
bool Backend::hasDirty() const {
    for (const auto &entry : sessions) if (modified(entry.toMap())) return true;
    return false;
}
bool Backend::editable() const { return currentTab >= 0 && !selectedDocument.value("read_only").toBool(); }
void Backend::publishSelection() {
    selectedDocument = currentTab >= 0 && currentTab < sessions.size() ? sessions[currentTab].toMap() : QVariantMap{};
    currentReview.clear(); reviewedText.clear();
    emit selectionChanged(); emit tabsChanged(); emit reviewChanged();
    recoveryTimer.start();
}
void Backend::activateTab(int index) {
    if (busy() || index < 0 || index >= sessions.size()) return;
    currentTab = index; publishSelection();
}
void Backend::addTab(QVariantMap doc) {
    for (int i = 0; i < sessions.size(); ++i) {
        auto existing = sessions[i].toMap();
        if (pathOf(existing) == pathOf(doc) && sourceOf(existing) == sourceOf(doc)) {
            sessions[i] = doc; currentTab = i; publishSelection(); return;
        }
    }
    sessions.append(doc); currentTab = sessions.size()-1; publishSelection();
}
void Backend::openDocument(const QVariantMap &doc) {
    if (busy()) return;
    for (int i=0;i<sessions.size();++i) {
        const auto tab = sessions[i].toMap();
        if (pathOf(tab)==pathOf(doc) && sourceOf(tab)==sourceOf(doc)) {
            auto updated=tab;
            if(doc.contains("match")) updated.insert("match",doc.value("match"));
            sessions[i]=updated;activateTab(i);return;
        }
    }
    if (doc.value("metadata").toMap().value("provenance").toMap().value("layer","local").toString() != "local") {
        auto inherited = doc;
        inherited.insert("raw",doc.value("text")); inherited.insert("original",doc.value("text"));
        inherited.insert("read_only",true); addTab(inherited);
    } else { pendingDocument=doc; start("read",projectPath,pathOf(doc)); }
}
void Backend::setEditorText(const QString &text) {
    if (!editable() || busy() || text == editorText()) return;
    selectedDocument.insert("raw",text); sessions[currentTab]=selectedDocument;
    currentReview.clear(); reviewedText.clear();
    emit tabsChanged(); emit reviewChanged();
    recoveryTimer.start();
    // editor text is already current in the TextArea; avoid resetting its cursor.
}
void Backend::closeTab(int index) {
    if (busy() || index<0 || index>=sessions.size()) return;
    if (modified(sessions[index].toMap())) { deferredAction="close";deferredArgument=QString::number(index);emit discardRequested();return; }
    sessions.removeAt(index);
    if (sessions.isEmpty()) currentTab=-1;
    else if (index<currentTab) --currentTab;
    else if (currentTab>=sessions.size()) currentTab=sessions.size()-1;
    publishSelection();
}
void Backend::newDraft(const QString &kind,const QString &title,const QString &path) {
    if (projectPath.isEmpty() || busy()) return;
    start("template",projectPath,path,{{"kind",kind},{"title",title}});
}
void Backend::reviewSave() {
    if (!editable() || busy() || !dirty()) return;
    reviewedText=editorText();
    start("review",projectPath,pathOf(selectedDocument),{{"content",reviewedText},{"expected_hash",selectedDocument.value("hash")},{"new_file",selectedDocument.value("new_file",false)}});
}
void Backend::applySave() {
    if (!editable() || busy() || !currentReview.value("valid").toBool() || reviewedText!=editorText()) return;
    start("save",projectPath,pathOf(selectedDocument),{{"content",reviewedText},{"expected_hash",selectedDocument.value("hash")},{"new_file",selectedDocument.value("new_file",false)}});
}
void Backend::cancelReview() { currentReview.clear();reviewedText.clear();emit reviewChanged(); }
void Backend::reloadSelected() {
    if (!editable() || busy()) return;
    if (dirty()) { deferredAction="reload";emit discardRequested();return; }
    pendingDocument=selectedDocument;start("read",projectPath,pathOf(selectedDocument));
}
void Backend::requestQuit() {
    if (busy()) { errorText="Wait for the current operation or cancel it before quitting.";emit changed();return; }
    if (hasDirty()) { deferredAction="quit";emit discardRequested();return; }
    emit quitReady();
}
void Backend::cancelDiscard() { deferredAction.clear();deferredArgument.clear(); }
void Backend::confirmDiscard() {
    const auto action=deferredAction, argument=deferredArgument;cancelDiscard();
    if (action=="quit") { sessions.clear();currentTab=-1;publishSelection();persistRecovery();emit quitReady(); }
    else if (action=="open") { sessions.clear();currentTab=-1;publishSelection();persistRecovery();start("open",argument); }
    else if (action=="close") {
        int index=argument.toInt();if(index>=0 && index<sessions.size()) {
            sessions.removeAt(index);currentTab=sessions.isEmpty()?-1:qMin(index,int(sessions.size()-1));publishSelection();
        }
    } else if (action=="reload") { pendingDocument=selectedDocument;start("read",projectPath,pathOf(selectedDocument)); }
}
QVariantList Backend::navigation() const {
    QMap<QString,QVariantList> folders;
    for(int i=0;i<visibleDocuments.size();++i) {
        auto doc=visibleDocuments[i].toMap();
        auto path=pathOf(doc);
        QString directory=QFileInfo(QDir(corpusPath).relativeFilePath(path)).path();
        if(doc.value("metadata").toMap().value("provenance").toMap().value("layer")=="inherited") directory="Inherited / "+sourceOf(doc);
        if(directory==".") directory="Corpus";
        folders[directory].append(QVariantMap{{"folder",false},{"title",doc.value("title")},{"kind",doc.value("type")},{"status",doc.value("status")},{"index",i},{"path",path}});
    }
    QVariantList rows;
    for(auto i=folders.cbegin();i!=folders.cend();++i) {
        rows.append(QVariantMap{{"folder",true},{"title",i.key()},{"expanded",!collapsedFolders.contains(i.key())},{"count",i.value().size()}});
        if(!collapsedFolders.contains(i.key())) rows.append(i.value());
    }
    return rows;
}
void Backend::toggleFolder(const QString &folder) {
    if(collapsedFolders.contains(folder)) collapsedFolders.remove(folder);else collapsedFolders.insert(folder);
    emit documentsChanged();
}
QVariantList Backend::links() const {
    QVariantList result;
    if(selectedDocument.isEmpty()) return result;
    const auto id=selectedDocument.value("id").toString(), source=sourceOf(selectedDocument);
    for(const auto &entry:graphEdges) {
        const auto edge=entry.toMap();
        const auto from=edge.value("source_identity").toMap(), to=edge.value("target_identity").toMap();
        bool outgoing=edge.value("source").toString()==id && (from.isEmpty() || from.value("source").toString()==source);
        bool incoming=edge.value("target").toString()==id && (to.isEmpty() || to.value("source").toString()==source);
        if(!outgoing && !incoming) continue;
        const auto target=outgoing?edge.value("target").toString():edge.value("source").toString();
        const auto targetSource=(outgoing?to:from).value("source").toString();
        int index=-1;QString title=target;
        for(int i=0;i<allDocuments.size();++i) {
            const auto doc=allDocuments[i].toMap();
            if(doc.value("id").toString()==target && (targetSource.isEmpty() || sourceOf(doc)==targetSource)) { index=i;title=doc.value("title").toString();break; }
        }
        // Unresolved/external edges stay informational, not executable links.
        if(!edge.value("resolved").toBool() || edge.value("external").toBool()) index=-1;
        result.append(QVariantMap{{"title",title},{"direction",outgoing?"Outgoing":"Backlink"},{"kind",edge.value("type")},{"index",index},{"reference",target}});
    }
    return result;
}
void Backend::followLink(int index) {
    const auto items=links();if(index<0 || index>=items.size())return;
    const int target=items[index].toMap().value("index").toInt();
    if(target>=0 && target<allDocuments.size()) openDocument(allDocuments[target].toMap());
}

void Backend::initializeProject(const QUrl &folder,const QString &key) {
    if(busy() || !folder.isLocalFile())return;
    if(hasDirty()){errorText="Save or close your drafts before initializing another project.";emit changed();return;}
    start("initialize",folder.toLocalFile(),key);
}

QString Backend::recoveryPath() const {
    if(projectPath.isEmpty())return {};
    auto root=QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation)+"/drafts";
    return root+"/"+QString::fromLatin1(QCryptographicHash::hash(projectPath.toUtf8(),QCryptographicHash::Sha256).toHex())+".json";
}
void Backend::persistRecovery() {
    recoveryTimer.stop();
    if(recoveryBlocked)return;
    const auto path=recoveryPath();if(path.isEmpty())return;
    QVariantList drafts;
    for(const auto &entry:sessions)if(modified(entry.toMap()))drafts.append(entry);
    if(drafts.isEmpty()){QFile::remove(path);return;}
    if(!QDir().mkpath(QFileInfo(path).path())){errorText="Could not create local draft recovery storage. Keep the window open and save your work.";emit changed();return;}
    QSaveFile file(path);
    if(!file.open(QIODevice::WriteOnly)){errorText="Could not store draft recovery: "+file.errorString();emit changed();return;}
    file.setPermissions(QFile::ReadOwner|QFile::WriteOwner);
    const auto bytes=QJsonDocument::fromVariant(QVariantMap{{"version",1},{"project",projectPath},{"drafts",drafts}}).toJson(QJsonDocument::Compact);
    if(file.write(bytes)!=bytes.size() || !file.commit()){errorText="Could not update local draft recovery. Save your work before closing.";emit changed();}
}
void Backend::restoreRecovery() {
    recoveryBlocked=false;
    QFile file(recoveryPath());if(!file.exists())return;
    if(!file.open(QIODevice::ReadOnly) || file.size()>32*1024*1024){recoveryBlocked=true;errorText="Draft recovery could not be read. Your recovery file has been retained.";return;}
    QJsonParseError error;
    const auto data=QJsonDocument::fromJson(file.readAll(),&error).object().toVariantMap();
    if(error.error!=QJsonParseError::NoError || data.value("version").toInt()!=1 || data.value("project").toString()!=projectPath){recoveryBlocked=true;errorText="Draft recovery is invalid. Your recovery file has been retained.";return;}
    for(const auto &entry:data.value("drafts").toList()) {
        const auto doc=entry.toMap();
        if(doc.value("raw").toString().toUtf8().size()>1024*1024 || pathOf(doc).isEmpty() || doc.value("read_only").toBool())continue;
        sessions.append(doc);
    }
    currentTab=sessions.isEmpty()?-1:0;
}
