import QtQuick
import Papercut 1.0

Column {
    id: root
    property int currentPage: 1
    property int pageCount: 1
    property bool labelEnabled: true
    property bool show: true
    signal changed(int page)
    signal finished
    signal labelClicked

    width: parent ? parent.width : 300
    visible: show
    height: show ? implicitHeight : 0
    spacing: 0

    AppText {
        anchors.horizontalCenter: parent.horizontalCenter
        text: root.currentPage + " / " + root.pageCount
        color: App.appFontColor
        opacity: root.enabled ? 0.85 : 0.35
        font.pixelSize: Math.round(12 * App.uiScale)
        topPadding: 4
        bottomPadding: 4
        MouseArea {
            anchors.fill: parent
            anchors.margins: -8
            enabled: root.enabled && root.labelEnabled
            onClicked: root.labelClicked()
        }
    }
    ThinGreySlider {
        id: slider
        width: parent.width
        enabled: root.enabled
        from: 1
        to: Math.max(2, root.pageCount)
        stepSize: 1
        // Thumb tracks drag; model when idle.
        value: root.currentPage
        onMoved: v => root.changed(Math.round(v))
        onFinished: root.finished()
    }
}
