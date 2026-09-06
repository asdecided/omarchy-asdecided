import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs

ApplicationWindow {
    id: window
    width: 1220
    height: 800
    minimumWidth: 880
    minimumHeight: 580
    visible: true
    title: companion.source ? companion.source + " — AsDecided" : "AsDecided"
    color: palette.window
    property bool hasProject: companion.project.length > 0
    property var selected: companion.selected
    property var meta: selected.metadata || ({})
    property var origin: meta.provenance || ({})
    property color muted: palette.placeholderText
    property int space: 16

    FolderDialog {
        id: folderDialog
        title: "Open an AsDecided repository"
        onAccepted: { searchInput.clear(); companion.openProjectUrl(selectedFolder) }
    }
    Shortcut { sequence: "Ctrl+O"; enabled: !companion.busy; onActivated: folderDialog.open() }
    Shortcut { sequence: "Ctrl+F"; enabled: hasProject; onActivated: searchInput.forceActiveFocus() }
    Shortcut { sequence: "Ctrl+R"; enabled: hasProject && !companion.busy; onActivated: { searchInput.clear(); companion.refresh() } }
    Shortcut { sequence: "Ctrl+Shift+V"; enabled: hasProject && !companion.busy; onActivated: companion.validate() }
    Shortcut { sequence: "Escape"; onActivated: { if (companion.busy) companion.cancel(); else companion.dismissReport() } }
    Shortcut { sequence: "Ctrl+Q"; onActivated: Qt.quit() }

    header: ToolBar {
        implicitHeight: 52
        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: space
            anchors.rightMargin: space
            spacing: 12
            Label { text: "AsDecided"; font.pixelSize: 19; font.weight: Font.DemiBold }
            Label { text: "Build, as decided."; color: muted; visible: window.width > 1050 }
            Item { Layout.fillWidth: true }
            Button { text: "Open project"; enabled: !companion.busy; onClicked: folderDialog.open(); ToolTip.text: "Ctrl+O"; ToolTip.visible: hovered }
            Button { text: "Refresh"; enabled: hasProject && !companion.busy; onClicked: { searchInput.clear(); companion.refresh() } ToolTip.text: "Ctrl+R"; ToolTip.visible: hovered }
            Button { text: "Validate"; enabled: hasProject && !companion.busy; onClicked: companion.validate() }
            Button { text: "Sources"; enabled: hasProject && !companion.busy; onClicked: companion.federation() }
        }
    }
    footer: ToolBar {
        implicitHeight: 36
        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: space
            anchors.rightMargin: space
            Label { text: companion.status; elide: Text.ElideRight; Layout.fillWidth: true; color: muted }
            BusyIndicator { running: companion.busy; visible: running; implicitWidth: 24; implicitHeight: 24 }
            Button { text: "Cancel"; visible: companion.busy; onClicked: companion.cancel() }
            Label { text: "LOCAL WORKSPACE"; font.pixelSize: 10; font.letterSpacing: 1.2; color: muted }
        }
    }
    ColumnLayout {
        anchors.fill: parent
        spacing: 0
        Rectangle {
            visible: companion.error.length > 0
            color: palette.alternateBase
            Layout.fillWidth: true
            implicitHeight: errorLabel.implicitHeight + 24
            Label { id: errorLabel; anchors.fill: parent; anchors.margins: 12; text: companion.error; wrapMode: Text.Wrap; textFormat: Text.PlainText }
        }
        RowLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 0
            Pane {
                Layout.preferredWidth: 205
                Layout.fillHeight: true
                padding: 18
                background: Rectangle { color: palette.alternateBase }
                ColumnLayout {
                    anchors.fill: parent
                    spacing: 14
                    Label { text: "WORKSPACE"; font.pixelSize: 10; font.letterSpacing: 1.6; color: muted }
                    Label { text: companion.source ? companion.source.replace("/", "/\n") : "Your projects"; font.weight: Font.DemiBold; wrapMode: Text.WrapAnywhere; Layout.fillWidth: true; textFormat: Text.PlainText }
                    Label { text: companion.project; font.pixelSize: 11; color: muted; wrapMode: Text.WrapAnywhere; Layout.fillWidth: true; visible: hasProject; textFormat: Text.PlainText }
                    Button { text: "All artifacts"; visible: hasProject; Layout.fillWidth: true; enabled: !companion.busy; onClicked: { searchInput.clear(); companion.search("") } }
                    Label { text: "RECENT PROJECTS"; font.pixelSize: 10; font.letterSpacing: 1.3; color: muted; Layout.topMargin: 18 }
                    Repeater {
                        model: companion.recentProjects
                        delegate: ItemDelegate {
                            required property string modelData
                            Layout.fillWidth: true
                            text: modelData.split("/").pop()
                            enabled: !companion.busy
                            onClicked: { searchInput.clear(); companion.openProject(modelData) }
                            ToolTip.text: modelData
                            ToolTip.visible: hovered
                        }
                    }
                    Item { Layout.fillHeight: true }
                    Label { text: "Decisions stay in your repository.\n\nCtrl+O  Open project\nCtrl+F  Search\nCtrl+R  Refresh"; font.pixelSize: 11; color: muted; wrapMode: Text.Wrap; Layout.fillWidth: true }
                }
            }
            ColumnLayout {
                visible: !hasProject
                Layout.fillWidth: true
                Layout.fillHeight: true
                Layout.margins: 50
                spacing: 18
                Item { Layout.fillHeight: true }
                Label { text: "The decisions behind\nyour next change."; font.pixelSize: 34; font.weight: Font.DemiBold; lineHeight: 1.15 }
                Label { text: "Browse your engineering knowledge, check its health,\nand see which accepted decisions govern a code path."; font.pixelSize: 15; color: muted; lineHeight: 1.5 }
                Button { text: "Open a project…"; enabled: !companion.busy; onClicked: folderDialog.open() }
                Label { text: "Choose a repository containing decisions/ and .decided/.\nExisting rac/ projects are also supported."; font.pixelSize: 12; color: muted }
                Item { Layout.fillHeight: true }
            }
            SplitView {
                visible: hasProject
                Layout.fillWidth: true
                Layout.fillHeight: true
                orientation: Qt.Horizontal
                Pane {
                    SplitView.preferredWidth: 365
                    SplitView.minimumWidth: 265
                    padding: 14
                    ColumnLayout {
                        anchors.fill: parent
                        spacing: 10
                        Label { text: "Explore decisions"; font.pixelSize: 20; font.weight: Font.DemiBold }
                        RowLayout {
                            Layout.fillWidth: true
                            ComboBox { id: mode; model: ["Search", "Code path"]; implicitWidth: 108; enabled: !companion.busy }
                            TextField {
                                id: searchInput
                                objectName: "searchInput"
                                Layout.fillWidth: true
                                placeholderText: mode.currentIndex === 0 ? "Search corpus…" : "src/example.rs"
                                enabled: !companion.busy
                                onAccepted: { if (mode.currentIndex === 0) companion.search(text); else companion.scope(text) }
                                Accessible.name: mode.currentIndex === 0 ? "Search corpus" : "Repository-relative code path"
                            }
                        }
                        Button {
                            text: mode.currentIndex === 0 ? "Search" : "Find governing decisions"
                            Layout.fillWidth: true
                            enabled: !companion.busy
                            onClicked: { if (mode.currentIndex === 0) companion.search(searchInput.text); else companion.scope(searchInput.text) }
                        }
                        Label {
                            text: mode.currentIndex === 0 ? "Engine-ranked results from your local corpus." : "Matches declared scope. This is not an agent activity log."
                            color: muted; font.pixelSize: 11; wrapMode: Text.Wrap; Layout.fillWidth: true
                        }
                        ListView {
                            id: artifactList
                            objectName: "artifactList"
                            Layout.fillWidth: true
                            Layout.fillHeight: true
                            clip: true
                            model: companion.documents
                            spacing: 4
                            currentIndex: -1
                            ScrollBar.vertical: ScrollBar {}
                            onCurrentIndexChanged: if (currentIndex >= 0) companion.select(currentIndex)
                            delegate: ItemDelegate {
                                required property var modelData
                                required property int index
                                width: artifactList.width
                                implicitHeight: titleLabel.implicitHeight + 44
                                highlighted: artifactList.currentIndex === index
                                onClicked: { artifactList.currentIndex = index; companion.select(index) }
                                contentItem: Column {
                                    spacing: 6
                                    Label { text: modelData.type.toUpperCase() + "  ·  " + (modelData.status || "Unspecified"); font.pixelSize: 10; color: muted; width: parent.width; elide: Text.ElideRight; textFormat: Text.PlainText }
                                    Label { id: titleLabel; text: modelData.title; width: parent.width; wrapMode: Text.Wrap; font.weight: Font.Medium; textFormat: Text.PlainText }
                                }
                            }
                            Label { anchors.centerIn: parent; visible: artifactList.count === 0; text: "No artifacts to show"; color: muted }
                        }
                    }
                }
                Pane {
                    SplitView.fillWidth: true
                    SplitView.minimumWidth: 330
                    padding: 22
                    ColumnLayout {
                        anchors.fill: parent
                        visible: Object.keys(selected).length > 0
                        spacing: 12
                        RowLayout {
                            Layout.fillWidth: true
                            Label { text: (selected.type || "").toUpperCase() + "  /  " + (selected.status || ""); color: muted; font.pixelSize: 11; Layout.fillWidth: true; textFormat: Text.PlainText }
                            Button { text: "Open file"; enabled: !companion.busy && (origin.layer || "local") === "local"; onClicked: companion.openSelected() }
                        }
                        Label { text: selected.title || ""; font.pixelSize: 23; font.weight: Font.DemiBold; wrapMode: Text.Wrap; Layout.fillWidth: true; textFormat: Text.PlainText }
                        Label { text: (meta.source || "") + " · " + (origin.layer || "local") + "\n" + (meta.path || ""); font.pixelSize: 11; color: muted; wrapMode: Text.WrapAnywhere; Layout.fillWidth: true; textFormat: Text.PlainText }
                        Label {
                            visible: selected.match !== undefined
                            text: selected.match ? (selected.match.matching_entry ? "Governs this path through: " + selected.match.matching_entry : "Matched in: " + ((selected.match.evidence || {}).field || "corpus")) : ""
                            font.pixelSize: 12; wrapMode: Text.Wrap; Layout.fillWidth: true; textFormat: Text.PlainText
                        }
                        Label {
                            visible: origin.overrides !== undefined && origin.overrides.length > 0
                            text: "Override history present. Review source details before treating this record as effective."
                            wrapMode: Text.Wrap; Layout.fillWidth: true; font.weight: Font.Medium
                        }
                        CheckBox { id: showProvenance; text: "Source and override details"; visible: Object.keys(origin).length > 0 }
                        ScrollView {
                            Layout.fillWidth: true
                            Layout.fillHeight: true
                            clip: true
                            TextArea {
                                id: body
                                objectName: "documentBody"
                                text: showProvenance.checked && Object.keys(origin).length > 0 ? JSON.stringify(origin, null, 2) : (selected.text || "")
                                readOnly: true
                                selectByMouse: true
                                wrapMode: TextEdit.Wrap
                                textFormat: TextEdit.PlainText
                                font.pixelSize: 14
                                background: null
                                Accessible.name: "Decision text"
                            }
                        }
                    }
                    Label { anchors.centerIn: parent; visible: Object.keys(selected).length === 0; text: "Select an artifact to read it."; color: muted }
                }
            }
        }
    }
    Dialog {
        id: reportDialog
        title: companion.reportTitle
        modal: true
        anchors.centerIn: parent
        width: Math.min(window.width - 80, 850)
        height: window.height - 100
        standardButtons: Dialog.Close
        onClosed: companion.dismissReport()
        ScrollView {
            anchors.fill: parent
            clip: true
            TextArea { text: companion.report; textFormat: TextEdit.PlainText; readOnly: true; selectByMouse: true; wrapMode: TextEdit.Wrap; font.family: "monospace" }
        }
    }
    Connections {
        target: companion
        function onChanged() {
            if (companion.report.length > 0 && !reportDialog.opened) reportDialog.open()
        }
        function onCompleted(operation, success) {
            if (success && (operation === "open" || operation === "search" || operation === "scope")) artifactList.currentIndex = -1
        }
    }
}
