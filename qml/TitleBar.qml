import QtQuick
import QtQuick.Controls.Basic
import Papercut 1.0

Rectangle {
    id: root
    objectName: "titleBarItem"
    property Window window
    property string title: "Papercut"
    property bool allowMove: true
    height: 36
    color: App.bgColor

    readonly property bool maximized: window && window.windowZoomed()
    signal maximizeRequested

    function toggleMax() { maximizeRequested() }

    MouseArea {
        anchors { left: parent.left; right: buttons.left; top: parent.top; bottom: parent.bottom }
        acceptedButtons: Qt.LeftButton
        onPressed: mouse => {
            if (!root.allowMove || !window) return
            const g = mapToGlobal(mouse.x, mouse.y)
            if (window.windowZoomed())
                window.beginSystemMoveFromZoomed(g.x, g.y)
            else
                window.startSystemMove()
        }
        onDoubleClicked: root.toggleMax()
    }

    AppText {
        anchors {
            left: parent.left
            leftMargin: 12
            right: buttons.left
            rightMargin: 8
            verticalCenter: parent.verticalCenter
        }
        text: root.title
        color: App.appFontColor
        font.pixelSize: Math.round(13 * App.uiScale)
        elide: Text.ElideRight
    }

    Row {
        id: buttons
        z: 1
        anchors { right: parent.right; top: parent.top; bottom: parent.bottom }

        CaptionButton {
            glyph: "minimize"
            hint: "Minimize"
            onClicked: if (window) window.showMinimized()
        }
        CaptionButton {
            glyph: root.maximized ? "restore" : "maximize"
            hint: root.maximized ? "Restore" : "Maximize"
            onClicked: root.toggleMax()
        }
        CaptionButton {
            glyph: "close"
            hint: "Close"
            hoverColor: "#C42B1C"
            hoverIconColor: "#FFFFFF"
            onClicked: if (window) window.close()
        }
    }

    component CaptionGlyph: Canvas {
        id: g
        property string kind
        property color strokeColor: App.appFontColor
        width: 10
        height: 10
        onKindChanged: requestPaint()
        onStrokeColorChanged: requestPaint()
        Component.onCompleted: requestPaint()
        onPaint: {
            const ctx = getContext("2d")
            ctx.reset()
            ctx.strokeStyle = g.strokeColor
            ctx.fillStyle = g.strokeColor
            ctx.lineWidth = 1.2
            ctx.lineCap = "square"
            ctx.lineJoin = "miter"
            if (g.kind === "minimize") {
                ctx.fillRect(0, 4.2, 10, 1.6)
            } else if (g.kind === "maximize") {
                ctx.strokeRect(0.6, 0.6, 8.8, 8.8)
            } else if (g.kind === "restore") {
                ctx.strokeRect(2.8, 0.6, 6.6, 6.6)
                ctx.clearRect(0.2, 2.4, 7.4, 7.4)
                ctx.strokeRect(0.6, 2.8, 6.6, 6.6)
            } else if (g.kind === "close") {
                ctx.beginPath()
                ctx.moveTo(0.6, 0.6)
                ctx.lineTo(9.4, 9.4)
                ctx.moveTo(9.4, 0.6)
                ctx.lineTo(0.6, 9.4)
                ctx.stroke()
            }
        }
    }

    component CaptionButton: Item {
        id: btn
        property string glyph
        property string hint
        property color hoverColor: Qt.rgba(App.appFontColor.r, App.appFontColor.g, App.appFontColor.b, 0.12)
        property color hoverIconColor: App.appFontColor
        signal clicked
        width: 46
        height: root.height

        Rectangle {
            anchors.fill: parent
            color: area.containsMouse ? btn.hoverColor : "transparent"
        }
        CaptionGlyph {
            anchors.centerIn: parent
            kind: btn.glyph
            strokeColor: area.containsMouse ? btn.hoverIconColor : App.appFontColor
        }
        MouseArea {
            id: area
            anchors.fill: parent
            hoverEnabled: true
            cursorShape: Qt.PointingHandCursor
            onClicked: btn.clicked()
            onPressAndHold: if (btn.hint.length) ToolTip.show(btn.hint, 1500)
        }
    }
}
