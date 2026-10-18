import QtQuick
import Papercut 1.0

Rectangle {
    id: root
    property string title
    property string hint
    property bool expanded: false
    default property alias content: inner.data

    width: parent ? parent.width : 300
    height: column.implicitHeight + 8
    radius: 20
    color: "#662A2A2E"

    Column {
        id: column
        x: 16
        y: 4
        width: parent.width - 32

        Item {
            width: parent.width
            height: 48
            AppText {
                anchors { left: parent.left; right: arrow.left; verticalCenter: parent.verticalCenter }
                text: root.title
                color: App.appFontColor
                font.pixelSize: Math.round(18 * App.uiScale)
                elide: Text.ElideRight
            }
            Icon {
                id: arrow
                anchors { right: parent.right; verticalCenter: parent.verticalCenter }
                name: root.expanded ? "expand_less" : "expand_more"
                size: 24
            }
            MouseArea {
                anchors.fill: parent
                acceptedButtons: Qt.LeftButton | Qt.RightButton
                onClicked: mouse => { if (mouse.button === Qt.RightButton) hintDialog.open(); else root.expanded = !root.expanded }
                onPressAndHold: hintDialog.open()
            }
        }
        Column {
            id: inner
            visible: root.expanded
            width: parent.width
            bottomPadding: root.expanded ? 12 : 0
            spacing: 8
        }
    }

    PapercutDialog {
        id: hintDialog
        title: root.title
        confirmText: "OK"
        dismissText: ""
        AppText { text: root.hint; color: App.appFontColor; wrapMode: Text.Wrap; width: parent.width }
    }
}
