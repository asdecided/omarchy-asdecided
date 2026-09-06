import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs
import AsDecided 1.0

ApplicationWindow {
    id: window
    width: 1420; height: 900
    minimumWidth: 920; minimumHeight: 620
    visible: true
    title: (companion.dirty ? "● " : "") + (selected.title || companion.source || "Workspace") + " — AsDecided"
    color: theme.background
    font.family: theme.bodyFont
    font.pixelSize: 13
    palette.window: theme.background
    palette.base: theme.editor
    palette.alternateBase: theme.sidebar
    palette.text: theme.text
    palette.windowText: theme.text
    palette.button: theme.surface
    palette.buttonText: theme.text
    palette.highlight: theme.selection
    palette.highlightedText: theme.text
    palette.placeholderText: theme.muted
    palette.mid: theme.border
    property var selected: companion.selected
    property var metadata: selected.metadata || ({})
    property var provenance: metadata.provenance || ({})
    property bool hasProject: companion.project.length > 0
    property bool sidebarVisible: true
    property bool inspectorVisible: true
    property int viewMode: 1
    property bool allowClose: false
    property int editorZoom: 14
    Theme { id: theme }

    function runSearch() {
        if (searchMode.currentIndex === 0) companion.search(searchInput.text)
        else companion.scope(searchInput.text)
    }
    function newDocument() { if (hasProject && !companion.busy) newDialog.open() }
    function reviewDocument() { if (companion.dirty && !companion.busy) companion.reviewSave() }
    onClosing: function(close) {
        if (!allowClose) { close.accepted = false; companion.requestQuit() }
    }
    FolderDialog {
        id: folderDialog
        title: "Open an AsDecided project"
        onAccepted: companion.openProjectUrl(selectedFolder)
    }
    FolderDialog {
        id: initializeFolderDialog
        title: "Choose a folder to initialize for AsDecided"
        onAccepted: initializeDialog.open()
    }
    Dialog {
        id: initializeDialog; title: "Initialize project"; modal: true; anchors.centerIn: parent; width: 480
        standardButtons: Dialog.Cancel
        ColumnLayout {
            anchors.fill: parent; spacing: 14
            Label { text: "Create .decided/config.yaml and an empty decisions/ folder here. Existing layouts are never replaced."; wrapMode: Text.Wrap; color: theme.secondary; Layout.fillWidth: true }
            Label { text: initializeFolderDialog.selectedFolder.toString(); wrapMode: Text.WrapAnywhere; color: theme.muted; Layout.fillWidth: true; textFormat: Text.PlainText }
            TextField { id: projectKey; placeholderText: "Artifact ID prefix, e.g. APP"; maximumLength: 10; Layout.fillWidth: true; Accessible.name: "Repository artifact prefix" }
            Button { text: "Initialize project"; enabled: projectKey.text.length >= 2 && !companion.busy; onClicked: { companion.initializeProject(initializeFolderDialog.selectedFolder,projectKey.text); initializeDialog.close() } }
        }
    }
    Shortcut { sequence: "Ctrl+O"; enabled: !companion.busy && !reviewDialog.opened; onActivated: folderDialog.open() }
    Shortcut { sequence: "Ctrl+N"; enabled: !reviewDialog.opened; onActivated: newDocument() }
    Shortcut { sequence: "Ctrl+S"; enabled: !reviewDialog.opened; onActivated: reviewDocument() }
    Shortcut { sequence: "Ctrl+P"; enabled: !reviewDialog.opened; onActivated: commandDialog.open() }
    Shortcut { sequence: "Ctrl+F"; onActivated: { sidebarVisible = true; searchInput.forceActiveFocus(); searchInput.selectAll() } }
    Shortcut { sequence: "Ctrl+R"; enabled: !reviewDialog.opened; onActivated: companion.refresh() }
    Shortcut { sequence: "Ctrl+Shift+V"; onActivated: companion.validate() }
    Shortcut { sequence: "Ctrl+E"; onActivated: viewMode = (viewMode + 1) % 3 }
    Shortcut { sequence: "Ctrl+W"; enabled: !reviewDialog.opened; onActivated: companion.closeTab(companion.activeTab) }
    Shortcut { sequence: "Ctrl+Tab"; enabled: !reviewDialog.opened; onActivated: { if (companion.tabs.length > 0) companion.activateTab((companion.activeTab + 1) % companion.tabs.length) } }
    Shortcut { sequence: "Ctrl+Q"; onActivated: companion.requestQuit() }
    Shortcut { sequence: "Ctrl+="; onActivated: editorZoom = Math.min(22, editorZoom + 1) }
    Shortcut { sequence: "Ctrl+-"; onActivated: editorZoom = Math.max(11, editorZoom - 1) }
    Shortcut { sequence: "Escape"; onActivated: { if (companion.busy) companion.cancel(); else companion.dismissReport() } }

    header: ToolBar {
        implicitHeight: 54
        background: Rectangle { color: theme.sidebar; Rectangle { anchors.bottom: parent.bottom; width: parent.width; height: 1; color: theme.border } }
        RowLayout {
            anchors.fill: parent; anchors.leftMargin: 18; anchors.rightMargin: 18; spacing: 14
            Label { text: "A"; color: theme.accent; font.pixelSize: 23; font.weight: Font.DemiBold; Accessible.name: "AsDecided" }
            Label { text: "AsDecided"; font.pixelSize: 16; font.weight: Font.DemiBold }
            Rectangle { width: 1; height: 18; color: theme.border }
            Label { text: companion.source || "Your decision workspace"; color: theme.secondary; elide: Text.ElideRight; Layout.maximumWidth: 340; textFormat: Text.PlainText }
            Item { Layout.fillWidth: true }
            Button { text: "Commands"; onClicked: commandDialog.open(); ToolTip.text: "Ctrl+P"; ToolTip.visible: hovered }
            Button { text: "New artifact"; enabled: hasProject && !companion.busy; onClicked: newDocument() }
            Button { text: "Review && save"; highlighted: true; enabled: companion.editable && companion.dirty && !companion.busy; onClicked: reviewDocument() }
        }
    }
    footer: ToolBar {
        implicitHeight: 32
        background: Rectangle { color: theme.sidebar; Rectangle { width: parent.width; height: 1; color: theme.border } }
        RowLayout {
            anchors.fill: parent; anchors.leftMargin: 14; anchors.rightMargin: 14; spacing: 14
            Label { text: "●"; color: companion.busy ? theme.accent : theme.success; font.pixelSize: 9 }
            Label { text: companion.status; color: theme.secondary; elide: Text.ElideRight; Layout.fillWidth: true; font.pixelSize: 11; textFormat: Text.PlainText }
            BusyIndicator { visible: companion.busy; running: visible; implicitWidth: 20; implicitHeight: 20 }
            ToolButton { text: "Cancel"; visible: companion.busy; onClicked: companion.cancel(); implicitHeight: 26 }
            Label { text: companion.dirty ? "UNSAVED DRAFT" : "LOCAL FILES"; color: companion.dirty ? theme.accent : theme.muted; font.pixelSize: 10; font.letterSpacing: 1 }
            Label { text: "Markdown · UTF-8"; visible: companion.activeTab >= 0; color: theme.muted; font.pixelSize: 11 }
        }
    }
    ColumnLayout {
        anchors.fill: parent; spacing: 0
        Rectangle {
            visible: companion.error.length > 0
            Layout.fillWidth: true; implicitHeight: errorLabel.implicitHeight + 22
            color: theme.raised
            Label { id: errorLabel; anchors.fill: parent; anchors.margins: 11; text: companion.error; color: theme.danger; wrapMode: Text.Wrap; textFormat: Text.PlainText }
        }
        SplitView {
            Layout.fillWidth: true; Layout.fillHeight: true
            handle: Rectangle { implicitWidth: 1; color: theme.border }
            Pane {
                visible: sidebarVisible
                SplitView.preferredWidth: 265; SplitView.minimumWidth: 215; SplitView.maximumWidth: 420
                padding: 0
                background: Rectangle { color: theme.sidebar }
                ColumnLayout {
                    anchors.fill: parent; spacing: 0
                    RowLayout {
                        Layout.fillWidth: true; Layout.margins: 14
                        Label { text: "EXPLORER"; color: theme.muted; font.pixelSize: 10; font.letterSpacing: 1.7; Layout.fillWidth: true }
                        ToolButton { text: "Open…"; enabled: !companion.busy; onClicked: projectMenu.open() }
                        Menu {
                            id: projectMenu
                            MenuItem { text: "Open project…"; onTriggered: folderDialog.open() }
                            MenuItem { text: "Initialize project…"; onTriggered: initializeFolderDialog.open() }
                            MenuSeparator {}
                            Instantiator {
                                model: companion.recentProjects
                                delegate: MenuItem { required property string modelData; text: modelData; onTriggered: companion.openProject(modelData) }
                                onObjectAdded: function(index, object) { projectMenu.insertItem(index + 3, object) }
                                onObjectRemoved: function(index, object) { projectMenu.removeItem(object) }
                            }
                        }
                    }
                    Label { text: hasProject ? companion.project.split("/").pop() : "No project open"; font.weight: Font.DemiBold; font.pixelSize: 15; Layout.leftMargin: 18; Layout.bottomMargin: 16; textFormat: Text.PlainText }
                    ColumnLayout {
                        visible: hasProject; Layout.fillWidth: true; Layout.leftMargin: 12; Layout.rightMargin: 12; spacing: 8
                        TextField {
                            id: searchInput; objectName: "searchInput"
                            Layout.fillWidth: true; placeholderText: searchMode.currentIndex === 0 ? "Search decisions…" : "src/example.rs"
                            enabled: !companion.busy; onAccepted: runSearch(); Accessible.name: "Search corpus or code path"
                        }
                        RowLayout {
                            Layout.fillWidth: true
                            ComboBox { id: searchMode; model: ["Full-text", "Code scope"]; Layout.fillWidth: true; enabled: !companion.busy }
                            Button { text: "Go"; enabled: !companion.busy; onClicked: runSearch() }
                            ToolButton { text: "×"; ToolTip.text: "Show all files"; ToolTip.visible: hovered; enabled: !companion.busy; onClicked: { searchInput.clear(); companion.search("") } }
                        }
                    }
                    ListView {
                        id: artifactList; objectName: "artifactList"
                        Layout.fillWidth: true; Layout.fillHeight: true; Layout.topMargin: 16
                        model: companion.navigation; clip: true; currentIndex: -1
                        ScrollBar.vertical: ScrollBar {}
                        delegate: ItemDelegate {
                            required property var modelData
                            width: artifactList.width; implicitHeight: modelData.folder ? 35 : 48
                            highlighted: !modelData.folder && metadata.path === modelData.path
                            enabled: !companion.busy
                            onClicked: { if (modelData.folder) companion.toggleFolder(modelData.title); else companion.select(modelData.index) }
                            contentItem: RowLayout {
                                spacing: 9
                                Label { text: modelData.folder ? (modelData.expanded ? "⌄" : "›") : "·"; color: modelData.folder ? theme.muted : theme.accent; font.pixelSize: 17; Layout.leftMargin: modelData.folder ? 0 : 10 }
                                ColumnLayout {
                                    spacing: 3; Layout.fillWidth: true
                                    Label { text: modelData.title; elide: Text.ElideRight; Layout.fillWidth: true; font.pixelSize: modelData.folder ? 11 : 12; font.weight: modelData.folder ? Font.DemiBold : Font.Normal; color: modelData.folder ? theme.secondary : theme.text; textFormat: Text.PlainText }
                                    Label { visible: !modelData.folder; text: modelData.folder ? "" : modelData.kind + " · " + modelData.status; color: theme.muted; font.pixelSize: 10; elide: Text.ElideRight; Layout.fillWidth: true; textFormat: Text.PlainText }
                                }
                                Label { visible: modelData.folder; text: modelData.count || ""; color: theme.muted; font.pixelSize: 10 }
                            }
                            ToolTip.text: modelData.title; ToolTip.visible: hovered
                        }
                        Label { anchors.centerIn: parent; visible: artifactList.count === 0; text: hasProject ? "No matching artifacts" : "Open a local repository"; color: theme.muted; font.pixelSize: 12 }
                    }
                    RowLayout {
                        Layout.fillWidth: true; Layout.margins: 10
                        Button { text: "Validate"; enabled: hasProject && !companion.busy; onClicked: companion.validate(); Layout.fillWidth: true }
                        Button { text: "Sources"; enabled: hasProject && !companion.busy; onClicked: companion.federation(); Layout.fillWidth: true }
                    }
                }
            }
            Pane {
                SplitView.fillWidth: true; SplitView.minimumWidth: 430
                padding: 0; background: Rectangle { color: theme.background }
                ColumnLayout {
                    anchors.fill: parent; spacing: 0
                    Rectangle {
                        Layout.fillWidth: true; height: 42; color: theme.background
                        ListView {
                            id: tabList; anchors.fill: parent; orientation: ListView.Horizontal; clip: true
                            model: companion.tabs; spacing: 1
                            delegate: Rectangle {
                                required property var modelData
                                required property int index
                                width: Math.min(245, Math.max(145, tabTitle.implicitWidth + 55)); height: 42
                                color: index === companion.activeTab ? theme.editor : theme.background
                                Rectangle { anchors.top: parent.top; height: 2; width: parent.width; color: index === companion.activeTab ? theme.accent : "transparent" }
                                RowLayout {
                                    anchors.fill: parent; anchors.leftMargin: 12; anchors.rightMargin: 4; spacing: 5
                                    ToolButton {
                                        Layout.fillWidth: true; implicitHeight: 38; enabled: !companion.busy
                                        onClicked: companion.activateTab(index)
                                        contentItem: Label { id: tabTitle; text: (modelData.dirty ? "● " : "") + modelData.title; elide: Text.ElideRight; color: index === companion.activeTab ? theme.text : theme.muted; font.pixelSize: 12; textFormat: Text.PlainText }
                                        ToolTip.text: modelData.path; ToolTip.visible: hovered
                                    }
                                    ToolButton { text: "×"; font.pixelSize: 16; enabled: !companion.busy; implicitWidth: 25; onClicked: companion.closeTab(index); Accessible.name: "Close document tab" }
                                }
                            }
                            ScrollBar.horizontal: ScrollBar {}
                        }
                    }
                    Rectangle {
                        visible: companion.activeTab >= 0
                        Layout.fillWidth: true; height: 43; color: theme.editor
                        RowLayout {
                            anchors.fill: parent; anchors.leftMargin: 18; anchors.rightMargin: 12
                            Label { text: selected.id || "New draft"; color: theme.muted; font.pixelSize: 10; textFormat: Text.PlainText }
                            Item { Layout.fillWidth: true }
                            ToolButton { text: "Source"; checked: viewMode === 0; checkable: true; onClicked: viewMode = 0; implicitHeight: 30 }
                            ToolButton { text: "Split"; checked: viewMode === 1; checkable: true; onClicked: viewMode = 1; implicitHeight: 30 }
                            ToolButton { text: "Read"; checked: viewMode === 2; checkable: true; onClicked: viewMode = 2; implicitHeight: 30 }
                            ToolButton { text: "☷"; onClicked: inspectorVisible = !inspectorVisible; ToolTip.text: "Toggle decision context"; ToolTip.visible: hovered; Accessible.name: "Toggle decision context" }
                        }
                    }
                    SplitView {
                        visible: companion.activeTab >= 0
                        Layout.fillWidth: true; Layout.fillHeight: true
                        handle: Rectangle { implicitWidth: 1; color: theme.border }
                        Pane {
                            visible: viewMode !== 2
                            SplitView.fillWidth: viewMode === 0
                            SplitView.preferredWidth: parent.width / 2
                            SplitView.minimumWidth: 200
                            padding: 0; background: Rectangle { color: theme.editor }
                            ColumnLayout {
                                anchors.fill: parent; spacing: 0
                                Label { text: companion.editable ? "MARKDOWN" : "INHERITED · READ-ONLY"; color: theme.muted; font.pixelSize: 9; font.letterSpacing: 1.3; Layout.leftMargin: 22; Layout.topMargin: 14; Layout.bottomMargin: 10 }
                                ScrollView {
                                    Layout.fillWidth: true; Layout.fillHeight: true; clip: true
                                    TextArea {
                                        id: sourceEditor; objectName: "sourceEditor"
                                        readOnly: !companion.editable || companion.busy
                                        selectByMouse: true; persistentSelection: true
                                        wrapMode: TextEdit.Wrap; textFormat: TextEdit.PlainText
                                        font.family: theme.codeFont; font.pixelSize: editorZoom
                                        color: theme.text; selectionColor: theme.selection; selectedTextColor: theme.text
                                        leftPadding: 22; rightPadding: 22; topPadding: 4; bottomPadding: 36
                                        background: null
                                        onTextChanged: { companion.setEditorText(text); previewTimer.restart() }
                                        Accessible.name: "Markdown source editor"
                                    }
                                }
                            }
                        }
                        Pane {
                            visible: viewMode !== 0
                            SplitView.fillWidth: true; SplitView.minimumWidth: 200
                            padding: 0; background: Rectangle { color: theme.background }
                            ColumnLayout {
                                anchors.fill: parent; spacing: 0
                                Label { text: "PREVIEW"; color: theme.muted; font.pixelSize: 9; font.letterSpacing: 1.3; Layout.leftMargin: 24; Layout.topMargin: 14; Layout.bottomMargin: 10 }
                                ScrollView {
                                    id: previewScroll
                                    Layout.fillWidth: true; Layout.fillHeight: true; clip: true
                                    contentWidth: availableWidth
                                    MarkdownView {
                                        id: preview; objectName: "markdownPreview"
                                        width: previewScroll.availableWidth - 48
                                        x: 24; height: implicitHeight
                                        textColor: theme.text; linkColor: theme.accent; fontSize: editorZoom
                                        Accessible.name: "Local Markdown preview"
                                    }
                                }
                            }
                        }
                    }
                    ColumnLayout {
                        visible: companion.activeTab < 0
                        Layout.fillWidth: true; Layout.fillHeight: true; Layout.margins: 60; spacing: 18
                        Item { Layout.fillHeight: true }
                        Label { text: "Make the reasoning\neasy to return to."; font.pixelSize: 31; font.weight: Font.Medium; color: theme.text; lineHeight: 1.15 }
                        Label { text: hasProject ? "Open a decision from the explorer, or start a new artifact.\nYour writing, its relationships and the rules it records, together." : "A local workspace for the decisions your agents follow.\nOpen a repository to browse, write and review its knowledge."; color: theme.secondary; wrapMode: Text.Wrap; Layout.fillWidth: true; lineHeight: 1.5 }
                        RowLayout {
                            Button { text: hasProject ? "New artifact" : "Open project"; onClicked: { if (hasProject) newDocument(); else folderDialog.open() } }
                            Button { text: "Initialize project"; onClicked: initializeFolderDialog.open() }
                        }
                        Label { text: "Ctrl+P  Commands     Ctrl+N  New artifact     Ctrl+S  Review save"; font.pixelSize: 11; color: theme.muted; wrapMode: Text.Wrap; Layout.fillWidth: true }
                        Item { Layout.fillHeight: true }
                    }
                }
            }
            Pane {
                visible: inspectorVisible && window.width >= 1150 && companion.activeTab >= 0
                SplitView.preferredWidth: 260; SplitView.minimumWidth: 220; SplitView.maximumWidth: 340
                padding: 18; background: Rectangle { color: theme.sidebar }
                ColumnLayout {
                    anchors.fill: parent; spacing: 14
                    Label { text: "DECISION CONTEXT"; font.pixelSize: 10; font.letterSpacing: 1.5; color: theme.muted }
                    Label { text: selected.title || ""; font.pixelSize: 15; font.weight: Font.DemiBold; wrapMode: Text.Wrap; Layout.fillWidth: true; textFormat: Text.PlainText }
                    Label { text: (selected.type || "") + " · " + (selected.status || "Draft"); color: theme.accent; font.pixelSize: 11; textFormat: Text.PlainText }
                    Rectangle { height: 1; Layout.fillWidth: true; color: theme.border }
                    Label { text: "SOURCE"; font.pixelSize: 9; font.letterSpacing: 1.3; color: theme.muted }
                    Label { text: metadata.source || ""; color: theme.secondary; wrapMode: Text.WrapAnywhere; Layout.fillWidth: true; font.pixelSize: 12; textFormat: Text.PlainText }
                    Label { text: companion.editable ? "Local · editable" : "Inherited · read-only"; color: theme.secondary; font.pixelSize: 11 }
                    Label {
                        visible: provenance.overrides !== undefined && provenance.overrides.length > 0
                        text: "Override history present. This record may be historical."
                        color: theme.accent; wrapMode: Text.Wrap; Layout.fillWidth: true; font.pixelSize: 11
                    }
                    Label {
                        visible: selected.match !== undefined
                        text: selected.match ? (selected.match.matching_entry ? "Governs: " + selected.match.matching_entry : "Matched in: " + ((selected.match.evidence || {}).field || "corpus")) : ""
                        wrapMode: Text.Wrap; Layout.fillWidth: true; color: theme.secondary; font.pixelSize: 11; textFormat: Text.PlainText
                    }
                    RowLayout {
                        Layout.fillWidth: true
                        ToolButton { text: "Open file"; enabled: companion.editable && !selected.new_file && !companion.busy; onClicked: companion.openSelected() }
                        ToolButton { text: "Reload"; enabled: companion.editable && !selected.new_file && !companion.busy; onClicked: companion.reloadSelected() }
                    }
                    Rectangle { height: 1; Layout.fillWidth: true; color: theme.border }
                    Label { text: "LINKED KNOWLEDGE"; font.pixelSize: 9; font.letterSpacing: 1.3; color: theme.muted }
                    Label { text: "Recorded relationships on disk"; font.pixelSize: 10; color: theme.muted }
                    ListView {
                        Layout.fillWidth: true; Layout.fillHeight: true; clip: true; model: companion.links; spacing: 5
                        delegate: ItemDelegate {
                            required property var modelData
                            required property int index
                            width: ListView.view.width; implicitHeight: linkTitle.implicitHeight + 28
                            enabled: modelData.index >= 0 && !companion.busy
                            onClicked: companion.followLink(index)
                            contentItem: Column {
                                spacing: 5
                                Label { text: modelData.direction + " · " + modelData.kind; color: theme.muted; font.pixelSize: 9; width: parent.width; elide: Text.ElideRight }
                                Label { id: linkTitle; text: modelData.title; width: parent.width; wrapMode: Text.Wrap; color: theme.text; font.pixelSize: 12; textFormat: Text.PlainText }
                            }
                        }
                        Label { anchors.top: parent.top; width: parent.width; visible: companion.links.length === 0; text: "No recorded links yet. Add a relationship section to connect this artifact to another."; wrapMode: Text.Wrap; color: theme.muted; font.pixelSize: 11 }
                    }
                    Button { text: "Provenance details"; visible: Object.keys(provenance).length > 0; onClicked: provenanceDialog.open(); Layout.fillWidth: true }
                }
            }
        }
    }
    MarkdownHighlighter { target: sourceEditor.textDocument; accentColor: theme.accent; mutedColor: theme.muted }
    Timer { id: previewTimer; interval: 180; onTriggered: preview.markdown = sourceEditor.text }

    Dialog {
        id: newDialog; title: "New artifact"; modal: true; anchors.centerIn: parent; width: 460
        standardButtons: Dialog.Cancel
        ColumnLayout {
            anchors.fill: parent; spacing: 12
            Label { text: "Start with a core template. Nothing is written until you review and save."; color: theme.secondary; wrapMode: Text.Wrap; Layout.fillWidth: true }
            ComboBox { id: artifactType; model: ["decision", "requirement", "design", "roadmap", "prompt"]; Layout.fillWidth: true; Accessible.name: "Artifact type" }
            TextField { id: artifactTitle; placeholderText: "Title"; Layout.fillWidth: true; Accessible.name: "Artifact title" }
            TextField { id: artifactPath; placeholderText: "new-decision.md"; Layout.fillWidth: true; Accessible.name: "Filename relative to corpus" }
            Label { text: "Filename is relative to the corpus. Parent folders must already exist."; color: theme.muted; font.pixelSize: 11; wrapMode: Text.Wrap; Layout.fillWidth: true }
            Button { text: "Create draft"; enabled: artifactTitle.text.trim().length > 0 && artifactPath.text.trim().length > 0 && !companion.busy; onClicked: { companion.newDraft(artifactType.currentText, artifactTitle.text, artifactPath.text); newDialog.close() } }
        }
        onOpened: artifactTitle.forceActiveFocus()
    }
    Dialog {
        id: reviewDialog; title: companion.reviewData.valid ? "Review your changes" : "Correct the draft before saving"
        modal: true; anchors.centerIn: parent; width: Math.min(window.width - 80, 980); height: window.height - 100
        closePolicy: Popup.NoAutoClose
        ColumnLayout {
            anchors.fill: parent; spacing: 12
            Label { text: "Validation runs again when you apply. External file changes will block replacement."; color: theme.secondary; wrapMode: Text.Wrap; Layout.fillWidth: true }
            TabBar { id: reviewTabs; Layout.fillWidth: true; TabButton { text: "Changes" } TabButton { text: "Validation findings" } }
            ScrollView {
                Layout.fillWidth: true; Layout.fillHeight: true; clip: true
                TextArea { text: reviewTabs.currentIndex === 0 ? (companion.reviewData.diff || "No textual changes") : (companion.reviewData.findingsText || "No findings"); readOnly: true; selectByMouse: true; font.family: theme.codeFont; font.pixelSize: 12; wrapMode: TextEdit.Wrap; textFormat: TextEdit.PlainText; color: theme.text }
            }
            RowLayout {
                Item { Layout.fillWidth: true }
                Button { text: "Back to editing"; enabled: !companion.busy; onClicked: { reviewDialog.close(); companion.cancelReview() } }
                Button { text: "Apply && save"; highlighted: true; enabled: companion.reviewData.valid === true && !companion.busy; onClicked: companion.applySave() }
            }
        }
    }
    Dialog {
        id: discardDialog; title: "Keep your unsaved work?"; modal: true; anchors.centerIn: parent; width: 440
        ColumnLayout {
            anchors.fill: parent; spacing: 16
            Label { text: "There are unsaved changes. Return to the editor to review and save them, or explicitly discard them to continue."; wrapMode: Text.Wrap; Layout.fillWidth: true; color: theme.secondary }
            RowLayout {
                Button { text: "Keep editing"; onClicked: { discardDialog.close(); companion.cancelDiscard() } }
                Button { text: "Discard && continue"; onClicked: { discardDialog.close(); companion.confirmDiscard() } }
            }
        }
        onRejected: companion.cancelDiscard()
    }
    Dialog {
        id: reportDialog; title: companion.reportTitle; modal: true; anchors.centerIn: parent
        width: Math.min(window.width - 80, 850); height: window.height - 100; standardButtons: Dialog.Close
        onClosed: companion.dismissReport()
        ScrollView { anchors.fill: parent; clip: true; TextArea { text: companion.report; readOnly: true; selectByMouse: true; wrapMode: TextEdit.Wrap; font.family: theme.codeFont; textFormat: TextEdit.PlainText } }
    }
    Dialog {
        id: provenanceDialog; title: "Source and override history"; modal: true; anchors.centerIn: parent
        width: Math.min(window.width - 80, 740); height: window.height - 130; standardButtons: Dialog.Close
        ScrollView { anchors.fill: parent; clip: true; TextArea { text: JSON.stringify(provenance, null, 2); readOnly: true; selectByMouse: true; wrapMode: TextEdit.Wrap; font.family: theme.codeFont; textFormat: TextEdit.PlainText } }
    }
    Dialog {
        id: commandDialog; title: "Commands"; modal: true; anchors.centerIn: parent; width: 540
        standardButtons: Dialog.Close
        ColumnLayout {
            anchors.fill: parent; spacing: 4
            Repeater {
                model: ["Open project…                   Ctrl+O", "New artifact…                     Ctrl+N", "Review and save                 Ctrl+S", "Validate corpus                   Ctrl+Shift+V", "Refresh project                    Ctrl+R", "Switch source / split / read     Ctrl+E", "Toggle explorer", "Toggle decision context", "Initialize project…"]
                delegate: ItemDelegate {
                    required property string modelData
                    required property int index
                    text: modelData; Layout.fillWidth: true
                    enabled: !companion.busy && (index === 0 || index === 8 || hasProject)
                    onClicked: {
                        commandDialog.close()
                        if (index === 0) folderDialog.open()
                        else if (index === 1) newDocument()
                        else if (index === 2) reviewDocument()
                        else if (index === 3) companion.validate()
                        else if (index === 4) companion.refresh()
                        else if (index === 5) viewMode = (viewMode + 1) % 3
                        else if (index === 6) sidebarVisible = !sidebarVisible
                        else if (index === 7) inspectorVisible = !inspectorVisible
                        else initializeFolderDialog.open()
                    }
                }
            }
        }
    }
    Connections {
        target: companion
        function onSelectionChanged() {
            if (sourceEditor.text !== companion.editorText) sourceEditor.text = companion.editorText
            preview.markdown = sourceEditor.text
        }
        function onChanged() { if (companion.report.length > 0 && !reportDialog.opened) reportDialog.open() }
        function onReviewReady() { reviewTabs.currentIndex = companion.reviewData.valid ? 0 : 1; reviewDialog.open() }
        function onDiscardRequested() { discardDialog.open() }
        function onQuitReady() { allowClose = true; window.close() }
        function onCompleted(operation, success) {
            if (operation === "save") { reviewDialog.close(); if (!success) companion.cancelReview() }
        }
    }
}
