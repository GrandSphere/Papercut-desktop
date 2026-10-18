#pragma once

#include "ZoomMode.h"

#include <QtGlobal>
#include <algorithm>
#include <QString>
#include <QStringList>
#include <optional>

namespace papercut {

struct ViewerSettings {
    static constexpr int DEFAULT_FG = static_cast<int>(0xFFC8C8C8);
    static constexpr int DEFAULT_BG = static_cast<int>(0xFF121212);
    static constexpr int DEFAULT_HIGHLIGHT = static_cast<int>(0xFF707070);
    static constexpr int DEFAULT_PAGE_BORDER = static_cast<int>(0xFF707070);
    static constexpr int DEFAULT_APP_FONT = static_cast<int>(0xFFB3B3B3);
    static constexpr int DEFAULT_ACTION = static_cast<int>(0xFF545454);
    static constexpr int DEFAULT_APP_FONT_WEIGHT = 500;
    static constexpr float MIN_FONT_SCALE = 0.3f;
    static constexpr float MAX_FONT_SCALE = 3.f;
    static constexpr int DEFAULT_WINDOW_W = 1000;
    static constexpr int DEFAULT_WINDOW_H = 760;
    static constexpr int MIN_WINDOW_W = 360;
    static constexpr int MIN_WINDOW_H = 480;

    int id = 1;
    int fgColor = DEFAULT_FG;
    int bgColor = DEFAULT_BG;
    float imageColorBlend = 0.f;
    QString zoomMode = QStringLiteral("FIT_WIDTH");
    float customZoomPercent = 100.f;
    bool restoreLastPdf = false;
    std::optional<QString> lastPdfUri;
    bool rememberLastZoom = false;
    QString startupZoomMode = QStringLiteral("FIT_WIDTH");
    float startupZoomPercent = 100.f;
    bool lockZoom = false;
    bool lockSideScroll = false;
    float fontScale = 1.f;
    int highlightColor = DEFAULT_HIGHLIGHT;
    int pagePaddingDp = 0;
    int horizontalPagePaddingDp = 0;
    bool pageBorderEnabled = false;
    int pageBorderColor = DEFAULT_PAGE_BORDER;
    bool allowFollowLinks = true;
    bool doubleTapMenu = false;
    bool singleTapMenu = false;
    bool swipeFromBottomMenu = false;
    int appFontColor = DEFAULT_APP_FONT;
    int actionColor = DEFAULT_ACTION;
    bool originalColors = false;
    bool twoFingerPan = false;
    bool autoDisablePan = true;
    bool allowTextSelection = true;
    bool restoreLastPosition = true;
    bool alwaysDarkMode = false;
    float imageContrast = 1.f;
    float imageAlpha = 1.f;
    float darkImageBlend = 0.f;
    float darkImageContrast = 0.20f;
    float darkImageAlpha = 0.20f;
    int lastPageIndex = 0;
    float lastScrollXPdf = 0.f;
    float lastScrollYPdf = 0.f;
    bool folioEnabled = false;
    bool openFolioOnStartup = false;
    bool folioOpenLastViewed = true;
    bool folioStoreLastPosition = true;
    bool allowMultipleInstances = false;
    qint64 lastFolioItemId = 0;
    bool fullScreenOnMaximize = false;
    bool centerHorizontally = false;
    bool titleBarEnabled = true;
    int windowPaddingV = 0;
    int windowPaddingH = 0;
    int defaultWindowWidth = DEFAULT_WINDOW_W;
    int defaultWindowHeight = DEFAULT_WINDOW_H;
    QString startupWindowPosition = QStringLiteral("CENTER");
    QString filesRoot;
    QString appIcon = QStringLiteral("papercut-dark-off");
    QString appFontFamily = QStringLiteral("Roboto");
    int appFontWeight = DEFAULT_APP_FONT_WEIGHT;

    static int canonicalDefaultWindowWidth(int w) {
        if (w < MIN_WINDOW_W) return DEFAULT_WINDOW_W;
        return std::min(w, 16384);
    }
    static int canonicalDefaultWindowHeight(int h) {
        if (h < MIN_WINDOW_H) return DEFAULT_WINDOW_H;
        return std::min(h, 16384);
    }

    static QString canonicalStartupWindowPosition(QString id) {
        static const QStringList kOk{QStringLiteral("CENTER"), QStringLiteral("SNAP_LEFT"),
                                     QStringLiteral("SNAP_RIGHT"), QStringLiteral("MAXIMIZED"),
                                     QStringLiteral("FULLSCREEN")};
        const QString u = id.toUpper();
        return kOk.contains(u) ? u : QStringLiteral("CENTER");
    }

    static QString canonicalAppIcon(QString id) {
        if (id.endsWith(QLatin1String(".ico"), Qt::CaseInsensitive)) id.chop(4);
        static const QStringList kOk{
            QStringLiteral("papercut-dark"),
            QStringLiteral("papercut-light"),
            QStringLiteral("papercut-dark-off"),
            QStringLiteral("papercut-light-off"),
        };
        return kOk.contains(id) ? id : QStringLiteral("papercut-dark-off");
    }

    static QString canonicalAppFontFamily(QString id) {
        id = id.trimmed();
        if (id.isEmpty()) return QStringLiteral("Roboto");
        if (id.toLower() == QLatin1String("roboto")) return QStringLiteral("Roboto");
        return id;
    }

    static int canonicalAppFontWeight(int w) { return qBound(100, w, 900); }
    static float canonicalFontScale(float s) {
        return qRound(qBound(MIN_FONT_SCALE, s, MAX_FONT_SCALE) * 100.f) / 100.f;
    }

