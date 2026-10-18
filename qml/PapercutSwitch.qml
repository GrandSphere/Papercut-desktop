import QtQuick
import QtQuick.Controls.Basic
import Papercut 1.0

Switch {
    id: sw
    indicator: Rectangle {
        implicitWidth: 52
        implicitHeight: 32
        x: sw.leftPadding
        y: parent.height / 2 - height / 2
        radius: 16
        color: sw.checked ? App.actionColor : "#2A2A2E"
        border.color: sw.checked ? "transparent" : "#5F6368"
        Rectangle {
            x: sw.checked ? parent.width - width - 4 : 4
            width: sw.checked ? 24 : 16
            height: width
            anchors.verticalCenter: parent.verticalCenter
            radius: width / 2
            color: Qt.rgba(App.appFontColor.r, App.appFontColor.g, App.appFontColor.b, 0.4)
            Behavior on x { NumberAnimation { duration: 90 } }
        }
    }
}
