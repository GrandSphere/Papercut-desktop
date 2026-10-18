import QtQuick
import QtQuick.Controls.Basic
import Papercut 1.0

PapercutDialog {
    id: root
    property color initial: "#B3B3B3"
    property real hue: 0
    property real sat: 0
    property real val: 0
    readonly property color current: Qt.hsva(hue / 360, sat, val, 1)
    signal picked(color c)

    title: "Colour"
    confirmText: "OK"

    function hex(c) {
        const part = v => ("0" + Math.round(v * 255).toString(16)).slice(-2).toUpperCase()
        return "#" + part(c.r) + part(c.g) + part(c.b)
    }
    function load(c) {
        // Grey: hsvHue -1; keep hue 0 (colorToHSV uses 0).
        hue = c.hsvHue < 0 ? 0 : c.hsvHue * 360
        sat = c.hsvSaturation
        val = c.hsvValue
        hexField.text = hex(root.current)
    }
    function fromPicker(h, s, v) {
        hue = h
        sat = s
        val = v
        hexField.text = hex(root.current)
    }

    onOpened: load(initial)
    onAccepted: picked(Qt.rgba(current.r, current.g, current.b, 1))

    Rectangle {
        width: 36; height: 36; radius: 18
        color: root.current
        border.color: "#5F6368"
    }

    Item {
        id: sv
        width: parent.width
        height: 160
        Rectangle {
            anchors.fill: parent
            gradient: Gradient {
                orientation: Gradient.Horizontal
                GradientStop { position: 0; color: "#FFFFFF" }
                GradientStop { position: 1; color: Qt.hsva(root.hue / 360, 1, 1, 1) }
            }
        }
        Rectangle {
            anchors.fill: parent
            gradient: Gradient {
                GradientStop { position: 0; color: "#00000000" }
                GradientStop { position: 1; color: "#FF000000" }
            }
        }
        Rectangle {
            x: root.sat * sv.width - 8
            y: (1 - root.val) * sv.height - 8
            width: 16; height: 16; radius: 8
            color: "white"
            border.color: "black"
            border.width: 2
        }
        MouseArea {
            anchors.fill: parent
            preventStealing: true
            function pick(mouse) {
                root.fromPicker(root.hue, Math.max(0, Math.min(1, mouse.x / width)), 1 - Math.max(0, Math.min(1, mouse.y / height)))
            }
            onPressed: mouse => pick(mouse)
            onPositionChanged: mouse => pick(mouse)
        }
    }

    Item {
        id: hueBar
        width: parent.width
        height: 24
        Rectangle {
            anchors.fill: parent
            gradient: Gradient {
                orientation: Gradient.Horizontal
                GradientStop { position: 0.000; color: "#FF0000" }
                GradientStop { position: 0.167; color: "#FFFF00" }
                GradientStop { position: 0.333; color: "#00FF00" }
                GradientStop { position: 0.500; color: "#00FFFF" }
                GradientStop { position: 0.667; color: "#0000FF" }
                GradientStop { position: 0.833; color: "#FF00FF" }
                GradientStop { position: 1.000; color: "#FF0000" }
            }
        }
        Rectangle {
            x: root.hue / 360 * hueBar.width - 10
            anchors.verticalCenter: parent.verticalCenter
            width: 20; height: 20; radius: 10
            color: "white"
            border.color: "black"
            border.width: 2
        }
        MouseArea {
            anchors.fill: parent
            preventStealing: true
            function pick(mouse) {
                root.fromPicker(Math.max(0, Math.min(1, mouse.x / width)) * 360, root.sat, root.val)
            }
            onPressed: mouse => pick(mouse)
            onPositionChanged: mouse => pick(mouse)
        }
    }

    PapercutField {
        id: hexField
        width: parent.width
        color: App.appFontColor
        font.capitalization: Font.AllUppercase
        selectByMouse: true
        onTextEdited: {
            let t = text.trim().replace(/^#|^0x/i, "")
            if (!/^([0-9a-fA-F]{6}|[0-9a-fA-F]{8})$/.test(t)) return
            const c = Qt.color("#" + t)
            root.hue = c.hsvHue < 0 ? root.hue : c.hsvHue * 360
            root.sat = c.hsvSaturation
            root.val = c.hsvValue
        }
        onAccepted: { root.accepted(); root.close() }
    }
}
