#include "Database.h"
#include "Schema.h"

#include <QDir>
#include <QFileInfo>
#include <QSqlError>
#include <QSqlQuery>
#include <QUuid>
#include <QVariant>

namespace papercut {
namespace {

const char *kSettingsSelect =
    "SELECT id, fgColor, bgColor, imageColorBlend, zoomMode, customZoomPercent, restoreLastPdf, lastPdfUri, "
    "rememberLastZoom, startupZoomMode, startupZoomPercent, lockZoom, lockSideScroll, fontScale, highlightColor, "
    "pagePaddingDp, horizontalPagePaddingDp, pageBorderEnabled, pageBorderColor, allowFollowLinks, doubleTapMenu, "
    "singleTapMenu, swipeFromBottomMenu, appFontColor, actionColor, originalColors, twoFingerPan, autoDisablePan, "
    "allowTextSelection, restoreLastPosition, alwaysDarkMode, imageContrast, imageAlpha, darkImageBlend, "
    "darkImageContrast, darkImageAlpha, lastPageIndex, lastScrollXPdf, lastScrollYPdf, folioEnabled, "
    "openFolioOnStartup, folioOpenLastViewed, folioStoreLastPosition, allowMultipleInstances, lastFolioItemId, "
    "fullScreenOnMaximize, centerHorizontally FROM settings WHERE id = 1";

} // namespace

ViewerSettings settingsFromQuery(const QSqlQuery &q) {
    ViewerSettings s;
    int i = 0;
    s.id = q.value(i++).toInt();
    s.fgColor = q.value(i++).toInt();
    s.bgColor = q.value(i++).toInt();
    s.imageColorBlend = q.value(i++).toFloat();
    s.zoomMode = q.value(i++).toString();
    s.customZoomPercent = q.value(i++).toFloat();
    s.restoreLastPdf = q.value(i++).toInt() != 0;
    if (q.value(i).isNull()) {
        s.lastPdfUri.reset();
        ++i;
    } else {
        s.lastPdfUri = q.value(i++).toString();
    }
    s.rememberLastZoom = q.value(i++).toInt() != 0;
    s.startupZoomMode = q.value(i++).toString();
    s.startupZoomPercent = q.value(i++).toFloat();
    s.lockZoom = q.value(i++).toInt() != 0;
    s.lockSideScroll = q.value(i++).toInt() != 0;
    s.fontScale = q.value(i++).toFloat();
    s.highlightColor = q.value(i++).toInt();
    s.pagePaddingDp = q.value(i++).toInt();
    s.horizontalPagePaddingDp = q.value(i++).toInt();
    s.pageBorderEnabled = q.value(i++).toInt() != 0;
    s.pageBorderColor = q.value(i++).toInt();
    s.allowFollowLinks = q.value(i++).toInt() != 0;
    s.doubleTapMenu = q.value(i++).toInt() != 0;
    s.singleTapMenu = q.value(i++).toInt() != 0;
    s.swipeFromBottomMenu = q.value(i++).toInt() != 0;
    s.appFontColor = q.value(i++).toInt();
    s.actionColor = q.value(i++).toInt();
    s.originalColors = q.value(i++).toInt() != 0;
    s.twoFingerPan = q.value(i++).toInt() != 0;
    s.autoDisablePan = q.value(i++).toInt() != 0;
    s.allowTextSelection = q.value(i++).toInt() != 0;
    s.restoreLastPosition = q.value(i++).toInt() != 0;
    s.alwaysDarkMode = q.value(i++).toInt() != 0;
    s.imageContrast = q.value(i++).toFloat();
    s.imageAlpha = q.value(i++).toFloat();
    s.darkImageBlend = q.value(i++).toFloat();
    s.darkImageContrast = q.value(i++).toFloat();
    s.darkImageAlpha = q.value(i++).toFloat();
    s.lastPageIndex = q.value(i++).toInt();
    s.lastScrollXPdf = q.value(i++).toFloat();
    s.lastScrollYPdf = q.value(i++).toFloat();
    s.folioEnabled = q.value(i++).toInt() != 0;
    s.openFolioOnStartup = q.value(i++).toInt() != 0;
    s.folioOpenLastViewed = q.value(i++).toInt() != 0;
    s.folioStoreLastPosition = q.value(i++).toInt() != 0;
    s.allowMultipleInstances = q.value(i++).toInt() != 0;
    s.lastFolioItemId = q.value(i++).toLongLong();
    s.fullScreenOnMaximize = q.value(i++).toInt() != 0;
    s.centerHorizontally = q.value(i++).toInt() != 0;
    return s;
}

void bindSettings(QSqlQuery &q, const ViewerSettings &s) {
    q.addBindValue(s.id);
    q.addBindValue(s.fgColor);
    q.addBindValue(s.bgColor);
    q.addBindValue(s.imageColorBlend);
    q.addBindValue(s.zoomMode);
    q.addBindValue(s.customZoomPercent);
    q.addBindValue(s.restoreLastPdf ? 1 : 0);
    q.addBindValue(s.lastPdfUri ? QVariant(*s.lastPdfUri) : QVariant());
    q.addBindValue(s.rememberLastZoom ? 1 : 0);
    q.addBindValue(s.startupZoomMode);
    q.addBindValue(s.startupZoomPercent);
    q.addBindValue(s.lockZoom ? 1 : 0);
    q.addBindValue(s.lockSideScroll ? 1 : 0);
    q.addBindValue(s.fontScale);
    q.addBindValue(s.highlightColor);
    q.addBindValue(s.pagePaddingDp);
    q.addBindValue(s.horizontalPagePaddingDp);
    q.addBindValue(s.pageBorderEnabled ? 1 : 0);
    q.addBindValue(s.pageBorderColor);
    q.addBindValue(s.allowFollowLinks ? 1 : 0);
    q.addBindValue(s.doubleTapMenu ? 1 : 0);
    q.addBindValue(s.singleTapMenu ? 1 : 0);
    q.addBindValue(s.swipeFromBottomMenu ? 1 : 0);
    q.addBindValue(s.appFontColor);
    q.addBindValue(s.actionColor);
    q.addBindValue(s.originalColors ? 1 : 0);
    q.addBindValue(s.twoFingerPan ? 1 : 0);
    q.addBindValue(s.autoDisablePan ? 1 : 0);
    q.addBindValue(s.allowTextSelection ? 1 : 0);
    q.addBindValue(s.restoreLastPosition ? 1 : 0);
    q.addBindValue(s.alwaysDarkMode ? 1 : 0);
    q.addBindValue(s.imageContrast);
    q.addBindValue(s.imageAlpha);
    q.addBindValue(s.darkImageBlend);
    q.addBindValue(s.darkImageContrast);
    q.addBindValue(s.darkImageAlpha);
    q.addBindValue(s.lastPageIndex);
    q.addBindValue(s.lastScrollXPdf);
    q.addBindValue(s.lastScrollYPdf);
    q.addBindValue(s.folioEnabled ? 1 : 0);
    q.addBindValue(s.openFolioOnStartup ? 1 : 0);
    q.addBindValue(s.folioOpenLastViewed ? 1 : 0);
    q.addBindValue(s.folioStoreLastPosition ? 1 : 0);
    q.addBindValue(s.allowMultipleInstances ? 1 : 0);
    q.addBindValue(s.lastFolioItemId);
    q.addBindValue(s.fullScreenOnMaximize ? 1 : 0);
    q.addBindValue(s.centerHorizontally ? 1 : 0);
}

bool Database::open(const QString &path) {
    QDir().mkpath(QFileInfo(path).absolutePath());
    const QString name = QStringLiteral("papercut-") + QUuid::createUuid().toString(QUuid::WithoutBraces);
    db_ = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), name);
    db_.setDatabaseName(path);
    if (!db_.open())
        return false;
    QSqlQuery pragma(db_);
    if (!pragma.exec(QStringLiteral("PRAGMA foreign_keys = ON")))
        return false;
    const QStringList statements = QString::fromUtf8(kCreateSchema).split(QLatin1Char(';'), Qt::SkipEmptyParts);
    for (QString sql : statements) {
        sql = sql.trimmed();
        if (sql.isEmpty()) continue;
        QSqlQuery q(db_);
        if (!q.exec(sql))
            return false;
    }
    {
        QSqlQuery info(db_);
        bool hasCol = false;
        if (info.exec(QStringLiteral("PRAGMA table_info(settings)"))) {
            while (info.next()) {
                if (info.value(1).toString() == QLatin1String("fullScreenOnMaximize")) hasCol = true;
            }
        }
        if (!hasCol) {
            QSqlQuery alter(db_);
            if (!alter.exec(QStringLiteral(
                    "ALTER TABLE settings ADD COLUMN fullScreenOnMaximize INTEGER NOT NULL DEFAULT 0")))
                return false;
        }
    }
    {
        QSqlQuery info(db_);
        bool hasCol = false;
        if (info.exec(QStringLiteral("PRAGMA table_info(settings)"))) {
            while (info.next()) {
                if (info.value(1).toString() == QLatin1String("centerHorizontally")) hasCol = true;
            }
        }
        if (!hasCol) {
            QSqlQuery alter(db_);
            if (!alter.exec(QStringLiteral(
                    "ALTER TABLE settings ADD COLUMN centerHorizontally INTEGER NOT NULL DEFAULT 0")))
                return false;
        }
    }
    {
        QSqlQuery info(db_);
        bool hasCol = false;
        if (info.exec(QStringLiteral("PRAGMA table_info(folio_items)"))) {
            while (info.next()) {
                if (info.value(1).toString() == QLatin1String("pageRotations")) hasCol = true;
            }
        }
        if (!hasCol) {
            QSqlQuery alter(db_);
            if (!alter.exec(QStringLiteral(
                    "ALTER TABLE folio_items ADD COLUMN pageRotations TEXT NOT NULL DEFAULT '[]'")))
                return false;
        }
    }
    ViewerSettings def;
    QSqlQuery ins(db_);
    ins.prepare(
        QStringLiteral("INSERT OR IGNORE INTO settings ("
                       "id, fgColor, bgColor, imageColorBlend, zoomMode, customZoomPercent, restoreLastPdf, lastPdfUri, "
                       "rememberLastZoom, startupZoomMode, startupZoomPercent, lockZoom, lockSideScroll, fontScale, "
                       "highlightColor, pagePaddingDp, horizontalPagePaddingDp, pageBorderEnabled, pageBorderColor, "
                       "allowFollowLinks, doubleTapMenu, singleTapMenu, swipeFromBottomMenu, appFontColor, actionColor, "
                       "originalColors, twoFingerPan, autoDisablePan, allowTextSelection, restoreLastPosition, "
                       "alwaysDarkMode, imageContrast, imageAlpha, darkImageBlend, darkImageContrast, darkImageAlpha, "
                       "lastPageIndex, lastScrollXPdf, lastScrollYPdf, folioEnabled, openFolioOnStartup, "
                       "folioOpenLastViewed, folioStoreLastPosition, allowMultipleInstances, lastFolioItemId, "
                       "fullScreenOnMaximize, centerHorizontally) "
                       "VALUES (?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?)"));
    bindSettings(ins, def);
    return ins.exec();
}

