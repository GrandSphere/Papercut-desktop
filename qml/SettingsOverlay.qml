import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Dialogs
import Papercut 1.0

PapercutOverlay {
    id: root
    title: "Settings"
    property var draft: Object.assign({}, App.settings)
    property var toastFn: function(msg) {}
    property var onExportReady: function(path, name) {}
    property string colourKey: ""
    property string exportKind: "All"
    property string paddingKey: "pagePaddingDp"
    property var pendingDelete: []
    signal settingsSaved

    readonly property bool startupCustom: draft.startupZoomMode === "CUSTOM"
    readonly property real startupPercent: Math.max(50, Math.min(400, draft.startupZoomPercent || 100))

    onOverlayVisibleChanged: if (overlayVisible) draft = Object.assign({}, App.settings)

    function set(key, value) {
        const n = Object.assign({}, draft)
        n[key] = value
        draft = n
    }
    function setMany(values) { draft = Object.assign({}, draft, values) }
    function save() {
        App.saveSettingsDraft(draft)
        draft = Object.assign({}, App.settings)
        toastFn("Saved")
        settingsSaved()
    }
    function pick(key, title) {
        colourKey = key
        colourPicker.title = title
        colourPicker.initial = draft[key]
        colourPicker.open()
    }
    function percentText(v) { return Math.round(Math.max(0, Math.min(1, v)) * 100) + "%" }
    function fontScaleValue(v) {
        const n = Number(v)
        if (!isFinite(n)) return 1
        return Math.round(Math.max(0.3, Math.min(3, n)) * 100) / 100
    }
    function fontScaleText(v) {
        const n = root.fontScaleValue(v === undefined ? 1 : v)
        return n.toFixed(2).replace(/\.?0+$/, "")
    }
    function fontScaleLabel(v) { return "x" + root.fontScaleText(v) }
    function parseFontScale(text) {
        let s = String(text || "").trim()
        if (s.startsWith("x") || s.startsWith("X")) s = s.slice(1)
        const n = parseFloat(s.replace(",", "."))
        if (!isFinite(n)) return 1
        return root.fontScaleValue(n)
    }
    function defaultWindowSizeLabel(w, h) {
        return String(w || 1000) + "x" + String(h || 760)
    }
    function startupPositionLabel(id) {
        switch (String(id || "CENTER").toUpperCase()) {
        case "SNAP_LEFT": return "Snapped left"
        case "SNAP_RIGHT": return "Snapped right"
        case "MAXIMIZED": return "Maximized"
        case "FULLSCREEN": return "Fullscreen"
        default: return "Center"
        }
    }
    function startupZoomSummary(d) {
        if (d.rememberLastZoom) return "Last zoom"
        const mode = d.startupZoomMode || "FIT_WIDTH"
        if (mode === "FIT_PAGE") return "Fit"
        if (mode === "FIT_WIDTH") return "Fit width"
        return Math.round(Math.max(50, Math.min(400, d.startupZoomPercent || 100))) + "%"
    }
    function parseDefaultWindowField(text, fallback, min) {
        const n = parseInt(String(text || "").trim(), 10)
        if (!isFinite(n) || n < min) return fallback
        return Math.min(n, 16384)
    }

    ChromeIcon { parent: root.actionHost; icon: "save"; hint: "Save"; onClicked: root.save() }

    Flickable {
        anchors.fill: parent
        contentWidth: width
        contentHeight: col.implicitHeight + 48
        clip: true
        boundsBehavior: Flickable.StopAtBounds
        ScrollBar.vertical: ScrollBar {}

        Column {
            id: col
            x: 16
            y: 16
            width: parent.width - 32
            spacing: 12

            CollapsibleSection {
                title: "On start"
                hint: "Reopen the last document, open Folio, and optionally always load themed dark colours"
                SettingsSwitchRow {
                    label: "Open last PDF"
                    checked: !!draft.restoreLastPdf
                    onToggled: on => root.setMany(on ? { restoreLastPdf: true, openFolioOnStartup: false } : { restoreLastPdf: false })
                }
                SettingsSwitchRow {
                    label: "Open Folio"
                    checked: !!draft.openFolioOnStartup
                    enabled: !!draft.folioEnabled
                    onToggled: on => {
                        if (!draft.folioEnabled) return
                        root.setMany(on ? { openFolioOnStartup: true, restoreLastPdf: false } : { openFolioOnStartup: false })
                    }
                }
                Rectangle { width: parent.width; height: 1; color: "#5F6368"; opacity: 0.5 }
                SettingsSwitchRow { label: "Always dark mode"; checked: !!draft.alwaysDarkMode; onToggled: on => root.set("alwaysDarkMode", on) }
                Rectangle { width: parent.width; height: 1; color: "#5F6368"; opacity: 0.5 }
                SettingsTapRow {
                    label: "Default window size"
                    value: root.defaultWindowSizeLabel(draft.defaultWindowWidth, draft.defaultWindowHeight)
                    onClicked: {
                        defaultWinWField.text = String(draft.defaultWindowWidth || 1000)
                        defaultWinHField.text = String(draft.defaultWindowHeight || 760)
                        defaultWindowSizeDialog.open()
                    }
                }
                SettingsTapRow {
                    label: "Startup position"
                    value: root.startupPositionLabel(draft.startupWindowPosition)
                    onClicked: startupPositionDialog.open()
                }
                SettingsTapRow {
                    label: "Zoom"
                    value: root.startupZoomSummary(draft)
                    onClicked: zoomStartupDialog.open()
                }
            }

            CollapsibleSection {
                title: "Folio"
                hint: "Turn the document wallet on, and which item to show first"
                SettingsSwitchRow {
                    label: "Enable Folio"
                    checked: !!draft.folioEnabled
                    onToggled: on => root.setMany(on ? { folioEnabled: true } : { folioEnabled: false, openFolioOnStartup: false })
                }
                SettingsSwitchRow { label: "Open last viewed folio item"; checked: !!draft.folioOpenLastViewed; enabled: !!draft.folioEnabled; onToggled: on => root.set("folioOpenLastViewed", on) }
                SettingsSwitchRow { label: "Store last position"; checked: !!draft.folioStoreLastPosition; enabled: !!draft.folioEnabled; onToggled: on => root.set("folioStoreLastPosition", on) }
            }

            CollapsibleSection {
                title: "Appearance"
                hint: "Document colours, UI font, action accent, font size, and page padding"
                ColourSettingRow { label: "Foreground"; swatch: draft.fgColor; onClicked: root.pick("fgColor", "Foreground"); onLongClicked: root.set("fgColor", Qt.color("#FFC8C8C8")) }
                ColourSettingRow { label: "Background"; swatch: draft.bgColor; onClicked: root.pick("bgColor", "Background"); onLongClicked: root.set("bgColor", Qt.color("#FF121212")) }
                ColourSettingRow { label: "Highlight"; swatch: draft.highlightColor; onClicked: root.pick("highlightColor", "Highlight"); onLongClicked: root.set("highlightColor", Qt.color("#FF707070")) }
                ColourSettingRow {
                    label: "Active page border"
                    swatch: draft.pageBorderColor
                    switchVisible: true
                    checked: !!draft.pageBorderEnabled
                    onToggled: on => root.set("pageBorderEnabled", on)
                    onClicked: root.pick("pageBorderColor", "Active page border")
                    onLongClicked: root.set("pageBorderColor", Qt.color("#FF707070"))
                }
                Rectangle { width: parent.width; height: 1; color: "#5F6368"; opacity: 0.5 }
                ColourSettingRow { label: "Font"; swatch: draft.appFontColor; onClicked: root.pick("appFontColor", "Font"); onLongClicked: root.set("appFontColor", Qt.color("#FFB3B3B3")) }
                ColourSettingRow { label: "Action"; swatch: draft.actionColor; onClicked: root.pick("actionColor", "Action"); onLongClicked: root.set("actionColor", Qt.color("#FF545454")) }
                Rectangle { width: parent.width; height: 1; color: "#5F6368"; opacity: 0.5 }
                SettingsTapRow {
                    label: "Font size"
                    value: root.fontScaleLabel(draft.fontScale)
                    onClicked: {
                        scaleField.text = root.fontScaleText(draft.fontScale)
                        scaleDialog.open()
                    }
                }
                SettingsTapRow {
                    label: "Font style"
                    value: (draft.appFontFamily || "Roboto") + " " + (draft.appFontWeight === undefined ? 500 : draft.appFontWeight)
                    onClicked: fontDialog.open()
                }
                Rectangle { width: parent.width; height: 1; color: "#5F6368"; opacity: 0.5 }
                SettingsTapRow {
                    label: "Vertical page padding"
                    value: String(draft.pagePaddingDp || 0)
                    onClicked: { root.paddingKey = "pagePaddingDp"; padField.text = String(draft.pagePaddingDp || 0); padDialog.open() }
                }
                SettingsTapRow {
                    label: "Horizontal page padding"
                    value: String(draft.horizontalPagePaddingDp || 0)
                    onClicked: { root.paddingKey = "horizontalPagePaddingDp"; padField.text = String(draft.horizontalPagePaddingDp || 0); padDialog.open() }
                }
                Rectangle { width: parent.width; height: 1; color: "#5F6368"; opacity: 0.5 }
                SettingsTapRow {
                    label: "Vertical window padding"
                    value: String(draft.windowPaddingV || 0)
                    onClicked: { root.paddingKey = "windowPaddingV"; padField.text = String(draft.windowPaddingV || 0); padDialog.open() }
                }
                SettingsTapRow {
                    label: "Horizontal window padding"
                    value: String(draft.windowPaddingH || 0)
                    onClicked: { root.paddingKey = "windowPaddingH"; padField.text = String(draft.windowPaddingH || 0); padDialog.open() }
                }
            }

            CollapsibleSection {
                title: "Graphics"
                hint: "Whether pictures use themed colours or their original colours, and picture alpha"
                Column {
                    width: parent.width
                    spacing: 8
                    Row { width: parent.width
                        AppText { width: parent.width - pictures.implicitWidth; text: "Pictures"; color: App.appFontColor; opacity: 0.7; font.pixelSize: Math.round(16 * App.uiScale) }
                        AppText { id: pictures; text: root.percentText(draft.imageColorBlend || 0); color: App.appFontColor; opacity: 0.7; font.pixelSize: Math.round(16 * App.uiScale) }
                    }
                    ThinGreySlider { value: draft.imageColorBlend || 0; onMoved: v => root.set("imageColorBlend", v) }
                    Row { width: parent.width
                        AppText { width: parent.width - original.implicitWidth; text: "Themed"; color: App.appFontColor; opacity: 0.5; font.pixelSize: Math.round(16 * App.uiScale) }
                        AppText { id: original; text: "Original"; color: App.appFontColor; opacity: 0.5; font.pixelSize: Math.round(16 * App.uiScale) }
                    }
                    Row { width: parent.width
                        AppText { width: parent.width - alphaValue.implicitWidth; text: "Alpha"; color: App.appFontColor; opacity: 0.7; font.pixelSize: Math.round(16 * App.uiScale) }
                        AppText { id: alphaValue; text: root.percentText(draft.imageAlpha === undefined ? 1 : draft.imageAlpha); color: App.appFontColor; opacity: 0.7; font.pixelSize: Math.round(16 * App.uiScale) }
                    }
                    ThinGreySlider { value: draft.imageAlpha === undefined ? 1 : draft.imageAlpha; onMoved: v => root.set("imageAlpha", v) }
                }
            }

            CollapsibleSection {
                title: "Dark defaults"
                hint: "Picture theming and alpha applied only when Always dark mode is on at startup"
                Column {
                    width: parent.width
                    spacing: 8
                    Row { width: parent.width
                        AppText { width: parent.width - darkPictures.implicitWidth; text: "Pictures"; color: App.appFontColor; opacity: 0.7; font.pixelSize: Math.round(16 * App.uiScale) }
                        AppText { id: darkPictures; text: root.percentText(draft.darkImageBlend || 0); color: App.appFontColor; opacity: 0.7; font.pixelSize: Math.round(16 * App.uiScale) }
                    }
                    ThinGreySlider { value: draft.darkImageBlend || 0; onMoved: v => root.set("darkImageBlend", v) }
                    Row { width: parent.width
                        AppText { width: parent.width - darkOriginal.implicitWidth; text: "Themed"; color: App.appFontColor; opacity: 0.5; font.pixelSize: Math.round(16 * App.uiScale) }
                        AppText { id: darkOriginal; text: "Original"; color: App.appFontColor; opacity: 0.5; font.pixelSize: Math.round(16 * App.uiScale) }
                    }
                    Row { width: parent.width
                        AppText { width: parent.width - darkAlphaValue.implicitWidth; text: "Alpha"; color: App.appFontColor; opacity: 0.7; font.pixelSize: Math.round(16 * App.uiScale) }
                        AppText { id: darkAlphaValue; text: root.percentText(draft.darkImageAlpha === undefined ? 0.2 : draft.darkImageAlpha); color: App.appFontColor; opacity: 0.7; font.pixelSize: Math.round(16 * App.uiScale) }
                    }
                    ThinGreySlider { value: draft.darkImageAlpha === undefined ? 0.2 : draft.darkImageAlpha; onMoved: v => root.set("darkImageAlpha", v) }
                }
            }

            CollapsibleSection {
                title: "Interaction"
                hint: "Links, text selection, side pan, and how to open the menu"
                SettingsSwitchRow { label: "Enable following links"; checked: !!draft.allowFollowLinks; onToggled: on => root.set("allowFollowLinks", on) }
                SettingsSwitchRow { label: "Enable text selection"; checked: !!draft.allowTextSelection; onToggled: on => root.set("allowTextSelection", on) }
                Rectangle { width: parent.width; height: 1; color: "#5F6368"; opacity: 0.5 }
                SettingsSwitchRow {
                    label: "Auto-enable pan"
                    checked: !!draft.autoDisablePan
                    onToggled: on => root.set("autoDisablePan", on)
                    onLongPressed: root.toastFn("Zooming unlocks panning")
                }
                SettingsSwitchRow {
                    label: "Center horizontally"
                    checked: !!draft.centerHorizontally
                    onToggled: on => root.set("centerHorizontally", on)
                    onLongPressed: root.toastFn("Hold Fit width to center the page")
                }
            }

            CollapsibleSection {
                title: "Shortcuts"
                hint: "Keyboard and mouse bindings. Save them with the save icon on that screen; Settings save does not write keybinds."
                SettingsTapRow { label: "Change Keybinds"; onClicked: shortcuts.open() }
            }

            CollapsibleSection {
                title: "Bookmarks"
                hint: "Last reading place, manage stored bookmarks per document, or clear every bookmark"
                SettingsSwitchRow { label: "Open last position"; checked: !!draft.restoreLastPosition; onToggled: on => root.set("restoreLastPosition", on) }
                SettingsTapRow { label: "Manage bookmarks"; onClicked: manageDocs.open() }
                SettingsTapRow { label: "Clear bookmarks"; onClicked: clearDialog.open() }
            }

            CollapsibleSection {
                title: "Manage"
                hint: "Window behaviour, where Folio copies live, and stored files"
                SettingsSwitchRow { label: "Allow multiple instances"; checked: !!draft.allowMultipleInstances; onToggled: on => root.set("allowMultipleInstances", on) }
                SettingsSwitchRow {
                    label: "Full Screen on Maximize"
                    checked: !!draft.fullScreenOnMaximize
                    onToggled: on => root.set("fullScreenOnMaximize", on)
                }
                SettingsSwitchRow {
                    label: "Show title bar"
                    checked: draft.titleBarEnabled !== false
                    onToggled: on => root.set("titleBarEnabled", on)
                }
                SettingsTapRow {
                    label: "Select Icon"
                    value: (draft.appIcon || "papercut-dark-off") + ".ico"
                    onClicked: iconDialog.open()
                }
                SettingsTapRow {
                    label: "Store files in"
                    value: (draft.filesRoot && draft.filesRoot.length) ? draft.filesRoot : App.defaultFilesRoot()
                    onClicked: folderDialog.open()
                }
                SettingsTapRow { label: "View stored files"; onClicked: stored.open() }
            }

            CollapsibleSection {
                title: "Export"
                hint: "Save or load settings and bookmarks as a Papercut JSON file"
                Repeater {
                    model: [{ k: "All", t: "Export All" }, { k: "Settings", t: "Export Settings" }, { k: "Bookmarks", t: "Export Bookmarks" }, { k: "Folio", t: "Export Folio" }]
                    delegate: SettingsTapRow {
                        label: modelData.t
                        shareVisible: true
                        onClicked: { root.exportKind = modelData.k; exportDialog.currentFile = App.suggestedFile(App.exportFileName(modelData.k)); exportDialog.open() }
                        onShareClicked: root.shareExport(modelData.k)
                    }
                }
                SettingsTapRow { label: "Import"; onClicked: importDialog.open() }
            }
        }
    }

    // Desktop share: write JSON to temp dir and reveal folder.
    function shareExport(kind) {
        const path = App.tempDir + "/" + App.exportFileName(kind)
        if (App.writeTextFile(path, App.exportBackup(kind))) { App.revealFile(path); toastFn("Exported") }
        else toastFn("Export failed")
    }

    ColorPickerDialog {
        id: colourPicker
        onPicked: c => root.set(root.colourKey, c)
    }
    PapercutDialog {
        id: padDialog
        title: root.paddingKey.indexOf("window") >= 0
               ? (root.paddingKey === "windowPaddingH" ? "Horizontal window padding" : "Vertical window padding")
               : (root.paddingKey === "horizontalPagePaddingDp" ? "Horizontal page padding" : "Vertical page padding")
        confirmText: "OK"
        onOpened: padField.forceActiveFocus()
        onAccepted: root.set(root.paddingKey, Math.max(0, parseInt(padField.text) || 0))
        PapercutField {
            id: padField
            width: parent.width
            color: App.appFontColor
            placeholderText: "dp"
            validator: IntValidator { bottom: 0; top: 9999 }
            inputMethodHints: Qt.ImhDigitsOnly
            onAccepted: { padDialog.close(); padDialog.accepted() }
        }
    }
    PapercutDialog {
        id: defaultWindowSizeDialog
        title: "Default window size"
        confirmText: "OK"
        onOpened: defaultWinWField.forceActiveFocus()
        onAccepted: {
            const fbW = draft.defaultWindowWidth || 1000
            const fbH = draft.defaultWindowHeight || 760
            root.setMany({
                defaultWindowWidth: root.parseDefaultWindowField(defaultWinWField.text, fbW, 360),
                defaultWindowHeight: root.parseDefaultWindowField(defaultWinHField.text, fbH, 480)
            })
        }
        Row {
            width: parent.width
            spacing: 8
            PapercutField {
                id: defaultWinWField
                width: (parent.width - parent.spacing - sizeLabel.implicitWidth) / 2
                color: App.appFontColor
                placeholderText: "Width"
                validator: IntValidator { bottom: 360; top: 16384 }
                inputMethodHints: Qt.ImhDigitsOnly
                onAccepted: { defaultWindowSizeDialog.close(); defaultWindowSizeDialog.accepted() }
            }
            AppText {
                id: sizeLabel
                anchors.verticalCenter: parent.verticalCenter
                text: "×"
                color: App.appFontColor
                opacity: 0.7
                font.pixelSize: Math.round(18 * App.uiScale)
            }
            PapercutField {
                id: defaultWinHField
                width: defaultWinWField.width
                color: App.appFontColor
                placeholderText: "Height"
                validator: IntValidator { bottom: 480; top: 16384 }
                inputMethodHints: Qt.ImhDigitsOnly
                onAccepted: { defaultWindowSizeDialog.close(); defaultWindowSizeDialog.accepted() }
            }
        }
    }
    PapercutDialog {
        id: scaleDialog
        title: "Font size"
        confirmText: "OK"
        onOpened: scaleField.forceActiveFocus()
        onAccepted: root.set("fontScale", root.parseFontScale(scaleField.text))
        PapercutField {
            id: scaleField
            width: parent.width
            color: App.appFontColor
            placeholderText: "1"
            inputMethodHints: Qt.ImhFormattedNumbersOnly
            validator: RegularExpressionValidator { regularExpression: /x?[0-9]*[.,]?[0-9]{0,2}/i }
            onAccepted: { scaleDialog.close(); scaleDialog.accepted() }
        }
    }
    FolderDialog {
        id: folderDialog
        onAccepted: root.set("filesRoot", App.localPath(selectedFolder))
    }
    PapercutDialog {
        id: fontDialog
        title: "Font style"
        confirmText: "OK"
        dismissText: "Cancel"
        property string fontPick: "Roboto"
        onOpened: {
            fontSearch.text = ""
            fontPick = draft.appFontFamily || "Roboto"
            fontWeightField.text = String(draft.appFontWeight === undefined ? 500 : draft.appFontWeight)
        }
        onAccepted: {
            const w = Math.max(100, Math.min(900, parseInt(fontWeightField.text) || 500))
            root.setMany({ appFontFamily: fontPick, appFontWeight: w })
        }
        PapercutField {
            id: fontSearch
            width: parent.width
            placeholderText: "Search fonts"
        }
        Row {
            width: parent.width
            spacing: 8
            AppText {
                id: weightLabel
                anchors.verticalCenter: parent.verticalCenter
                text: "Weight"
                color: App.appFontColor
                opacity: 0.7
                font.pixelSize: Math.round(16 * App.uiScale)
            }
            PapercutField {
                id: fontWeightField
                width: parent.width - weightLabel.width - parent.spacing
                color: App.appFontColor
                placeholderText: "500"
                validator: IntValidator { bottom: 100; top: 900 }
                inputMethodHints: Qt.ImhDigitsOnly
                onAccepted: { fontDialog.close(); fontDialog.accepted() }
            }
        }
        ListView {
            id: fontList
            width: parent.width
            height: 280
            clip: true
            boundsBehavior: Flickable.StopAtBounds
            ScrollBar.vertical: ScrollBar {}
            model: {
                const q = (fontSearch.text || "").trim().toLowerCase()
                const all = App.systemFontFamilies
                if (!q) return all
                return all.filter(f => String(f).toLowerCase().indexOf(q) >= 0)
            }
            delegate: Item {
                required property string modelData
                width: fontList.width
                height: 40
                Rectangle {
                    anchors.fill: parent
                    radius: 8
                    color: fontDialog.fontPick === modelData
                           ? Qt.rgba(App.appFontColor.r, App.appFontColor.g, App.appFontColor.b, 0.12)
                           : "transparent"
                }
                AppText {
                    anchors {
                        left: parent.left
                        leftMargin: 12
                        right: parent.right
                        verticalCenter: parent.verticalCenter
                    }
                    text: modelData
                    color: App.appFontColor
                    font.family: modelData
                    font.pixelSize: Math.round(16 * App.uiScale)
                    elide: Text.ElideRight
                }
                MouseArea {
                    anchors.fill: parent
                    onClicked: fontDialog.fontPick = modelData
                }
            }
        }
    }
    PapercutDialog {
        id: zoomStartupDialog
        title: "Zoom"
        confirmText: ""
        dismissText: "Close"
        SettingsSwitchRow {
            label: "Remember last zoom"
            checked: !!draft.rememberLastZoom
            onToggled: on => root.set("rememberLastZoom", on)
        }
        AppText {
            text: "Zoom on startup:"
            color: App.appFontColor
            opacity: draft.rememberLastZoom ? 0.35 : 0.7
            font.pixelSize: Math.round(16 * App.uiScale)
        }
        Row {
            spacing: 8
            Repeater {
                model: [{ m: "FIT_PAGE", t: "Fit" }, { m: "FIT_WIDTH", t: "Fit width" }, { m: "CUSTOM", t: "Zoom" }]
                delegate: FilterChip {
                    text: modelData.t
                    enabled: !draft.rememberLastZoom
                    selected: (draft.startupZoomMode || "FIT_WIDTH") === modelData.m
                    onClicked: root.set("startupZoomMode", modelData.m)
                }
            }
        }
        Column {
            visible: root.startupCustom
            width: parent.width
            spacing: 4
            AppText {
                text: Math.round(root.startupPercent) + "%"
                color: App.appFontColor
                font.pixelSize: Math.round(16 * App.uiScale)
            }
            ThinGreySlider {
                from: 50
                to: 400
                value: root.startupPercent
                enabled: !draft.rememberLastZoom
                onMoved: v => root.setMany({ startupZoomPercent: v, startupZoomMode: "CUSTOM" })
            }
        }
    }
    PapercutDialog {
        id: startupPositionDialog
        title: "Startup position"
        confirmText: ""
        dismissText: "Close"
        width: Math.min(640, (Overlay.overlay ? Overlay.overlay.width : 700) - 48)
        AppText {
            text: "Window position on startup:"
            color: App.appFontColor
            opacity: 0.7
            font.pixelSize: Math.round(16 * App.uiScale)
        }
        Row {
            spacing: 8
            Repeater {
                model: [
                    { m: "SNAP_LEFT", t: "Snapped left" },
                    { m: "SNAP_RIGHT", t: "Snapped right" },
                    { m: "CENTER", t: "Center" },
                    { m: "MAXIMIZED", t: "Maximized" },
                    { m: "FULLSCREEN", t: "Fullscreen" }
                ]
                delegate: FilterChip {
                    text: modelData.t
                    selected: (root.draft.startupWindowPosition || "CENTER").toUpperCase() === modelData.m
                    onClicked: {
                        root.set("startupWindowPosition", modelData.m)
                        startupPositionDialog.close()
                    }
                }
            }
        }
    }
    PapercutDialog {
        id: iconDialog
        title: "Select Icon"
        confirmText: ""
        dismissText: "Close"
        Repeater {
            model: ["papercut-dark", "papercut-light", "papercut-dark-off", "papercut-light-off"]
            delegate: Item {
                required property string modelData
                width: parent.width
                height: 44
                Rectangle {
                    anchors.fill: parent
                    radius: 8
                    color: (root.draft.appIcon || "papercut-dark-off") === modelData
                           ? Qt.rgba(App.appFontColor.r, App.appFontColor.g, App.appFontColor.b, 0.12)
                           : "transparent"
                }
                AppText {
                    anchors {
                        left: parent.left
                        leftMargin: 12
                        right: parent.right
                        verticalCenter: parent.verticalCenter
                    }
                    text: modelData + ".ico"
                    color: App.appFontColor
                    font.pixelSize: Math.round(16 * App.uiScale)
                    elide: Text.ElideRight
                }
                MouseArea {
                    anchors.fill: parent
                    onClicked: {
                        root.set("appIcon", modelData)
                        App.setSetting("appIcon", modelData)
                        iconDialog.close()
                    }
                }
            }
        }
    }
    PapercutDialog {
        id: clearDialog
        title: "Clear bookmarks"
        confirmText: "Clear"
        onAccepted: { App.clearAllBookmarks(); root.toastFn("Bookmarks cleared") }
        AppText { text: "Delete every bookmark for every document? Last reading places are kept."; color: App.appFontColor; wrapMode: Text.Wrap; width: parent.width }
    }
    PapercutDialog {
        id: deleteStoredDialog
        title: "Delete stored file"
        confirmText: "Delete"
        onAccepted: { App.deleteStored(root.pendingDelete); stored.refresh() }
        AppText {
            width: parent.width
            wrapMode: Text.Wrap
            color: App.appFontColor
            text: root.pendingDelete.length === 1 ? "This file is in use. Delete it and remove that entry?"
                                                  : "Some selected files are in use. Delete them and remove those entries?"
        }
    }

    // Nested overlays parented on root (cover settings top bar).
    function escapePressed() {
        if (shortcuts.capturing) shortcuts.cancelCapture()
        else if (shortcuts.overlayVisible) shortcuts.back()
        else if (preview.overlayVisible) preview.back()
        else if (stored.overlayVisible) stored.back()
        else if (manageDocs.overlayVisible) manageDocs.back()
        else back()
    }

    ShortcutsOverlay { id: shortcuts; parent: root }
    ManageDocumentsOverlay { id: manageDocs; parent: root }
    StoredFilesOverlay {
        id: stored
        parent: root
        onPreview: entry => preview.show(entry.path)
        onShareEntry: entry => root.onExportReady(entry.path, entry.title)
        onDeleteEntries: entries => {
            if (entries.some(e => e.inDatabase)) { root.pendingDelete = entries; deleteStoredDialog.open() }
            else { App.deleteStored(entries); stored.refresh() }
        }
    }
    PreviewOverlay { id: preview; parent: root; onToast: message => root.toastFn(message) }

    FileDialog {
        id: exportDialog
        fileMode: FileDialog.SaveFile
        defaultSuffix: "json"
        nameFilters: ["JSON (*.json)", "All files (*)"]
        onAccepted: root.toastFn(App.writeTextFile(App.localPath(selectedFile), App.exportBackup(root.exportKind)) ? "Exported" : "Export failed")
    }
    FileDialog {
        id: importDialog
        nameFilters: ["JSON (*.json)", "All files (*)"]
        onAccepted: {
            root.toastFn(App.importBackup(App.localPath(selectedFile)))
            root.draft = Object.assign({}, App.settings)
        }
    }
}
