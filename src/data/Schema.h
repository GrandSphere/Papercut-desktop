#pragma once

namespace papercut {

// Room schema v21.
inline constexpr const char kCreateSchema[] = R"SQL(
PRAGMA foreign_keys = ON;

CREATE TABLE IF NOT EXISTS settings (
    id INTEGER NOT NULL PRIMARY KEY,
    fgColor INTEGER NOT NULL,
    bgColor INTEGER NOT NULL,
    imageColorBlend REAL NOT NULL,
    zoomMode TEXT NOT NULL,
    customZoomPercent REAL NOT NULL,
    restoreLastPdf INTEGER NOT NULL,
    lastPdfUri TEXT,
    rememberLastZoom INTEGER NOT NULL,
    startupZoomMode TEXT NOT NULL,
    startupZoomPercent REAL NOT NULL,
    lockZoom INTEGER NOT NULL,
    lockSideScroll INTEGER NOT NULL,
    fontScale REAL NOT NULL,
    highlightColor INTEGER NOT NULL,
    pagePaddingDp INTEGER NOT NULL,
    horizontalPagePaddingDp INTEGER NOT NULL,
    pageBorderEnabled INTEGER NOT NULL,
    pageBorderColor INTEGER NOT NULL,
    allowFollowLinks INTEGER NOT NULL,
    doubleTapMenu INTEGER NOT NULL,
    singleTapMenu INTEGER NOT NULL,
    swipeFromBottomMenu INTEGER NOT NULL,
    appFontColor INTEGER NOT NULL,
    actionColor INTEGER NOT NULL,
    originalColors INTEGER NOT NULL,
    twoFingerPan INTEGER NOT NULL,
    autoDisablePan INTEGER NOT NULL,
    allowTextSelection INTEGER NOT NULL,
    restoreLastPosition INTEGER NOT NULL,
    alwaysDarkMode INTEGER NOT NULL,
    imageContrast REAL NOT NULL,
    imageAlpha REAL NOT NULL,
    darkImageBlend REAL NOT NULL,
    darkImageContrast REAL NOT NULL,
    darkImageAlpha REAL NOT NULL,
    lastPageIndex INTEGER NOT NULL,
    lastScrollXPdf REAL NOT NULL,
    lastScrollYPdf REAL NOT NULL,
    folioEnabled INTEGER NOT NULL,
    openFolioOnStartup INTEGER NOT NULL,
    folioOpenLastViewed INTEGER NOT NULL,
    folioStoreLastPosition INTEGER NOT NULL,
    allowMultipleInstances INTEGER NOT NULL,
    lastFolioItemId INTEGER NOT NULL,
    fullScreenOnMaximize INTEGER NOT NULL DEFAULT 0,
    centerHorizontally INTEGER NOT NULL DEFAULT 0
);

CREATE TABLE IF NOT EXISTS documents (
    uri TEXT NOT NULL PRIMARY KEY,
    displayName TEXT NOT NULL,
    displayPath TEXT NOT NULL,
    lastPageIndex INTEGER NOT NULL DEFAULT 0,
    lastScrollXPdf REAL NOT NULL DEFAULT 0,
    lastScrollYPdf REAL NOT NULL DEFAULT 0
);

CREATE TABLE IF NOT EXISTS bookmarks (
    id INTEGER PRIMARY KEY AUTOINCREMENT NOT NULL,
    documentUri TEXT NOT NULL,
    name TEXT NOT NULL,
    pageIndex INTEGER NOT NULL,
    scrollXPdf REAL NOT NULL,
    scrollYPdf REAL NOT NULL,
    sortOrder INTEGER NOT NULL,
    FOREIGN KEY(documentUri) REFERENCES documents(uri) ON DELETE CASCADE
);
CREATE INDEX IF NOT EXISTS index_bookmarks_documentUri ON bookmarks(documentUri);

CREATE TABLE IF NOT EXISTS folio_items (
    id INTEGER PRIMARY KEY AUTOINCREMENT NOT NULL,
    uri TEXT NOT NULL,
    displayName TEXT NOT NULL,
    displayPath TEXT NOT NULL,
    sortOrder INTEGER NOT NULL,
    hidden INTEGER NOT NULL DEFAULT 0,
    isStartItem INTEGER NOT NULL DEFAULT 0,
    zoomMode TEXT NOT NULL DEFAULT 'FIT_WIDTH',
    customZoomPercent REAL NOT NULL DEFAULT 100,
    lockZoom INTEGER NOT NULL DEFAULT 0,
    lockSideScroll INTEGER NOT NULL DEFAULT 1,
    originalColors INTEGER NOT NULL DEFAULT 0,
    lastPageIndex INTEGER NOT NULL DEFAULT 0,
    lastScrollXPdf REAL NOT NULL DEFAULT 0,
    lastScrollYPdf REAL NOT NULL DEFAULT 0,
    localName TEXT NOT NULL DEFAULT '',
    pageRotations TEXT NOT NULL DEFAULT '[]'
);
CREATE UNIQUE INDEX IF NOT EXISTS index_folio_items_uri ON folio_items(uri);
)SQL";

} // namespace papercut