ViewerSettings loadSettings(QSqlDatabase &db) {
    QSqlQuery q(db);
    q.prepare(QLatin1String(kSettingsSelect));
    if (q.exec() && q.next())
        return settingsFromQuery(q);
    return {};
}

bool saveSettings(QSqlDatabase &db, const ViewerSettings &s) {
    QSqlQuery q(db);
    q.prepare(
        QStringLiteral("INSERT OR REPLACE INTO settings ("
                       "id, fgColor, bgColor, imageColorBlend, zoomMode, customZoomPercent, restoreLastPdf, lastPdfUri, "
                       "rememberLastZoom, startupZoomMode, startupZoomPercent, lockZoom, lockSideScroll, fontScale, "
                       "highlightColor, pagePaddingDp, horizontalPagePaddingDp, pageBorderEnabled, pageBorderColor, "
                       "allowFollowLinks, doubleTapMenu, singleTapMenu, swipeFromBottomMenu, appFontColor, actionColor, "
                       "originalColors, twoFingerPan, autoDisablePan, allowTextSelection, restoreLastPosition, "
                       "alwaysDarkMode, imageContrast, imageAlpha, darkImageBlend, darkImageContrast, darkImageAlpha, "
                       "lastPageIndex, lastScrollXPdf, lastScrollYPdf, folioEnabled, openFolioOnStartup, "
                       "folioOpenLastViewed, folioStoreLastPosition, allowMultipleInstances, lastFolioItemId, "
                       "fullScreenOnMaximize, centerHorizontally) "
                       "VALUES (?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?)"));
    bindSettings(q, s);
    return q.exec();
}

} // namespace papercut
