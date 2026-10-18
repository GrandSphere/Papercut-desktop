#include "BackupJson.h"

#include <stdexcept>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

namespace papercut {
namespace {

int intOr(const QJsonObject &obj, int fallback, const char *key) {
    if (!obj.contains(QLatin1String(key)) || obj.value(QLatin1String(key)).isNull())
        return fallback;
    return obj.value(QLatin1String(key)).toInt(fallback);
}

float floatOr(const QJsonObject &obj, float fallback, const char *key) {
    if (!obj.contains(QLatin1String(key)) || obj.value(QLatin1String(key)).isNull())
        return fallback;
    return static_cast<float>(obj.value(QLatin1String(key)).toDouble(fallback));
}

bool boolOr(const QJsonObject &obj, bool fallback, const char *key) {
    if (!obj.contains(QLatin1String(key)) || obj.value(QLatin1String(key)).isNull())
        return fallback;
    const QJsonValue v = obj.value(QLatin1String(key));
    if (v.isBool()) return v.toBool();
    if (v.isDouble()) return v.toInt() != 0;
    return fallback;
}

QString stringOr(const QJsonObject &obj, const QString &fallback, const char *key) {
    if (!obj.contains(QLatin1String(key)) || obj.value(QLatin1String(key)).isNull())
        return fallback;
    const QString v = obj.value(QLatin1String(key)).toString(fallback);
    return v.isEmpty() ? fallback : v;
}

QJsonObject settingsObject(const ViewerSettings &s) {
    QJsonObject o;
    o.insert(QStringLiteral("fgColor"), s.fgColor);
    o.insert(QStringLiteral("bgColor"), s.bgColor);
    o.insert(QStringLiteral("imageColorBlend"), static_cast<double>(s.imageColorBlend));
    o.insert(QStringLiteral("zoomMode"), s.zoomMode);
    o.insert(QStringLiteral("customZoomPercent"), static_cast<double>(s.customZoomPercent));
    o.insert(QStringLiteral("rememberLastZoom"), s.rememberLastZoom);
    o.insert(QStringLiteral("lockZoom"), s.lockZoom);
    o.insert(QStringLiteral("lockSideScroll"), s.lockSideScroll);
    o.insert(QStringLiteral("originalColors"), s.originalColors);
    o.insert(QStringLiteral("startupZoomMode"), s.startupZoomMode);
    o.insert(QStringLiteral("startupZoomPercent"), static_cast<double>(s.startupZoomPercent));
    o.insert(QStringLiteral("restoreLastPdf"), s.restoreLastPdf);
    o.insert(QStringLiteral("restoreLastPosition"), s.restoreLastPosition);
    o.insert(QStringLiteral("fontScale"), static_cast<double>(s.fontScale));
    o.insert(QStringLiteral("highlightColor"), s.highlightColor);
    o.insert(QStringLiteral("pagePaddingDp"), s.pagePaddingDp);
    o.insert(QStringLiteral("horizontalPagePaddingDp"), s.horizontalPagePaddingDp);
    o.insert(QStringLiteral("pageBorderEnabled"), s.pageBorderEnabled);
    o.insert(QStringLiteral("pageBorderColor"), s.pageBorderColor);
    o.insert(QStringLiteral("allowFollowLinks"), s.allowFollowLinks);
    o.insert(QStringLiteral("doubleTapMenu"), s.doubleTapMenu);
    o.insert(QStringLiteral("singleTapMenu"), s.singleTapMenu);
    o.insert(QStringLiteral("swipeFromBottomMenu"), s.swipeFromBottomMenu);
    o.insert(QStringLiteral("appFontColor"), s.appFontColor);
    o.insert(QStringLiteral("actionColor"), s.actionColor);
    o.insert(QStringLiteral("twoFingerPan"), s.twoFingerPan);
    o.insert(QStringLiteral("autoDisablePan"), s.autoDisablePan);
    o.insert(QStringLiteral("allowTextSelection"), s.allowTextSelection);
    o.insert(QStringLiteral("alwaysDarkMode"), s.alwaysDarkMode);
    o.insert(QStringLiteral("imageContrast"), static_cast<double>(s.imageContrast));
    o.insert(QStringLiteral("imageAlpha"), static_cast<double>(s.imageAlpha));
    o.insert(QStringLiteral("darkImageBlend"), static_cast<double>(s.darkImageBlend));
    o.insert(QStringLiteral("darkImageContrast"), static_cast<double>(s.darkImageContrast));
    o.insert(QStringLiteral("darkImageAlpha"), static_cast<double>(s.darkImageAlpha));
    o.insert(QStringLiteral("openFolioOnStartup"), s.openFolioOnStartup);
    o.insert(QStringLiteral("folioOpenLastViewed"), s.folioOpenLastViewed);
    o.insert(QStringLiteral("folioEnabled"), s.folioEnabled);
    o.insert(QStringLiteral("folioStoreLastPosition"), s.folioStoreLastPosition);
    o.insert(QStringLiteral("allowMultipleInstances"), s.allowMultipleInstances);
    o.insert(QStringLiteral("fullScreenOnMaximize"), s.fullScreenOnMaximize);
    o.insert(QStringLiteral("centerHorizontally"), s.centerHorizontally);
    o.insert(QStringLiteral("titleBarEnabled"), s.titleBarEnabled);
    o.insert(QStringLiteral("windowPaddingV"), s.windowPaddingV);
    o.insert(QStringLiteral("windowPaddingH"), s.windowPaddingH);
    o.insert(QStringLiteral("defaultWindowWidth"), s.defaultWindowWidth);
    o.insert(QStringLiteral("defaultWindowHeight"), s.defaultWindowHeight);
    o.insert(QStringLiteral("startupWindowPosition"), s.startupWindowPosition);
    o.insert(QStringLiteral("filesRoot"), s.filesRoot);
    o.insert(QStringLiteral("appIcon"), s.appIcon);
    o.insert(QStringLiteral("appFontFamily"), s.appFontFamily);
    o.insert(QStringLiteral("appFontWeight"), s.appFontWeight);
    if (s.lastPdfUri) o.insert(QStringLiteral("lastPdfUri"), *s.lastPdfUri);
    return o;
}

QJsonObject documentObject(const PdfDocument &d) {
    QJsonObject o;
    o.insert(QStringLiteral("uri"), d.uri);
    o.insert(QStringLiteral("displayName"), d.displayName);
    o.insert(QStringLiteral("displayPath"), d.displayPath);
    o.insert(QStringLiteral("lastPageIndex"), d.lastPageIndex);
    o.insert(QStringLiteral("lastScrollXPdf"), static_cast<double>(d.lastScrollXPdf));
    o.insert(QStringLiteral("lastScrollYPdf"), static_cast<double>(d.lastScrollYPdf));
    return o;
}

QJsonObject bookmarkObject(const Bookmark &b) {
    QJsonObject o;
    o.insert(QStringLiteral("documentUri"), b.documentUri);
    o.insert(QStringLiteral("name"), b.name);
    o.insert(QStringLiteral("pageIndex"), b.pageIndex);
    o.insert(QStringLiteral("scrollXPdf"), static_cast<double>(b.scrollXPdf));
    o.insert(QStringLiteral("scrollYPdf"), static_cast<double>(b.scrollYPdf));
    o.insert(QStringLiteral("sortOrder"), b.sortOrder);
    return o;
}

std::optional<PdfDocument> documentFrom(const QJsonObject &obj) {
    const QString uri = stringOr(obj, {}, "uri");
    if (uri.isEmpty()) return std::nullopt;
    return PdfDocument{
        uri,
        stringOr(obj, QStringLiteral("document.pdf"), "displayName"),
        stringOr(obj, uri, "displayPath"),
        intOr(obj, 0, "lastPageIndex"),
        floatOr(obj, 0.f, "lastScrollXPdf"),
        floatOr(obj, 0.f, "lastScrollYPdf"),
    };
}

std::optional<Bookmark> bookmarkFrom(const QJsonObject &obj) {
    const QString documentUri = stringOr(obj, {}, "documentUri");
    if (documentUri.isEmpty()) return std::nullopt;
    return Bookmark{
        0,
        documentUri,
        stringOr(obj, QStringLiteral("Bookmark"), "name"),
        intOr(obj, 0, "pageIndex"),
        floatOr(obj, 0.f, "scrollXPdf"),
        floatOr(obj, 0.f, "scrollYPdf"),
        intOr(obj, 0, "sortOrder"),
    };
}

} // namespace

QString BackupJson::exportKind(Kind kind, const ViewerSettings &settings,
                               const QVector<PdfDocument> &documents,
                               const QVector<Bookmark> &bookmarks) {
    QJsonObject root;
    root.insert(QStringLiteral("format"), QLatin1String(kFormat));
    root.insert(QStringLiteral("schemaVersion"), kSchemaVersion);
    if (kind == Kind::All || kind == Kind::Settings)
        root.insert(QStringLiteral("settings"), settingsObject(settings));
    if (kind == Kind::All || kind == Kind::Bookmarks) {
        QJsonArray docs;
        for (const auto &d : documents) docs.append(documentObject(d));
        QJsonArray marks;
        for (const auto &b : bookmarks) marks.append(bookmarkObject(b));
        root.insert(QStringLiteral("documents"), docs);
        root.insert(QStringLiteral("bookmarks"), marks);
    }
    return QString::fromUtf8(QJsonDocument(root).toJson(QJsonDocument::Indented));
}

QString BackupJson::exportFolio(const QVector<FolioItem> &items) {
    QJsonObject root;
    root.insert(QStringLiteral("format"), QLatin1String(kFormat));
    root.insert(QStringLiteral("schemaVersion"), kSchemaVersion);
    QJsonArray arr;
    for (const FolioItem &item : items) {
        QJsonObject o;
        o.insert(QStringLiteral("uri"), item.uri);
        o.insert(QStringLiteral("displayName"), item.displayName);
        o.insert(QStringLiteral("displayPath"), item.displayPath);
        o.insert(QStringLiteral("sortOrder"), item.sortOrder);
        o.insert(QStringLiteral("hidden"), item.hidden);
        o.insert(QStringLiteral("isStartItem"), item.isStartItem);
        o.insert(QStringLiteral("zoomMode"), item.zoomMode);
        o.insert(QStringLiteral("customZoomPercent"), static_cast<double>(item.customZoomPercent));
        o.insert(QStringLiteral("lockZoom"), item.lockZoom);
        o.insert(QStringLiteral("lockSideScroll"), item.lockSideScroll);
        o.insert(QStringLiteral("originalColors"), item.originalColors);
        o.insert(QStringLiteral("lastPageIndex"), item.lastPageIndex);
        o.insert(QStringLiteral("lastScrollXPdf"), static_cast<double>(item.lastScrollXPdf));
        o.insert(QStringLiteral("lastScrollYPdf"), static_cast<double>(item.lastScrollYPdf));
        o.insert(QStringLiteral("pageRotations"), item.pageRotations);
        arr.append(o);
    }
    root.insert(QStringLiteral("folio"), arr);
    return QString::fromUtf8(QJsonDocument(root).toJson(QJsonDocument::Indented));
}

BackupJson::Parsed BackupJson::parse(const QString &text) {
    QJsonParseError err;
    const QJsonDocument doc = QJsonDocument::fromJson(text.toUtf8(), &err);
    if (err.error != QJsonParseError::NoError || !doc.isObject())
        throw std::runtime_error("FORMAT");
    const QJsonObject root = doc.object();
    if (root.value(QStringLiteral("format")).toString() != QLatin1String(kFormat))
        throw std::runtime_error("FORMAT");
    Parsed parsed;
    if (root.contains(QStringLiteral("settings")) && !root.value(QStringLiteral("settings")).isNull()) {
        parsed.settingsJson = QString::fromUtf8(
            QJsonDocument(root.value(QStringLiteral("settings")).toObject()).toJson(QJsonDocument::Compact));
    }
    if (root.contains(QStringLiteral("documents")) && root.value(QStringLiteral("documents")).isArray()) {
        QVector<PdfDocument> docs;
        for (const auto &v : root.value(QStringLiteral("documents")).toArray()) {
            if (auto d = documentFrom(v.toObject())) docs.push_back(*d);
        }
        parsed.documents = docs;
    }
    if (root.contains(QStringLiteral("bookmarks")) && root.value(QStringLiteral("bookmarks")).isArray()) {
        QVector<Bookmark> marks;
        for (const auto &v : root.value(QStringLiteral("bookmarks")).toArray()) {
            if (auto b = bookmarkFrom(v.toObject())) marks.push_back(*b);
        }
        parsed.bookmarks = marks;
    }
    return parsed;
}

ViewerSettings BackupJson::mergeSettings(const ViewerSettings &current, const QString &settingsJson) {
    const QJsonObject obj = QJsonDocument::fromJson(settingsJson.toUtf8()).object();
    ViewerSettings next = current;
    next.fgColor = intOr(obj, current.fgColor, "fgColor");
    next.bgColor = intOr(obj, current.bgColor, "bgColor");
    next.imageColorBlend = floatOr(obj, current.imageColorBlend, "imageColorBlend");
    next.zoomMode = stringOr(obj, current.zoomMode, "zoomMode");
    next.customZoomPercent = floatOr(obj, current.customZoomPercent, "customZoomPercent");
    next.rememberLastZoom = boolOr(obj, current.rememberLastZoom, "rememberLastZoom");
    next.lockZoom = boolOr(obj, current.lockZoom, "lockZoom");
    next.lockSideScroll = boolOr(obj, current.lockSideScroll, "lockSideScroll");
    next.originalColors = boolOr(obj, current.originalColors, "originalColors");
    next.startupZoomMode = stringOr(obj, current.startupZoomMode, "startupZoomMode");
    next.startupZoomPercent = floatOr(obj, current.startupZoomPercent, "startupZoomPercent");
    next.restoreLastPdf = boolOr(obj, current.restoreLastPdf, "restoreLastPdf");
    next.restoreLastPosition = boolOr(obj, current.restoreLastPosition, "restoreLastPosition");
    next.fontScale = floatOr(obj, current.fontScale, "fontScale");
    next.highlightColor = intOr(obj, current.highlightColor, "highlightColor");
    next.pagePaddingDp = intOr(obj, current.pagePaddingDp, "pagePaddingDp");
    next.horizontalPagePaddingDp = intOr(obj, current.horizontalPagePaddingDp, "horizontalPagePaddingDp");
    next.pageBorderEnabled = boolOr(obj, current.pageBorderEnabled, "pageBorderEnabled");
    next.pageBorderColor = intOr(obj, current.pageBorderColor, "pageBorderColor");
    next.allowFollowLinks = boolOr(obj, current.allowFollowLinks, "allowFollowLinks");
    next.doubleTapMenu = boolOr(obj, current.doubleTapMenu, "doubleTapMenu");
    next.singleTapMenu = boolOr(obj, current.singleTapMenu, "singleTapMenu");
    next.swipeFromBottomMenu = boolOr(obj, current.swipeFromBottomMenu, "swipeFromBottomMenu");
    next.appFontColor = intOr(obj, current.appFontColor, "appFontColor");
    next.actionColor = intOr(obj, current.actionColor, "actionColor");
    next.twoFingerPan = boolOr(obj, current.twoFingerPan, "twoFingerPan");
    next.autoDisablePan = boolOr(obj, current.autoDisablePan, "autoDisablePan");
    next.allowTextSelection = boolOr(obj, current.allowTextSelection, "allowTextSelection");
    next.alwaysDarkMode = boolOr(obj, current.alwaysDarkMode, "alwaysDarkMode");
    next.imageContrast = floatOr(obj, current.imageContrast, "imageContrast");
    next.imageAlpha = floatOr(obj, current.imageAlpha, "imageAlpha");
    next.darkImageBlend = floatOr(obj, current.darkImageBlend, "darkImageBlend");
    next.darkImageContrast = floatOr(obj, current.darkImageContrast, "darkImageContrast");
    next.darkImageAlpha = floatOr(obj, current.darkImageAlpha, "darkImageAlpha");
    next.openFolioOnStartup = boolOr(obj, current.openFolioOnStartup, "openFolioOnStartup");
    next.folioOpenLastViewed = boolOr(obj, current.folioOpenLastViewed, "folioOpenLastViewed");
    next.folioEnabled = boolOr(obj, current.folioEnabled, "folioEnabled");
    next.folioStoreLastPosition = boolOr(obj, current.folioStoreLastPosition, "folioStoreLastPosition");
    next.allowMultipleInstances = boolOr(obj, current.allowMultipleInstances, "allowMultipleInstances");
    next.fullScreenOnMaximize = boolOr(obj, current.fullScreenOnMaximize, "fullScreenOnMaximize");
    next.centerHorizontally = boolOr(obj, current.centerHorizontally, "centerHorizontally");
    next.titleBarEnabled = boolOr(obj, current.titleBarEnabled, "titleBarEnabled");
    next.windowPaddingV = intOr(obj, current.windowPaddingV, "windowPaddingV");
    next.windowPaddingH = intOr(obj, current.windowPaddingH, "windowPaddingH");
    next.defaultWindowWidth =
        ViewerSettings::canonicalDefaultWindowWidth(intOr(obj, current.defaultWindowWidth, "defaultWindowWidth"));
    next.defaultWindowHeight =
        ViewerSettings::canonicalDefaultWindowHeight(intOr(obj, current.defaultWindowHeight, "defaultWindowHeight"));
    next.startupWindowPosition = ViewerSettings::canonicalStartupWindowPosition(
        stringOr(obj, current.startupWindowPosition, "startupWindowPosition"));
    if (obj.contains(QLatin1String("filesRoot")))
        next.filesRoot = obj.value(QLatin1String("filesRoot")).toString();
    next.appIcon = ViewerSettings::canonicalAppIcon(stringOr(obj, current.appIcon, "appIcon"));
    next.appFontFamily = ViewerSettings::canonicalAppFontFamily(stringOr(obj, current.appFontFamily, "appFontFamily"));
    next.appFontWeight = ViewerSettings::canonicalAppFontWeight(intOr(obj, current.appFontWeight, "appFontWeight"));
    next.fontScale = ViewerSettings::canonicalFontScale(next.fontScale);
    if (!obj.contains(QLatin1String("appFontWeight")) && next.fgColor == static_cast<int>(0xFFB3B3B3)
        && next.appFontColor == static_cast<int>(0xFFC8C8C8)) {
        next.fgColor = ViewerSettings::DEFAULT_FG;
        next.appFontColor = ViewerSettings::DEFAULT_APP_FONT;
    }
    if (obj.contains(QLatin1String("lastPdfUri"))) {
        const QString uri = obj.value(QLatin1String("lastPdfUri")).toString();
        if (uri.isEmpty()) next.lastPdfUri.reset();
        else next.lastPdfUri = uri;
    }
    return next.withExclusiveMenuTaps().withExclusiveStartup();
}

QString BackupJson::settingsToJson(const ViewerSettings &settings) {
    return QString::fromUtf8(QJsonDocument(settingsObject(settings)).toJson(QJsonDocument::Indented));
}

} // namespace papercut
