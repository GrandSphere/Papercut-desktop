import QtQuick
import Papercut 1.0

PapercutOverlay {
    id: root
    property var combiner
    property var onAdd: function() {}
    title: "Combine"

    ReorderableList {
        anchors { fill: parent; bottomMargin: 56 }
        model: root.combiner ? root.combiner.entries : []
        rowHeight: 56
        dragMode: "move"
        onMoved: (from, to) => root.combiner.move(from, to)
        rowDelegate: Item {
            property var rowData
            property int rowIndex
            Row {
                anchors { fill: parent; leftMargin: 12 }
                AppText {
                    width: 42
                    anchors.verticalCenter: parent.verticalCenter
                    text: rowIndex + 1
                    color: App.appFontColor
                    opacity: 0.45
                    font.pixelSize: Math.round(16 * App.uiScale)
                }
                Column {
                    width: parent.width - 90
                    anchors.verticalCenter: parent.verticalCenter
                    AppText { text: rowData.displayName || ""; color: App.appFontColor; opacity: 0.7; font.pixelSize: Math.round(16 * App.uiScale); elide: Text.ElideRight; width: parent.width }
                    AppText { visible: !!rowData.displayPath; text: rowData.displayPath || ""; color: App.appFontColor; opacity: 0.45; font.pixelSize: Math.round(12 * App.uiScale); elide: Text.ElideRight; width: parent.width }
                }
                ChromeIcon { icon: "close"; hint: "Remove"; onClicked: root.combiner.removeAt(rowIndex) }
            }
        }
    }
    Row {
        anchors { bottom: parent.bottom; horizontalCenter: parent.horizontalCenter; bottomMargin: 8 }
        ChromeIcon { icon: "add"; hint: "Add"; onClicked: root.onAdd() }
        ChromeIcon { icon: "share"; hint: "Share"; enabled: !!root.combiner && root.combiner.entries.length > 0 && root.combiner.busy.length === 0; onClicked: root.combiner.combine() }
        ChromeIcon { icon: "save"; hint: "Save"; enabled: !!root.combiner && root.combiner.entries.length > 0 && root.combiner.busy.length === 0; onClicked: root.combiner.combine() }
    }
}
