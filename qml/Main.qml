import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Dialogs
import QtQuick.Layouts
import QtQuick.Window
import Papercut 1.0

ApplicationWindow {
    id: win
    width: 1000
    height: 760
    minimumWidth: 360
    minimumHeight: 480
    visible: false
    title: chromeTitle
    color: App.bgColor
    flags: Qt.Window | Qt.FramelessWindowHint | Qt.WindowMinimizeButtonHint | Qt.WindowMaximizeButtonHint | Qt.WindowSystemMenuHint
    font.family: App.uiFontFamily
    font.weight: App.uiFontWeight
    font.pixelSize: Math.round(16 * App.uiScale)

    property bool inFolio: false
    property bool chromeOpen: false
    property bool forwarding: false
    property double suppressMenuUntil: 0
    property bool findOpen: false
    property bool hitsOpen: false
    property bool tocOpen: false
    property bool showPageScrubber: false
    property bool sawContent: false
    property string pendingExport: ""
    property string pendingText: ""
    property int launchPage: Launch.page
    readonly property var ctl: inFolio ? folioCtl : viewerCtl
    readonly property var pwCtl: folioCtl.passwordNeeded ? folioCtl : viewerCtl
    readonly property bool folioEnabled: !!App.settings.folioEnabled
    property bool presentationEnteredFullscreen: false
    property bool wantFullscreen: false
    property bool restoringWindowed: false
    property bool leavingWindowed: false
    property real windowedX: x
    property real windowedY: y
    function defaultWindowW() {
        return Math.max(minimumWidth, App.settings.defaultWindowWidth || 1000)
    }
    function defaultWindowH() {
        return Math.max(minimumHeight, App.settings.defaultWindowHeight || 760)
    }
    function applyConfiguredDefaultSize() {
        if (windowZoomed()) return
        width = defaultWindowW()
        height = defaultWindowH()
    }
    property int normalSizeW: defaultWindowW()
    property int normalSizeH: defaultWindowH()
    property int lastSizeW: defaultWindowW()
    property int lastSizeH: defaultWindowH()
    property int zoomedRefW: 0
    property int zoomedRefH: 0
    property bool shareStoreRotation: true
    readonly property int winPadV: Math.max(0, App.settings.windowPaddingV || 0)
    readonly property int winPadH: Math.max(0, App.settings.windowPaddingH || 0)
    readonly property bool isFullscreen: visibility === Window.FullScreen
    readonly property bool titleBarOccupies: !isFullscreen && !!App.settings.titleBarEnabled
    readonly property bool titleBarOverlay: isFullscreen && drawer.visible
    readonly property bool titleBarShown: titleBarOccupies || titleBarOverlay
    readonly property int reservedTop: titleBarOccupies ? titleBarItem.height : 0
    readonly property int contentTop: winPadV + reservedTop
    readonly property string chromeTitle: {
        const uri = ctl.documentUri
        const idx = folioCtl.folioIndex
        const items = folioCtl.visibleItems
        let name = ""
        if (inFolio) {
            const it = items && idx >= 0 ? items[idx] : null
            name = it && it.displayName ? String(it.displayName) : ""
        } else if (viewerCtl.hasDocument) {
            name = viewerCtl.documentName()
        }
        return name.length ? ("Papercut - " + name) : "Papercut"
    }

    ViewerController {
        id: viewerCtl
        onToast: message => toast.show(message)
        onExportReady: path => win.saveExport(path, win.exportName())
        onMenuRequested: win.openChromeFromGesture()
        onPresentingChanged: {
            if (presenting) { win.chromeOpen = false; win.findOpen = false; pageStrip.forceActiveFocus() }
            win.applyPresentationFullscreen(presenting)
        }
        onLaunchFolio: win.enterFolio()
        onDocumentChanged: {
            if (hasDocument && win.launchPage > 1 && !win.inFolio) win.goToLaunchPage()
            win.updateContent()
        }
        onPageTapped: page => { if (pageSelectMode) togglePage(page) }
        onPasswordChanged: win.syncPassword()
        onPageSelectChanged: win.pageSelectUpdated(viewerCtl)
        onStartupReadyChanged: win.updateContent()
        onPageLoadingChanged: win.updateContent()
    }
    FolioController {
        id: folioCtl
        onToast: message => toast.show(message)
        onExportReady: path => win.saveExport(path, win.exportName())
        onMenuRequested: win.openChromeFromGesture()
        onPresentingChanged: {
            if (presenting) { win.chromeOpen = false; win.findOpen = false; pageStrip.forceActiveFocus() }
            win.applyPresentationFullscreen(presenting)
        }
        onPageTapped: page => { if (pageSelectMode) togglePage(page) }
        onItemsAdded: count => toast.show(count ? ("Added " + count) : "Could not add to Folio")
        onPasswordChanged: win.syncPassword()
        onPageSelectChanged: win.pageSelectUpdated(folioCtl)
        onDocumentChanged: win.updateContent()
        onItemsChanged: win.updateContent()
        onStartupReadyChanged: win.updateContent()
        onPageLoadingChanged: win.updateContent()
    }
    CombineController {
        id: combiner
        onToast: message => toast.show(message)
        onExportReady: path => win.saveExport(path, "Papercut-combined.pdf")
    }

    function attachStrip(toFolio) {
        if (toFolio) {
            viewerCtl.strip = null
            inFolio = true
            folioCtl.strip = pageStrip
        } else {
            folioCtl.strip = null
            inFolio = false
            viewerCtl.strip = pageStrip
        }
    }
    function enterFolio() {
        if (inFolio || !folioEnabled) return
        saveCurrent()
        resetOverlays()
        attachStrip(true)
        folioCtl.refreshItems()
        if (!folioCtl.hasDocument && !folioCtl.pageLoading) folioCtl.loadStart()
        updateContent()
    }
    function leaveFolio() {
        if (!inFolio) return
        saveCurrent()
        resetOverlays()
        attachStrip(false)
        showPageScrubber = false
        folioJump.overlayVisible = false
        updateContent()
    }
    function saveCurrent() {
        if (inFolio) {
            folioCtl.rememberPosition()
        } else if (viewerCtl.hasDocument) {
            const p = viewerCtl.readingPosition()
            viewerCtl.saveReadingPosition(p.page, p.scrollX, p.scrollY)
        }
    }
    function quitApp() {
        saveCurrent()
        Qt.quit()
    }
    function windowZoomed() {
        return visibility === Window.Maximized || visibility === Window.FullScreen
            || App.windowIsZoomed(win)
    }
    function syncNormalSize() {
        if (restoringWindowed || leavingWindowed) return
        if (windowZoomed()) return
        normalSizeW = Math.round(width)
        normalSizeH = Math.round(height)
    }
    function syncLastSize() {
        if (restoringWindowed || leavingWindowed) return
        const w = Math.round(width)
        const h = Math.round(height)
        if (windowZoomed()) {
            zoomedRefW = w
            zoomedRefH = h
            return
        }
        if (zoomedRefW <= 0 || zoomedRefH <= 0) {
            lastSizeW = w
            lastSizeH = h
            return
        }
        if (lastSizeW < zoomedRefW && lastSizeH < zoomedRefH) {
            lastSizeW = w
            lastSizeH = h
        }
    }
    function rememberWindowed() {
        if (restoringWindowed || leavingWindowed) return
        if (windowZoomed()) return
        if (width < minimumWidth || height < minimumHeight) return
        if (width >= Screen.width - 8 && height >= Screen.height - 8) return
        windowedX = x
        windowedY = y
    }
    function storedNormalW() {
        return Math.max(lastSizeW, minimumWidth)
    }
    function storedNormalH() {
        return Math.max(lastSizeH, minimumHeight)
    }
    function restoreForSystemDrag(globalX, globalY, pointerAnchor) {
        if (restoringWindowed || !windowZoomed()) return false
        const fracX = (globalX - x) / width
        const nw = storedNormalW()
        const nh = storedNormalH()
        const offY = globalY - y
        const newY = pointerAnchor ? globalY - ((globalY - y) / height) * nh : globalY - offY
        const newX = globalX - fracX * nw
        wantFullscreen = false
        restoringWindowed = true
        if (visibility === Window.FullScreen) {
            App.clearNativeWindowZoom(win)
            width = nw
            height = nh
            x = newX
            y = newY
        } else if (Qt.platform.os === "windows" && App.windowIsZoomed(win)) {
            App.unzoomWindowToGeometry(win, newX, newY, nw, nh)
        } else {
            App.clearNativeWindowZoom(win)
            width = nw
            height = nh
            x = newX
            y = newY
        }
        Qt.callLater(function() { restoringWindowed = false })
        return true
    }
    function beginSystemMoveFromZoomed(globalX, globalY, pointerAnchor) {
        if (pointerAnchor === undefined) pointerAnchor = false
        if (!windowZoomed()) {
            startSystemMove()
            return true
        }
        if (!restoreForSystemDrag(globalX, globalY, pointerAnchor)) return false
        startSystemMove()
        return true
    }
    function restoreWindowed() {
        wantFullscreen = false
        restoringWindowed = true
        const nw = storedNormalW()
        const nh = storedNormalH()
        if (visibility === Window.FullScreen) {
            App.clearNativeWindowZoom(win)
            x = windowedX
            y = windowedY
            width = nw
            height = nh
        } else if (Qt.platform.os === "windows" && windowZoomed()) {
            App.unzoomWindowToGeometry(win, windowedX, windowedY, nw, nh)
        } else {
            App.clearNativeWindowZoom(win)
            x = windowedX
            y = windowedY
            width = nw
            height = nh
        }
        Qt.callLater(function() { restoringWindowed = false })
    }
    function enterMaximizedOrFullscreen() {
        rememberWindowed()
        if (Qt.platform.os === "windows") App.syncWindowPlacement(win)
        leavingWindowed = true
        if (App.settings.fullScreenOnMaximize) {
            wantFullscreen = true
            showFullScreen()
        } else {
            wantFullscreen = false
            showMaximized()
        }
        Qt.callLater(function() { leavingWindowed = false })
    }
    function toggleFullScreen() {
        if (visibility === Window.FullScreen) {
            restoreWindowed()
        } else {
            rememberWindowed()
            leavingWindowed = true
            wantFullscreen = true
            showFullScreen()
            Qt.callLater(function() { leavingWindowed = false })
        }
    }
    function beginStartupFullscreen() {
        leavingWindowed = true
        wantFullscreen = true
        showFullScreen()
        Qt.callLater(function() { leavingWindowed = false })
    }
    function toggleMaxRestore() {
        if (windowZoomed())
            restoreWindowed()
        else
            enterMaximizedOrFullscreen()
    }
    function toggleTitleBar() {
        App.setSetting("titleBarEnabled", !App.settings.titleBarEnabled)
    }
    function applyPresentationFullscreen(on) {
        if (on) {
            if (visibility !== Window.FullScreen) {
                rememberWindowed()
                leavingWindowed = true
                presentationEnteredFullscreen = true
                wantFullscreen = true
                showFullScreen()
                Qt.callLater(function() { leavingWindowed = false })
            }
        } else if (presentationEnteredFullscreen) {
            presentationEnteredFullscreen = false
            if (visibility === Window.FullScreen) restoreWindowed()
        }
    }
    function goToLaunchPage() {
        viewerCtl.goToPageIndex(win.launchPage - 1)
        win.launchPage = 0
    }

    function openPath(path, page, password) {
        if (path.toLowerCase().endsWith(".json")) {
            toast.show(App.importBackup(path))
            return
        }
        if (inFolio) leaveFolio()
        resetOverlays()
        win.launchPage = page > 1 ? page : 0
        if (viewerCtl.hasDocument) {
            const p = viewerCtl.readingPosition()
            viewerCtl.openWithPrevious(path, p.page, p.scrollX, p.scrollY, password || "")
        } else {
            viewerCtl.open(path, true, password || "")
        }
    }
    function showOpenDialog() {
        openDialog.fileMode = (inFolio || combine.overlayVisible) ? FileDialog.OpenFiles : FileDialog.OpenFile
        openDialog.open()
    }
    function handlePaths(paths) {
        if (paths.length === 0) return
        if (combine.overlayVisible) combiner.addPaths(paths)
        else if (inFolio) folioCtl.addFromPaths(paths)
        else openPath(paths[0], 0, "")
    }

    function resetOverlays() {
        findOpen = false
        hitsOpen = false
        tocOpen = false
        chromeOpen = false
        bookmarks.overlayVisible = false
        rearrange.overlayVisible = false
        folioJump.overlayVisible = false
        manageFolio.overlayVisible = false
        combine.overlayVisible = false
        settings.overlayVisible = false
        about.overlayVisible = false
        shareDialog.close()
        drawer.close()
        findField.text = ""
        ctl.exitPageSelect()
        ctl.resetOverlays()
    }
    function openChromeFromGesture() {
        if (Date.now() > win.suppressMenuUntil) win.chromeOpen = !win.chromeOpen
    }
    function pageSelectUpdated(source) {
        if (source.pageSelectMode) {
            chromeOpen = false
            findOpen = false
        }
        if (source === ctl && source.selectedPages.length === 0)
            rearrange.overlayVisible = false
    }
    function handleAppKey(event) {
        if (Keybinds.capturing) return
        const id = Keybinds.actionForKey(event.key, event.modifiers)
        if (id === "grabWindow") return
        if (id && Keybinds.usesWindowKeyEvent(Keybinds.sequenceFromKey(event.key, event.modifiers))) {
            if (!(ctl.presenting && String(id).indexOf("scroll") === 0)) {
                win.runAction(id)
                event.accepted = true
            }
        }
    }
    function exportName() {
        let name = ""
        if (inFolio) {
            const item = folioCtl.visibleItems[folioCtl.folioIndex]
            name = item ? item.displayName : ""
        } else {
            name = viewerCtl.documentName()
        }
        if (name.length === 0) name = "Papercut"
        return /\.pdf$/i.test(name) ? name : name.replace(/\.[^.\\/]{1,5}$/, "") + ".pdf"
    }
    function saveExport(path, name) {
        pendingExport = path
        saveDialog.currentFile = App.suggestedFile(name)
        saveDialog.open()
    }

    function updateContent() {
        if (ctl.hasDocument || (inFolio && folioCtl.visibleItems.length > 0)) {
            sawContent = true
            if (ctl.hasDocument) drawer.close()
            return
        }
        if (!ctl.startupReady || sawContent || ctl.pageLoading) return
        drawer.open()
    }
    function syncPassword() {
        const need = viewerCtl.passwordNeeded || folioCtl.passwordNeeded
        if (need && !passwordDialog.opened) {
            pwField.text = ""
            passwordDialog.open()
        } else if (!need && passwordDialog.opened) {
            passwordDialog.close()
        }
    }

    Component.onCompleted: {
        syncNormalSize()
        syncLastSize()
        Keybinds.watchWindow(win)
        viewerCtl.strip = pageStrip
        if (Launch.file.length > 0) {
            viewerCtl.maybeRestoreLast(true)
            openPath(Launch.file, Launch.page, Launch.password)
        } else {
            viewerCtl.maybeRestoreLast(false)
        }
    }
    onClosing: saveCurrent()
    onActiveChanged: if (!active) saveCurrent()
    onXChanged: rememberWindowed()
    onYChanged: rememberWindowed()
    onWidthChanged: {
        rememberWindowed()
        syncNormalSize()
        syncLastSize()
    }
    onHeightChanged: {
        rememberWindowed()
        syncNormalSize()
        syncLastSize()
    }
    onVisibilityChanged: {
        if (restoringWindowed) return
        if (visibility === Window.Maximized && App.settings.fullScreenOnMaximize) {
            wantFullscreen = true
            showFullScreen()
            return
        }
        if (visibility === Window.FullScreen && !wantFullscreen && !presentationEnteredFullscreen
                && !App.settings.fullScreenOnMaximize) {
            Qt.callLater(function() {
                if (win.visibility === Window.FullScreen && !win.wantFullscreen
                        && !win.presentationEnteredFullscreen && !App.settings.fullScreenOnMaximize)
                    win.showMaximized()
            })
            return
        }
        if (visibility !== Window.FullScreen) wantFullscreen = false
    }

    Connections {
        target: App
        function onFileRequested(file, page, password) {
            if (win.visibility === Window.Minimized) win.showNormal()
            win.raise()
            win.requestActivate()
            if (file.length > 0) win.openPath(file, page, password)
        }
        function onAboutToDeleteFiles(paths) {
            viewerCtl.releaseIfUsing(paths)
            folioCtl.releaseIfUsing(paths)
        }
        function onSettingsChanged() {
            if (!App.settings.folioEnabled && win.inFolio) win.leaveFolio()
        }
    }

    PdfStripItem {
        id: pageStrip
        anchors.fill: parent
        anchors.topMargin: win.winPadV + win.reservedTop
        anchors.bottomMargin: win.winPadV
        anchors.leftMargin: win.winPadH
        anchors.rightMargin: win.winPadH
        focus: true
        Keys.priority: Keys.BeforeItem
        Keys.onPressed: (event) => win.handleAppKey(event)
    }

    Connections {
        target: pageStrip
        function onContextMenuRequested(x, y) {
            const p = pageStrip.mapToItem(Overlay.overlay, x, y)
            selectionMenu.popup(Overlay.overlay, p.x, p.y)
        }
    }

    MouseArea {
        id: sidebarDismiss
        z: 20
        visible: drawer.opened
        x: win.winPadH + drawer.width
        y: win.contentTop
        width: parent.width - x - win.winPadH
        height: parent.height - y - win.winPadV
        acceptedButtons: Qt.AllButtons
        propagateComposedEvents: true
        onPressed: mouse => { drawer.close(); mouse.accepted = false }
        onWheel: wheel => { drawer.close(); wheel.accepted = false }
    }

    TitleBar {
        id: titleBarItem
        visible: win.titleBarShown
        allowMove: true
        z: 40
        x: win.winPadH
        y: win.winPadV
        width: parent.width - win.winPadH * 2
        window: win
        title: win.chromeTitle
        onMaximizeRequested: win.toggleMaxRestore()
    }

    Repeater {
        model: win.isFullscreen || win.windowZoomed() ? [] : [
            { e: Qt.LeftEdge, w: 6, h: 0, x: 0, y: 0, c: Qt.SizeHorCursor },
            { e: Qt.RightEdge, w: 6, h: 0, x: 1, y: 0, c: Qt.SizeHorCursor },
            { e: Qt.TopEdge, w: 0, h: 6, x: 0, y: 0, c: Qt.SizeVerCursor },
            { e: Qt.BottomEdge, w: 0, h: 6, x: 0, y: 1, c: Qt.SizeVerCursor },
            { e: Qt.LeftEdge | Qt.TopEdge, w: 10, h: 10, x: 0, y: 0, c: Qt.SizeFDiagCursor },
            { e: Qt.RightEdge | Qt.TopEdge, w: 10, h: 10, x: 1, y: 0, c: Qt.SizeBDiagCursor },
            { e: Qt.LeftEdge | Qt.BottomEdge, w: 10, h: 10, x: 0, y: 1, c: Qt.SizeBDiagCursor },
            { e: Qt.RightEdge | Qt.BottomEdge, w: 10, h: 10, x: 1, y: 1, c: Qt.SizeFDiagCursor }
        ]
        MouseArea {
            z: 80
            width: modelData.w || parent.width
            height: modelData.h || parent.height
            x: modelData.x ? parent.width - width : 0
            y: modelData.y ? parent.height - height : 0
            cursorShape: modelData.c
            onPressed: win.startSystemResize(modelData.e)
        }
    }

    DropArea {
        anchors.fill: parent
        keys: ["text/uri-list"]
        onDropped: drop => {
            const paths = []
            for (let i = 0; i < drop.urls.length; i++) paths.push(App.localPath(drop.urls[i]))
            win.handlePaths(paths)
        }
    }

    MouseArea {
        anchors.fill: parent
        z: 1
        visible: win.chromeOpen || win.forwarding
        onPressed: mouse => {
            win.chromeOpen = false
            win.suppressMenuUntil = Number.MAX_VALUE
            win.forwarding = true
            pageStrip.forwardPointer(0, mouse.x, mouse.y)
        }
        onPositionChanged: mouse => pageStrip.forwardPointer(1, mouse.x, mouse.y)
        onReleased: mouse => {
            pageStrip.forwardPointer(2, mouse.x, mouse.y)
            win.suppressMenuUntil = Date.now() + 300
            win.forwarding = false
        }
        onCanceled: {
            pageStrip.forwardPointer(2, 0, 0)
            win.suppressMenuUntil = Date.now() + 300
            win.forwarding = false
        }
    }

    AppLogo {
        visible: !ctl.hasDocument && !ctl.pageLoading && !win.inFolio
        anchors.centerIn: parent
        size: 96
        color: App.appFontColor
    }

    ModeTopBar {
        z: 15
        visible: ctl.pageSelectMode
        x: win.winPadH
        y: win.contentTop
        width: parent.width - win.winPadH * 2
        title: ctl.selectedPages.length + " pages"
        onBack: ctl.exitPageSelect()
        ChromeIcon { icon: "reorder"; hint: "Rearrange"; enabled: ctl.selectedPages.length > 0; onClicked: rearrange.open() }
        ChromeIcon {
            readonly property bool all: ctl.totalPages > 0 && ctl.selectedPages.length === ctl.totalPages
            icon: all ? "check_box_outline_blank" : "select_all"
            hint: all ? "Deselect all" : "Select all"
            enabled: ctl.totalPages > 0
            onClicked: {
                const l = []
                if (!all) for (let i = 0; i < ctl.totalPages; i++) l.push(i)
                ctl.setSelectedPages(l)
            }
        }
        ChromeIcon { icon: "share"; hint: "Share"; enabled: ctl.selectedPages.length > 0; onClicked: ctl.shareSelectedPages(win.shareStoreRotation) }
    }
    ModeTopBar {
        z: 15
        visible: !ctl.pageSelectMode && ctl.selectionText.length === 0 && win.findOpen
        x: win.winPadH
        y: win.contentTop
        width: parent.width - win.winPadH * 2
        title: ctl.searching ? "Searching..." : "Search"
        onBack: win.closeFind()
    }
    function closeFind() {
        findOpen = false
        hitsOpen = false
        findField.text = ""
        ctl.search("")
    }

    Rectangle {
        id: topChrome
        z: 2
        x: win.winPadH
        y: win.contentTop
        visible: win.chromeOpen && !win.findOpen
        width: parent.width - win.winPadH * 2
        height: 64
        color: App.bgColor
        MouseArea { anchors.fill: parent }
        ChromeIcon {
            icon: "menu"; hint: "Menu"
            anchors { left: parent.left; leftMargin: 4; verticalCenter: parent.verticalCenter }
            onClicked: drawer.open()
        }
        Row {
            anchors.centerIn: parent
            ChromeIcon {
                icon: ctl.originalColors ? "invert_colors_off" : "invert_colors"
                hint: ctl.originalColors ? "Themed colours" : "Original colours"
                onClicked: ctl.toggleOriginalColors()
            }
            Item {
                width: 64; height: 48
                enabled: ctl.hasDocument
                AppText {
                    anchors.centerIn: parent
                    text: ctl.zoomLabel
                    color: App.appFontColor
                    opacity: ctl.hasDocument ? 0.85 : 0.35
                    font.pixelSize: Math.round(12 * App.uiScale)
                }
                MouseArea {
                    anchors.fill: parent
                    enabled: ctl.hasDocument
                    onClicked: { zoomField.text = String(Math.round(ctl.zoomPercent)); zoomDialog.open() }
                    onPressAndHold: toast.show("Zoom")
                }
            }
            ChromeIcon {
                icon: "search"; hint: "Find"; enabled: ctl.hasDocument
                onClicked: { win.chromeOpen = false; win.findOpen = true; findField.forceActiveFocus() }
            }
        }
        ChromeIcon {
            visible: win.inFolio
            icon: "save"; hint: "Save"; enabled: ctl.hasDocument
            anchors { right: parent.right; rightMargin: 4; verticalCenter: parent.verticalCenter }
            onClicked: {
                const p = ctl.readingPosition()
                folioCtl.saveFolioSnapshot(p.page, p.scrollX, p.scrollY)
                toast.show("Saved")
            }
        }
    }

    Rectangle {
        id: bottomChrome
        z: 2
        visible: win.chromeOpen && !win.findOpen
        x: win.winPadH
        anchors.bottom: parent.bottom
        anchors.bottomMargin: win.winPadV
        width: parent.width - win.winPadH * 2
        height: bottomCol.implicitHeight + 16
        color: App.bgColor
        MouseArea { anchors.fill: parent }
        DragHandler {
            enabled: win.inFolio
            target: null
            onTranslationChanged: {
                if (Math.abs(translation.y) > 24 && Math.abs(translation.y) > Math.abs(translation.x))
                    win.showPageScrubber = translation.y < 0
            }
        }
        Column {
            id: bottomCol
            anchors { left: parent.left; right: parent.right; margins: 16; verticalCenter: parent.verticalCenter }
            Row {
                anchors.horizontalCenter: parent.horizontalCenter
                ChromeIcon {
                    icon: "swap_horiz"; hint: "Lock side scrolling"
                    struck: ctl.lockSideScroll
                    onClicked: ctl.setLockSideScroll(!ctl.lockSideScroll)
                }
                ChromeIcon {
                    icon: "zoom_in"; hint: "Lock zoom"
                    struck: ctl.lockZoom
                    onClicked: ctl.setLockZoom(!ctl.lockZoom)
                }
                ChromeIcon {
                    icon: "width_full"; hint: "Fit width"; longHint: "Center horizontally"
                    onClicked: { if (ctl.hasDocument) ctl.fitWidth() }
                    onLongClicked: { if (ctl.hasDocument) ctl.centerHorizontally() }
                }
                ChromeIcon { icon: "bookmark_border"; hint: "Bookmarks"; onClicked: { if (ctl.hasDocument) bookmarks.open() } }
            }
            PageSlider {
                visible: win.inFolio
                width: parent.width
                enabled: folioCtl.visibleItems.length > 0
                currentPage: folioCtl.folioIndex + 1
                pageCount: Math.max(1, folioCtl.visibleItems.length)
                onChanged: page => folioCtl.showFolioIndex(page - 1)
                onLabelClicked: folioJump.open()
            }
            PageSlider {
                show: !win.inFolio || win.showPageScrubber
                width: parent.width
                enabled: ctl.hasDocument && ctl.visibleCount > 0
                currentPage: ctl.currentPage
                pageCount: ctl.visibleCount
                onChanged: page => { ctl.setScrubbing(true); ctl.goToPageIndex(page - 1) }
                onFinished: ctl.setScrubbing(false)
                onLabelClicked: { gotoField.text = String(ctl.currentPage); gotoDialog.open() }
            }
        }
    }

    Rectangle {
        visible: win.findOpen
        x: win.winPadH
        anchors.bottom: parent.bottom
        anchors.bottomMargin: win.winPadV
        width: parent.width - win.winPadH * 2
        height: 56
        color: App.bgColor
        MouseArea { anchors.fill: parent }
        RowLayout {
            anchors { fill: parent; margins: 8 }
            spacing: 0
            PapercutField {
                id: findField
                Layout.fillWidth: true
                placeholderText: "Find"
                color: App.appFontColor
                font.pixelSize: Math.round(16 * App.uiScale)
                onTextChanged: searchTimer.restart()
                onAccepted: ctl.search(text)
            }
            Timer { id: searchTimer; interval: 250; onTriggered: ctl.search(findField.text) }
            ChromeIcon { icon: "search"; hint: "Search"; enabled: findField.text.length > 0; onClicked: ctl.search(findField.text) }
            AppText {
                text: ctl.hitCount === 0 ? "0 / 0" : (ctl.hitIndex + 1) + " / " + ctl.hitCount
                color: App.appFontColor
                opacity: ctl.hitCount > 0 ? 1 : 0.4
                font.pixelSize: Math.round(12 * App.uiScale)
                Layout.leftMargin: 8
                MouseArea { anchors.fill: parent; enabled: ctl.hitCount > 0; onClicked: win.hitsOpen = true }
            }
            ChromeIcon { icon: "keyboard_arrow_left"; hint: "Previous"; enabled: ctl.hitCount > 0; onClicked: ctl.prevHit() }
            ChromeIcon { icon: "keyboard_arrow_right"; hint: "Next"; enabled: ctl.hitCount > 0; onClicked: ctl.nextHit() }
        }
    }

    PapercutOverlay {
        id: hitsOverlay
        occupyTop: win.contentTop
        occupyLeft: win.winPadH
        occupyRight: win.winPadH
        occupyBottom: win.winPadV
        overlayVisible: win.hitsOpen && win.findOpen
        title: "Matches"
        interceptBack: true
        onBackRequested: win.hitsOpen = false
        ListView {
            anchors.fill: parent
            clip: true
            boundsBehavior: Flickable.StopAtBounds
            ScrollBar.vertical: ScrollBar {}
            model: ctl.hitList
            delegate: Item {
                width: ListView.view.width
                height: 56
                Column {
                    anchors { left: parent.left; right: parent.right; verticalCenter: parent.verticalCenter; leftMargin: 16; rightMargin: 16 }
                    AppText { text: modelData.snippet || "…"; color: App.appFontColor; opacity: index === ctl.hitIndex ? 1 : 0.7; font.pixelSize: Math.round(16 * App.uiScale); elide: Text.ElideRight; width: parent.width }
                    AppText { text: "Page " + (modelData.page + 1); color: App.appFontColor; opacity: index === ctl.hitIndex ? 0.7 : 0.45; font.pixelSize: Math.round(12 * App.uiScale) }
                }
                MouseArea { anchors.fill: parent; onClicked: { ctl.selectHit(index); win.hitsOpen = false } }
            }
        }
    }
    PapercutOverlay {
        id: tocOverlay
        occupyTop: win.contentTop
        occupyLeft: win.winPadH
        occupyRight: win.winPadH
        occupyBottom: win.winPadV
        overlayVisible: win.tocOpen
        title: "Table of Contents"
        interceptBack: true
        onBackRequested: win.tocOpen = false
        ListView {
            anchors.fill: parent
            clip: true
            boundsBehavior: Flickable.StopAtBounds
            ScrollBar.vertical: ScrollBar {}
            model: ctl.outline
            delegate: Item {
                width: ListView.view.width
                height: 44
                AppText {
                    anchors { left: parent.left; right: parent.right; verticalCenter: parent.verticalCenter; leftMargin: 16 + modelData.depth * 16; rightMargin: 16 }
                    text: modelData.title
                    color: App.appFontColor
                    opacity: modelData.pageIndex >= 0 ? 0.7 : 0.4
                    font.pixelSize: Math.round(16 * App.uiScale)
                    elide: Text.ElideRight
                }
                MouseArea {
                    anchors.fill: parent
                    enabled: modelData.pageIndex >= 0
                    onClicked: { win.tocOpen = false; ctl.goToPageIndex(modelData.pageIndex, modelData.hasY ? modelData.y : -1) }
                }
            }
        }
    }

    Rectangle {
        visible: ctl.busy.length === 0 && (ctl.pageLoading || (win.inFolio && !ctl.hasDocument && folioCtl.visibleItems.length > 0))
        anchors.fill: parent
        anchors.topMargin: win.contentTop
        anchors.leftMargin: win.winPadH
        anchors.rightMargin: win.winPadH
        anchors.bottomMargin: win.winPadV
        color: Qt.rgba(App.bgColor.r, App.bgColor.g, App.bgColor.b, 0.92)
        MouseArea { anchors.fill: parent }
        AppText { anchors.centerIn: parent; text: "Loading..."; color: App.appFontColor }
    }
    Rectangle {
        visible: ctl.busy.length > 0 || combiner.busy.length > 0
        anchors.centerIn: parent
        width: busyText.width + 48
        height: busyText.height + 48
        color: Qt.rgba(App.bgColor.r, App.bgColor.g, App.bgColor.b, 0.92)
        AppText { id: busyText; anchors.centerIn: parent; text: ctl.busy.length ? ctl.busy : combiner.busy; color: App.appFontColor }
    }

    Rectangle {
        id: drawer
        property bool opened: false
        function open() { opened = true }
        function close() { opened = false }
        visible: opened
        width: 280
        x: win.winPadH
        y: win.contentTop
        height: parent.height - y - win.winPadV
        z: 35
        color: App.bgColor
        Flickable {
            anchors.fill: parent
            contentHeight: drawerCol.implicitHeight
            clip: true
            boundsBehavior: Flickable.StopAtBounds
            Column {
                id: drawerCol
                width: drawer.width
                Item {
                    width: parent.width; height: 64
                    AppText {
                        x: 16
                        anchors.verticalCenter: parent.verticalCenter
                        visible: !win.isFullscreen
                        text: "Papercut"
                        color: App.appFontColor
                        font.pixelSize: Math.round(22 * App.uiScale)
                    }
                }
                Rectangle { width: parent.width; height: 1; color: "#5F6368"; opacity: 0.5 }
                Row {
                    visible: win.folioEnabled
                    width: parent.width
                    leftPadding: 16
                    topPadding: 10
                    bottomPadding: 10
                    spacing: 20
                    AppText {
                        text: "Viewer"
                        color: App.appFontColor
                        opacity: win.inFolio ? 0.4 : 1
                        font.pixelSize: Math.round(16 * App.uiScale)
                        MouseArea { anchors.fill: parent; enabled: win.inFolio; onClicked: { drawer.close(); win.leaveFolio() } }
                    }
                    AppText {
                        text: "Folio"
                        color: App.appFontColor
                        opacity: win.inFolio ? 1 : 0.4
                        font.pixelSize: Math.round(16 * App.uiScale)
                        MouseArea { anchors.fill: parent; enabled: !win.inFolio; onClicked: { drawer.close(); win.enterFolio() } }
                    }
                }
                DrawerItem { label: win.inFolio ? "Add" : "Open"; onClicked: { drawer.close(); win.showOpenDialog() } }
                DrawerItem { label: "Share"; enabled: ctl.hasDocument; onClicked: { drawer.close(); shareDialog.open() } }
                DrawerItem {
                    visible: win.folioEnabled && !win.inFolio
                    label: "Add to Folio"
                    enabled: ctl.hasDocument && !viewerCtl.alreadyInFolio()
                    onClicked: { drawer.close(); viewerCtl.addCurrentToFolio() }
                }
                DrawerItem { visible: !win.inFolio; label: "Combine"; onClicked: { drawer.close(); combine.open() } }
                Rectangle { width: parent.width; height: 1; color: "#5F6368"; opacity: 0.5 }
                DrawerItem {
                    visible: win.inFolio
                    label: "Manage Folio"
                    onClicked: { drawer.close(); manageFolio.open() }
                }
                DrawerItem {
                    visible: !win.inFolio
                    label: "Table of Contents"
                    enabled: ctl.hasDocument && ctl.hasOutline
                    onClicked: { drawer.close(); win.tocOpen = true }
                }
                Rectangle { width: parent.width; height: 1; color: "#5F6368"; opacity: 0.5 }
                DrawerItem { label: "About"; onClicked: { drawer.close(); about.open() } }
                DrawerItem { label: "Settings"; onClicked: { drawer.close(); settings.open() } }
                Rectangle { width: parent.width; height: 1; color: "#5F6368"; opacity: 0.5 }
                DrawerItem { label: "Quit"; onClicked: win.quitApp() }
            }
        }
    }

    PapercutDialog {
        id: shareDialog
        title: "Share"
        confirmText: ""
        dismissText: "Cancel"
        Button {
            flat: true
            width: parent.width
            padding: 0
            text: "Full"
            contentItem: AppText { text: parent.text; color: App.appFontColor; font.pixelSize: Math.round(16 * App.uiScale); horizontalAlignment: Text.AlignLeft }
            background: Item {}
            onClicked: { shareDialog.close(); ctl.shareFull(win.shareStoreRotation) }
        }
        Button {
            flat: true
            width: parent.width
            padding: 0
            enabled: ctl.isPdf
            text: "Partial"
            contentItem: AppText { text: parent.text; color: App.appFontColor; opacity: parent.enabled ? 1 : 0.4; font.pixelSize: Math.round(16 * App.uiScale); horizontalAlignment: Text.AlignLeft }
            background: Item {}
            onClicked: {
                shareDialog.close()
                if (ctl.isPdf) ctl.startPageSelect()
                else toast.show("Partial share needs a PDF")
            }
        }
        Rectangle { width: parent.width; height: 1; color: "#5F6368"; opacity: 0.5 }
        SettingsSwitchRow {
            label: "Store rotation"
            checked: win.shareStoreRotation
            onToggled: on => win.shareStoreRotation = on
        }
    }
    PapercutDialog {
        id: zoomDialog
        title: "Zoom"
        confirmText: "OK"
        onOpened: { zoomField.forceActiveFocus(); zoomField.selectAll() }
        onAccepted: {
            if (ctl.presenting) return
            const v = parseFloat(zoomField.text)
            if (!isNaN(v)) ctl.setZoomPercent(Math.max(25, Math.min(400, v)))
        }
        PapercutField {
            id: zoomField
            width: parent.width
            color: App.appFontColor
            placeholderText: "%"
            validator: IntValidator { bottom: 0; top: 9999 }
            inputMethodHints: Qt.ImhDigitsOnly
            onAccepted: { zoomDialog.close(); zoomDialog.accepted() }
        }
        SettingsSwitchRow {
            label: "Presentation"
            checked: ctl.presenting
            enabled: ctl.hasDocument
            onToggled: on => ctl.setPresenting(on)
        }
    }
    PapercutDialog {
        id: gotoDialog
        title: "Go to page"
        confirmText: "OK"
        onOpened: { gotoField.forceActiveFocus(); gotoField.selectAll() }
        onAccepted: {
            const n = parseInt(gotoField.text)
            if (!isNaN(n) && n >= 1 && n <= ctl.visibleCount) ctl.goToPageIndex(n - 1)
        }
        PapercutField {
            id: gotoField
            width: parent.width
            placeholderText: "Page"
            color: App.appFontColor
            validator: IntValidator { bottom: 0; top: 999999 }
            inputMethodHints: Qt.ImhDigitsOnly
            onAccepted: { gotoDialog.close(); gotoDialog.accepted() }
        }
    }
    PapercutDialog {
        id: passwordDialog
        title: "Password"
        confirmText: "OK"
        confirmEnabled: pwField.text.length > 0
        closeOnConfirm: false
        closePolicy: Popup.NoAutoClose
        onOpened: pwField.forceActiveFocus()
        onAccepted: { if (pwField.text.length > 0) win.pwCtl.submitPassword(pwField.text) }
        onRejected: win.pwCtl.cancelPassword()
        AppText { text: "This PDF is password protected"; color: App.appFontColor; wrapMode: Text.Wrap; width: parent.width }
        PapercutField {
            id: pwField
            width: parent.width
            echoMode: TextInput.Password
            color: App.appFontColor
            placeholderText: "Password"
            onAccepted: passwordDialog.accepted()
        }
        AppText { visible: win.pwCtl.passwordWrong; text: "Wrong password"; color: App.appFontColor; opacity: 0.75 }
    }

    FileDialog {
        id: openDialog
        nameFilters: App.openFilters()
        fileMode: FileDialog.OpenFile
        onAccepted: {
            const paths = []
            if (selectedFiles && selectedFiles.length) {
                for (let i = 0; i < selectedFiles.length; i++) paths.push(App.localPath(selectedFiles[i]))
            } else {
                paths.push(App.localPath(selectedFile))
            }
            win.handlePaths(paths)
        }
    }
    FileDialog {
        id: saveDialog
        fileMode: FileDialog.SaveFile
        defaultSuffix: "pdf"
        nameFilters: ["PDF (*.pdf)", "All files (*)"]
        onAccepted: toast.show(App.copyFileTo(win.pendingExport, App.localPath(selectedFile)) ? "Saved" : "Could not save the file")
    }
    FileDialog {
        id: saveTextDialog
        fileMode: FileDialog.SaveFile
        defaultSuffix: "txt"
        nameFilters: ["Text (*.txt)", "All files (*)"]
        onAccepted: toast.show(App.writeTextFile(App.localPath(selectedFile), win.pendingText) ? "Saved" : "Could not save the file")
    }
    Menu {
        id: selectionMenu
        palette.windowText: App.appFontColor
        palette.text: App.appFontColor
        palette.buttonText: App.appFontColor
        palette.window: App.bgColor
        background: Rectangle {
            implicitWidth: 168
            color: App.bgColor
            border.width: 0
            radius: 8
        }
        MenuItem {
            text: "Copy"
            contentItem: AppText {
                text: parent.text
                color: App.appFontColor
                font.pixelSize: Math.round(14 * App.uiScale)
                leftPadding: 12
                verticalAlignment: Text.AlignVCenter
            }
            background: Rectangle {
                color: parent.highlighted ? Qt.rgba(App.appFontColor.r, App.appFontColor.g, App.appFontColor.b, 0.12) : "transparent"
            }
            onTriggered: ctl.copySelection()
        }
        MenuItem {
            text: "Share"
            contentItem: AppText {
                text: parent.text
                color: App.appFontColor
                font.pixelSize: Math.round(14 * App.uiScale)
                leftPadding: 12
                verticalAlignment: Text.AlignVCenter
            }
            background: Rectangle {
                color: parent.highlighted ? Qt.rgba(App.appFontColor.r, App.appFontColor.g, App.appFontColor.b, 0.12) : "transparent"
            }
            onTriggered: {
                win.pendingText = ctl.selectionText
                saveTextDialog.currentFile = App.suggestedFile("Papercut-selection.txt")
                saveTextDialog.open()
            }
        }
    }

    BookmarkOverlay { id: bookmarks; ctl: win.ctl; occupyTop: win.contentTop; occupyLeft: win.winPadH; occupyRight: win.winPadH; occupyBottom: win.winPadV }
    RearrangeOverlay { id: rearrange; ctl: win.ctl; occupyTop: win.contentTop; occupyLeft: win.winPadH; occupyRight: win.winPadH; occupyBottom: win.winPadV }
    FolioJumpOverlay { id: folioJump; folio: folioCtl; occupyTop: win.contentTop; occupyLeft: win.winPadH; occupyRight: win.winPadH; occupyBottom: win.winPadV; onAdd: function() { win.showOpenDialog() }; onManage: function() { manageFolio.open() } }
    ManageFolioOverlay { id: manageFolio; folio: folioCtl; occupyTop: win.contentTop; occupyLeft: win.winPadH; occupyRight: win.winPadH; occupyBottom: win.winPadV }
    CombineOverlay { id: combine; combiner: combiner; occupyTop: win.contentTop; occupyLeft: win.winPadH; occupyRight: win.winPadH; occupyBottom: win.winPadV; onAdd: function() { win.showOpenDialog() } }
    SettingsOverlay {
        id: settings
        occupyTop: win.contentTop
        occupyLeft: win.winPadH
        occupyRight: win.winPadH
        occupyBottom: win.winPadV
        toastFn: function(msg) { toast.show(msg) }
        onExportReady: function(path, name) { win.saveExport(path, name && name.length ? name : "Papercut.pdf") }
    }
    AboutOverlay { id: about; occupyTop: win.contentTop; occupyLeft: win.winPadH; occupyRight: win.winPadH; occupyBottom: win.winPadV }

    Rectangle {
        id: toast
        function show(msg) { toastText.text = msg; opacity = 1; toastTimer.restart() }
        opacity: 0
        visible: opacity > 0
        z: 50
        anchors { horizontalCenter: parent.horizontalCenter; bottom: parent.bottom; bottomMargin: 96 + win.winPadV }
        width: toastText.width + 32
        height: toastText.height + 20
        radius: 20
        color: "#CC2A2A2E"
        Behavior on opacity { NumberAnimation { duration: 200 } }
        AppText { id: toastText; anchors.centerIn: parent; color: "#E8EAED"; font.pixelSize: Math.round(14 * App.uiScale) }
        Timer { id: toastTimer; interval: 2200; onTriggered: toast.opacity = 0 }
    }

    function jumpBookmarkSlot(n) {
        if (!ctl.hasDocument) return
        const list = ctl.bookmarks
        if (!list || n < 0 || n >= list.length) {
            toast.show("No bookmark " + (n + 1))
            return
        }
        ctl.jumpBookmark(list[n])
    }
    function runAction(id) {
        if (Keybinds.capturing) return
        if (String(id).indexOf("bookmark") === 0) {
            const digit = id === "bookmark0" ? 9 : parseInt(String(id).slice(8), 10) - 1
            jumpBookmarkSlot(digit)
            return
        }
        switch (id) {
        case "open":
            win.showOpenDialog()
            break
        case "combine":
            if (win.inFolio) toast.show("Combine is in Viewer")
            else { drawer.close(); combine.open() }
            break
        case "quit":
            win.quitApp()
            break
        case "viewer":
            drawer.close()
            if (win.inFolio) win.leaveFolio()
            break
        case "folio":
            if (!win.folioEnabled) { toast.show("Folio is off"); break }
            drawer.close()
            if (!win.inFolio) win.enterFolio()
            break
        case "bookmarks":
            if (bookmarks.overlayVisible) bookmarks.back()
            else if (ctl.hasDocument) { win.chromeOpen = false; bookmarks.open() }
            break
        case "settings":
            if (settings.overlayVisible) settings.escapePressed()
            else { drawer.close(); settings.open() }
            break
        case "closeDocument":
            ctl.closeDocument()
            break
        case "openSidebar":
            if (drawer.visible) drawer.close()
            else drawer.open()
            break
        case "toggleTitleBar":
            win.toggleTitleBar()
            break
        case "toggleMenuBar":
            win.chromeOpen = !win.chromeOpen
            break
        case "grabWindow":
            break
        case "fullScreen":
            win.toggleFullScreen()
            break
        case "togglePresentation":
            ctl.togglePresentation()
            break
        case "rotateLeft":
            if (ctl.hasDocument) ctl.rotateCurrent(-90)
            break
        case "rotateRight":
            if (ctl.hasDocument) ctl.rotateCurrent(90)
            break
        case "rotateAllLeft":
            if (ctl.hasDocument) ctl.rotateAll(-90)
            break
        case "rotateAllRight":
            if (ctl.hasDocument) ctl.rotateAll(90)
            break
        case "find":
            if (!ctl.hasDocument) break
            win.chromeOpen = false
            win.findOpen = true
            findField.forceActiveFocus()
            break
        case "goToPage":
            if (!ctl.hasDocument) break
            gotoField.text = String(ctl.currentPage)
            gotoDialog.open()
            break
        case "colourMode":
            if (ctl.hasDocument) ctl.toggleOriginalColors()
            break
        case "zoomIn":
            if (ctl.hasDocument) ctl.setZoomPercent(Math.max(25, Math.min(400, ctl.zoomPercent * 1.1)))
            break
        case "zoomOut":
            if (ctl.hasDocument) ctl.setZoomPercent(Math.max(25, Math.min(400, ctl.zoomPercent / 1.1)))
            break
        case "fitWidth":
            if (ctl.hasDocument) ctl.fitWidth()
            break
        case "lockHScroll":
            if (ctl.hasDocument) ctl.setLockSideScroll(!ctl.lockSideScroll)
            break
        case "shareFull":
            if (ctl.hasDocument) ctl.shareFull(win.shareStoreRotation)
            break
        case "sharePartial":
            if (!ctl.hasDocument) break
            if (ctl.isPdf) ctl.startPageSelect()
            else toast.show("Partial share needs a PDF")
            break
        case "copy":
            if (ctl.selectionText.length > 0) ctl.copySelection()
            break
        case "selectAll":
            if (ctl.hasDocument) ctl.selectAll()
            break
        case "scrollUp":
            if (ctl.presenting) ctl.stepPage(-1)
            else pageStrip.panByPixels(0, -60)
            break
        case "scrollDown":
            if (ctl.presenting) ctl.stepPage(1)
            else pageStrip.panByPixels(0, 60)
            break
        case "scrollLeft":
            if (ctl.presenting) ctl.stepPage(-1)
            else pageStrip.panByPixels(60, 0)
            break
        case "scrollRight":
            if (ctl.presenting) ctl.stepPage(1)
            else pageStrip.panByPixels(-60, 0)
            break
        case "pageDown":
            if (!ctl.hasDocument) break
            if (ctl.presenting) ctl.stepPage(1)
            else pageStrip.panByPixels(0, Math.round(pageStrip.height * 0.9))
            break
        case "pageUp":
            if (!ctl.hasDocument) break
            if (ctl.presenting) ctl.stepPage(-1)
            else pageStrip.panByPixels(0, -Math.round(pageStrip.height * 0.9))
            break
        case "goToStart":
            if (ctl.hasDocument) ctl.goToPageIndex(0)
            break
        case "goToEnd":
            if (ctl.hasDocument && ctl.totalPages > 0) ctl.goToPageIndex(ctl.totalPages - 1)
            break
        }
    }

    Item {
        width: 0
        height: 0
        Repeater {
            model: Keybinds.shortcutEntries
            Item {
                Shortcut {
                    sequence: modelData.sequence
                    autoRepeat: !!modelData.repeat
                    enabled: !Keybinds.capturing && !(ctl.presenting && String(modelData.id).indexOf("scroll") === 0)
                    onActivated: {
                        if (modelData.id === "selectAll" || modelData.id === "copy") {
                            const f = win.activeFocusItem
                            if (f && f.text !== undefined && typeof f.selectAll === "function") return
                        }
                        win.runAction(modelData.id)
                    }
                }
            }
        }
    }
    Keys.onPressed: (event) => {
        if (!pageStrip.activeFocus) win.handleAppKey(event)
        if (event.accepted) return
        if (!ctl.presenting || pageStrip.activeFocus) return
        if (event.key === Qt.Key_Down || event.key === Qt.Key_Right) {
            ctl.stepPage(1)
            event.accepted = true
        } else if (event.key === Qt.Key_Up || event.key === Qt.Key_Left) {
            ctl.stepPage(-1)
            event.accepted = true
        }
    }
    Shortcut {
        sequence: "Escape"
        onActivated: {
            if (about.overlayVisible) about.back()
            else if (settings.overlayVisible) settings.escapePressed()
            else if (combine.overlayVisible) combine.back()
            else if (manageFolio.overlayVisible) manageFolio.back()
            else if (folioJump.overlayVisible) folioJump.back()
            else if (bookmarks.overlayVisible) bookmarks.back()
            else if (rearrange.overlayVisible) rearrange.back()
            else if (win.tocOpen) win.tocOpen = false
            else if (win.hitsOpen) win.hitsOpen = false
            else if (ctl.pageSelectMode) ctl.exitPageSelect()
            else if (drawer.visible) drawer.close()
            else if (ctl.selectionText.length > 0) pageStrip.doClearSelection()
            else if (win.findOpen) win.closeFind()
            else if (win.chromeOpen) win.chromeOpen = false
            else if (ctl.presenting) ctl.setPresenting(false)
            else drawer.open()
        }
    }
}