    ZoomMode lastMode() const { return zoomModeFromStorage(zoomMode); }
    ZoomMode startupMode() const { return zoomModeFromStorage(startupZoomMode); }
    ZoomMode effectiveMode() const { return rememberLastZoom ? lastMode() : startupMode(); }
    float effectivePercent() const { return rememberLastZoom ? customZoomPercent : startupZoomPercent; }

    ViewerSettings withExclusiveMenuTaps() const {
        ViewerSettings next = *this;
        if (next.doubleTapMenu && next.singleTapMenu) next.singleTapMenu = false;
        return next;
    }

    ViewerSettings withExclusiveStartup() const {
        ViewerSettings next = *this;
        if (!next.folioEnabled) next.openFolioOnStartup = false;
        if (next.openFolioOnStartup && next.restoreLastPdf) next.restoreLastPdf = false;
        return next;
    }

    bool operator==(const ViewerSettings &o) const {
        auto sameF = [](float a, float b) { return qFuzzyCompare(a + 1.f, b + 1.f); };
        return id == o.id && fgColor == o.fgColor && bgColor == o.bgColor && sameF(imageColorBlend, o.imageColorBlend)
               && zoomMode == o.zoomMode && sameF(customZoomPercent, o.customZoomPercent)
               && restoreLastPdf == o.restoreLastPdf && lastPdfUri == o.lastPdfUri
               && rememberLastZoom == o.rememberLastZoom && startupZoomMode == o.startupZoomMode
               && sameF(startupZoomPercent, o.startupZoomPercent) && lockZoom == o.lockZoom
               && lockSideScroll == o.lockSideScroll && sameF(fontScale, o.fontScale)
               && highlightColor == o.highlightColor && pagePaddingDp == o.pagePaddingDp
               && horizontalPagePaddingDp == o.horizontalPagePaddingDp && pageBorderEnabled == o.pageBorderEnabled
               && pageBorderColor == o.pageBorderColor && allowFollowLinks == o.allowFollowLinks
               && doubleTapMenu == o.doubleTapMenu && singleTapMenu == o.singleTapMenu
               && swipeFromBottomMenu == o.swipeFromBottomMenu && appFontColor == o.appFontColor
               && actionColor == o.actionColor && originalColors == o.originalColors
               && twoFingerPan == o.twoFingerPan && autoDisablePan == o.autoDisablePan
               && allowTextSelection == o.allowTextSelection && restoreLastPosition == o.restoreLastPosition
               && alwaysDarkMode == o.alwaysDarkMode && sameF(imageContrast, o.imageContrast)
               && sameF(imageAlpha, o.imageAlpha) && sameF(darkImageBlend, o.darkImageBlend)
               && sameF(darkImageContrast, o.darkImageContrast) && sameF(darkImageAlpha, o.darkImageAlpha)
               && lastPageIndex == o.lastPageIndex && sameF(lastScrollXPdf, o.lastScrollXPdf)
               && sameF(lastScrollYPdf, o.lastScrollYPdf) && folioEnabled == o.folioEnabled
               && openFolioOnStartup == o.openFolioOnStartup && folioOpenLastViewed == o.folioOpenLastViewed
               && folioStoreLastPosition == o.folioStoreLastPosition
               && allowMultipleInstances == o.allowMultipleInstances && lastFolioItemId == o.lastFolioItemId
               && fullScreenOnMaximize == o.fullScreenOnMaximize && centerHorizontally == o.centerHorizontally
               && titleBarEnabled == o.titleBarEnabled && windowPaddingV == o.windowPaddingV
               && windowPaddingH == o.windowPaddingH && defaultWindowWidth == o.defaultWindowWidth
               && defaultWindowHeight == o.defaultWindowHeight
               && startupWindowPosition == o.startupWindowPosition && filesRoot == o.filesRoot
               && appIcon == o.appIcon
               && appFontFamily == o.appFontFamily && appFontWeight == o.appFontWeight;
    }

    bool appearanceEqual(const ViewerSettings &o) const {
        ViewerSettings a = *this, b = o;
        a.lastPageIndex = b.lastPageIndex = 0;
        a.lastScrollXPdf = b.lastScrollXPdf = 0.f;
        a.lastScrollYPdf = b.lastScrollYPdf = 0.f;
        a.lastPdfUri.reset();
        b.lastPdfUri.reset();
        a.lastFolioItemId = b.lastFolioItemId = 0;
        a.zoomMode.clear();
        b.zoomMode.clear();
        a.customZoomPercent = b.customZoomPercent = 0.f;
        a.lockZoom = b.lockZoom = false;
        a.lockSideScroll = b.lockSideScroll = false;
        a.originalColors = b.originalColors = false;
        return a == b;
    }
};

struct PdfDocument {
    QString uri;
    QString displayName;
    QString displayPath;
    int lastPageIndex = 0;
    float lastScrollXPdf = 0.f;
    float lastScrollYPdf = 0.f;
};

struct Bookmark {
    qint64 id = 0;
    QString documentUri;
    QString name;
    int pageIndex = 0;
    float scrollXPdf = 0.f;
    float scrollYPdf = 0.f;
    int sortOrder = 0;
};

struct FolioItem {
    qint64 id = 0;
    QString uri;
    QString displayName;
    QString displayPath;
    int sortOrder = 0;
    bool hidden = false;
    bool isStartItem = false;
    QString zoomMode = QStringLiteral("FIT_WIDTH");
    float customZoomPercent = 100.f;
    bool lockZoom = false;
    bool lockSideScroll = true;
    bool originalColors = false;
    int lastPageIndex = 0;
    float lastScrollXPdf = 0.f;
    float lastScrollYPdf = 0.f;
    QString localName;
    QString pageRotations = QStringLiteral("[]");
};

} // namespace papercut
