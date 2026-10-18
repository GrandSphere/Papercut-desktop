import QtQuick
import QtQuick.Controls.Basic
import Papercut 1.0

PapercutOverlay {
    id: root
    title: "About"

    Flickable {
        anchors.fill: parent
        contentWidth: width
        contentHeight: col.implicitHeight + 48
        clip: true
        boundsBehavior: Flickable.StopAtBounds
        ScrollBar.vertical: ScrollBar {}

        Column {
            id: col
            x: 24
            y: 24
            width: parent.width - 48
            spacing: 4

            AppLogo { size: 72; anchors.horizontalCenter: parent.horizontalCenter }
            Item { height: 8; width: 1 }
            AppText { text: "Papercut"; color: App.appFontColor; font.pixelSize: Math.round(24 * App.uiScale); anchors.horizontalCenter: parent.horizontalCenter }
            AppText { text: "Grand Sphere Studios"; color: App.appFontColor; opacity: 0.75; font.pixelSize: Math.round(16 * App.uiScale); anchors.horizontalCenter: parent.horizontalCenter }
            Item { height: 4; width: 1 }
            AppText { text: "Version: " + App.version; color: App.appFontColor; opacity: 0.7; font.pixelSize: Math.round(14 * App.uiScale); anchors.horizontalCenter: parent.horizontalCenter }
            Item { height: 16; width: 1 }
            AppText { text: "License:"; color: App.appFontColor; opacity: 0.7; font.pixelSize: Math.round(16 * App.uiScale) }
            AboutLink { title: "GNU General Public License 3"; subtitle: "(GPLv3)"; url: "https://github.com/GrandSphere/Papercut-desktop/blob/main/LICENSE" }
            Item { height: 12; width: 1 }
            AppText { text: "Github:"; color: App.appFontColor; opacity: 0.7; font.pixelSize: Math.round(16 * App.uiScale) }
            AboutLink { title: "Github Repository"; url: "https://github.com/GrandSphere/Papercut-desktop" }
            AboutLink { title: "Latest Release"; url: "https://github.com/GrandSphere/Papercut-desktop/releases/latest" }
            Item { height: 12; width: 1 }
            AppText { text: "Donate:"; color: App.appFontColor; opacity: 0.7; font.pixelSize: Math.round(16 * App.uiScale) }
            AboutLink { title: "Ko-fi"; url: "https://ko-fi.com/grandspherestudios" }
            AboutLink { title: "Liberapay"; url: "https://liberapay.com/GrandSphere" }
        }
    }

    component AboutLink: Item {
        property string title
        property string subtitle: ""
        property string url
        width: col.width
        height: (subtitle.length ? 44 : 24) + 12
        Column {
            anchors.verticalCenter: parent.verticalCenter
            AppText { text: title; color: App.appFontColor; font.pixelSize: Math.round(16 * App.uiScale) }
            AppText { visible: subtitle.length > 0; text: subtitle; color: App.appFontColor; font.pixelSize: Math.round(16 * App.uiScale) }
        }
        Icon { name: "open_in_new"; size: 18; anchors { right: parent.right; verticalCenter: parent.verticalCenter } }
        MouseArea { anchors.fill: parent; cursorShape: Qt.PointingHandCursor; onClicked: App.openUrl(url) }
    }
}
