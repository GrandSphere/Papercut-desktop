import QtQuick
import QtQuick.Shapes
import Papercut 1.0

Item {
    id: root
    property real size: 72
    property color color: App.appFontColor
    width: size
    height: size

    component Stroke: ShapePath {
        strokeColor: root.color
        strokeWidth: 1.05
        fillColor: "transparent"
        capStyle: ShapePath.RoundCap
        joinStyle: ShapePath.RoundJoin
    }

    Shape {
        width: 48
        height: 48
        preferredRendererType: Shape.CurveRenderer
        transform: Scale { xScale: root.size / 48; yScale: root.size / 48 }
        Stroke {
            PathSvg { path: "M9,9.1 A4.7,4.7 0 0,1 13.7,4.4 H30 L38.6,13 V19.7 A9,9 0 0,1 29.6,28.7 H18" }
        }
        Stroke {
            PathSvg { path: "M9,9.1 V41.9 A1.5,1.5 0 0,0 10.5,43.4 H11.5 L18,36.5" }
        }
        Stroke {
            PathSvg { path: "M18,13 V36.5" }
        }
        Stroke {
            PathSvg { path: "M30,4.4 V10.7 A2.3,2.3 0 0,0 32.3,13 H38.6" }
        }
        Stroke {
            PathSvg { path: "M18,36.5 H13.8 A2.3,2.3 0 0,0 11.5,38.8 V43.4" }
        }
    }
}
