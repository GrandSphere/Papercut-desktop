import QtQuick
import QtQuick.Controls.Basic
import Papercut 1.0

TextField {
    id: root
    font.family: App.uiFontFamily
    font.weight: App.uiFontWeight
    color: App.appFontColor
    placeholderTextColor: Qt.rgba(App.appFontColor.r, App.appFontColor.g, App.appFontColor.b, 0.45)
    selectionColor: App.actionColor
    selectedTextColor: App.bgColor
    selectByMouse: true
    leftPadding: 12
    rightPadding: 12
    topPadding: 10
    bottomPadding: 10
    background: Rectangle {
        color: "transparent"
        radius: 8
        border.width: 1
        border.color: Qt.rgba(App.appFontColor.r, App.appFontColor.g, App.appFontColor.b, 0.4)
    }
}
