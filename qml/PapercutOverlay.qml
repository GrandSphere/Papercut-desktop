import QtQuick
import Papercut 1.0

Item {
    id: root
    property string title
    property bool overlayVisible: false
    // Multi-step overlays set interceptBack and handle back themselves.
    property bool interceptBack: false
    property int occupyTop: 0 // title bar inset; 0 in fullscreen for overlay bar
    property int occupyLeft: 0
    property int occupyRight: 0
    property int occupyBottom: 0
    readonly property alias actionHost: bar.actionHost
    readonly property alias contentItem: body
    default property alias content: body.data

    signal dismissed
    signal backRequested

    visible: overlayVisible
    anchors.fill: parent
    anchors.topMargin: occupyTop
    anchors.leftMargin: occupyLeft
    anchors.rightMargin: occupyRight
    anchors.bottomMargin: occupyBottom
    z: 20

    function dismiss() {
        overlayVisible = false
        dismissed()
    }
    function open() { overlayVisible = true }
    function back() {
        if (interceptBack) backRequested()
        else dismiss()
    }

    Rectangle { anchors.fill: parent; color: App.bgColor }
    MouseArea { anchors.fill: parent }

    ModeTopBar {
        id: bar
        width: parent.width
        title: root.title
        onBack: root.back()
    }
    Item {
        id: body
        anchors { top: bar.bottom; left: parent.left; right: parent.right; bottom: parent.bottom }
    }
}
