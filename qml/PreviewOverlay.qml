import QtQuick
import Papercut 1.0

PapercutOverlay {
    id: root
    title: "Preview"
    property string path: ""
    signal toast(string message)

    function show(filePath) {
        path = filePath
        overlayVisible = true
    }
    onDismissed: path = ""

    // Preview controller lives only while overlay visible.
    Loader {
        anchors.fill: parent
        active: root.overlayVisible && root.path.length > 0
        sourceComponent: Item {
            ViewerController {
                id: previewCtl
                strip: previewStrip
                onToast: message => root.toast(message)
            }
            PdfStripItem {
                id: previewStrip
                anchors.fill: parent
                focus: true
            }
            Component.onCompleted: previewCtl.openPreview(root.path)
        }
    }
}
