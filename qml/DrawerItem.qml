import QtQuick
import Papercut 1.0

Item {
    id: root
    property string label
    signal clicked
    width: parent ? parent.width : 280
    height: 48
    opacity: enabled ? 1 : 0.35
    AppText {
        anchors { left: parent.left; leftMargin: 16; verticalCenter: parent.verticalCenter; right: parent.right; rightMargin: 16 }
        text: root.label
        color: App.appFontColor
        font.pixelSize: Math.round(16 * App.uiScale)
    }
    MouseArea { anchors.fill: parent; enabled: root.enabled; onClicked: root.clicked() }
}
