import QtQuick
import Papercut 1.0

Item {
    id: root
    property string label
    property bool checked
    signal toggled(bool on)
    signal longPressed

    width: parent ? parent.width : 300
    height: 48
    opacity: enabled ? 1 : 0.5

    AppText {
        anchors { left: parent.left; verticalCenter: parent.verticalCenter; right: sw.left; rightMargin: 8 }
        text: root.label
        color: App.appFontColor
        opacity: root.enabled ? 0.7 : 0.5
        font.pixelSize: Math.round(16 * App.uiScale)
        elide: Text.ElideRight
        MouseArea { anchors.fill: parent; onPressAndHold: root.longPressed() }
    }
    PapercutSwitch {
        id: sw
        anchors { right: parent.right; verticalCenter: parent.verticalCenter }
        checked: root.checked
        onToggled: root.toggled(checked)
    }
}
