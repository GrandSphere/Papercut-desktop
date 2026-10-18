import QtQuick
import QtQuick.Controls.Basic
import Papercut 1.0

Popup {
    id: root
    property string title
    property string confirmText: "OK"
    property string dismissText: "Cancel"
    property bool confirmEnabled: true
    // closeOnConfirm false: confirm only emits accepted() (password dialog).
    property bool closeOnConfirm: true
    default property alias content: body.data
    signal accepted
    signal rejected

    anchors.centerIn: Overlay.overlay
    modal: true
    focus: true
    width: Math.min(360, (Overlay.overlay ? Overlay.overlay.width : 400) - 48)
    padding: 24
    closePolicy: Popup.CloseOnEscape
    Overlay.modal: Rectangle { color: "#99000000" }
    font.family: App.uiFontFamily
    font.weight: App.uiFontWeight
    palette.windowText: App.appFontColor
    palette.text: App.appFontColor
    palette.buttonText: App.appFontColor
    background: Rectangle { color: App.bgColor; radius: 28; border.width: 0 }

    contentItem: Column {
        spacing: 16
        AppText { visible: root.title.length > 0; text: root.title; color: App.appFontColor; font.pixelSize: Math.round(20 * App.uiScale); width: parent.width; wrapMode: Text.Wrap }
        Column { id: body; width: parent.width; spacing: 8 }
        Row {
            anchors.right: parent.right
            spacing: 8
            Button {
                visible: root.dismissText.length > 0
                flat: true
                text: root.dismissText
                contentItem: AppText { text: parent.text; color: App.appFontColor; font.pixelSize: Math.round(14 * App.uiScale) }
                background: Item {}
                onClicked: { root.close(); root.rejected() }
            }
            Button {
                visible: root.confirmText.length > 0
                flat: true
                enabled: root.confirmEnabled
                text: root.confirmText
                contentItem: AppText { text: parent.text; color: App.appFontColor; opacity: parent.enabled ? 1 : 0.4; font.pixelSize: Math.round(14 * App.uiScale) }
                background: Item {}
                onClicked: { if (root.closeOnConfirm) root.close(); root.accepted() }
            }
        }
    }
}
