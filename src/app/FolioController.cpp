#include "FolioController.h"

#include "AppServices.h"
#include "data/DocumentPaths.h"

#include <QFileInfo>
#include <algorithm>

namespace papercut {

FolioController::FolioController(QObject *parent) : DocumentController(parent) { refreshItems(); }

ViewerSettings FolioController::viewingSettings() const {
    ViewerSettings s = m_live ? *m_live : m_app->settings();
    s.centerHorizontally = m_app->settings().centerHorizontally;
    return s;
}

void FolioController::mutateView(const std::function<ViewerSettings(ViewerSettings)> &transform) {
    const ViewerSettings current = m_live ? *m_live : m_app->settings();
    m_live = transform(current);
    pushViewingToStrip();
}

QVector<FolioItem> FolioController::visible() const {
    QVector<FolioItem> out;
    for (const FolioItem &i : m_items)
        if (!i.hidden) out.push_back(i);
    return out;
}

QVariantMap FolioController::itemToMap(const FolioItem &i) const {
    QVariantMap m;
    m.insert(QStringLiteral("id"), i.id);
    m.insert(QStringLiteral("uri"), i.uri);
    m.insert(QStringLiteral("displayName"), i.displayName);
    m.insert(QStringLiteral("displayPath"), i.displayPath);
    m.insert(QStringLiteral("sortOrder"), i.sortOrder);
    m.insert(QStringLiteral("hidden"), i.hidden);
    m.insert(QStringLiteral("isStartItem"), i.isStartItem);
    return m;
}

QVariantList FolioController::visibleItems() const {
    QVariantList l;
    for (const FolioItem &i : visible()) l.append(itemToMap(i));
    return l;
}

QVariantList FolioController::allItems() const {
    QVariantList l;
    for (const FolioItem &i : m_items) l.append(itemToMap(i));
    return l;
}

void FolioController::refreshItems() {
    m_items = m_app->folio().all();
    emit itemsChanged();
}

ViewerSettings FolioController::folioViewSettings(const FolioItem &item, const ViewerSettings &global) const {
    ViewerSettings s = global;
    s.zoomMode = item.zoomMode;
    s.customZoomPercent = item.customZoomPercent;
    s.rememberLastZoom = true;
    s.lockZoom = item.lockZoom;
    s.lockSideScroll = item.lockSideScroll;
    s.originalColors = item.originalColors;
    s.twoFingerPan = true;
    return s;
}

int FolioController::startFolioIndex(const QVector<FolioItem> &items, const ViewerSettings &s) const {
    if (s.folioOpenLastViewed && s.lastFolioItemId != 0) {
        for (int i = 0; i < items.size(); ++i)
            if (items[i].id == s.lastFolioItemId) return i;
    }
    for (int i = 0; i < items.size(); ++i)
        if (items[i].isStartItem) return i;
    return 0;
}

void FolioController::clearCurrent() {
    m_currentId = 0;
    m_folioIndex = 0;
    m_live.reset();
    setSourceUri(QString());
    setDocument(std::nullopt);
    emit itemsChanged();
}

void FolioController::loadStart() {
    refreshItems();
    const ViewerSettings s = m_app->settingsRepo().get();
    const QVector<FolioItem> items = visible();
    if (items.isEmpty()) {
        clearCurrent();
        setPageLoading(false);
        markStartupReady();
        return;
    }
    const int start = startFolioIndex(items, s);
    m_currentId = items[start].id;
    m_folioIndex = start;
    m_live = folioViewSettings(items[start], s);
    emit itemsChanged();
    openItem(items[start]);
    markStartupReady();
}

void FolioController::openItem(const FolioItem &item) {
    const int gen = ++m_openGen;
    cancelAllRenders();
    m_live = folioViewSettings(item, m_app->settingsRepo().get());
    setSourceUri(item.uri);
    setPageLoading(true);
    setDocument(std::nullopt);
    resetSearchState();
    if (!m_app->folio().ensureLocal(item)) {
        setPageLoading(false);
        emitError(QStringLiteral("OPEN"));
        return;
    }
    const QString file = m_app->folio().fileFor(item);
    const int startPage = item.lastPageIndex;
    const float scale = float(m_app->fontScale());
    const QString css = m_app->documentUserCss();
    m_worker.post([=, this] {
        QString code;
        OpenData data;
        try {
            openDocumentOnEngine(file, startPage, scale, css);
            data = captureOpenData();
        } catch (const EngineError &e) {
            code = e.code();
        } catch (...) {
            code = QStringLiteral("OPEN");
        }
        postToUi([=, this] {
            if (gen != m_openGen) return;
            if (code.isEmpty()) {
                finishOpen(item, data);
            } else if (code == QLatin1String("PASSWORD")) {
                showPasswordPrompt();
            } else {
                setPageLoading(false);
                setDocument(std::nullopt);
                emitError(QStringLiteral("OPEN"));
            }
        });
    });
}

void FolioController::finishOpen(const FolioItem &item, const OpenData &data) {
    setSourceUri(item.uri);
    m_live = folioViewSettings(item, m_app->settingsRepo().get());
    stashRestore(item.lastPageIndex, item.lastScrollXPdf, item.lastScrollYPdf);
    DocState d;
    d.valid = true;
    d.uri = m_app->folio().fileFor(item);
    d.pages = data.pages;
    d.mime = data.mime;
    d.reflowable = data.reflowable;
    setDocument(d);
    setRotationsJson(item.pageRotations);
    resetSearchState();
    fillPagesAndOutline();
    pushViewingToStrip();
}

void FolioController::resumeAfterPassword(const OpenData &data) {
    for (const FolioItem &i : visible()) {
        if (i.id == m_currentId) {
            clearPasswordPrompt();
            finishOpen(i, data);
            return;
        }
    }
}

void FolioController::addFromPaths(const QStringList &paths) {
    refreshItems();
    const bool wasEmpty = visible().isEmpty();
    int added = 0;
    for (const QString &p : paths) {
        const PathLabel label = DocumentPaths::label(p);
        if (m_app->folio().add(p, label.displayName, label.displayPath, p)) ++added;
    }
    refreshItems();
    if (wasEmpty && added > 0) loadStart();
    emit itemsAdded(added);
}

bool FolioController::storesLastPosition() const { return m_app->settings().folioStoreLastPosition; }

void FolioController::showFolioIndex(int index) {
    const QVector<FolioItem> items = visible();
    if (items.isEmpty()) {
        clearCurrent();
        setPageLoading(false);
        return;
    }
    const int clamped = std::clamp(index, 0, int(items.size()) - 1);
    const FolioItem &item = items[clamped];
    if (m_folioIndex == clamped && m_currentId == item.id && m_doc.valid) return;
    rememberPosition();
    m_folioIndex = clamped;
    m_currentId = item.id;
    m_app->updateSettings([&](ViewerSettings s) {
        s.lastFolioItemId = item.id;
        return s;
    });
    emit itemsChanged();
    openItem(item);
}

void FolioController::onFolioSwipe(int delta) {
    const int count = int(visible().size());
    if (count == 0) return;
    const int next = std::clamp(m_folioIndex + delta, 0, count - 1);
    if (next != m_folioIndex) showFolioIndex(next);
}

void FolioController::rememberPosition() {
    if (!storesLastPosition() || !m_doc.valid || !m_strip) return;
    const QVariantMap p = readingPosition();
    saveFolioSnapshot(p.value(QStringLiteral("page")).toInt(), p.value(QStringLiteral("scrollX")).toDouble(),
                      p.value(QStringLiteral("scrollY")).toDouble());
}

void FolioController::saveFolioSnapshot(int page, double scrollX, double scrollY) {
    if (!m_live || m_currentId == 0) return;
    const ViewerSettings &live = *m_live;
    m_app->folio().saveSnapshot(m_currentId, live.zoomMode, live.customZoomPercent, live.lockZoom,
                                live.lockSideScroll, live.originalColors, page, float(scrollX), float(scrollY),
                                rotationsJson());
}

void FolioController::onRotationsChanged() {
    if (m_currentId == 0) return;
    m_app->folio().saveRotations(m_currentId, rotationsJson());
    for (FolioItem &item : m_items) {
        if (item.id == m_currentId) item.pageRotations = rotationsJson();
    }
}

static QVector<qint64> toIds(const QVariantList &ids) {
    QVector<qint64> v;
    for (const QVariant &i : ids) v.append(i.toLongLong());
    return v;
}

void FolioController::setFolioHidden(const QVariantList &ids, bool hidden) {
    m_app->folio().setHidden(toIds(ids), hidden);
    refreshItems();
}

void FolioController::deleteFolioItems(const QVariantList &ids) {
    m_app->folio().remove(toIds(ids));
    refreshItems();
    const QVector<FolioItem> vis = visible();
    const bool stillThere = std::any_of(vis.begin(), vis.end(), [&](const FolioItem &i) { return i.id == m_currentId; });
    if (vis.isEmpty() || !stillThere) loadStart();
}

void FolioController::reorderFolio(const QVariantList &ids) {
    m_app->folio().reorder(toIds(ids));
    refreshItems();
}

void FolioController::setFolioStart(qint64 id) {
    m_app->folio().setStartItem(id);
    refreshItems();
}

QString FolioController::currentCopy() const {
    for (const FolioItem &i : m_items) {
        if (i.id == m_currentId) {
            const QString f = m_app->folio().fileFor(i);
            const QFileInfo fi(f);
            return fi.exists() && fi.size() > 0 ? f : QString();
        }
    }
    return QString();
}

} // namespace papercut
