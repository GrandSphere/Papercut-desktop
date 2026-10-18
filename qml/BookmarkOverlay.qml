import QtQuick
import QtQuick.Controls.Basic
import Papercut 1.0

PapercutOverlay {
    id: root
    property var ctl
    property bool managing: false
    property bool selecting: false
    property var selected: ({})
    property var editTarget: ({})
    property var local: ctl ? ctl.bookmarks : []

    title: selecting ? selectedCount() + " selected" : managing ? "Manage bookmarks" : "Bookmarks"

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
        if (managing) { managing = false; return }
        dismiss()
    }
    function pageTop(pages, index) {
        let y = 0
        const last = Math.max(0, Math.min(pages.length, index))
        for (let i = 0; i < last; i++) y += pages[i].height
        return y
    }
    function formatPercent(v) {
        const r = Math.round(v * 10) / 10
        return String(r)
    }
    function openEditor(bookmark) {
        const pages = ctl.pageSizes()
        const page = pages[bookmark.pageIndex]
        const w = page && page.width > 0 ? page.width : 1
        const h = page && page.height > 0 ? page.height : 1
        editTarget = bookmark
        editName.text = bookmark.name
        editPage.text = String(bookmark.pageIndex + 1)
        editH.text = formatPercent(100 * bookmark.scrollXPdf / w)
        editV.text = formatPercent(100 * (bookmark.scrollYPdf - pageTop(pages, bookmark.pageIndex)) / h)
        editDialog.open()
    }
    function saveQuickBookmark(name) {
        const p = ctl.readingPosition()
        ctl.addBookmark(name, p.page, p.scrollX, p.scrollY)
    }

    onDismissed: { managing = false; selecting = false; selected = {} }

    // Back: exit selection, then manage mode, then close.
    interceptBack: true
    onBackRequested: exitModes()

    ChromeIcon {
        parent: root.actionHost
        visible: managing || selecting
        icon: selecting ? (selectedCount() === local.length && local.length > 0 ? "check_box_outline_blank" : "select_all")
                        : "check_box_outline_blank"
        hint: selecting ? "Select all" : "Select"
        onClicked: {
            if (!selecting) { selecting = true; return }
            if (selectedCount() === local.length) selected = {}
            else {
                const next = {}
                for (let i = 0; i < local.length; i++) next[local[i].id] = true
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
        onClicked: { ctl.deleteBookmarks(selectedIds()); selected = {}; selecting = false }
    }

    ReorderableList {
        anchors.fill: parent
        model: root.local
        rowHeight: 52
        dragMode: root.managing && !root.selecting ? "hold" : "none"
        onClicked: index => {
            const item = root.local[index]
            if (root.selecting) root.toggleId(item.id)
            else if (root.managing) root.openEditor(item)
            else { root.ctl.jumpBookmark(item); root.dismiss() }
        }
        onMoved: (from, to) => {
            const ids = root.local.map(b => b.id)
            ids.splice(to, 0, ids.splice(from, 1)[0])
            root.ctl.reorderBookmarks(ids)
        }
        rowDelegate: Item {
            property var rowData
            property int rowIndex
            Row {
                anchors { fill: parent; leftMargin: 12; rightMargin: 12 }
                spacing: 8
                ChromeIcon {
                    visible: root.selecting
                    width: 40; height: 40
                    anchors.verticalCenter: parent.verticalCenter
                    icon: root.selected[rowData.id] ? "check_box" : "check_box_outline_blank"
                    onClicked: root.toggleId(rowData.id)
                }
                AppText {
                    width: 22
                    anchors.verticalCenter: parent.verticalCenter
                    text: rowIndex + 1
                    color: App.appFontColor
                    opacity: 0.45
                    font.pixelSize: Math.round(16 * App.uiScale)
                }
                Column {
                    width: parent.width - 80 - (root.selecting ? 48 : 0)
                    anchors.verticalCenter: parent.verticalCenter
                    AppText { text: rowData.name || ""; color: App.appFontColor; opacity: 0.7; font.pixelSize: Math.round(16 * App.uiScale); elide: Text.ElideRight; width: parent.width }
                    AppText { text: "Page " + ((rowData.pageIndex || 0) + 1); color: App.appFontColor; opacity: 0.45; font.pixelSize: Math.round(12 * App.uiScale) }
                }
                ChromeIcon {
                    visible: root.managing && !root.selecting
                    anchors.verticalCenter: parent.verticalCenter
                    icon: "close"
                    hint: "Delete"
                    onClicked: root.ctl.deleteBookmarks([rowData.id])
                }
            }
        }
    }

    ChromeIcon {
        parent: root
        visible: !root.managing && !root.selecting
        icon: "edit"
        hint: "Manage"
        anchors { bottom: parent.bottom; horizontalCenter: parent.horizontalCenter; bottomMargin: 8; horizontalCenterOffset: -32 }
        onClicked: root.managing = true
    }
    ChromeIcon {
        parent: root
        visible: !root.managing && !root.selecting
        icon: "add"
        hint: "Add bookmark (long press to name it)"
        anchors { bottom: parent.bottom; horizontalCenter: parent.horizontalCenter; bottomMargin: 8; horizontalCenterOffset: 32 }
        onClicked: root.saveQuickBookmark("Quick Bookmark")
        onLongClicked: { nameField.text = ""; nameDialog.open() }
    }

    PapercutDialog {
        id: nameDialog
        title: "Bookmark"
        confirmText: "Save"
        onOpened: nameField.forceActiveFocus()
        onAccepted: root.saveQuickBookmark(nameField.text.trim() || "Quick Bookmark")
        AppText { text: "Page " + ((root.ctl ? root.ctl.readingPosition().page : 0) + 1); color: App.appFontColor; opacity: 0.45 }
        PapercutField {
            id: nameField
            width: parent.width
            placeholderText: "Name"
            color: App.appFontColor
            onAccepted: { nameDialog.close(); nameDialog.accepted() }
        }
    }
    PapercutDialog {
        id: editDialog
        title: "Edit bookmark"
        confirmText: "Save"
        onAccepted: {
            const pages = root.ctl.pageSizes()
            const pageIndex = Math.max(0, Math.min(Math.max(0, pages.length - 1), (parseInt(editPage.text) || (root.editTarget.pageIndex + 1)) - 1))
            const page = pages[pageIndex]
            const w = page && page.width > 0 ? page.width : 1
            const h = page && page.height > 0 ? page.height : 1
            const next = Object.assign({}, root.editTarget)
            next.name = editName.text.trim() || "Quick Bookmark"
            next.pageIndex = pageIndex
            next.scrollXPdf = (parseFloat(editH.text) || 0) / 100 * w
            next.scrollYPdf = root.pageTop(pages, pageIndex) + (parseFloat(editV.text) || 0) / 100 * h
            root.ctl.updateBookmark(next)
        }
        PapercutField { id: editName; width: parent.width; placeholderText: "Name"; color: App.appFontColor }
        PapercutField { id: editPage; width: parent.width; placeholderText: "Page"; color: App.appFontColor; inputMethodHints: Qt.ImhDigitsOnly; validator: IntValidator { bottom: 1; top: 999999 } }
        PapercutField { id: editH; width: parent.width; placeholderText: "Horizontal %"; color: App.appFontColor; inputMethodHints: Qt.ImhFormattedNumbersOnly }
        PapercutField { id: editV; width: parent.width; placeholderText: "Vertical %"; color: App.appFontColor; inputMethodHints: Qt.ImhFormattedNumbersOnly }
    }
}
