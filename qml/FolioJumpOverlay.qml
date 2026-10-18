import QtQuick
import Papercut 1.0

PapercutOverlay {
    id: root
    property var folio
    property var onAdd: function() {}
    property var onManage: function() {}
    title: "Folio"

    ListView {
        anchors { fill: parent; bottomMargin: 56 }
        clip: true
        boundsBehavior: Flickable.StopAtBounds
        model: folio ? folio.visibleItems : []
        delegate: Item {
            width: ListView.view.width
            height: 56
            readonly property bool current: index === folio.folioIndex
            Row {
                anchors.fill: parent
                anchors.leftMargin: 12
                anchors.rightMargin: 12
                AppText {
                    width: 42
                    anchors.verticalCenter: parent.verticalCenter
                    text: (index + 1)
                    color: App.appFontColor
                    opacity: 0.45
                    font.pixelSize: Math.round(16 * App.uiScale)
                }
                Column {
                    width: parent.width - 50
                    anchors.verticalCenter: parent.verticalCenter
                    AppText {
                        text: modelData.displayName
                        color: App.appFontColor
                        opacity: current ? 1 : 0.7
                        font.pixelSize: Math.round(16 * App.uiScale)
                        elide: Text.ElideRight
                        width: parent.width
                    }
                    AppText {
                        visible: modelData.displayPath.length > 0
                        text: modelData.displayPath
                        color: App.appFontColor
                        opacity: current ? 0.7 : 0.45
                        font.pixelSize: Math.round(12 * App.uiScale)
                        elide: Text.ElideRight
                        width: parent.width
                    }
                }
            }
            MouseArea {
                anchors.fill: parent
                onClicked: { folio.showFolioIndex(index); root.dismiss() }
            }
        }
    }
    Row {
        anchors { bottom: parent.bottom; horizontalCenter: parent.horizontalCenter; bottomMargin: 8 }
        ChromeIcon { icon: "edit"; hint: "Manage Folio"; onClicked: { root.dismiss(); root.onManage() } }
        ChromeIcon { icon: "add"; hint: "Add"; onClicked: { root.dismiss(); root.onAdd() } }
    }
}
