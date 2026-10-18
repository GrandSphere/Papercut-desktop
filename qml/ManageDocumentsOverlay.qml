import QtQuick
import QtQuick.Controls.Basic
import Papercut 1.0

PapercutOverlay {
    id: root
    property bool selecting: false
    property var selected: ({})
    property var docs: App.documents()

    title: selecting ? selectedCount() + " selected" : "Manage bookmarks"
    interceptBack: true
    onBackRequested: { if (selecting) { selecting = false; selected = {} } else dismiss() }
    onOverlayVisibleChanged: if (overlayVisible) { docs = App.documents(); selecting = false; selected = {} }

    function selectedCount() { let n = 0; for (const k in selected) if (selected[k]) n++; return n }
    function selectedUris() { const u = []; for (const k in selected) if (selected[k]) u.push(k); return u }
    function toggle(uri) {
        const n = Object.assign({}, selected)
        if (n[uri]) delete n[uri]
        else n[uri] = true
        selected = n
    }

    ChromeIcon {
        parent: root.actionHost
        icon: selecting ? (selectedCount() === docs.length && docs.length > 0 ? "check_box_outline_blank" : "select_all") : "check_box_outline_blank"
        hint: selecting ? "Select all" : "Select"
        onClicked: {
            if (!selecting) { selecting = true; return }
            if (selectedCount() === docs.length) selected = {}
            else { const n = {}; for (let i = 0; i < docs.length; i++) n[docs[i].uri] = true; selected = n }
        }
    }
    ChromeIcon {
        parent: root.actionHost
        visible: selecting
        icon: "delete"
        hint: "Delete"
        enabled: selectedCount() > 0
        onClicked: { App.deleteDocuments(selectedUris()); docs = App.documents(); selected = {}; selecting = false }
    }

    ListView {
        anchors.fill: parent
        clip: true
        model: root.docs
        boundsBehavior: Flickable.StopAtBounds
        ScrollBar.vertical: ScrollBar {}
        delegate: Item {
            width: ListView.view.width
            height: 60
            MouseArea { anchors.fill: parent; onClicked: { if (root.selecting) root.toggle(modelData.uri) } }
            Row {
                anchors { fill: parent; leftMargin: 12; rightMargin: 4 }
                spacing: 4
                ChromeIcon {
                    visible: root.selecting
                    anchors.verticalCenter: parent.verticalCenter
                    icon: root.selected[modelData.uri] ? "check_box" : "check_box_outline_blank"
                    onClicked: root.toggle(modelData.uri)
                }
                Column {
                    width: parent.width - 52 - (root.selecting ? 52 : 0)
                    anchors.verticalCenter: parent.verticalCenter
                    AppText { text: modelData.displayName; color: App.appFontColor; opacity: 0.7; font.pixelSize: Math.round(16 * App.uiScale); elide: Text.ElideRight; width: parent.width }
                    AppText { visible: modelData.displayPath.length > 0; text: modelData.displayPath; color: App.appFontColor; opacity: 0.45; font.pixelSize: Math.round(12 * App.uiScale); elide: Text.ElideMiddle; width: parent.width }
                }
                ChromeIcon {
                    visible: !root.selecting
                    anchors.verticalCenter: parent.verticalCenter
                    icon: "close"
                    hint: "Delete"
                    onClicked: { App.deleteDocuments([modelData.uri]); root.docs = App.documents() }
                }
            }
        }
    }
}
