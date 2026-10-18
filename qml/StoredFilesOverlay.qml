import QtQuick
import QtQuick.Controls.Basic
import Papercut 1.0

PapercutOverlay {
    id: root
    property bool selecting: false
    property var selected: ({})
    property var files: []
    signal preview(var entry)
    signal shareEntry(var entry)
    signal deleteEntries(var entries)

    title: selecting ? selectedCount() + " selected" : "Stored files"
    interceptBack: true
    onBackRequested: { if (selecting) { selecting = false; selected = {} } else dismiss() }
    onOverlayVisibleChanged: if (overlayVisible) { refresh(); selecting = false; selected = {} }

    function refresh() { files = App.storedFiles() }
    function selectedCount() { let n = 0; for (const k in selected) if (selected[k]) n++; return n }
    function selectedEntries() {
        const out = []
        for (let i = 0; i < files.length; i++) if (selected[files[i].path]) out.push(files[i])
        return out
    }
    function toggle(path) {
        const n = Object.assign({}, selected)
        if (n[path]) delete n[path]
        else n[path] = true
        selected = n
    }

    ChromeIcon {
        parent: root.actionHost
        icon: selecting ? (selectedCount() === files.length && files.length > 0 ? "check_box_outline_blank" : "select_all") : "check_box_outline_blank"
        hint: selecting ? "Select all" : "Select"
        onClicked: {
            if (!selecting) { selecting = true; return }
            if (selectedCount() === files.length) selected = {}
            else { const n = {}; for (let i = 0; i < files.length; i++) n[files[i].path] = true; selected = n }
        }
    }
    ChromeIcon {
        parent: root.actionHost
        visible: selecting
        icon: "delete"
        hint: "Delete"
        enabled: selectedCount() > 0
        onClicked: { const entries = selectedEntries(); selected = {}; selecting = false; root.deleteEntries(entries) }
    }

    AppText {
        visible: root.files.length === 0
        x: 16; y: 16
        text: "No stored files"
        color: App.appFontColor
        opacity: 0.7
        font.pixelSize: Math.round(16 * App.uiScale)
    }
    ListView {
        anchors.fill: parent
        clip: true
        model: root.files
        boundsBehavior: Flickable.StopAtBounds
        ScrollBar.vertical: ScrollBar {}
        delegate: Item {
            width: ListView.view.width
            height: 60
            MouseArea {
                anchors.fill: parent
                onClicked: { if (root.selecting) root.toggle(modelData.path); else root.preview(modelData) }
            }
            Row {
                anchors { fill: parent; leftMargin: 12; rightMargin: 4 }
                Column {
                    width: parent.width - (root.selecting ? 48 : 96)
                    anchors.verticalCenter: parent.verticalCenter
                    AppText { text: modelData.title; color: App.appFontColor; opacity: 0.7; font.pixelSize: Math.round(16 * App.uiScale); elide: Text.ElideRight; width: parent.width }
                    AppText { text: modelData.status; color: App.appFontColor; opacity: 0.45; font.pixelSize: Math.round(12 * App.uiScale); elide: Text.ElideRight; width: parent.width }
                }
                ChromeIcon { visible: !root.selecting; anchors.verticalCenter: parent.verticalCenter; icon: "share"; hint: "Share"; onClicked: root.shareEntry(modelData) }
                ChromeIcon { visible: !root.selecting; anchors.verticalCenter: parent.verticalCenter; icon: "close"; hint: "Delete"; onClicked: root.deleteEntries([modelData]) }
                ChromeIcon {
                    visible: root.selecting
                    anchors.verticalCenter: parent.verticalCenter
                    icon: root.selected[modelData.path] ? "check_box" : "check_box_outline_blank"
                    onClicked: root.toggle(modelData.path)
                }
            }
        }
    }
}
