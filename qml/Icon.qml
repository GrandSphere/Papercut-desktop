import QtQuick
import QtQuick.Shapes
import Papercut 1.0

Item {
    id: root
    property string name
    property color color: App.appFontColor
    property real size: 24
    property bool struck: false
    readonly property string liga: {
        if (name === "bookmark_border") return "bookmark"
        if (name === "expand_less") return "keyboard_arrow_up"
        if (name === "expand_more") return "keyboard_arrow_down"
        if (name === "width_full") return "fit_width"
        if (name === "save") return "check"
        if (name === "edit") return "edit_note"
        return name
    }
    width: size
    height: size

    Text {
        anchors.fill: parent
        text: root.liga
        color: root.color
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
        font.family: App.iconFontFamily
        font.pixelSize: root.size
        font.weight: 300
        font.hintingPreference: Font.PreferFullHinting
        font.features: { "rlig": 1, "liga": 1 }
    }
    Shape {
        visible: root.struck
        anchors.fill: parent
        preferredRendererType: Shape.CurveRenderer
        ShapePath {
            strokeColor: root.color
            strokeWidth: 1.5
            fillColor: "transparent"
            capStyle: ShapePath.RoundCap
            startX: 4
            startY: 4
            PathLine { x: root.size - 4; y: root.size - 4 }
        }
    }
}
