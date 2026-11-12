import QtQuick
import Papercut 1.0

Item {
    id: root
    property string label
    property string value: ""
    property bool shareVisible: false
    signal clicked
    signal shareClicked

    width: parent ? parent.width : 300
    height: 44
    opacity: enabled ? 1 : 0.5

    AppText {
        id: labelText
        anchors { left: parent.left; verticalCenter: parent.verticalCenter }
        text: root.label
        color: App.appFontColor
        opacity: root.enabled ? 0.7 : 0.5
        font.pixelSize: Math.round(16 * App.uiScale)
    }
    MouseArea {
        enabled: root.enabled
        anchors { left: parent.left; top: parent.top; bottom: parent.bottom; right: valueText.left }
        onClicked: root.clicked()
    }
    AppText {
        id: valueText
        visible: root.value.length > 0
        anchors { right: shareIcon.visible ? shareIcon.left : parent.right; rightMargin: shareIcon.visible ? 8 : 0; verticalCenter: parent.verticalCenter }
        text: root.value
        color: App.appFontColor
        font.pixelSize: Math.round(16 * App.uiScale)
        MouseArea { enabled: root.enabled; anchors.fill: parent; onClicked: root.clicked() }
    }
    Icon {
        id: shareIcon
        visible: root.shareVisible
        anchors { right: parent.right; verticalCenter: parent.verticalCenter }
        name: "share"
        size: 18
        MouseArea { enabled: root.enabled; anchors { fill: parent; margins: -8 } onClicked: root.shareClicked() }
    }
}
