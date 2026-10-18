import QtQuick
import QtQuick.Controls.Basic
import Papercut 1.0

Item {
    id: root

    property var model: []
    property real rowHeight: 52
    // dragMode: none | move | hold-then-drag
    property string dragMode: "none"
    property bool swipeEnabled: false
    property Component rowDelegate
    property real swipeCommitDistance: 48
    property alias bottomMargin: list.bottomMargin

    signal clicked(int index)
    signal moved(int from, int to)
    signal swiped(int index)

    readonly property bool busy: draggingIndex >= 0 || swipeIndex >= 0

    property int draggingIndex: -1
    property int insertIndex: -1
    property int swipeIndex: -1
    property real swipeX: 0
    property real grabY: 0
    property real fingerY: 0
    property real liftedTop: 0
    readonly property int ghostIndex: draggingIndex >= 0 ? draggingIndex : swipeIndex

    function shiftFor(i, from, to) {
        if (from < 0 || to < 0) return 0
        if (from < to && i > from && i <= to) return -rowHeight
        if (from > to && i >= to && i < from) return rowHeight
        return 0
    }

    function finish(commit) {
        const from = draggingIndex
        const to = insertIndex
        const swiped = swipeIndex
        const farEnough = swipeX > swipeCommitDistance
        draggingIndex = -1
        insertIndex = -1
        swipeIndex = -1
        swipeX = 0
        if (commit && from >= 0 && to >= 0 && from !== to) root.moved(from, to)
        if (commit && swiped >= 0 && farEnough) root.swiped(swiped)
    }

    ListView {
        id: list
        anchors.fill: parent
        clip: true
        interactive: !root.busy
        boundsBehavior: Flickable.StopAtBounds
        model: root.model
        ScrollBar.vertical: ScrollBar {}

        // New model array rebuilds delegates; restore scroll Y after reorder.
        property real keepY: 0
        onModelChanged: {
            keepY = contentY
            Qt.callLater(function() {
                list.contentY = Math.max(0, Math.min(list.keepY, Math.max(0, list.contentHeight - list.height)))
            })
        }

        delegate: Item {
            id: row
            width: ListView.view.width
            height: root.rowHeight
            opacity: root.ghostIndex === index ? 0 : 1
            transform: Translate { y: root.shiftFor(index, root.draggingIndex, root.insertIndex) }

            MouseArea {
                id: area
                anchors.fill: parent
                z: -1
                preventStealing: root.dragMode !== "none" || root.swipeEnabled
                property real pressX: 0
                property real pressY: 0
                property int axis: 0 // 0 undecided, 1 drag, 2 swipe
                property bool lifted: false

                function lift(mouse) {
                    axis = 1
                    lifted = true
                    root.grabY = pressY
                    root.liftedTop = row.y - list.contentY
                    root.draggingIndex = index
                    root.insertIndex = index
                    track(mouse)
                }
                function track(mouse) {
                    const p = area.mapToItem(root, mouse.x, mouse.y)
                    root.fingerY = p.y
                    const over = Math.floor((list.contentY + p.y) / root.rowHeight)
                    root.insertIndex = Math.max(0, Math.min(root.model.length - 1, over))
                }

                onPressed: mouse => { pressX = mouse.x; pressY = mouse.y; axis = 0; lifted = false }
                onPressAndHold: mouse => {
                    if (root.dragMode === "hold" && axis === 0) lift(mouse)
                }
                onPositionChanged: mouse => {
                    if (!pressed) return
                    const dx = mouse.x - pressX
                    const dy = mouse.y - pressY
                    if (axis === 0 && root.dragMode !== "hold") {
                        if (Math.abs(dx) < 8 && Math.abs(dy) < 8) return
                        if (root.swipeEnabled && Math.abs(dx) > Math.abs(dy)) {
                            axis = 2
                            lifted = true
                            root.liftedTop = row.y - list.contentY
                            root.swipeIndex = index
                        } else if (root.dragMode === "move") {
                            lift(mouse)
                        }
                    } else if (axis === 0 && root.swipeEnabled && (Math.abs(dx) > 8 && Math.abs(dx) > Math.abs(dy))) {
                        axis = 2
                        lifted = true
                        root.liftedTop = row.y - list.contentY
                        root.swipeIndex = index
                    }
                    if (axis === 1) track(mouse)
                    else if (axis === 2) root.swipeX = Math.max(0, dx)
                }
                onReleased: root.finish(true)
                onCanceled: root.finish(false)
                onClicked: { if (!lifted) root.clicked(index) }
            }

            Loader {
                anchors.fill: parent
                sourceComponent: root.rowDelegate
                onLoaded: {
                    item.rowData = Qt.binding(function() { return modelData })
                    item.rowIndex = Qt.binding(function() { return index })
                }
            }
        }
    }

    Item {
        id: ghost
        z: 5
        visible: root.ghostIndex >= 0
        width: root.width
        height: root.rowHeight
        x: root.swipeIndex >= 0 ? root.swipeX : 0
        y: root.swipeIndex >= 0 ? root.liftedTop
                                : Math.max(-root.rowHeight / 2, Math.min(root.height - root.rowHeight / 2, root.fingerY - root.grabY))
        scale: 1.03
        Rectangle { anchors.fill: parent; anchors.topMargin: 2; anchors.leftMargin: -2; anchors.rightMargin: -2; anchors.bottomMargin: -4; color: "#55000000"; radius: 4 }
        Rectangle { anchors.fill: parent; color: App.bgColor }
        Loader {
            anchors.fill: parent
            active: root.ghostIndex >= 0
            sourceComponent: root.rowDelegate
            onLoaded: {
                item.rowData = Qt.binding(function() { return root.ghostIndex >= 0 ? root.model[root.ghostIndex] : ({}) })
                item.rowIndex = Qt.binding(function() { return root.ghostIndex })
            }
        }
    }
}
