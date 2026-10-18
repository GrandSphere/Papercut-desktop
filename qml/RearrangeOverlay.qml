import QtQuick
import Papercut 1.0

PapercutOverlay {
    id: root
    property var ctl
    property var local: []
    title: "Rearrange pages"

    onOverlayVisibleChanged: if (overlayVisible) local = ctl.selectedPages.slice()

    function publish(next) {
        local = next
        ctl.setSelectedPages(next)
        if (next.length === 0) dismiss() // dismiss when selection empty
    }

    ReorderableList {
        anchors.fill: parent
        model: root.local
        rowHeight: 48
        dragMode: "move"
        onMoved: (from, to) => {
            const next = root.local.slice()
            next.splice(to, 0, next.splice(from, 1)[0])
            root.publish(next)
        }
        rowDelegate: Item {
            property var rowData
            property int rowIndex
            Row {
                anchors { fill: parent; leftMargin: 12; rightMargin: 12 }
                AppText {
                    width: 42
                    anchors.verticalCenter: parent.verticalCenter
                    text: rowIndex + 1
                    color: App.appFontColor
                    opacity: 0.45
                    font.pixelSize: Math.round(16 * App.uiScale)
                }
                AppText {
                    width: parent.width - 90
                    anchors.verticalCenter: parent.verticalCenter
                    text: "page " + (rowData + 1)
                    color: App.appFontColor
                    opacity: 0.7
                    font.pixelSize: Math.round(16 * App.uiScale)
                }
                ChromeIcon {
                    icon: "close"
                    hint: "Remove"
                    onClicked: {
                        const next = root.local.slice()
                        next.splice(rowIndex, 1)
                        root.publish(next)
                    }
                }
            }
        }
    }
}
