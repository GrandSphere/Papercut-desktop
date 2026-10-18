import QtQuick
import Papercut 1.0

Item {
    id: root
    property real from: 0
    property real to: 1
    property real value: 0
    property real stepSize: 0
    readonly property bool pressed: area.pressed
    // value not written internally; binding survives drag; thumb follows pointer while pressed.
    property real dragValue: value
    readonly property real shown: pressed ? dragValue : value
    readonly property real fraction: Math.max(0, Math.min(1, (shown - from) / Math.max(0.0001, to - from)))
    readonly property color trackColor: root.enabled ? "#5C5C5C" : Qt.rgba(0.36, 0.36, 0.36, 0.38)

    signal moved(real value)
    signal finished

    implicitWidth: 200
    implicitHeight: 24
    width: parent ? parent.width : implicitWidth
    height: 24

    function valueAt(x) {
        const travel = Math.max(1, width - 12)
        const f = Math.max(0, Math.min(1, (x - 6) / travel))
        let v = from + f * (to - from)
        if (stepSize > 0) v = from + Math.round((v - from) / stepSize) * stepSize
        return Math.max(from, Math.min(to, v))
    }

    Rectangle {
        anchors { left: parent.left; right: parent.right; verticalCenter: parent.verticalCenter }
        height: 2
        radius: 1
        color: root.trackColor
    }
    Rectangle {
        x: (root.width - 12) * root.fraction
        anchors.verticalCenter: parent.verticalCenter
        width: 12; height: 12; radius: 6
        color: root.trackColor
    }
    MouseArea {
        id: area
        anchors.fill: parent
        enabled: root.enabled
        preventStealing: true
        onPressed: mouse => { root.dragValue = root.valueAt(mouse.x); root.moved(root.dragValue) }
        onPositionChanged: mouse => { if (pressed) { root.dragValue = root.valueAt(mouse.x); root.moved(root.dragValue) } }
        onReleased: root.finished()
        onCanceled: root.finished()
    }
}
