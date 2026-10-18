import QtQuick
import Papercut 1.0

Rectangle {
    id: root
    property string text
    property string fontFamily
    property bool selected: false
    signal clicked

    implicitWidth: label.implicitWidth + 32
    implicitHeight: 32
    radius: 8
    color: selected ? (enabled ? App.actionColor : Qt.rgba(App.actionColor.r, App.actionColor.g, App.actionColor.b, 0.4)) : "transparent"
    border.color: "#5F6368"
    border.width: selected ? 0 : 1

    AppText {
        id: label
        anchors.centerIn: parent
        text: root.text
        color: App.appFontColor
        opacity: root.enabled ? 1 : 0.35
        font.family: root.fontFamily.length ? root.fontFamily : App.uiFontFamily
        font.weight: App.uiFontWeight
        font.pixelSize: Math.round(14 * App.uiScale)
    }
    MouseArea {
        anchors.fill: parent
        enabled: root.enabled
        cursorShape: Qt.PointingHandCursor
        onClicked: root.clicked()
    }
}
