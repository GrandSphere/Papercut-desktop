import QtQuick
import QtQuick.Controls.Basic
import Papercut 1.0

Item {
    id: root
    property string icon
    property string hint
    property string longHint
    property bool selected: false
    property bool struck: false
    signal clicked
    signal longClicked

    width: 48
    height: 48
    opacity: 1

    Icon {
        anchors.centerIn: parent
        name: root.icon
        struck: root.struck
        color: App.appFontColor
    }
    MouseArea {
        id: area
        anchors.fill: parent
        hoverEnabled: true
        cursorShape: Qt.PointingHandCursor
        onClicked: root.clicked()
        onPressAndHold: {
            root.longClicked()
            const t = root.longHint.length ? root.longHint : root.hint
            if (t.length > 0) ToolTip.show(t, 1500)
        }
    }
}
