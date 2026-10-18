import QtQuick
import QtQuick.Controls.Basic
import Papercut 1.0

PapercutOverlay {
    id: root
    title: capturing ? "Press a key" : "Shortcuts"
    property string captureId: ""
    property int captureSlot: 0
    property string pendingModifier: ""
    property bool filterOpen: false
    property string filterText: ""
    readonly property bool capturing: Keybinds.capturing
    readonly property int bindChipWidth: Math.ceil(chipMeasure.implicitWidth) + 28
    readonly property int labelColumnWidth: {
        const _ = App.uiScale
        const all = Keybinds.actions
        let max = 80
        for (let i = 0; i < all.length; i++) {
            labelMeasure.text = all[i].label
            max = Math.max(max, Math.ceil(labelMeasure.implicitWidth))
        }
        return max
    }
    // Snapshot groups on open so rebind does not reset CollapsibleSection.
    property var groupLayout: []
    readonly property var filtered: {
        const _ = Keybinds.actions
        const __ = root.filterText
        return root.filteredActions()
    }
    // Focus only while capturing; overlayVisible focus steals filter typing.
    focus: capturing

    function cancelCapture() { pendingModifier = ""; Keybinds.capturing = false }
    function startCapture(id, slot) {
        captureId = id
        captureSlot = slot
        pendingModifier = ""
        Keybinds.capturing = true
        forceActiveFocus()
    }
    function cmpLabel(a, b) {
        const la = a && a.label ? String(a.label) : ""
        const lb = b && b.label ? String(b.label) : ""
        return la.localeCompare(lb, undefined, { numeric: true, sensitivity: "base" })
    }
    function actionById(id) {
        const all = Keybinds.actions
        for (let i = 0; i < all.length; i++) if (all[i].id === id) return all[i]
        return { id: id, label: id, primary: "", secondary: "", primaryDisplay: "None", secondaryDisplay: "None" }
    }
    function filteredActions() {
        const all = Keybinds.actions
        const out = []
        for (let i = 0; i < all.length; i++) {
            const a = all[i]
            const hay = [a.label, a.group, a.primaryDisplay, a.secondaryDisplay, a.id].join(" ")
            if (Keybinds.matchesFilter(hay, filterText) || Keybinds.matchesFilter(a.label, filterText)) out.push(a)
        }
        out.sort((a, b) => root.cmpLabel(a, b))
        return out
    }
    function snapshotGroups() {
        const all = Keybinds.actions
        const groups = []
        const byName = {}
        for (let i = 0; i < all.length; i++) {
            const a = all[i]
            const g = a.group || ""
            if (!byName[g]) {
                byName[g] = { title: g, ids: [] }
                groups.push(byName[g])
            }
            byName[g].ids.push(a.id)
        }
        for (let i = 0; i < groups.length; i++) {
            groups[i].ids.sort((id1, id2) => root.cmpLabel(root.actionById(id1), root.actionById(id2)))
        }
        groupLayout = groups
    }

    AppText {
        id: chipMeasure
        visible: false
        text: "Ctrl + Shift + Mouse"
        font.pixelSize: Math.round(13 * App.uiScale)
    }
    AppText {
        id: labelMeasure
        visible: false
        font.pixelSize: Math.round(16 * App.uiScale)
    }

    onOverlayVisibleChanged: {
        if (overlayVisible) {
            Keybinds.beginEdit()
            snapshotGroups()
            filterOpen = false
            filterText = ""
            filterField.text = ""
        } else {
            cancelCapture()
            Keybinds.revert()
        }
    }
    onDismissed: {
        cancelCapture()
        Keybinds.revert()
    }
    onBackRequested: {
        if (capturing) cancelCapture()
        else dismiss()
    }
    interceptBack: capturing

    Keys.onPressed: event => {
        if (!capturing) return
        event.accepted = true
        if (event.key === Qt.Key_Escape) { cancelCapture(); return }
        const seq = Keybinds.sequenceFromKey(event.key, event.modifiers)
        if (seq.length) {
            Keybinds.setBinding(captureId, captureSlot, seq)
            cancelCapture()
            return
        }
        // Held modifier: wait for release so chord records correctly.
        pendingModifier = captureId === "grabWindow" ? Keybinds.modifierSequence(event.key) : ""
    }
    Keys.onReleased: event => {
        if (!capturing) return
        event.accepted = true
        if (captureId !== "grabWindow" || !pendingModifier.length) return
        if (Keybinds.modifierSequence(event.key) !== pendingModifier) return
        Keybinds.setBinding(captureId, captureSlot, pendingModifier)
        cancelCapture()
    }

    WheelHandler {
        enabled: capturing
        acceptedDevices: PointerDevice.Mouse | PointerDevice.TouchPad
        onWheel: event => {
            if (event.angleDelta.y === 0 && event.pixelDelta.y === 0) return
            const up = (event.angleDelta.y !== 0 ? event.angleDelta.y : event.pixelDelta.y) > 0
            Keybinds.setBinding(captureId, captureSlot, Keybinds.sequenceFromWheel(event.modifiers, up))
            cancelCapture()
            event.accepted = true
        }
    }

    Row {
        parent: root.actionHost
        spacing: 4
        layoutDirection: Qt.RightToLeft
        ChromeIcon {
            icon: "save"
            hint: "Save shortcuts"
            onClicked: {
                Keybinds.commit()
                Keybinds.beginEdit()
            }
        }
        ChromeIcon {
            icon: "search"
            hint: "Filter"
            onClicked: {
                filterOpen = !filterOpen
                if (filterOpen) {
                    filterField.forceActiveFocus()
                    filterField.selectAll()
                } else {
                    filterField.text = ""
                    filterText = ""
                }
            }
        }
        ChromeIcon {
            icon: "restart_alt"
            hint: "Reset"
            onClicked: resetDialog.open()
        }
    }

    Column {
        anchors.fill: parent
        Item {
            visible: root.filterOpen
            width: parent.width
            height: visible ? filterField.height + 16 : 0
            PapercutField {
                id: filterField
                x: 16
                y: 16
                width: parent.width - 32
                placeholderText: "Filter shortcuts"
                color: App.appFontColor
                font.pixelSize: Math.round(16 * App.uiScale)
                onTextChanged: root.filterText = text
            }
        }
        Flickable {
            width: parent.width
            height: parent.height - (filterField.visible ? filterField.height + 16 : 0)
            contentWidth: width
            contentHeight: col.implicitHeight + 48
            clip: true
            boundsBehavior: Flickable.StopAtBounds
            ScrollBar.vertical: ScrollBar {}

            Column {
                id: col
                x: 16
                y: 16
                width: parent.width - 32
                spacing: 10

                Repeater {
                    model: root.filterText.length > 0 ? root.filtered : []
                    delegate: ShortcutRow {
                        width: col.width
                        item: modelData
                    }
                }
                Repeater {
                    model: root.filterText.length > 0 ? [] : root.groupLayout
                    delegate: CollapsibleSection {
                        width: col.width
                        title: modelData.title
                        Repeater {
                            model: modelData.ids
                            delegate: ShortcutRow {
                                width: parent.width
                                item: {
                                    const _ = Keybinds.actions
                                    return root.actionById(modelData)
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    component ShortcutRow: Item {
        id: row
        property var item
        height: 40
        AppText {
            id: lab
            anchors { left: parent.left; verticalCenter: parent.verticalCenter }
            width: root.labelColumnWidth
            text: row.item.label
            color: App.appFontColor
            opacity: 0.7
            font.pixelSize: Math.round(16 * App.uiScale)
            elide: Text.ElideRight
        }
        Row {
            anchors { left: lab.right; leftMargin: 16; verticalCenter: parent.verticalCenter }
            spacing: 8
            BindChip {
                seq: row.item.primary
                display: row.item.primaryDisplay
                active: capturing && captureId === row.item.id && captureSlot === 0
                onClicked: startCapture(row.item.id, 0)
                onClear: Keybinds.clearBinding(row.item.id, 0)
            }
            BindChip {
                seq: row.item.secondary
                display: row.item.secondaryDisplay
                active: capturing && captureId === row.item.id && captureSlot === 1
                onClicked: startCapture(row.item.id, 1)
                onClear: Keybinds.clearBinding(row.item.id, 1)
            }
        }
    }

    component BindChip: Rectangle {
        id: chip
        property string seq
        property string display
        property bool active
        signal clicked
        signal clear
        width: root.bindChipWidth
        height: 32
        radius: 8
        color: active ? Qt.rgba(App.actionColor.r, App.actionColor.g, App.actionColor.b, 0.18) : "#332A2A2E"
        border.width: active ? 1 : 0
        border.color: App.actionColor
        MouseArea {
            anchors.fill: parent
            cursorShape: Qt.PointingHandCursor
            onClicked: chip.clicked()
        }
        AppText {
            id: lab
            anchors { left: parent.left; leftMargin: 8; verticalCenter: parent.verticalCenter; right: xBtn.visible ? xBtn.left : parent.right; rightMargin: 8 }
            text: chip.active ? "…" : chip.display
            color: App.appFontColor
            font.pixelSize: Math.round(13 * App.uiScale)
            elide: Text.ElideRight
            horizontalAlignment: Text.AlignHCenter
        }
        AppText {
            id: xBtn
            visible: chip.seq.length > 0 && !chip.active
            anchors { right: parent.right; rightMargin: 6; verticalCenter: parent.verticalCenter }
            text: "×"
            color: App.appFontColor
            opacity: 0.45
            font.pixelSize: Math.round(14 * App.uiScale)
            MouseArea { anchors.fill: parent; anchors.margins: -6; onClicked: mouse => { mouse.accepted = true; chip.clear() } }
        }
    }

    PapercutDialog {
        id: resetDialog
        title: ""
        confirmText: "Reset"
        onAccepted: {
            Keybinds.resetDefaults()
            root.snapshotGroups()
        }
        AppText {
            width: parent.width
            wrapMode: Text.Wrap
            color: App.appFontColor
            text: "Reset all shortcuts to default?"
        }
    }
}
