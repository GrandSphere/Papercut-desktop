import QtQuick
import Papercut 1.0

PapercutOverlay {
    id: root
    property var folio
    property bool selecting: false
    property var selected: ({})
    readonly property var items: folio ? folio.allItems : []

    title: selecting ? selectedCount() + " selected" : "Manage Folio"
    interceptBack: true
    onBackRequested: exitModes()
    onDismissed: { selecting = false; selected = {} }

    function selectedCount() {
        let n = 0
        for (const k in selected) if (selected[k]) n++
        return n
    }
    function selectedIds() {
        const ids = []
        for (const k in selected) if (selected[k]) ids.push(Number(k))
        return ids
    }
    function toggleId(id) {
        const next = Object.assign({}, selected)
        if (next[id]) delete next[id]
        else next[id] = true
        selected = next
    }
    function exitModes() {
        if (selecting) { selecting = false; selected = {}; return }
        dismiss()
    }

    ChromeIcon {
        parent: root.actionHost
        icon: selecting ? (selectedCount() === items.length && items.length > 0 ? "check_box_outline_blank" : "select_all")
                        : "check_box_outline_blank"
        hint: selecting ? "Select all" : "Select"
        onClicked: {
            if (!selecting) { selecting = true; return }
            if (selectedCount() === items.length) selected = {}
            else {
                const next = {}
                for (let i = 0; i < items.length; i++) next[items[i].id] = true
                selected = next
            }
        }
    }
    ChromeIcon {
        parent: root.actionHost
        visible: selecting
        icon: "delete"
        hint: "Delete"
        enabled: selectedCount() > 0
        onClicked: { folio.deleteFolioItems(selectedIds()); selected = {}; selecting = false }
    }

    ReorderableList {
        anchors.fill: parent
        model: root.items
        rowHeight: 64
        dragMode: root.selecting ? "none" : "move"
        swipeEnabled: !root.selecting // swipe right sets folio start item
        onClicked: index => { if (root.selecting) root.toggleId(root.items[index].id) }
        onMoved: (from, to) => {
            const ids = root.items.map(i => i.id)
            ids.splice(to, 0, ids.splice(from, 1)[0])
            root.folio.reorderFolio(ids)
        }
        onSwiped: index => root.folio.setFolioStart(root.items[index].id)
        rowDelegate: Item {
            property var rowData
            property int rowIndex
            Row {
                anchors { fill: parent; leftMargin: 12; rightMargin: 4 }
                spacing: 4
                ChromeIcon {
                    visible: root.selecting
                    width: 40; height: 40
                    anchors.verticalCenter: parent.verticalCenter
                    icon: root.selected[rowData.id] ? "check_box" : "check_box_outline_blank"
                    onClicked: root.toggleId(rowData.id)
                }
                Column {
                    width: 28
                    anchors.verticalCenter: parent.verticalCenter
                    AppText { text: rowIndex + 1; color: App.appFontColor; opacity: 0.45; font.pixelSize: Math.round(16 * App.uiScale) }
                    AppText { text: rowData.isStartItem ? "*" : " "; color: App.appFontColor; opacity: rowData.isStartItem ? 0.7 : 0; font.pixelSize: Math.round(12 * App.uiScale) }
                }
                Column {
                    width: parent.width - 28 - (root.selecting ? 44 : 0) - (root.selecting ? 0 : 100)
                    anchors.verticalCenter: parent.verticalCenter
                    AppText {
                        text: rowData.displayName || ""
                        color: App.appFontColor
                        opacity: rowData.hidden ? 0.4 : 0.7
                        font.pixelSize: Math.round(16 * App.uiScale)
                        elide: Text.ElideRight
                        width: parent.width
                    }
                    AppText {
                        text: rowData.displayPath || rowData.uri || ""
                        color: App.appFontColor
                        opacity: 0.45
                        font.pixelSize: Math.round(12 * App.uiScale)
                        elide: Text.ElideMiddle
                        width: parent.width
                    }
                }
                ChromeIcon {
                    visible: !root.selecting
                    anchors.verticalCenter: parent.verticalCenter
                    icon: "visibility_off"
                    hint: rowData.hidden ? "Show" : "Hide"
                    opacity: rowData.hidden ? 0.35 : 0.85
                    onClicked: root.folio.setFolioHidden([rowData.id], !rowData.hidden)
                }
                ChromeIcon {
                    visible: !root.selecting
                    anchors.verticalCenter: parent.verticalCenter
                    icon: "close"
                    hint: "Delete"
                    onClicked: root.folio.deleteFolioItems([rowData.id])
                }
            }
        }
    }
}
