#include "PdfStripItem.h"

#include "core/Keybinds.h"

#include <QDateTime>
#include <QGuiApplication>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPointingDevice>
#include <QQuickWindow>
#include <QScreen>
#include <QTouchEvent>
#include <QWheelEvent>
#include <algorithm>
#include <cmath>
#include <limits>
#include <memory>

namespace papercut {

namespace {

constexpr int kPrefetchPages = 8;
constexpr int kMaxCachedPages = 32;
constexpr qint64 kMaxCacheBytes = 96LL * 1024LL * 1024LL;
constexpr int kDoubleTapTimeoutMs = 300;
constexpr int kDoubleTapMinMs = 40;
constexpr int kLongPressMs = 500;
constexpr float kTouchSlop = 8.f;
constexpr float kDoubleTapSlop = 100.f;
constexpr float kBiggerTouchSlop = 16.f;
constexpr float kMinFlingVelocity = 50.f;
constexpr float kMaxFlingVelocity = 8000.f;
constexpr float kEdgeSlop = 24.f;

QColor argb(int c) { return QColor::fromRgba(static_cast<QRgb>(static_cast<quint32>(c))); }

template <class T> T coerce(T v, T lo, T hi) { return std::min(std::max(v, lo), hi); }

} // namespace

PdfStripItem::PdfStripItem(QQuickItem *parent) : QQuickPaintedItem(parent) {
    setAcceptedMouseButtons(Qt::LeftButton | Qt::RightButton);
    setAcceptTouchEvents(true);
    setFlag(ItemHasContents, true);
    setFlag(ItemIsFocusScope, true);
    setActiveFocusOnTab(true);
    setAntialiasing(true);
    setOpaquePainting(true);
    setFillColor(Qt::black);
    for (QTimer *t : {&m_confirmTimer, &m_longPressTimer, &m_pendingZoomTimer, &m_zoomEndTimer}) t->setSingleShot(true);
    m_flingTimer.setInterval(8);
    connect(&m_confirmTimer, &QTimer::timeout, this, [this] {
        if (!m_stillDown) onSingleTapConfirmed();
        else m_deferConfirmTap = true;
    });
    connect(&m_longPressTimer, &QTimer::timeout, this, [this] {
        m_confirmTimer.stop();
        m_deferConfirmTap = false;
        m_inLongPress = true;
        onLongPress();
    });
    connect(&m_pendingZoomTimer, &QTimer::timeout, this, [this] { m_pendingZoom = false; });
    connect(&m_zoomEndTimer, &QTimer::timeout, this, [this] {
        if (m_pages.isEmpty()) return;
        finishPinchZoom();
    });
    connect(&m_flingTimer, &QTimer::timeout, this, [this] { flingStep(); });
    tintSearchColors();
}

PdfStripItem::~PdfStripItem() = default;

void PdfStripItem::setFolioPaging(bool on) {
    if (m_folioPaging == on) return;
    m_folioPaging = on;
    emit folioPagingChanged();
}

float PdfStripItem::dpi() const {
    QScreen *s = window() ? window()->screen() : QGuiApplication::primaryScreen();
    const double d = s ? s->logicalDotsPerInch() : 96.0;
    return d > 1.0 ? float(d) : 96.f;
}

float PdfStripItem::dpr() const {
    return window() ? float(window()->devicePixelRatio()) : 1.f;
}

float PdfStripItem::scaleForPercent(float percent) const {
    return coerce((dpi() / 72.f) * (percent / 100.f), minScale(), std::max(minScale(), maxScale()));
}

float PdfStripItem::currentZoomPercent() const {
    const float oneHundred = dpi() / 72.f;
    return oneHundred <= 0.f ? 100.f : (m_scale / oneHundred) * 100.f;
}

void PdfStripItem::tintSearchColors() {
    const QColor c = argb(m_highlightColor);
    m_searchColor = QColor(c.red(), c.green(), c.blue(), 0x55);
    m_searchCurrentColor = QColor(c.red(), c.green(), c.blue(), 0x99);
    m_highlightPaintColor = QColor(c.red(), c.green(), c.blue(), 0x66);
}

float PdfStripItem::screenCenterPdfX() const {
    return m_scrollXPdf + (m_scale <= 0.f ? 0.f : float(width() / 2.0) / m_scale);
}

float PdfStripItem::screenCenterPdfY() const {
    return m_scrollYPdf + (m_scale <= 0.f ? 0.f : float(height() / 2.0) / m_scale);
}

float PdfStripItem::fitWidthPageWidth() const {
    if (m_pages.isEmpty()) return m_maxWidthPdf;
    const int i = currentVisiblePage();
    const float w = (i >= 0 && i < m_pages.size()) ? m_pages[i].width : m_maxWidthPdf;
    return std::max(w, 1.f);
}

float PdfStripItem::fitWidthUsable() const {
    return std::max(float(width()) - 2.f * m_horizontalPagePaddingPx, 1.f);
}

float PdfStripItem::minScale() const {
    if (width() <= 0 || m_pages.isEmpty()) return 0.1f;
    return std::min(fitWidthUsable() / m_maxWidthPdf, float(height()) / std::max(m_pages[0].height, 1.f)) * 0.5f;
}

float PdfStripItem::maxScale() const {
    return std::max(8.f * (dpi() / 72.f), (float(width()) / m_maxWidthPdf) * 8.f);
}

float PdfStripItem::centerScrollX() const {
    if (m_scale <= 0.f) return 0.f;
    const float pageW = fitWidthPageWidth();
    return (pageW * m_scale - float(width())) / (2.f * m_scale);
}

bool PdfStripItem::allowSidePan(int pointerCount) const {
    if (m_folioPaging && pointerCount < 2 && !m_twoFingerGesture) return false;
    if (m_lockSideScroll) return false;
    return true;
}

void PdfStripItem::rebuildTops() {
    m_pageTops = QVector<float>(m_pages.size());
    float y = 0.f, maxW = 1.f;
    const float gap = m_scale > 0.f ? m_pagePaddingPx / m_scale : 0.f;
    for (int i = 0; i < m_pages.size(); ++i) {
        m_pageTops[i] = y;
        y += m_pages[i].height;
        if (i < m_pages.size() - 1) y += gap;
        maxW = std::max(maxW, m_pages[i].width);
    }
    m_totalHeightPdf = y;
    m_maxWidthPdf = maxW;
}

void PdfStripItem::rebuildTopsPreservingScroll() {
    const int vis = currentVisiblePage();
    float offset = 0.f;
    if (vis >= 0 && vis < m_pages.size() && vis < m_pageTops.size()) offset = m_scrollYPdf - m_pageTops[vis];
    rebuildTops();
    if (vis >= 0 && vis < m_pageTops.size()) m_scrollYPdf = m_pageTops[vis] + offset;
}

void PdfStripItem::clampScroll() {
    if (m_pages.isEmpty() || m_scale <= 0.f) return;
    const float w = float(width()), h = float(height());
    const float contentW = m_maxWidthPdf * m_scale;
    const float minX = -(w / 2.f) / m_scale;
    const float maxX = (contentW - w / 2.f) / m_scale;
    const float minY = -(h / 2.f) / m_scale;
    const float maxY = m_totalHeightPdf - (h / 2.f) / m_scale;
    m_scrollXPdf = coerce(m_scrollXPdf, std::min(minX, maxX), std::max(minX, maxX));
    m_scrollYPdf = coerce(m_scrollYPdf, std::min(minY, maxY), std::max(minY, maxY));
}

int PdfStripItem::firstVisiblePage(float topPdf) const {
    if (m_pages.isEmpty()) return 0;
    int lo = 0, hi = m_pages.size() - 1, ans = 0;
    while (lo <= hi) {
        const int mid = (lo + hi) >> 1;
        const float bottom = m_pageTops[mid] + m_pages[mid].height;
        if (bottom < topPdf) lo = mid + 1;
        else {
            ans = mid;
            hi = mid - 1;
        }
    }
    return ans;
}

int PdfStripItem::lastVisiblePage(float botPdf) const {
    int i = firstVisiblePage(botPdf);
    while (i < m_pages.size() - 1 && m_pageTops[i] < botPdf) ++i;
    return coerce(i, 0, std::max(0, int(m_pages.size()) - 1));
}

int PdfStripItem::currentVisiblePage() const {
    if (m_pages.isEmpty()) return 0;
    const float y = m_scrollYPdf + (m_scale <= 0.f ? 0.f : float(height() / 2.0) / m_scale);
    return coerce(firstVisiblePage(y), 0, int(m_pages.size()) - 1);
}

std::optional<int> PdfStripItem::pageAt(float y) const {
    if (m_pages.isEmpty() || m_scale <= 0.f) return std::nullopt;
    const float pdfY = y / m_scale + m_scrollYPdf;
    return coerce(firstVisiblePage(pdfY), 0, int(m_pages.size()) - 1);
}

std::optional<int> PdfStripItem::pageAt(float x, float y) const {
    if (m_pages.isEmpty() || m_scale <= 0.f) return std::nullopt;
    const float pdfY = y / m_scale + m_scrollYPdf;
    const float pdfX = x / m_scale + m_scrollXPdf;
    const int i = coerce(firstVisiblePage(pdfY), 0, int(m_pages.size()) - 1);
    if (pdfY < m_pageTops[i] || pdfY > m_pageTops[i] + m_pages[i].height) return std::nullopt;
    if (pdfX < 0.f || pdfX > m_pages[i].width) return std::nullopt;
    return i;
}

float PdfStripItem::currentOffsetOnPage() const {
    if (m_pages.isEmpty()) return 0.f;
    const int i = coerce(currentVisiblePage(), 0, int(m_pages.size()) - 1);
    if (i >= m_pageTops.size()) return 0.f;
    return coerce(m_scrollYPdf - m_pageTops[i], 0.f, m_pages[i].height);
}

void PdfStripItem::reportVisiblePage() {
    const int count = m_pages.size();
    const int index = count == 0 ? 0 : currentVisiblePage();
    if (index != m_lastReportedPage || count != m_lastReportedCount) {
        m_lastReportedPage = index;
        m_lastReportedCount = count;
        emit visiblePageChanged(index, count);
    }
}

std::pair<int, int> PdfStripItem::keepRange(int center) const {
    if (m_pages.isEmpty()) return {0, -1};
    return {std::max(center - kPrefetchPages, 0), std::min(center + kPrefetchPages, int(m_pages.size()) - 1)};
}

int PdfStripItem::targetWidthPx() const {
    return coerce(int(std::lround(m_maxWidthPdf * m_scale * dpr())), 1, 4096);
}

bool PdfStripItem::hasPainted(int index) const {
    const auto it = m_bitmaps.constFind(index);
    return it != m_bitmaps.constEnd() && it->key == paintKey() && !it->image.isNull();
}

void PdfStripItem::requestPagesAround(int center, int widthHint) {
    if (m_pages.isEmpty() || width() <= 0) return;
    const auto keep = keepRange(center);
    emit cancelRendersRequested(keep.first, keep.second);
    for (auto it = m_inFlight.begin(); it != m_inFlight.end();) {
        if (*it < keep.first || *it > keep.second) it = m_inFlight.erase(it);
        else ++it;
    }
    const int target = widthHint > 0 ? widthHint : targetWidthPx();
    requestIfNeeded(center, target, true);
    for (int d = 1; d <= kPrefetchPages; ++d) {
        const int before = center - d, after = center + d;
        if (before >= keep.first) requestIfNeeded(before, target, false);
        if (after <= keep.second) requestIfNeeded(after, target, false);
    }
}

void PdfStripItem::requestIfNeeded(int index, int targetWidth, bool urgent) {
    const auto it = m_bitmaps.constFind(index);
    const bool sharp = it != m_bitmaps.constEnd() && it->key == paintKey() && !it->image.isNull()
                       && std::abs(it->width - targetWidth) <= 1;
    if (!sharp && !m_inFlight.contains(index)) {
        m_inFlight.insert(index);
        emit pageRequested(index, targetWidth, urgent);
    }
}

void PdfStripItem::recycleBitmaps() {
    m_bitmaps.clear();
    m_cachedBytes = 0;
}

void PdfStripItem::dropCached(int key) {
    const auto it = m_bitmaps.find(key);
    if (it == m_bitmaps.end()) return;
    m_cachedBytes -= it->bytes;
    m_bitmaps.erase(it);
}

void PdfStripItem::trimCache() {
    const auto keep = keepRange(currentVisiblePage());
    while (m_bitmaps.size() > kMaxCachedPages || m_cachedBytes > kMaxCacheBytes) {
        int eldest = -1;
        quint64 best = std::numeric_limits<quint64>::max();
        for (auto it = m_bitmaps.cbegin(); it != m_bitmaps.cend(); ++it) {
            if (it.key() >= keep.first && it.key() <= keep.second) continue;
            if (it->stamp < best) {
                best = it->stamp;
                eldest = it.key();
            }
        }
        if (eldest < 0) break;
        dropCached(eldest);
    }
}

void PdfStripItem::trimOffscreen(int first, int last) {
    const int keepFrom = std::max(first - kPrefetchPages, 0);
    const int keepTo = std::min(last + kPrefetchPages, int(m_pages.size()) - 1);
    for (auto it = m_bitmaps.begin(); it != m_bitmaps.end();) {
        if (it.key() < keepFrom || it.key() > keepTo) {
            m_cachedBytes -= it->bytes;
            it = m_bitmaps.erase(it);
        } else {
            ++it;
        }
    }
    trimCache();
}

void PdfStripItem::schedulePostDraw() {
    if (m_postDrawPending) return;
    m_postDrawPending = true;
    QMetaObject::invokeMethod(this, "postDraw", Qt::QueuedConnection);
}

void PdfStripItem::postDraw() {
    m_postDrawPending = false;
    if (m_pages.isEmpty() || width() <= 0) {
        reportVisiblePage();
        return;
    }
    if (!m_scrubbing) {
        const int active = currentVisiblePage();
        requestPagesAround(active, targetWidthPx());
        const auto keep = keepRange(active);
        trimOffscreen(keep.first, keep.second);
        if (hasPainted(active) && !m_linkCache.contains(active) && !m_linksInFlight.contains(active)) {
            m_linksInFlight.insert(active);
            emit linksRequested(active);
        }
    }
    reportVisiblePage();
}

void PdfStripItem::replacePageSizes(const QVector<PageInfo> &pages) {
    if (pages.isEmpty() || pages.size() != m_pages.size()) return;
    m_pages = pages;
    rebuildTopsPreservingScroll();
    applyPendingPageRestore();
    clampScroll();
    if (m_presentation) applyFitPage();
    update();
}

void PdfStripItem::invalidatePages() {
    ++m_layoutGen;
    recycleBitmaps();
    m_inFlight.clear();
    m_textCache.clear();
    m_textWaiters.clear();
    m_linkCache.clear();
    m_linksInFlight.clear();
    update();
}

void PdfStripItem::applyFitPage() { fitPageAt(currentVisiblePage()); }

void PdfStripItem::fitPageAt(int page) {
    if (m_pages.isEmpty() || width() <= 0 || height() <= 0) return;
    page = coerce(page, 0, int(m_pages.size() - 1));
    m_zoomMode = ZoomMode::FitPage;
    const PageInfo &fit = m_pages[page];
    const float w = float(width()), h = float(height());
    m_scale = coerce(std::min(w / std::max(fit.width, 1.f), h / std::max(fit.height, 1.f)), minScale(),
                     std::max(minScale(), maxScale()));
    rebuildTops();
    const float viewH = h / std::max(m_scale, 0.0001f);
    m_scrollYPdf = m_pageTops[page] - std::max(0.f, (viewH - fit.height) / 2.f);
    m_scrollXPdf = centerScrollX();
    clampScroll();
    requestPagesAround(page);
    reportVisiblePage();
    update();
}

void PdfStripItem::setPresentation(bool on) {
    m_presentation = on;
    m_presWheel = 0.f;
    m_presPan = 0.f;
    if (on) {
        m_lockZoom = true;
        m_lockSideScroll = true;
        applyFitPage();
    }
}

void PdfStripItem::stepPage(int delta) {
    if (m_pages.isEmpty() || delta == 0) return;
    const int next = coerce(currentVisiblePage() + delta, 0, int(m_pages.size() - 1));
    if (m_presentation) {
        fitPageAt(next);
        return;
    }
    goToPage(next);
}

void PdfStripItem::setDocument(const QVector<PageInfo> &pages) {
    recycleBitmaps();
    m_inFlight.clear();
    m_textCache.clear();
    m_textWaiters.clear();
    m_linkCache.clear();
    m_linksInFlight.clear();
    m_selection.reset();
    m_searchHits.clear();
    m_searchByPage.clear();
    m_searchIndex = 0;
    emit selectionTextChanged(QString());
    m_tapZoomSnapshot.reset();
    m_userZoomed = false;
    m_fitWidthLockHeld = false;
    m_zoomMode = m_settingsZoomMode;
    m_customPercent = m_settingsZoomPercent;
    m_pages = pages;
    m_lastReportedPage = -1;
    m_lastReportedCount = -1;
    stopFling();
    rebuildTops();
    if (width() > 0 && height() > 0 && !m_pages.isEmpty()) {
        m_scrollXPdf = 0.f;
        m_scrollYPdf = 0.f;
        applyDocumentZoom(false);
    } else if (!hasPendingPageRestore()) {
        m_scrollXPdf = 0.f;
        m_scrollYPdf = 0.f;
    }
    reportVisiblePage();
    ensureFitWidthLock();
    schedulePostDraw();
    update();
}

void PdfStripItem::pendingGoToPage(int index, float offsetFromTop, float scrollX) {
    m_pendingRestorePage = index;
    m_pendingRestoreOffset = offsetFromTop;
    m_pendingRestoreX = scrollX;
}

void PdfStripItem::goToPageOffset(int index, float offsetFromTop, std::optional<float> scrollX) {
    m_pendingRestorePage = index;
    m_pendingRestoreOffset = offsetFromTop;
    if (scrollX) m_pendingRestoreX = scrollX;
    if (!applyPendingPageRestore()) return;
    stopFling();
    requestPagesAround(coerce(index, 0, int(m_pages.size()) - 1));
    update();
}

bool PdfStripItem::applyPendingPageRestore() {
    if (!m_pendingRestorePage) return false;
    const int index = *m_pendingRestorePage;
    if (width() <= 0 || m_pages.isEmpty() || index < 0 || index >= m_pages.size()
        || m_pageTops.size() != m_pages.size())
        return false;
    const float h = m_pages[index].height;
    float extra = 0.f;
    if (m_pendingRestoreOffset && *m_pendingRestoreOffset >= 0.f && *m_pendingRestoreOffset <= h)
        extra = *m_pendingRestoreOffset;
    if (m_pendingRestoreX) m_scrollXPdf = *m_pendingRestoreX;
    m_scrollYPdf = m_pageTops[index] + extra;
    clampScroll();
    m_pendingRestorePage.reset();
    m_pendingRestoreOffset.reset();
    m_pendingRestoreX.reset();
    stopFling();
    requestPagesAround(index);
    update();
    return true;
}

void PdfStripItem::goToPage(int index, std::optional<float> yOnPage) {
    if (m_pages.isEmpty() || index < 0 || index >= m_pages.size()) return;
    const PageInfo &info = m_pages[index];
    float extra = 0.f;
    if (yOnPage) {
        const float rel = *yOnPage - info.y0;
        if (rel >= 0.f && rel <= info.height) extra = rel;
    }
    m_scrollYPdf = m_pageTops[index] + extra;
    clampScroll();
    stopFling();
    if (m_scrubbing) {
        if (!m_inFlight.contains(index) && !hasPainted(index)) {
            m_inFlight.insert(index);
            emit pageRequested(index, targetWidthPx(), true);
        }
    } else {
        requestPagesAround(index);
    }
    update();
}

void PdfStripItem::setScrubbing(bool on) {
    if (m_scrubbing == on) return;
    m_scrubbing = on;
    if (!on) {
        schedulePostDraw();
        update();
    }
}

void PdfStripItem::setPageSelect(bool on, const QSet<int> &selected) {
    const bool changed = m_pageSelectMode != on;
    m_pageSelectMode = on;
    m_selectedPages = selected;
    if (on) {
        clearSelection();
    } else if (m_pageSelectLockSide) {
        m_lockSideScroll = m_savedLockSideScroll;
        m_pageSelectLockSide = false;
    }
    if (changed) emit pageSelectModeChanged();
    update();
}

void PdfStripItem::applyFitWidthForPageSelect() {
    applyFitWidth();
}

void PdfStripItem::setPageImage(int index, int w, const QImage &image, qint64 key) {
    if (key != paintKey()) return;
    m_inFlight.remove(index);
    CachedPage page;
    page.width = w;
    page.key = paintKey();
    page.image = image;
    page.bytes = image.sizeInBytes();
    page.stamp = ++m_stamp;
    const auto old = m_bitmaps.constFind(index);
    if (old != m_bitmaps.constEnd()) m_cachedBytes -= old->bytes;
    m_cachedBytes += page.bytes;
    m_bitmaps.insert(index, page);
    trimCache();
    schedulePostDraw();
    update();
}

void PdfStripItem::setPageLinks(int index, const QVector<PageLink> &links) {
    m_linksInFlight.remove(index);
    m_linkCache.insert(index, links);
}

void PdfStripItem::setPageText(int page, const QVector<CharBox> &chars) {
    m_textCache.insert(page, chars);
    const auto waiters = m_textWaiters.take(page);
    for (const auto &fn : waiters) fn(chars);
}

void PdfStripItem::ensureText(int page, std::function<void(const QVector<CharBox> &)> done) {
    const auto it = m_textCache.constFind(page);
    if (it != m_textCache.constEnd()) {
        done(*it);
        return;
    }
    auto &list = m_textWaiters[page];
    const bool first = list.isEmpty();
    list.append(std::move(done));
    if (first) emit textRequested(page);
}

void PdfStripItem::setSettings(const ViewerSettings &s) {
    m_bgColor = s.bgColor;
    m_highlightColor = s.highlightColor;
    m_lockZoom = s.lockZoom;
    m_autoDisablePan = s.autoDisablePan;
    if (!m_autoDisablePan) m_reLockOnFitWidth = false;
    if (!m_reLockOnFitWidth && !m_pageSelectLockSide) m_lockSideScroll = s.lockSideScroll;
    const bool wantCenter = s.centerHorizontally && !m_centerHorizontally;
    m_centerHorizontally = s.centerHorizontally;
    m_twoFingerPan = false;
    const float nextPad = float(s.pagePaddingDp);
    const float nextHoriz = float(s.horizontalPagePaddingDp);
    const bool paddingChanged = std::abs(nextPad - m_pagePaddingPx) > 0.1f;
    const bool horizChanged = std::abs(nextHoriz - m_horizontalPagePaddingPx) > 0.1f;
    m_pagePaddingPx = nextPad;
    m_horizontalPagePaddingPx = nextHoriz;
    m_pageBorderEnabled = s.pageBorderEnabled;
    m_pageBorderColor = s.pageBorderColor;
    m_allowFollowLinks = s.allowFollowLinks;
    m_allowTextSelection = s.allowTextSelection;
    m_doubleTapMenu = false;
    m_singleTapMenu = false;
    m_swipeFromBottomMenu = false;
    tintSearchColors();
    setFillColor(argb(m_bgColor));
    const qint64 newKey = (qint64(s.fgColor) << 32) ^ qint64(quint32(s.bgColor))
                          ^ qint64(s.imageColorBlend * 1000.f) ^ qint64(s.imageAlpha * 1000.f)
                          ^ (s.originalColors ? 1LL : 0LL);
    const bool colorsChanged = newKey != m_renderKey;
    m_renderKey = newKey;
    if (m_presentation) {
        m_lockZoom = true;
        m_lockSideScroll = true;
    }
    const ZoomMode mode = s.effectiveMode();
    const float percent = s.effectivePercent();
    m_settingsZoomMode = mode;
    m_settingsZoomPercent = percent;
    const bool zoomChanged = mode != m_zoomMode || (mode == ZoomMode::Custom && std::abs(percent - m_customPercent) > 0.4f);
    if (zoomChanged && !m_userZoomed && !m_presentation) {
        m_zoomMode = mode;
        m_customPercent = percent;
    }
    if (colorsChanged) {
        recycleBitmaps();
        m_inFlight.clear();
    }
    if (width() > 0 && height() > 0 && !m_pages.isEmpty() && m_presentation) {
        applyFitPage();
    } else if (width() > 0 && height() > 0 && !m_pages.isEmpty() && zoomChanged && !m_userZoomed) {
        if (mode == ZoomMode::FitPage) applyFitPageMode();
        else applyZoomInternal(mode, percent, mode == ZoomMode::Custom, mode != ZoomMode::Custom);
    } else if (width() > 0 && !m_pages.isEmpty() && colorsChanged) {
        m_inFlight.clear();
    } else if (!m_pages.isEmpty() && (paddingChanged || horizChanged)) {
        if (horizChanged && m_zoomMode == ZoomMode::FitWidth && width() > 0) {
            applyZoomInternal(ZoomMode::FitWidth, m_customPercent, true, true);
        } else if (paddingChanged) {
            rebuildTopsPreservingScroll();
            clampScroll();
        }
    }
    if (!m_allowTextSelection && m_selection) clearSelection();
    ensureFitWidthLock();
    if (wantCenter && width() > 0 && !m_pages.isEmpty()) recenterX();
    schedulePostDraw();
    update();
}

void PdfStripItem::applyResetZoom(const ViewerSettings &s) {
    const ZoomMode mode = s.effectiveMode();
    const float percent = s.effectivePercent();
    m_zoomMode = mode;
    m_customPercent = percent;
    if (mode == ZoomMode::FitPage) applyFitPageMode();
    else applyZoomInternal(mode, percent, true, true);
    update();
}

void PdfStripItem::applyFitWidth() {
    m_userZoomed = true;
    m_fitWidthLockHeld = false;
    m_zoomMode = ZoomMode::FitWidth;
    applyZoomInternal(ZoomMode::FitWidth, m_customPercent, true, true);
    m_scrollXPdf = centerScrollX();
    clampScroll();
    applyLockSideScroll(true);
    update();
}

void PdfStripItem::recenterX() {
    if (m_pages.isEmpty() || width() <= 0) return;
    m_scrollXPdf = centerScrollX();
    clampScroll();
    update();
}

void PdfStripItem::applyCustomZoom(float percent) {
    const float next = coerce(percent, 25.f, 400.f);
    m_userZoomed = true;
    if (m_zoomMode != ZoomMode::Custom || std::abs(currentZoomPercent() - next) > 0.4f) noteUserZoomTo(ZoomMode::Custom);
    m_zoomMode = ZoomMode::Custom;
    m_customPercent = next;
    applyScale(scaleForPercent(next));
}

void PdfStripItem::clearReLockOnFitWidth() {
    m_reLockOnFitWidth = false;
    m_fitWidthLockHeld = true;
}

void PdfStripItem::setLockSideScroll(bool on) {
    m_reLockOnFitWidth = false;
    m_fitWidthLockHeld = true;
    applyLockSideScroll(on);
}

void PdfStripItem::ensureFitWidthLock() {
    if (m_zoomMode != ZoomMode::FitWidth || m_fitWidthLockHeld || m_pageSelectLockSide) return;
    applyLockSideScroll(true);
}

void PdfStripItem::applyLockSideScroll(bool on) {
    if (m_lockSideScroll == on) return;
    m_lockSideScroll = on;
    emit lockSideScrollChanged(on);
}

void PdfStripItem::noteUserZoomTo(ZoomMode next) {
    if (!m_autoDisablePan) return;
    if (m_lockSideScroll) {
        if (next != ZoomMode::FitWidth) {
            m_reLockOnFitWidth = true;
            applyLockSideScroll(false);
        }
    } else if (next == ZoomMode::FitWidth && m_reLockOnFitWidth) {
        m_reLockOnFitWidth = false;
        applyLockSideScroll(true);
    }
}

void PdfStripItem::applyScale(float nextScale, std::optional<float> focusX, std::optional<float> focusY,
                              std::optional<float> fromX, std::optional<float> fromY) {
    if (m_pages.isEmpty() || width() <= 0 || m_scale <= 0.f) return;
    if (m_autoDisablePan && std::abs(nextScale - m_scale) > 0.001f) noteUserZoomTo(ZoomMode::Custom);
    const float prev = m_scale;
    const float toX = focusX.value_or(float(width() / 2.0));
    const float toY = focusY.value_or(float(height() / 2.0));
    const float originX = fromX.value_or(toX);
    const float originY = fromY.value_or(toY);
    const float pinX = m_lockSideScroll ? originX : toX;
    const float pdfX = originX / prev + m_scrollXPdf;
    const int vis = pageAt(originY).value_or(currentVisiblePage());
    const float offsetInPage = (vis >= 0 && vis < m_pageTops.size()) ? (originY / prev + m_scrollYPdf) - m_pageTops[vis] : 0.f;
    m_scale = coerce(nextScale, minScale(), std::max(minScale(), maxScale()));
    rebuildTops();
    m_scrollXPdf = pdfX - pinX / m_scale;
    m_scrollYPdf = (vis >= 0 && vis < m_pageTops.size()) ? m_pageTops[vis] + offsetInPage - toY / m_scale : 0.f;
    if (m_centerHorizontally) m_scrollXPdf = centerScrollX();
    clampScroll();
    m_inFlight.clear();
    update();
}

PdfStripItem::TapZoomSnapshot PdfStripItem::captureTapZoomSnapshot() const {
    TapZoomSnapshot s;
    s.mode = m_zoomMode;
    s.percent = currentZoomPercent();
    s.centerPdfX = screenCenterPdfX();
    if (m_autoDisablePan) s.lockSideScroll = m_lockSideScroll;
    return s;
}

void PdfStripItem::applyTapZoomSnapshot(const TapZoomSnapshot &snap) {
    const float keepPdfY = screenCenterPdfY();
    m_zoomMode = snap.mode;
    if (snap.mode == ZoomMode::Custom) m_customPercent = coerce(snap.percent, 25.f, 400.f);
    float nextScale = 1.f;
    switch (snap.mode) {
    case ZoomMode::FitWidth: nextScale = fitWidthUsable() / fitWidthPageWidth(); break;
    case ZoomMode::FitPage: {
        const int vis = currentVisiblePage();
        const PageInfo &page = (vis >= 0 && vis < m_pages.size()) ? m_pages[vis] : m_pages[0];
        nextScale = std::min(float(width()) / page.width, float(height()) / std::max(page.height, 1.f));
        break;
    }
    case ZoomMode::Custom: nextScale = scaleForPercent(snap.percent); break;
    }
    applyScale(coerce(nextScale, minScale(), std::max(minScale(), maxScale())), float(width() / 2.0),
               float(height() / 2.0));
    if (m_scale > 0.f) {
        m_scrollXPdf = snap.centerPdfX - float(width() / 2.0) / m_scale;
        m_scrollYPdf = keepPdfY - float(height() / 2.0) / m_scale;
    }
    if (m_autoDisablePan && snap.lockSideScroll) {
        m_reLockOnFitWidth = false;
        applyLockSideScroll(*snap.lockSideScroll);
    }
    clampScroll();
    m_inFlight.clear();
    m_tapZoomSnapshot.reset();
    update();
}

void PdfStripItem::finishPinchZoom() {
    if (m_lockZoom) return;
    m_zoomMode = ZoomMode::Custom;
    m_customPercent = coerce(currentZoomPercent(), 25.f, 400.f);
    noteUserZoomTo(ZoomMode::Custom);
    emit pinchEnded(m_customPercent);
}

void PdfStripItem::toggleTapZoom(float focusX, float focusY) {
    if (m_pages.isEmpty() || width() <= 0 || m_lockZoom) return;
    m_userZoomed = true;
    if (std::abs(currentZoomPercent() - 100.f) < 1.f) {
        const auto snap = m_tapZoomSnapshot;
        m_tapZoomSnapshot.reset();
        if (!snap) {
            applyFitWidth();
            emit tapZoomed(int(ZoomMode::FitWidth), currentZoomPercent());
        } else {
            applyTapZoomSnapshot(*snap);
            emit tapZoomed(int(snap->mode), snap->percent);
        }
    } else {
        m_tapZoomSnapshot = captureTapZoomSnapshot();
        noteUserZoomTo(ZoomMode::Custom);
        zoomToPercentAt(100.f, focusX, focusY);
        emit tapZoomed(int(ZoomMode::Custom), 100.f);
    }
}

void PdfStripItem::zoomToPercentAt(float percent, float focusX, float focusY) {
    m_zoomMode = ZoomMode::Custom;
    m_customPercent = coerce(percent, 25.f, 400.f);
    applyScale(scaleForPercent(m_customPercent), focusX, focusY);
}

int PdfStripItem::fitTargetPage() const {
    if (m_pages.isEmpty()) return 0;
    if (m_pendingRestorePage) return coerce(*m_pendingRestorePage, 0, int(m_pages.size()) - 1);
    const int vis = currentVisiblePage();
    return vis >= 0 ? vis : 0;
}

void PdfStripItem::applyFitPageMode() {
    if (width() <= 0 || height() <= 0 || m_pages.isEmpty()) return;
    fitPageAt(fitTargetPage());
    m_pendingRestorePage.reset();
    m_pendingRestoreOffset.reset();
    m_pendingRestoreX.reset();
}

void PdfStripItem::applyDocumentZoom(bool keepScroll) {
    if (width() <= 0 || height() <= 0 || m_pages.isEmpty()) return;
    if (m_zoomMode == ZoomMode::FitPage) {
        applyFitPageMode();
        return;
    }
    applyZoomInternal(m_zoomMode, m_customPercent, keepScroll,
                      m_zoomMode != ZoomMode::Custom || m_centerHorizontally);
    applyPendingPageRestore();
    if (m_centerHorizontally) m_scrollXPdf = centerScrollX();
    clampScroll();
}

void PdfStripItem::applyZoomInternal(ZoomMode mode, float percent, bool keepScroll, bool centerX) {
    if (width() <= 0 || height() <= 0 || m_pages.isEmpty()) return;
    const float w = float(width()), h = float(height());
    const int vis = currentVisiblePage();
    const float focusY = screenCenterPdfY();
    const float focusX = screenCenterPdfX();
    const float offsetInPage = (vis >= 0 && vis < m_pages.size() && vis < m_pageTops.size()) ? focusY - m_pageTops[vis] : 0.f;
    const PageInfo &fitPage = (vis >= 0 && vis < m_pages.size()) ? m_pages[vis] : m_pages[0];
    float next = 1.f;
    switch (mode) {
    case ZoomMode::FitWidth: next = fitWidthUsable() / fitWidthPageWidth(); break;
    case ZoomMode::FitPage: next = std::min(w / fitPage.width, h / std::max(fitPage.height, 1.f)); break;
    case ZoomMode::Custom: next = (dpi() / 72.f) * (percent / 100.f); break;
    }
    m_scale = coerce(next, minScale(), std::max(minScale(), maxScale()));
    rebuildTops();
    if (keepScroll) {
        m_scrollXPdf = focusX - (w / 2.f) / m_scale;
        m_scrollYPdf = (vis >= 0 && vis < m_pageTops.size()) ? m_pageTops[vis] + offsetInPage - (h / 2.f) / m_scale
                                                             : focusY - (h / 2.f) / m_scale;
    } else {
        m_scrollYPdf = 0.f;
    }
    if (centerX || m_centerHorizontally) m_scrollXPdf = centerScrollX();
    clampScroll();
    m_inFlight.clear();
}

void PdfStripItem::geometryChange(const QRectF &newGeometry, const QRectF &oldGeometry) {
    QQuickPaintedItem::geometryChange(newGeometry, oldGeometry);
    const float w = float(newGeometry.width()), h = float(newGeometry.height());
    const float oldw = float(oldGeometry.width()), oldh = float(oldGeometry.height());
    if (w <= 0.f || h <= 0.f || m_pages.isEmpty()) return;
    if (w == oldw && h == oldh) return;
    if (m_presentation) {
        applyFitPage();
        schedulePostDraw();
        return;
    }
    if (m_zoomMode == ZoomMode::FitPage) {
        applyFitPageMode();
        update();
        schedulePostDraw();
        return;
    }
    const bool restoring = hasPendingPageRestore();
    if (oldw > 0.f && w != oldw && m_scale > 0.f && !restoring) {
        if (m_zoomMode == ZoomMode::FitWidth) {
            applyZoomInternal(ZoomMode::FitWidth, m_customPercent, true, true);
        } else {
            const float oldUsable = std::max(oldw - 2.f * m_horizontalPagePaddingPx, 1.f);
            const float newUsable = std::max(w - 2.f * m_horizontalPagePaddingPx, 1.f);
            m_scale = coerce(m_scale * newUsable / oldUsable, minScale(), std::max(minScale(), maxScale()));
            m_customPercent = currentZoomPercent();
            m_userZoomed = true;
        }
        if (m_centerHorizontally) m_scrollXPdf = centerScrollX();
        clampScroll();
        m_inFlight.clear();
        update();
        schedulePostDraw();
        return;
    }
    if (oldw <= 0.f || restoring) {
        applyDocumentZoom(oldw > 0.f || restoring);
        update();
    } else if (h != oldh) {
        clampScroll();
        update();
    }
    schedulePostDraw();
}

void PdfStripItem::scrollToChar(int page, int index) {
    if (page < 0 || page >= m_pages.size()) return;
    const auto it = m_textCache.constFind(page);
    float pageY;
    if (it != m_textCache.constEnd() && index >= 0 && index < it->size()) {
        pageY = m_pageTops[page] + ((*it)[index].uly - m_pages[page].y0) - (float(height()) / m_scale) * 0.2f;
    } else {
        pageY = m_pageTops[page];
    }
    m_scrollYPdf = pageY;
    clampScroll();
    stopFling();
    update();
}

void PdfStripItem::setSearchHits(const QVector<SearchHit> &hits, int index) {
    m_searchHits = hits;
    m_searchIndex = std::max(index, 0);
    QHash<int, QVector<int>> byPage;
    QSet<int> needed;
    for (int i = 0; i < hits.size(); ++i) {
        const SearchHit &hit = hits[i];
        needed.insert(hit.startPage);
        needed.insert(hit.endPage);
        for (int p = std::min(hit.startPage, hit.endPage); p <= std::max(hit.startPage, hit.endPage); ++p)
            byPage[p].append(i);
    }
    m_searchByPage = byPage;
    for (int p : std::as_const(needed)) ensureText(p, [this](const QVector<CharBox> &) { update(); });
    if (!hits.isEmpty()) {
        const SearchHit hit = hits[coerce(m_searchIndex, 0, int(hits.size()) - 1)];
        ensureText(hit.startPage, [this, hit](const QVector<CharBox> &) { scrollToChar(hit.startPage, hit.startIndex); });
    }
    update();
}

float PdfStripItem::pageToViewX(int page, float x) const { return ((x - m_pages[page].x0) - m_scrollXPdf) * m_scale; }
float PdfStripItem::pageToViewY(int page, float y) const {
    return ((m_pageTops[page] + (y - m_pages[page].y0)) - m_scrollYPdf) * m_scale;
}
float PdfStripItem::viewToPageX(int page, float x) const { return x / m_scale + m_scrollXPdf + m_pages[page].x0; }
float PdfStripItem::viewToPageY(int page, float y) const {
    return y / m_scale + m_scrollYPdf - m_pageTops[page] + m_pages[page].y0;
}

std::optional<std::pair<int, int>> PdfStripItem::selectedIndices(int page, const SelectionRange &sel) const {
    const auto it = m_textCache.constFind(page);
    if (it == m_textCache.constEnd() || it->isEmpty()) return std::nullopt;
    const int startPage = std::min(sel.startPage, sel.endPage);
    const int endPage = std::max(sel.startPage, sel.endPage);
    if (page < startPage || page > endPage) return std::nullopt;
    const bool forward = sel.startPage < sel.endPage || (sel.startPage == sel.endPage && sel.startIndex <= sel.endIndex);
    const int sp = forward ? sel.startPage : sel.endPage;
    const int si = forward ? sel.startIndex : sel.endIndex;
    const int ep = forward ? sel.endPage : sel.startPage;
    const int ei = forward ? sel.endIndex : sel.startIndex;
    const int from = page == sp ? si : 0;
    const int to = page == ep ? ei : int(it->size()) - 1;
    if (from > to) return std::nullopt;
    return std::make_pair(from, to);
}

void PdfStripItem::drawChars(QPainter *p, int page, const QVector<CharBox> &chars, std::pair<int, int> range,
                             const QColor &color) {
    p->setPen(Qt::NoPen);
    p->setBrush(color);
    for (int idx = range.first; idx <= range.second; ++idx) {
        if (idx < 0 || idx >= chars.size()) continue;
        const CharBox &c = chars[idx];
        const QPointF poly[4] = {
            {pageToViewX(page, c.ulx), pageToViewY(page, c.uly)},
            {pageToViewX(page, c.urx), pageToViewY(page, c.ury)},
            {pageToViewX(page, c.lrx), pageToViewY(page, c.lry)},
            {pageToViewX(page, c.llx), pageToViewY(page, c.lly)},
        };
        p->drawPolygon(poly, 4);
    }
}

void PdfStripItem::drawSearchOnPage(QPainter *p, int page) {
    const auto hIt = m_searchByPage.constFind(page);
    if (hIt == m_searchByPage.constEnd()) return;
    const auto cIt = m_textCache.constFind(page);
    if (cIt == m_textCache.constEnd()) return;
    for (int i : *hIt) {
        const SearchHit &hit = m_searchHits[i];
        const auto range = selectedIndices(page, {hit.startPage, hit.startIndex, hit.endPage, hit.endIndex});
        if (!range) continue;
        drawChars(p, page, *cIt, *range, i == m_searchIndex ? m_searchCurrentColor : m_searchColor);
    }
}

void PdfStripItem::drawSelectionOnPage(QPainter *p, int page) {
    if (!m_selection) return;
    const auto cIt = m_textCache.constFind(page);
    if (cIt == m_textCache.constEnd()) return;
    const auto range = selectedIndices(page, *m_selection);
    if (!range) return;
    drawChars(p, page, *cIt, *range, m_highlightPaintColor);
}

void PdfStripItem::paint(QPainter *painter) {
    painter->setRenderHint(QPainter::Antialiasing, true);
    painter->setRenderHint(QPainter::SmoothPixmapTransform, true);
    painter->fillRect(QRectF(0, 0, width(), height()), argb(m_bgColor));
    if (m_pages.isEmpty() || width() <= 0) {
        schedulePostDraw();
        return;
    }
    const float topPdf = m_scrollYPdf;
    const float botPdf = m_scrollYPdf + float(height()) / m_scale;
    const int first = firstVisiblePage(topPdf);
    const int last = lastVisiblePage(botPdf);
    const int active = currentVisiblePage();
    const float left = -m_scrollXPdf * m_scale;
    for (int i = first; i <= last; ++i) {
        const PageInfo &info = m_pages[i];
        const float top = (m_pageTops[i] - m_scrollYPdf) * m_scale;
        const QRectF dest(left, top, info.width * m_scale, info.height * m_scale);
        auto it = m_bitmaps.find(i);
        if (it != m_bitmaps.end() && it->key == paintKey() && !it->image.isNull()) {
            it->stamp = ++m_stamp;
            painter->drawImage(dest, it->image);
        }
        const bool showFocusBorder = i == active && (m_pageSelectMode || m_pageBorderEnabled);
        if (showFocusBorder) {
            painter->setBrush(Qt::NoBrush);
            painter->setPen(QPen(argb(m_pageBorderColor), 2.0));
            painter->drawRect(dest);
        }
        if (m_pageSelectMode && m_selectedPages.contains(i)) {
            painter->setPen(Qt::NoPen);
            painter->setBrush(m_highlightPaintColor);
            painter->drawRect(dest);
        }
        drawSearchOnPage(painter, i);
        drawSelectionOnPage(painter, i);
    }
    schedulePostDraw();
}

QString PdfStripItem::selectedText() const {
    if (!m_selection) return QString();
    const SelectionRange &sel = *m_selection;
    const bool forward = sel.startPage < sel.endPage || (sel.startPage == sel.endPage && sel.startIndex <= sel.endIndex);
    const int sp = forward ? sel.startPage : sel.endPage;
    const int si = forward ? sel.startIndex : sel.endIndex;
    const int ep = forward ? sel.endPage : sel.startPage;
    const int ei = forward ? sel.endIndex : sel.startIndex;
    QString out;
    for (int p = sp; p <= ep; ++p) {
        const auto it = m_textCache.constFind(p);
        if (it == m_textCache.constEnd()) continue;
        if (!out.isEmpty()) out += QLatin1Char('\n');
        const int from = p == sp ? si : 0;
        const int to = p == ep ? ei : int(it->size()) - 1;
        if (from >= 0 && from < it->size() && to >= 0 && to < it->size() && from <= to)
            for (int i = from; i <= to; ++i) out += (*it)[i].c;
    }
    return out;
}

void PdfStripItem::emitSelectionText() {
    if (!m_selection) {
        emit selectionTextChanged(QString());
        return;
    }
    const QString text = selectedText();
    emit selectionTextChanged(text.trimmed().isEmpty() ? QString() : text);
}

void PdfStripItem::clearSelection() {
    m_selection.reset();
    m_selecting = false;
    emit selectionTextChanged(QString());
    update();
}

void PdfStripItem::selectAll() {
    if (m_pages.isEmpty() || !m_allowTextSelection) return;
    const int n = m_pages.size();
    const auto remaining = std::make_shared<int>(n);
    for (int p = 0; p < n; ++p) {
        ensureText(p, [this, remaining, n](const QVector<CharBox> &) {
            if (--(*remaining) != 0) return;
            int sp = -1, si = 0, ep = -1, ei = 0;
            for (int i = 0; i < n; ++i) {
                const auto it = m_textCache.constFind(i);
                if (it == m_textCache.constEnd() || it->isEmpty()) continue;
                if (sp < 0) {
                    sp = i;
                    si = 0;
                }
                ep = i;
                ei = it->size() - 1;
            }
            if (sp < 0) {
                clearSelection();
                return;
            }
            m_selecting = false;
            m_selection = SelectionRange{sp, si, ep, ei};
            emitSelectionText();
            update();
        });
    }
}

void PdfStripItem::startSelectionAt(float x, float y, bool word) {
    const auto page = pageAt(y);
    if (!page) return;
    const int pg = *page;
    ensureText(pg, [this, pg, x, y, word](const QVector<CharBox> &chars) {
        if (chars.isEmpty()) return;
        const float pdfX = viewToPageX(pg, x), pdfY = viewToPageY(pg, y);
        const int idx = nearestChar(chars, pdfX, pdfY);
        if (idx < 0) return;
        int start = idx, end = idx;
        if (word) {
            while (start > 0 && !chars[start - 1].c.isSpace()) --start;
            while (end < chars.size() - 1 && !chars[end + 1].c.isSpace()) ++end;
        }
        m_selection = SelectionRange{pg, start, pg, end};
        emitSelectionText();
        update();
    });
}

int PdfStripItem::nearestChar(const QVector<CharBox> &chars, float x, float y) const {
    for (int i = 0; i < chars.size(); ++i)
        if (chars[i].contains(x, y)) return i;
    int best = -1;
    float bestD = std::numeric_limits<float>::max();
    for (int i = 0; i < chars.size(); ++i) {
        const CharBox &c = chars[i];
        const float cx = (c.ulx + c.urx + c.llx + c.lrx) * 0.25f;
        const float cy = (c.uly + c.ury + c.lly + c.lry) * 0.25f;
        const float d = (cx - x) * (cx - x) + (cy - y) * (cy - y);
        if (d < bestD) {
            bestD = d;
            best = i;
        }
    }
    if (best >= 0 && bestD > 36.f * 36.f) return -1;
    return best;
}

void PdfStripItem::extendSelection(float x, float y) {
    const auto page = pageAt(y);
    if (!page) return;
    const int pg = *page;
    ensureText(pg, [this, pg, x, y](const QVector<CharBox> &chars) {
        if (!m_selecting || chars.isEmpty()) return;
        const float pdfX = viewToPageX(pg, x), pdfY = viewToPageY(pg, y);
        const int idx = nearestChar(chars, pdfX, pdfY);
        if (idx < 0) return;
        SelectionRange cur = m_selection.value_or(SelectionRange{pg, idx, pg, idx});
        cur.endPage = pg;
        cur.endIndex = idx;
        m_selection = cur;
        emitSelectionText();
        update();
    });
}

void PdfStripItem::cancelPendingZoom() {
    m_pendingZoom = false;
    m_pendingZoomTimer.stop();
}

QPointF PdfStripItem::twoFingerMid(const MotionEvent &e) const {
    return (e.pointers[0].pos + e.pointers[1].pos) / 2.0;
}

float PdfStripItem::fingerSpan(const MotionEvent &e) const {
    if (e.count() < 2) return 1.f;
    const QPointF d = e.pointers[1].pos - e.pointers[0].pos;
    return std::max(float(std::hypot(d.x(), d.y())), 1.f);
}

void PdfStripItem::gestureCancelTimers() {
    m_confirmTimer.stop();
    m_longPressTimer.stop();
    m_isDoubleTapping = false;
    m_alwaysInTapRegion = false;
    m_alwaysInBiggerTapRegion = false;
    m_deferConfirmTap = false;
    m_inLongPress = false;
}

bool PdfStripItem::onDown(const MotionEvent &) {
    stopFling();
    return true;
}

void PdfStripItem::gestureTouch(const MotionEvent &e) {
    const bool pointerUp = e.action == Action::PointerUp;
    double sx = 0, sy = 0;
    int n = 0;
    for (int i = 0; i < e.count(); ++i) {
        if (pointerUp && i == e.actionIndex) continue;
        sx += e.pointers[i].pos.x();
        sy += e.pointers[i].pos.y();
        ++n;
    }
    const QPointF focus = n ? QPointF(sx / n, sy / n) : QPointF();
    switch (e.action) {
    case Action::PointerDown:
        m_downFocus = m_lastFocus = focus;
        gestureCancelTimers();
        break;
    case Action::PointerUp:
        m_downFocus = m_lastFocus = focus;
        m_velocitySamples.clear();
        break;
    case Action::Down: {
        const bool hadTap = m_confirmTimer.isActive();
        if (hadTap) m_confirmTimer.stop();
        bool dbl = false;
        if (!e.fromMouse && m_hasCurDown && m_hasPrevUp && hadTap && m_alwaysInBiggerTapRegion) {
            const qint64 dt = e.time - m_prevUpTime;
            const QPointF d = e.pos() - m_curDown.pos();
            if (dt <= kDoubleTapTimeoutMs && dt >= kDoubleTapMinMs
                && d.x() * d.x() + d.y() * d.y() < double(kDoubleTapSlop) * kDoubleTapSlop)
                dbl = true;
        }
        if (dbl) {
            m_isDoubleTapping = true;
            onDoubleTap(e);
        } else {
            m_confirmTimer.start(kDoubleTapTimeoutMs);
        }
        m_downFocus = m_lastFocus = focus;
        m_curDown = e;
        m_hasCurDown = true;
        m_alwaysInTapRegion = true;
        m_alwaysInBiggerTapRegion = true;
        m_stillDown = true;
        m_inLongPress = false;
        m_deferConfirmTap = false;
        m_longPressTimer.start(kLongPressMs);
        m_velocitySamples.clear();
        m_velocitySamples.append({e.time, e.pos()});
        onDown(e);
        break;
    }
    case Action::Move: {
        m_velocitySamples.append({e.time, e.pos()});
        if (m_velocitySamples.size() > 20) m_velocitySamples.removeFirst();
        const float scrollX = float(m_lastFocus.x() - focus.x());
        const float scrollY = float(m_lastFocus.y() - focus.y());
        if (m_inLongPress) {
            if (m_selecting) onScroll(m_curDown, e, scrollX, scrollY);
            m_lastFocus = focus;
            break;
        }
        if (m_isDoubleTapping) {
        } else if (m_alwaysInTapRegion) {
            const double dx = focus.x() - m_downFocus.x(), dy = focus.y() - m_downFocus.y();
            const double dist = dx * dx + dy * dy;
            if (dist > double(kTouchSlop) * kTouchSlop) {
                if (e.fromMouse && !mouseDragPans(e) && m_allowTextSelection && !m_pageSelectMode) {
                    m_selecting = true;
                    m_longPressTimer.stop();
                    startSelectionAt(float(m_curDown.pos().x()), float(m_curDown.pos().y()), false);
                }
                if (mouseDragPans(e)) onScroll(m_curDown, e, scrollX, scrollY);
                m_lastFocus = focus;
                m_alwaysInTapRegion = false;
                m_confirmTimer.stop();
                m_longPressTimer.stop();
            }
            if (dist > double(kBiggerTouchSlop) * kBiggerTouchSlop) m_alwaysInBiggerTapRegion = false;
        } else if (std::abs(scrollX) >= 1.f || std::abs(scrollY) >= 1.f) {
            if (mouseDragPans(e)) onScroll(m_curDown, e, scrollX, scrollY);
            m_lastFocus = focus;
        }
        break;
    }
    case Action::Up: {
        m_stillDown = false;
        m_velocitySamples.append({e.time, e.pos()});
        if (m_isDoubleTapping) {
        } else if (m_inLongPress) {
            m_confirmTimer.stop();
            m_inLongPress = false;
        } else if (m_alwaysInTapRegion) {
            if (e.fromMouse && (e.modifiers & Qt::ControlModifier)) {
                m_confirmTimer.stop();
                onDoubleTap(e);
            } else if (m_deferConfirmTap) {
                onSingleTapConfirmed();
            }
        } else {
            float vx = 0, vy = 0;
            const qint64 horizon = e.time - 100;
            int firstIdx = m_velocitySamples.size() - 1;
            while (firstIdx > 0 && m_velocitySamples[firstIdx - 1].t >= horizon) --firstIdx;
            const Sample &a = m_velocitySamples[firstIdx];
            const Sample &b = m_velocitySamples.last();
            const double dt = double(b.t - a.t) / 1000.0;
            if (dt > 0.0005) {
                vx = float((b.p.x() - a.p.x()) / dt);
                vy = float((b.p.y() - a.p.y()) / dt);
            }
            vx = coerce(vx, -kMaxFlingVelocity, kMaxFlingVelocity);
            vy = coerce(vy, -kMaxFlingVelocity, kMaxFlingVelocity);
            if (mouseDragPans(e)
                && (std::abs(vy) > kMinFlingVelocity || std::abs(vx) > kMinFlingVelocity))
                onFling(vx, vy);
        }
        m_prevUpTime = e.time;
        m_prevUpPos = e.pos();
        m_hasPrevUp = true;
        m_longPressTimer.stop();
        m_isDoubleTapping = false;
        m_deferConfirmTap = false;
        m_velocitySamples.clear();
        break;
    }
    case Action::Cancel:
        gestureCancelTimers();
        m_stillDown = false;
        m_velocitySamples.clear();
        break;
    }
}

bool PdfStripItem::mouseDragPans(const MotionEvent &e) const {
    if (!e.fromMouse) return true;
    if (m_pageSelectMode) return true;
    if (m_selecting) return true;
    return e.modifiers.testFlag(Qt::ControlModifier);
}

void PdfStripItem::panByPixels(qreal dx, qreal dy) {
    if (m_pages.isEmpty()) return;
    if (m_presentation) {
        if (std::abs(dx) > std::abs(dy)) {
            if (std::abs(dx) > 1.0) stepPage(dx > 0 ? 1 : -1);
        } else if (std::abs(dy) > 1.0) {
            stepPage(dy > 0 ? 1 : -1);
        }
        return;
    }
    stopFling();
    if (allowSidePan(1)) m_scrollXPdf += float(dx) / m_scale;
    m_scrollYPdf += float(dy) / m_scale;
    clampScroll();
    update();
}

bool PdfStripItem::onScroll(const MotionEvent &, const MotionEvent &cur, float dx, float dy) {
    if (m_pinching || m_twoFingerTracking || m_bottomMenuTracking) return false;
    cancelPendingZoom();
    if (m_allowTextSelection && (m_selecting)) {
        extendSelection(float(cur.pos().x()), float(cur.pos().y()));
        return true;
    }
    if (m_presentation) {
        m_presPan += dy;
        if (m_presPan <= -48.f) {
            m_presPan = 0.f;
            stepPage(-1);
        } else if (m_presPan >= 48.f) {
            m_presPan = 0.f;
            stepPage(1);
        }
        return true;
    }
    if (allowSidePan(cur.count())) m_scrollXPdf += dx / m_scale;
    m_scrollYPdf += dy / m_scale;
    clampScroll();
    update();
    return true;
}

bool PdfStripItem::onFling(float vx, float vy) {
    if (m_selecting || m_pinching || m_twoFingerTracking || m_bottomMenuTracking
        || m_pages.isEmpty())
        return false;
    if (m_presentation) {
        if (std::abs(vy) > std::abs(vx) && std::abs(vy) > 200.f) stepPage(vy > 0 ? 1 : -1);
        return true;
    }
    const float w = float(width()), h = float(height());
    const int minXPx = int(-w / 2.f);
    const int maxXPx = int(m_maxWidthPdf * m_scale - w / 2.f);
    const int minYPx = int(-h / 2.f);
    const int maxYPx = int(m_totalHeightPdf * m_scale - h / 2.f);
    m_flingX = {};
    m_flingY = {};
    const float ppi = dpi();
    const float physicalCoeff = 386.0878f * ppi * 0.84f;
    const float friction = 0.015f;
    const float decel = std::log(0.78f) / std::log(0.9f);
    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    auto setup = [&](AxisFling &a, float startPx, float v, int lo, int hi) {
        a.start = startPx;
        a.cur = startPx;
        a.minV = float(std::min(lo, hi));
        a.maxV = float(std::max(lo, hi));
        if (std::abs(v) < 1.f) return;
        const float l = std::log(0.35f * std::abs(v) / (friction * physicalCoeff));
        float dist = friction * physicalCoeff * std::exp(decel / (decel - 1.f) * l);
        a.duration = qint64(1000.f * std::exp(l / (decel - 1.f)));
        a.dist = (v < 0 ? -dist : dist);
        a.t0 = now;
        a.active = a.duration > 0;
    };
    setup(m_flingX, m_scrollXPdf * m_scale, allowSidePan(m_twoFingerGesture ? 2 : 1) ? -vx : 0.f, minXPx, maxXPx);
    setup(m_flingY, m_scrollYPdf * m_scale, -vy, minYPx, maxYPx);
    if (m_flingX.active || m_flingY.active) m_flingTimer.start();
    return true;
}

void PdfStripItem::stopFling() {
    m_flingX.active = false;
    m_flingY.active = false;
    m_flingTimer.stop();
}

void PdfStripItem::flingStep() {
    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    bool any = false;
    for (AxisFling *a : {&m_flingX, &m_flingY}) {
        if (!a->active) continue;
        const float t = std::min(1.f, float(now - a->t0) / float(std::max<qint64>(a->duration, 1)));
        const float frac = 1.f - std::pow(1.f - t, 2.857f);
        float cur = a->start + a->dist * frac;
        if (cur <= a->minV) {
            cur = a->minV;
            a->active = false;
        } else if (cur >= a->maxV) {
            cur = a->maxV;
            a->active = false;
        }
        a->cur = cur;
        if (t >= 1.f) a->active = false;
        any = any || a->active;
    }
    if (m_scale > 0.f) {
        m_scrollXPdf = m_flingX.cur / m_scale;
        m_scrollYPdf = m_flingY.cur / m_scale;
    }
    clampScroll();
    update();
    if (!any) m_flingTimer.stop();
}

bool PdfStripItem::onSingleTapConfirmed() { return handleSingleTap(); }

bool PdfStripItem::onDoubleTap(const MotionEvent &e) {
    if (m_pageSelectMode) return true;
    if (m_selecting || m_pinching || m_twoFingerTracking || m_pages.isEmpty())
        return false;
    if (!m_doubleTapMenu) {
        toggleTapZoom(float(e.pos().x()), float(e.pos().y()));
        return true;
    }
    emit doubleTapped();
    cancelPendingZoom();
    if (!e.fromMouse) {
        m_pendingZoom = true;
        m_pendingZoomPos = e.pos();
        m_pendingZoomTimer.start(kDoubleTapTimeoutMs);
    }
    return true;
}

void PdfStripItem::onLongPress() {
    if (m_pageSelectMode || m_curDown.fromMouse) return;
    if (!m_allowTextSelection || m_pages.isEmpty()) return;
    m_selecting = true;
    startWordAt(float(m_curDown.pos().x()), float(m_curDown.pos().y()));
}

bool PdfStripItem::openMenuIfSingleTap() { return false; }

bool PdfStripItem::handleSingleTap() {
    if (m_pageSelectMode) {
        const auto page = pageAt(float(m_curDown.pos().x()), float(m_curDown.pos().y()));
        if (!page) return true;
        emit pageTapped(*page);
        return true;
    }
    if (m_selection) {
        clearSelection();
        return true;
    }
    const float x = float(m_curDown.pos().x()), y = float(m_curDown.pos().y());
    if (m_allowFollowLinks) {
        const auto page = pageAt(y);
        if (!page) return openMenuIfSingleTap();
        const float pdfX = viewToPageX(*page, x), pdfY = viewToPageY(*page, y);
        const auto lit = m_linkCache.constFind(*page);
        if (lit != m_linkCache.constEnd()) {
            for (const PageLink &link : *lit) {
                const PdfRect &b = link.bounds;
                if (pdfX >= b.left - 8.f && pdfX <= b.right + 8.f && pdfY >= b.top - 8.f && pdfY <= b.bottom + 8.f) {
                    emit linkTapped(link.uri, link.isExternal);
                    return true;
                }
            }
        }
    }
    return openMenuIfSingleTap();
}

bool PdfStripItem::handleBottomMenuSwipe(const MotionEvent &e) {
    if (m_pageSelectMode || !m_swipeFromBottomMenu) {
        m_bottomMenuTracking = false;
        return false;
    }
    if (e.count() >= 2) {
        m_bottomMenuTracking = false;
        return false;
    }
    switch (e.action) {
    case Action::Down: {
        const bool fromBottom = height() > 0 && e.pos().y() >= height() * 0.9;
        m_bottomMenuTracking = fromBottom && !m_selecting && !m_pinching
                               && !m_twoFingerTracking;
        if (m_bottomMenuTracking) m_bottomMenuStart = e.pos();
        return false;
    }
    case Action::Move: {
        if (!m_bottomMenuTracking) return false;
        const double dx = e.pos().x() - m_bottomMenuStart.x();
        const double dy = m_bottomMenuStart.y() - e.pos().y();
        if (dx * dx + dy * dy < double(kTouchSlop) * kTouchSlop) return true;
        if (dy > std::abs(dx)) {
            m_bottomMenuTracking = false;
            m_swallowUntilUp = true;
            emit doubleTapped();
            return true;
        }
        m_bottomMenuTracking = false;
        return false;
    }
    case Action::Up:
    case Action::Cancel:
        m_bottomMenuTracking = false;
        return false;
    default: return false;
    }
}

void PdfStripItem::endTwoFingerZoom() {
    if (!m_twoFingerTracking && !m_pinching) return;
    const bool scaled = std::abs(m_scale - m_twoFingerScaleStart) > 0.001f;
    m_twoFingerTracking = false;
    m_twoFingerSpan = 0.f;
    m_twoFingerLastMid = QPointF();
    m_pinching = false;
    if (scaled) {
        m_userZoomed = true;
        finishPinchZoom();
    }
}

bool PdfStripItem::handleTwoFingerZoom(const MotionEvent &e) {
    if (m_lockZoom || m_pages.isEmpty()) return false;
    const int count = e.count();
    if (e.action == Action::PointerDown && count >= 2) {
        m_twoFingerTracking = true;
        m_pinching = true;
        m_twoFingerSpan = fingerSpan(e);
        m_twoFingerLastMid = twoFingerMid(e);
        m_twoFingerScaleStart = m_scale;
        m_selecting = false;
        cancelPendingZoom();
        stopFling();
        return true;
    }
    if (m_twoFingerTracking && count >= 2 && e.action == Action::Move) {
        const float span = fingerSpan(e);
        const QPointF mid = twoFingerMid(e);
        if (m_twoFingerSpan > 0.f)
            applyScale(m_scale * (span / m_twoFingerSpan), float(mid.x()), float(mid.y()), float(m_twoFingerLastMid.x()),
                       float(m_twoFingerLastMid.y()));
        m_twoFingerSpan = span;
        m_twoFingerLastMid = mid;
        return true;
    }
    if (m_twoFingerTracking
        && (e.action == Action::PointerUp || e.action == Action::Up || e.action == Action::Cancel)) {
        const int left = (e.action == Action::PointerUp || e.action == Action::Up) ? std::max(count - 1, 0) : 0;
        if (left < 2) {
            endTwoFingerZoom();
            return true;
        }
        m_twoFingerSpan = fingerSpan(e);
        m_twoFingerLastMid = twoFingerMid(e);
        return true;
    }
    return false;
}

void PdfStripItem::handleMotion(const MotionEvent &e) {
    if (m_swallowUntilUp) {
        if (e.action == Action::Up || e.action == Action::Cancel) {
            m_swallowUntilUp = false;
            if (e.action == Action::Cancel) cancelPendingZoom();
        }
        return;
    }
    if (e.action == Action::Down && e.count() == 1 && m_pendingZoom && m_doubleTapMenu) {
        const double dx = e.pos().x() - m_pendingZoomPos.x();
        const double dy = e.pos().y() - m_pendingZoomPos.y();
        const bool inSlop = dx * dx + dy * dy <= double(kDoubleTapSlop) * kDoubleTapSlop;
        if (inSlop && !m_lockZoom && !m_selecting && !m_pinching) {
            cancelPendingZoom();
            toggleTapZoom(float(e.pos().x()), float(e.pos().y()));
            m_swallowUntilUp = true;
            return;
        }
        if (!inSlop) cancelPendingZoom();
    }
    if (handleBottomMenuSwipe(e)) return;
    if (m_folioPaging) {
        switch (e.action) {
        case Action::Down:
            m_folioDown = e.pos();
            m_folioSwiped = false;
            break;
        case Action::Move:
            if (e.count() == 1 && !m_folioSwiped) {
                const double dx = e.pos().x() - m_folioDown.x();
                const double dy = e.pos().y() - m_folioDown.y();
                if (std::abs(dx) > kTouchSlop && std::abs(dx) > std::abs(dy)) {
                    m_folioSwiped = true;
                    emit folioSwiped(dx < 0 ? 1 : -1);
                    m_swallowUntilUp = true;
                }
            }
            break;
        case Action::Up:
        case Action::Cancel: m_folioSwiped = false; break;
        default: break;
        }
    }
    if (e.count() >= 2) m_twoFingerGesture = true;
    if (handleTwoFingerZoom(e)) {
        if (e.action == Action::Up || e.action == Action::Cancel) {
            m_selecting = false;
            m_twoFingerGesture = false;
        }
        return;
    }
    if (e.action == Action::PointerDown) {
        m_selecting = false;
        cancelPendingZoom();
    }
    gestureTouch(e);
    if (e.action == Action::Up || e.action == Action::Cancel) {
        if (e.action == Action::Cancel) cancelPendingZoom();
        endTwoFingerZoom();
        if (m_selecting && !m_alwaysInTapRegion && selectedText().trimmed().isEmpty()) clearSelection();
        m_selecting = false;
        m_twoFingerGesture = false;
    }
}

void PdfStripItem::mousePressEvent(QMouseEvent *event) {
    if (event->button() == Qt::RightButton) {
        forceActiveFocus();
        event->accept();
        if (m_allowTextSelection && !selectedText().isEmpty())
            emit contextMenuRequested(event->position().x(), event->position().y());
        else
            emit doubleTapped();
        return;
    }
    forceActiveFocus();
    m_mouseDown = true;
    MotionEvent e;
    e.action = Action::Down;
    e.pointers = {{0, event->position()}};
    e.time = QDateTime::currentMSecsSinceEpoch();
    e.modifiers = event->modifiers();
    e.fromMouse = true;
    handleMotion(e);
    if ((event->modifiers() & Qt::ShiftModifier) && m_allowTextSelection && !m_pageSelectMode && !m_pages.isEmpty()
        && !m_swallowUntilUp) {
        m_selecting = true;
        m_longPressTimer.stop();
        startWordAt(float(event->position().x()), float(event->position().y()));
    }
    event->accept();
}

void PdfStripItem::mouseDoubleClickEvent(QMouseEvent *event) {
    if (event->button() != Qt::LeftButton) return;
    m_confirmTimer.stop();
    m_longPressTimer.stop();
    m_deferConfirmTap = false;
    m_isDoubleTapping = false;
    if (m_allowTextSelection && !m_pageSelectMode && !m_pages.isEmpty()
        && !(event->modifiers() & Qt::ControlModifier)) {
        m_selecting = true;
        startSelectionAt(float(event->position().x()), float(event->position().y()), true);
    }
    event->accept();
}

void PdfStripItem::forwardPointer(int phase, qreal x, qreal y) {
    MotionEvent e;
    e.action = phase == 0 ? Action::Down : phase == 1 ? Action::Move : Action::Up;
    e.pointers = {{0, QPointF(x, y)}};
    e.time = QDateTime::currentMSecsSinceEpoch();
    e.modifiers = QGuiApplication::keyboardModifiers();
    e.fromMouse = true;
    if (phase == 0) {
        m_mouseDown = true;
    } else if (!m_mouseDown) {
        return;
    } else if (phase == 2) {
        m_mouseDown = false;
    }
    handleMotion(e);
}

void PdfStripItem::mouseMoveEvent(QMouseEvent *event) {
    if (!m_mouseDown) return;
    MotionEvent e;
    e.action = Action::Move;
    e.pointers = {{0, event->position()}};
    e.time = QDateTime::currentMSecsSinceEpoch();
    e.modifiers = event->modifiers();
    e.fromMouse = true;
    handleMotion(e);
    event->accept();
}

void PdfStripItem::mouseReleaseEvent(QMouseEvent *event) {
    if (!m_mouseDown) return;
    m_mouseDown = false;
    MotionEvent e;
    e.action = Action::Up;
    e.pointers = {{0, event->position()}};
    e.time = QDateTime::currentMSecsSinceEpoch();
    e.modifiers = event->modifiers();
    e.fromMouse = true;
    handleMotion(e);
    event->accept();
}

void PdfStripItem::mouseUngrabEvent() {
    if (!m_mouseDown) return;
    m_mouseDown = false;
    MotionEvent e;
    e.action = Action::Cancel;
    e.pointers = {{0, QPointF()}};
    e.time = QDateTime::currentMSecsSinceEpoch();
    e.fromMouse = true;
    handleMotion(e);
}

void PdfStripItem::touchEvent(QTouchEvent *event) {
    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    if (event->type() == QEvent::TouchCancel) {
        m_touchTracked.clear();
        MotionEvent e;
        e.action = Action::Cancel;
        e.pointers = {{0, QPointF()}};
        e.time = now;
        handleMotion(e);
        event->accept();
        return;
    }
    auto indexOf = [this](int id) {
        for (int i = 0; i < m_touchTracked.size(); ++i)
            if (m_touchTracked[i].id == id) return i;
        return -1;
    };
    bool moved = false;
    for (const QEventPoint &p : event->points()) {
        const int i = indexOf(p.id());
        if (i >= 0 && p.state() != QEventPoint::Released) {
            if (m_touchTracked[i].pos != p.position()) moved = true;
            m_touchTracked[i].pos = p.position();
        }
    }
    for (const QEventPoint &p : event->points()) {
        if (p.state() != QEventPoint::Pressed) continue;
        event->setExclusiveGrabber(p, this);
        forceActiveFocus();
        m_touchTracked.append({p.id(), p.position()});
        MotionEvent e;
        e.action = m_touchTracked.size() == 1 ? Action::Down : Action::PointerDown;
        e.actionIndex = m_touchTracked.size() - 1;
        e.pointers = m_touchTracked;
        e.time = now;
        handleMotion(e);
    }
    if (moved && !m_touchTracked.isEmpty()) {
        MotionEvent e;
        e.action = Action::Move;
        e.pointers = m_touchTracked;
        e.time = now;
        handleMotion(e);
    }
    for (const QEventPoint &p : event->points()) {
        if (p.state() != QEventPoint::Released) continue;
        const int i = indexOf(p.id());
        if (i < 0) continue;
        m_touchTracked[i].pos = p.position();
        MotionEvent e;
        e.action = m_touchTracked.size() == 1 ? Action::Up : Action::PointerUp;
        e.actionIndex = i;
        e.pointers = m_touchTracked;
        e.time = now;
        handleMotion(e);
        m_touchTracked.removeAt(i);
    }
    event->accept();
}

void PdfStripItem::wheelEvent(QWheelEvent *event) {
    if (m_pages.isEmpty()) return;
    const QPoint angle = event->angleDelta();
    const QPoint pixel = event->pixelDelta();
    const int y = angle.y() != 0 ? angle.y() : pixel.y();
    if (m_presentation) {
        if (std::abs(y) >= 60) {
            stepPage(y > 0 ? -1 : 1);
            m_presWheel = 0.f;
        } else if (y != 0) {
            m_presWheel += float(y);
            while (m_presWheel >= 80.f) {
                m_presWheel -= 80.f;
                stepPage(-1);
            }
            while (m_presWheel <= -80.f) {
                m_presWheel += 80.f;
                stepPage(1);
            }
        }
        event->accept();
        return;
    }
    if (y != 0) {
        QString action;
        if (Keybinds *kb = Keybinds::instance())
            action = kb->actionForWheel(int(event->modifiers()), y > 0);
        else if (event->modifiers() & Qt::ControlModifier)
            action = y > 0 ? QStringLiteral("zoomIn") : QStringLiteral("zoomOut");
        if (action == QLatin1String("zoomIn") || action == QLatin1String("zoomOut")) {
            if (!m_lockZoom) {
                const double mag = std::abs(double(y) / (angle.y() != 0 ? 120.0 : 60.0));
                if (mag != 0.0) {
                    m_userZoomed = true;
                    const QPointF pos = event->position();
                    const double steps = action == QLatin1String("zoomIn") ? mag : -mag;
                    applyScale(m_scale * float(std::pow(1.1, steps)), float(pos.x()), float(pos.y()));
                    m_zoomEndTimer.start(250);
                }
            }
            event->accept();
            return;
        }
    }
    float dx = 0.f, dy = 0.f;
    if (!pixel.isNull()) {
        dx = float(pixel.x());
        dy = float(pixel.y());
    } else {
        dx = float(angle.x()) / 120.f * 80.f;
        dy = float(angle.y()) / 120.f * 80.f;
    }
    if ((event->modifiers() & Qt::ShiftModifier) && dx == 0.f) {
        dx = dy;
        dy = 0.f;
    }
    stopFling();
    if (allowSidePan(1)) m_scrollXPdf -= dx / m_scale;
    m_scrollYPdf -= dy / m_scale;
    clampScroll();
    update();
    event->accept();
}

void PdfStripItem::keyPressEvent(QKeyEvent *event) {
    if (m_pages.isEmpty() || (event->modifiers() & Qt::ControlModifier)) {
        event->ignore();
        return;
    }
    if (m_presentation) {
        switch (event->key()) {
        case Qt::Key_Down:
        case Qt::Key_Right:
            stepPage(1);
            event->accept();
            return;
        case Qt::Key_Up:
        case Qt::Key_Left:
            stepPage(-1);
            event->accept();
            return;
        default:
            event->ignore();
            return;
        }
    }
    event->ignore();
}

} // namespace papercut
