#include "AppServices.h"

#include "Version.h"
#include "core/AppPaths.h"
#include "data/BackupJson.h"
#include "pdf/DocumentFont.h"
#include "pdf/DocumentMagic.h"

#include <QClipboard>
#include <QDesktopServices>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QFontDatabase>
#include <QGuiApplication>
#include <QHash>
#include <QStandardPaths>
#include <QTextStream>
#include <algorithm>
#include <cmath>

namespace papercut {

namespace {

AppServices *g_instance = nullptr;
QHash<QString, QString> g_uiFontFamilies;
QString g_iconFontFamily;

QColor toColor(int argb) { return QColor::fromRgba(static_cast<QRgb>(static_cast<quint32>(argb))); }

int fromVariantColor(const QVariant &v) {
    if (v.metaType().id() == QMetaType::QColor) return static_cast<int>(v.value<QColor>().rgba());
    return v.toInt();
}

} // namespace

AppServices::AppServices(QObject *parent) : QObject(parent) {}

AppServices::~AppServices() {
    if (g_instance == this) g_instance = nullptr;
}

AppServices *AppServices::instance() { return g_instance; }

AppServices *AppServices::create() {
    if (g_instance) return g_instance;
    auto *s = new AppServices();
    AppPaths::migrateLegacyLayout();
    QDir().mkpath(AppPaths::dataRoot());
    QDir().mkpath(AppPaths::configRoot());
    s->m_settings = std::make_unique<SettingsRepository>();
    s->m_db.open(AppPaths::database());
    s->m_settings->migrateFromDatabaseIfEmpty(s->m_db);
    AppPaths::setFilesRootOverride(s->m_settings->snapshot.filesRoot);
    s->m_bookmarks = std::make_unique<BookmarkRepository>(s->m_db);
    s->m_folioStore = std::make_unique<FolioStore>(AppPaths::filesRoot());
    s->m_folio = std::make_unique<FolioRepository>(s->m_db, *s->m_folioStore);
    g_instance = s;
    return s;
}

void AppServices::updateSettings(const std::function<ViewerSettings(ViewerSettings)> &transform) {
    const ViewerSettings prev = m_settings->snapshot;
    ViewerSettings next = transform(prev);
    next.appIcon = ViewerSettings::canonicalAppIcon(next.appIcon);
    next.appFontFamily = ViewerSettings::canonicalAppFontFamily(next.appFontFamily);
    next.appFontWeight = ViewerSettings::canonicalAppFontWeight(next.appFontWeight);
    next.fontScale = ViewerSettings::canonicalFontScale(next.fontScale);
    if (next == prev) return;
    m_settings->update([&](ViewerSettings) { return next; });
    if (next.appearanceEqual(prev)) return;
    emit settingsChanged();
}

void AppServices::reloadSettings() {
    m_settings->get();
    emit settingsChanged();
}

QColor AppServices::bgColor() const { return toColor(settings().bgColor); }
QColor AppServices::pageFgColor() const { return toColor(settings().fgColor); }
QColor AppServices::appFontColor() const { return toColor(settings().appFontColor); }
QColor AppServices::actionColor() const { return toColor(settings().actionColor); }
QColor AppServices::highlightColor() const { return toColor(settings().highlightColor); }
double AppServices::fontScale() const {
    return double(ViewerSettings::canonicalFontScale(settings().fontScale));
}

void AppServices::registerUiFont(const QString &id, const QString &qtFamily) {
    if (!id.isEmpty() && !qtFamily.isEmpty()) g_uiFontFamilies.insert(id, qtFamily);
}

void AppServices::setIconFontFamily(const QString &family) { g_iconFontFamily = family; }

QString AppServices::iconFontFamily() const { return g_iconFontFamily; }

QStringList AppServices::systemFontFamilies() const {
    QStringList families = QFontDatabase::families();
    families.removeIf([](const QString &f) {
        return f.startsWith(QLatin1String("Material Symbols"), Qt::CaseInsensitive);
    });
    families.sort(Qt::CaseInsensitive);
    return families;
}

QString AppServices::uiFontFamily() const {
    const QString family = ViewerSettings::canonicalAppFontFamily(settings().appFontFamily);
    if (QFontDatabase::hasFamily(family)) return family;
    if (QFontDatabase::hasFamily(QStringLiteral("Roboto"))) return QStringLiteral("Roboto");
    const QStringList families = QFontDatabase::families();
    return families.isEmpty() ? family : families.first();
}

int AppServices::uiFontWeight() const {
    return ViewerSettings::canonicalAppFontWeight(settings().appFontWeight);
}

QString AppServices::documentUserCss() const {
    return DocumentFont::userCss(uiFontFamily(), uiFontWeight());
}

int AppServices::fontPx(int base) const {
    return std::max(1, int(std::lround(double(base) * fontScale())));
}
QString AppServices::defaultFilesRoot() const { return AppPaths::dataRoot(); }
QString AppServices::version() const { return QStringLiteral(PAPERCUT_VERSION); }
int AppServices::versionCode() const { return PAPERCUT_VERSION_CODE; }
QString AppServices::dataDir() const { return AppPaths::dataRoot(); }
QString AppServices::tempDir() const { return AppPaths::temp().absolutePath(); }

#define PC_COLORS(X) X(fgColor) X(bgColor) X(highlightColor) X(pageBorderColor) X(appFontColor) X(actionColor)
#define PC_FLOATS(X) \
    X(imageColorBlend) X(customZoomPercent) X(startupZoomPercent) X(fontScale) X(imageContrast) X(imageAlpha) \
    X(darkImageBlend) X(darkImageContrast) X(darkImageAlpha) X(lastScrollXPdf) X(lastScrollYPdf)
#define PC_BOOLS(X) \
    X(restoreLastPdf) X(rememberLastZoom) X(lockZoom) X(lockSideScroll) X(pageBorderEnabled) X(allowFollowLinks) \
    X(doubleTapMenu) X(singleTapMenu) X(swipeFromBottomMenu) X(originalColors) X(twoFingerPan) X(autoDisablePan) \
    X(allowTextSelection) X(restoreLastPosition) X(alwaysDarkMode) X(folioEnabled) X(openFolioOnStartup) \
    X(folioOpenLastViewed) X(folioStoreLastPosition)     X(allowMultipleInstances) X(fullScreenOnMaximize) \
    X(centerHorizontally) X(titleBarEnabled)
#define PC_INTS(X) \
    X(pagePaddingDp) X(horizontalPagePaddingDp) X(lastPageIndex) X(windowPaddingV) X(windowPaddingH) \
    X(appFontWeight)
#define PC_STRINGS(X) X(zoomMode) X(startupZoomMode) X(filesRoot) X(appIcon) X(appFontFamily)

QVariantMap AppServices::settingsMap() const {
    const ViewerSettings s = settings();
    QVariantMap m;
#define X(f) m.insert(QStringLiteral(#f), toColor(s.f));
    PC_COLORS(X)
#undef X
#define X(f) m.insert(QStringLiteral(#f), double(s.f));
    PC_FLOATS(X)
#undef X
#define X(f) m.insert(QStringLiteral(#f), s.f);
    PC_BOOLS(X) PC_INTS(X) PC_STRINGS(X)
#undef X
    m.insert(QStringLiteral("lastFolioItemId"), s.lastFolioItemId);
    m.insert(QStringLiteral("lastPdfUri"), s.lastPdfUri ? QVariant(*s.lastPdfUri) : QVariant());
    return m;
}

QVariant AppServices::setting(const QString &key) const { return settingsMap().value(key); }

void AppServices::setSetting(const QString &key, const QVariant &value) {
    updateSettings([&](ViewerSettings s) {
#define X(f) if (key == QLatin1String(#f)) s.f = fromVariantColor(value); else
        PC_COLORS(X)
#undef X
#define X(f) if (key == QLatin1String(#f)) s.f = value.toFloat(); else
        PC_FLOATS(X)
#undef X
#define X(f) if (key == QLatin1String(#f)) s.f = value.toBool(); else
        PC_BOOLS(X)
#undef X
#define X(f) if (key == QLatin1String(#f)) s.f = value.toInt(); else
        PC_INTS(X)
#undef X
#define X(f) if (key == QLatin1String(#f)) s.f = value.toString(); else
        PC_STRINGS(X)
#undef X
        if (key == QLatin1String("lastFolioItemId")) s.lastFolioItemId = value.toLongLong();
        else if (key == QLatin1String("lastPdfUri")) {
            if (value.isNull() || value.toString().isEmpty()) s.lastPdfUri.reset();
            else s.lastPdfUri = value.toString();
        }
        return s;
    });
}

void AppServices::saveSettingsDraft(const QVariantMap &draft) {
    updateSettings([&](ViewerSettings current) {
        ViewerSettings next = current;
        auto b = [&](const char *k, bool &f) {
            if (draft.contains(QLatin1String(k))) f = draft.value(QLatin1String(k)).toBool();
        };
        auto i = [&](const char *k, int &f) {
            if (draft.contains(QLatin1String(k))) f = fromVariantColor(draft.value(QLatin1String(k)));
        };
        auto fl = [&](const char *k, float &f) {
            if (draft.contains(QLatin1String(k))) f = draft.value(QLatin1String(k)).toFloat();
        };
        auto st = [&](const char *k, QString &f) {
            if (draft.contains(QLatin1String(k))) f = draft.value(QLatin1String(k)).toString();
        };
        i("fgColor", next.fgColor);
        i("bgColor", next.bgColor);
        i("highlightColor", next.highlightColor);
        i("pageBorderColor", next.pageBorderColor);
        i("appFontColor", next.appFontColor);
        i("actionColor", next.actionColor);
        fl("imageColorBlend", next.imageColorBlend);
        fl("customZoomPercent", next.customZoomPercent);
        fl("startupZoomPercent", next.startupZoomPercent);
        fl("fontScale", next.fontScale);
        fl("imageContrast", next.imageContrast);
        fl("imageAlpha", next.imageAlpha);
        fl("darkImageBlend", next.darkImageBlend);
        fl("darkImageContrast", next.darkImageContrast);
        fl("darkImageAlpha", next.darkImageAlpha);
        b("restoreLastPdf", next.restoreLastPdf);
        b("rememberLastZoom", next.rememberLastZoom);
        b("lockZoom", next.lockZoom);
        b("lockSideScroll", next.lockSideScroll);
        b("pageBorderEnabled", next.pageBorderEnabled);
        b("allowFollowLinks", next.allowFollowLinks);
        b("doubleTapMenu", next.doubleTapMenu);
        b("singleTapMenu", next.singleTapMenu);
        b("swipeFromBottomMenu", next.swipeFromBottomMenu);
        b("originalColors", next.originalColors);
        b("twoFingerPan", next.twoFingerPan);
        b("autoDisablePan", next.autoDisablePan);
        b("allowTextSelection", next.allowTextSelection);
        b("restoreLastPosition", next.restoreLastPosition);
        b("alwaysDarkMode", next.alwaysDarkMode);
        b("folioEnabled", next.folioEnabled);
        b("openFolioOnStartup", next.openFolioOnStartup);
        b("folioOpenLastViewed", next.folioOpenLastViewed);
        b("folioStoreLastPosition", next.folioStoreLastPosition);
        b("allowMultipleInstances", next.allowMultipleInstances);
        b("fullScreenOnMaximize", next.fullScreenOnMaximize);
        b("centerHorizontally", next.centerHorizontally);
        b("titleBarEnabled", next.titleBarEnabled);
        i("pagePaddingDp", next.pagePaddingDp);
        i("horizontalPagePaddingDp", next.horizontalPagePaddingDp);
        i("windowPaddingV", next.windowPaddingV);
        i("windowPaddingH", next.windowPaddingH);
        st("filesRoot", next.filesRoot);
        st("zoomMode", next.zoomMode);
        st("startupZoomMode", next.startupZoomMode);
        st("appIcon", next.appIcon);
        next.appIcon = ViewerSettings::canonicalAppIcon(next.appIcon);
        st("appFontFamily", next.appFontFamily);
        next.appFontFamily = ViewerSettings::canonicalAppFontFamily(next.appFontFamily);
        i("appFontWeight", next.appFontWeight);
        next.appFontWeight = ViewerSettings::canonicalAppFontWeight(next.appFontWeight);
        next.fontScale = ViewerSettings::canonicalFontScale(next.fontScale);
        next.id = current.id;
        next.lastPdfUri = current.lastPdfUri;
        next.lastFolioItemId = current.lastFolioItemId;
        next.zoomMode = current.zoomMode;
        next.customZoomPercent = current.customZoomPercent;
        next.lockZoom = current.lockZoom;
        next.lockSideScroll = current.lockSideScroll;
        next.originalColors = current.originalColors;
        next.lastPageIndex = current.lastPageIndex;
        next.lastScrollXPdf = current.lastScrollXPdf;
        next.lastScrollYPdf = current.lastScrollYPdf;
        next = next.withExclusiveMenuTaps().withExclusiveStartup();
        if (!next.restoreLastPdf) next.lastPdfUri.reset();
        const QString oldFolio = AppPaths::folio().absolutePath();
        AppPaths::setFilesRootOverride(next.filesRoot);
        const QString newFolio = AppPaths::folio().absolutePath();
        if (QDir::cleanPath(oldFolio).compare(QDir::cleanPath(newFolio), Qt::CaseInsensitive) != 0) {
            QDir().mkpath(newFolio);
            const QFileInfoList files = QDir(oldFolio).entryInfoList(QDir::Files | QDir::NoDotAndDotDot);
            for (const QFileInfo &fi : files) {
                const QString dest = QDir(newFolio).filePath(fi.fileName());
                if (QFile::exists(dest)) continue;
                if (!QFile::rename(fi.absoluteFilePath(), dest) && QFile::copy(fi.absoluteFilePath(), dest))
                    QFile::remove(fi.absoluteFilePath());
            }
        }
        if (m_folioStore) m_folioStore->setFilesDir(AppPaths::filesRoot());
        return next;
    });
}

QVariantList AppServices::documents() const {
    QVariantList list;
    for (const PdfDocument &d : m_bookmarks->documents()) {
        QVariantMap m;
        m.insert(QStringLiteral("uri"), d.uri);
        m.insert(QStringLiteral("displayName"), d.displayName);
        m.insert(QStringLiteral("displayPath"), d.displayPath);
        m.insert(QStringLiteral("lastPageIndex"), d.lastPageIndex);
        list.append(m);
    }
    return list;
}

void AppServices::deleteDocuments(const QVariantList &uris) {
    QVector<QString> v;
    for (const QVariant &u : uris) v.append(u.toString());
    m_bookmarks->deleteDocuments(v);
}

void AppServices::clearAllBookmarks() { m_bookmarks->clearAllBookmarks(); }

QString AppServices::exportBackup(const QString &kind) const {
    if (kind == QLatin1String("Folio")) return BackupJson::exportFolio(m_folio->all());
    BackupJson::Kind k = BackupJson::Kind::All;
    if (kind == QLatin1String("Settings")) k = BackupJson::Kind::Settings;
    else if (kind == QLatin1String("Bookmarks")) k = BackupJson::Kind::Bookmarks;
    return m_bookmarks->snapshot(k, settings());
}

QString AppServices::importBackup(const QString &path) {
    const QString text = readTextFile(path);
    if (text.isEmpty()) return QStringLiteral("Import failed");
    const BackupJson::Parsed parsed = BackupJson::parse(text);
    if (!parsed.settings && !parsed.documents && !parsed.bookmarks && parsed.settingsJson.isEmpty())
        return QStringLiteral("Not a Papercut file");
    m_bookmarks->importParsed(parsed);
    if (!parsed.settingsJson.isEmpty())
        m_settings->update([&](ViewerSettings cur) { return BackupJson::mergeSettings(cur, parsed.settingsJson); });
    reloadSettings();
    return QStringLiteral("Imported");
}

QVariantList AppServices::storedFiles() const {
    const QDir temp = AppPaths::temp();
    const QString exportPath = QFileInfo(AppPaths::exportPdf()).absoluteFilePath();
    const QString folioDir = QFileInfo(AppPaths::folio().absolutePath()).absoluteFilePath();

    QHash<qint64, FolioItem> byId;
    for (const FolioItem &i : m_folio->all()) byId.insert(i.id, i);

    auto ignored = [](const QFileInfo &fi) {
        const QString lower = fi.fileName().toLower();
        return lower == QLatin1String("rlist") || lower == QLatin1String("profileinstalled")
               || lower.endsWith(QLatin1String(".lck")) || lower.startsWith(QLatin1String("papercut.db"))
               || lower.endsWith(QLatin1String(".tmp"));
    };
    auto classify = [&](const QFileInfo &fi) {
        const QString path = fi.absoluteFilePath();
        QVariantMap m;
        m.insert(QStringLiteral("path"), path);
        m.insert(QStringLiteral("folioId"), 0);
        QString title = fi.fileName(), status = QStringLiteral("Unused"), use = QStringLiteral("Unused");
        if (QFileInfo(fi.absolutePath()).absoluteFilePath() == folioDir) {
            bool ok = false;
            const qint64 id = fi.fileName().toLongLong(&ok);
            if (ok && byId.contains(id)) {
                const FolioItem &item = byId[id];
                title = item.displayName.isEmpty() ? fi.fileName() : item.displayName;
                status = QStringLiteral("Folio item ") + item.displayName;
                use = QStringLiteral("Folio");
                m.insert(QStringLiteral("folioId"), id);
            }
        } else if (path == exportPath) {
            status = QStringLiteral("Export cache");
            use = QStringLiteral("ExportCache");
        }
        m.insert(QStringLiteral("title"), title);
        m.insert(QStringLiteral("status"), status);
        m.insert(QStringLiteral("use"), use);
        m.insert(QStringLiteral("inDatabase"), use == QLatin1String("Folio"));
        return m;
    };

    QVariantList found;
    for (const QFileInfo &fi : temp.entryInfoList(QDir::Files | QDir::NoDotAndDotDot)) {
        if (!ignored(fi)) found.append(classify(fi));
    }
    for (const QFileInfo &fi : QDir(folioDir).entryInfoList(QDir::Files | QDir::NoDotAndDotDot)) {
        if (!ignored(fi)) found.append(classify(fi));
    }
    std::sort(found.begin(), found.end(), [](const QVariant &a, const QVariant &b) {
        const QVariantMap x = a.toMap(), y = b.toMap();
        const int byStatus = x.value(QStringLiteral("status")).toString().compare(y.value(QStringLiteral("status")).toString());
        if (byStatus != 0) return byStatus < 0;
        return x.value(QStringLiteral("title")).toString().toLower() < y.value(QStringLiteral("title")).toString().toLower();
    });
    return found;
}

void AppServices::deleteStored(const QVariantList &entries) {
    QVector<qint64> folioIds;
    QStringList paths;
    for (const QVariant &e : entries) {
        const QVariantMap m = e.toMap();
        const QString use = m.value(QStringLiteral("use")).toString();
        const QString path = m.value(QStringLiteral("path")).toString();
        if (use == QLatin1String("Folio")) folioIds.append(m.value(QStringLiteral("folioId")).toLongLong());
        else paths.append(path);
    }
    QStringList all = paths;
    for (const QVariant &e : entries) all.append(e.toMap().value(QStringLiteral("path")).toString());
    emit aboutToDeleteFiles(all);
    if (!folioIds.isEmpty()) m_folio->remove(folioIds);
    for (const QString &path : paths) {
        QFile f(path);
        f.setPermissions(f.permissions() | QFileDevice::WriteOwner | QFileDevice::WriteUser);
        if (!QFile::remove(path) && QFileInfo::exists(path))
            qWarning("papercut: could not delete %s", qUtf8Printable(path));
    }
}

void AppServices::copyText(const QString &text) const {
    if (QClipboard *cb = QGuiApplication::clipboard()) cb->setText(text);
}

bool AppServices::openUrl(const QString &url) const { return QDesktopServices::openUrl(QUrl::fromUserInput(url)); }

QString AppServices::localPath(const QUrl &url) const {
    return url.isLocalFile() ? QDir::toNativeSeparators(url.toLocalFile()) : url.toString();
}

QUrl AppServices::fileUrl(const QString &path) const { return QUrl::fromLocalFile(path); }

QString AppServices::fileName(const QString &path) const { return QFileInfo(path).fileName(); }

bool AppServices::copyFileTo(const QString &src, const QString &destPath) const {
    if (QFileInfo(src) == QFileInfo(destPath)) return true;
    QFile::remove(destPath);
    return QFile::copy(src, destPath);
}

QColor AppServices::argb(int value) const { return toColor(value); }
int AppServices::toArgb(const QColor &c) const { return static_cast<int>(c.rgba()); }

QString AppServices::readTextFile(const QString &path) const {
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) return QString();
    return QString::fromUtf8(f.readAll());
}

bool AppServices::writeTextFile(const QString &path, const QString &text) const {
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate)) return false;
    f.write(text.toUtf8());
    return true;
}

QUrl AppServices::suggestedFile(const QString &name) const {
    QString dir = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
    if (dir.isEmpty()) dir = QDir::homePath();
    return QUrl::fromLocalFile(QDir(dir).filePath(name));
}

bool AppServices::revealFile(const QString &path) const {
    return QDesktopServices::openUrl(QUrl::fromLocalFile(QFileInfo(path).absolutePath()));
}

QString AppServices::exportFileName(const QString &kind) const {
    return QStringLiteral("Papercut-%1.json").arg(kind.toLower());
}

QStringList AppServices::openFilters() const {
    return {QStringLiteral("Documents (*.pdf *.epub *.xps *.oxps *.cbz *.fb2 *.mobi *.txt *.text *.log *.md *.html *.htm *.xhtml *.svg *.png *.jpg *.jpeg *.gif *.tif *.tiff *.bmp *.webp *.docx *.xlsx *.pptx *.zip)"),
            QStringLiteral("All files (*)")};
}

} // namespace papercut
