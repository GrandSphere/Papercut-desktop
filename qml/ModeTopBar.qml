import QtQuick
import Papercut 1.0

Rectangle {
    id: root
    property string title
    signal back
    default property alias actions: actionRow.data
    readonly property alias actionHost: actionRow

    height: 56
    color: App.bgColor
    MouseArea { anchors.fill: parent } // interceptBarGestures

    ChromeIcon {
        id: backBtn
        icon: "arrow_back"
        hint: "Back"
        anchors { left: parent.left; leftMargin: 4; verticalCenter: parent.verticalCenter }
        onClicked: root.back()
    }
    AppText {
        anchors { left: backBtn.right; leftMargin: 4; right: actionRow.left; verticalCenter: parent.verticalCenter }
        text: root.title
        color: App.appFontColor
        font.pixelSize: Math.round(16 * App.uiScale)
        elide: Text.ElideRight
    }
    Row {
        id: actionRow
        anchors { right: parent.right; rightMargin: 4; verticalCenter: parent.verticalCenter }
    }
}
