import QtQuick
import Papercut 1.0

Item {
    id: root
    property string label
    property color swatch
    property bool switchVisible: false
    property bool checked: false
    signal clicked
    signal longClicked
    signal toggled(bool on)

    width: parent ? parent.width : 300
    height: 48

    Rectangle {
        id: dot
        width: 36; height: 36; radius: 18
        color: root.swatch
        border.color: "#5F6368"
        anchors.verticalCenter: parent.verticalCenter
    }
    AppText {
        anchors { left: dot.right; leftMargin: 12; right: sw.visible ? sw.left : parent.right; verticalCenter: parent.verticalCenter }
        text: root.label
        color: App.appFontColor
        opacity: 0.7
        font.pixelSize: Math.round(16 * App.uiScale)
        elide: Text.ElideRight
    }
    MouseArea {
        anchors { left: parent.left; top: parent.top; bottom: parent.bottom; right: sw.visible ? sw.left : parent.right }
        acceptedButtons: Qt.LeftButton | Qt.RightButton
        onClicked: mouse => { if (mouse.button === Qt.RightButton) root.longClicked(); else root.clicked() }
        onPressAndHold: root.longClicked()
    }
    PapercutSwitch {
        id: sw
        visible: root.switchVisible
        anchors { right: parent.right; verticalCenter: parent.verticalCenter }
        checked: root.checked
        onToggled: root.toggled(checked)
    }
}
