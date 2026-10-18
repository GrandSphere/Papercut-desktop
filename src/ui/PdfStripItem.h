#pragma once

#include "data/ViewerSettings.h"
#include "pdf/PdfModels.h"

#include <QElapsedTimer>
#include <QHash>
#include <QImage>
#include <QList>
#include <QPointF>
#include <QQuickPaintedItem>
#include <QSet>
#include <QTimer>
#include <QVector>
#include <functional>
#include <map>
#include <optional>

namespace papercut {

class PdfStripItem : public QQuickPaintedItem {
    Q_OBJECT
    Q_PROPERTY(bool folioPaging READ folioPaging WRITE setFolioPaging NOTIFY folioPagingChanged)
    Q_PROPERTY(bool pageSelectMode READ pageSelectMode NOTIFY pageSelectModeChanged)

public:
    explicit PdfStripItem(QQuickItem *parent = nullptr);
    ~PdfStripItem() override;

    bool folioPaging() const { return m_folioPaging; }
    void setFolioPaging(bool on);
    bool pageSelectMode() const { return m_pageSelectMode; }

    void setDocument(const QVector<PageInfo> &pages);
    void replacePageSizes(const QVector<PageInfo> &pages);
    bool hasDocument() const { return !m_pages.isEmpty(); }
    void setSettings(const ViewerSettings &settings);
    void setPageImage(int index, int width, const QImage &image, qint64 key);
    void setPageLinks(int index, const QVector<PageLink> &links);
    void setPageText(int page, const QVector<CharBox> &chars);
    void setSearchHits(const QVector<SearchHit> &hits, int index);
    void setPageSelect(bool on, const QSet<int> &selected);
    void setScrubbing(bool on);

    void goToPage(int index, std::optional<float> yOnPage = std::nullopt);
    void goToPageOffset(int index, float offsetFromTop, std::optional<float> scrollX = std::nullopt);
    void pendingGoToPage(int index, float offsetFromTop, float scrollX);
    void applyFitWidth();
    void recenterX();
    void applyFitWidthForPageSelect();
    void applyFitPage();
    void applyCustomZoom(float percent);
    void applyResetZoom(const ViewerSettings &settings);
    void clearReLockOnFitWidth();
    void clearSelection();
    void selectAll();
    void setPresentation(bool on);
    void stepPage(int delta);
    void invalidatePages();

    qint64 paintKey() const { return m_renderKey ^ (qint64(m_layoutGen) << 17); }
    float currentScrollXPdf() const { return m_scrollXPdf; }
    float currentScrollYPdf() const { return m_scrollYPdf; }
    float currentOffsetOnPage() const;
    int pageCount() const { return m_pages.size(); }
    int currentPageIndex() const { return currentVisiblePage(); }
    float currentZoomPercent() const;
    ZoomMode zoomMode() const { return m_zoomMode; }
    bool lockSideScroll() const { return m_lockSideScroll; }
    void setLockSideScroll(bool on);
    QString selectedText() const;

    void paint(QPainter *painter) override;

    Q_INVOKABLE void forwardPointer(int phase, qreal x, qreal y);
    Q_INVOKABLE void doClearSelection() { clearSelection(); }
    Q_INVOKABLE void panByPixels(qreal dx, qreal dy);

signals:
    void pageRequested(int index, int width, bool urgent);
    void cancelRendersRequested(int first, int last);
    void textRequested(int page);
    void linksRequested(int page);
    void pinchEnded(float percent);
    void selectionTextChanged(const QString &text);
    void visiblePageChanged(int index, int count);
    void doubleTapped();
    void contextMenuRequested(qreal x, qreal y);
    void tapZoomed(int mode, float percent);
    void lockSideScrollChanged(bool on);
    void linkTapped(const QString &uri, bool isExternal);
    void pageTapped(int index);
    void folioSwiped(int delta);
    void folioPagingChanged();
    void pageSelectModeChanged();

protected:
    void geometryChange(const QRectF &newGeometry, const QRectF &oldGeometry) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void mouseDoubleClickEvent(QMouseEvent *event) override;
    void mouseUngrabEvent() override;
    void touchEvent(QTouchEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;

private:
    void fitPageAt(int page);
    struct SelectionRange {
        int startPage = 0, startIndex = 0, endPage = 0, endIndex = 0;
    };
    struct CachedPage {
        int width = 0;
        qint64 key = 0;
        QImage image;
        qint64 bytes = 0;
        quint64 stamp = 0;
    };
    struct TapZoomSnapshot {
        ZoomMode mode = ZoomMode::FitWidth;
        float percent = 100.f;
        float centerPdfX = 0.f;
        std::optional<bool> lockSideScroll;
    };
    enum class Action { Down, PointerDown, Move, PointerUp, Up, Cancel };
    struct Pointer {
        int id = 0;
        QPointF pos;
    };
    struct MotionEvent {
        Action action = Action::Down;
        QVector<Pointer> pointers;
        qint64 time = 0;
        Qt::KeyboardModifiers modifiers;
        int actionIndex = 0;
        bool fromMouse = false;
        QPointF pos() const { return pointers.isEmpty() ? QPointF() : pointers[0].pos; }
        int count() const { return pointers.size(); }
    };

    float dpi() const;
    float dpr() const;
    float scaleForPercent(float percent) const;
    void applyScale(float nextScale, std::optional<float> focusX = std::nullopt,
                    std::optional<float> focusY = std::nullopt, std::optional<float> fromX = std::nullopt,
                    std::optional<float> fromY = std::nullopt);
    void applyZoomInternal(ZoomMode mode, float percent, bool keepScroll, bool centerX);
    void applyTapZoomSnapshot(const TapZoomSnapshot &snap);
    TapZoomSnapshot captureTapZoomSnapshot() const;
    void finishPinchZoom();
    void toggleTapZoom(float focusX, float focusY);
    void zoomToPercentAt(float percent, float focusX, float focusY);
    void ensureFitWidthLock();
    void applyLockSideScroll(bool on);
    void noteUserZoomTo(ZoomMode next);
    float screenCenterPdfX() const;
    float screenCenterPdfY() const;
    float fitWidthPageWidth() const;
    float fitWidthUsable() const;
    float minScale() const;
    float maxScale() const;
    void rebuildTops();
    void rebuildTopsPreservingScroll();
    void clampScroll();
    float centerScrollX() const;
    bool allowSidePan(int pointerCount) const;
    void scrollToChar(int page, int index);

    int currentVisiblePage() const;
    int firstVisiblePage(float topPdf) const;
    int lastVisiblePage(float botPdf) const;
    std::optional<int> pageAt(float y) const;
    std::optional<int> pageAt(float x, float y) const;
    void reportVisiblePage();
    bool hasPainted(int index) const;
    std::pair<int, int> keepRange(int center) const;
    int targetWidthPx() const;
    void requestPagesAround(int center, int widthHint = 0);
    void requestIfNeeded(int index, int targetWidth, bool urgent);
    void recycleBitmaps();
    void dropCached(int key);
    void trimCache();
    void trimOffscreen(int first, int last);
    bool hasPendingPageRestore() const { return m_pendingRestorePage.has_value(); }
    bool applyPendingPageRestore();
    int fitTargetPage() const;
    void applyFitPageMode();
    void applyDocumentZoom(bool keepScroll);
    Q_INVOKABLE void postDraw();
    void schedulePostDraw();

    void ensureText(int page, std::function<void(const QVector<CharBox> &)> done);
    void startWordAt(float x, float y) { startSelectionAt(x, y, true); }
    void startSelectionAt(float x, float y, bool word);
    void extendSelection(float x, float y);
    int nearestChar(const QVector<CharBox> &chars, float x, float y) const;
    void emitSelectionText();
    std::optional<std::pair<int, int>> selectedIndices(int page, const SelectionRange &sel) const;
    float pageToViewX(int page, float x) const;
    float pageToViewY(int page, float y) const;
    float viewToPageX(int page, float x) const;
    float viewToPageY(int page, float y) const;
    void drawSearchOnPage(QPainter *p, int page);
    void drawSelectionOnPage(QPainter *p, int page);
    void drawChars(QPainter *p, int page, const QVector<CharBox> &chars, std::pair<int, int> range,
                   const QColor &color);
    void tintSearchColors();

    void handleMotion(const MotionEvent &e);
    bool handleBottomMenuSwipe(const MotionEvent &e);
    bool handleTwoFingerZoom(const MotionEvent &e);
    void endTwoFingerZoom();
    void cancelPendingZoom();
    float fingerSpan(const MotionEvent &e) const;
    QPointF twoFingerMid(const MotionEvent &e) const;
    void gestureTouch(const MotionEvent &e);
    void gestureCancelTimers();
    bool onDown(const MotionEvent &e);
    bool mouseDragPans(const MotionEvent &e) const;
    bool onScroll(const MotionEvent &down, const MotionEvent &cur, float dx, float dy);
    bool onFling(float vx, float vy);
    bool onSingleTapConfirmed();
    bool onDoubleTap(const MotionEvent &e);
    void onLongPress();
    bool handleSingleTap();
    bool openMenuIfSingleTap();
    void startFling(float vx, float vy);
    void stopFling();
    void flingStep();
    void emitSelectionTextNull();

    QVector<PageInfo> m_pages;
    QVector<float> m_pageTops;
    float m_totalHeightPdf = 0.f;
    float m_maxWidthPdf = 1.f;
    float m_scale = 1.f;
    float m_scrollXPdf = 0.f;
    float m_scrollYPdf = 0.f;
    ZoomMode m_zoomMode = ZoomMode::FitWidth;
    float m_customPercent = 100.f;
    ZoomMode m_settingsZoomMode = ZoomMode::FitWidth;
    float m_settingsZoomPercent = 100.f;
    int m_bgColor = ViewerSettings::DEFAULT_BG;
    int m_highlightColor = ViewerSettings::DEFAULT_HIGHLIGHT;
    bool m_lockZoom = false;
    bool m_lockSideScroll = false;
    bool m_centerHorizontally = false;
    bool m_pageSelectLockSide = false;
    bool m_savedLockSideScroll = false;
    bool m_autoDisablePan = false;
    bool m_reLockOnFitWidth = false;
    bool m_twoFingerPan = false;
    float m_pagePaddingPx = 0.f;
    float m_horizontalPagePaddingPx = 0.f;
    bool m_pageBorderEnabled = false;
    int m_pageBorderColor = ViewerSettings::DEFAULT_PAGE_BORDER;
    bool m_allowFollowLinks = false;
    bool m_allowTextSelection = true;
    bool m_doubleTapMenu = true;
    bool m_singleTapMenu = false;
    bool m_swipeFromBottomMenu = false;
    bool m_bottomMenuTracking = false;
    QPointF m_bottomMenuStart;
    QPointF m_folioDown;
    bool m_folioSwiped = false;
    bool m_folioPaging = false;
    bool m_pageSelectMode = false;
    QSet<int> m_selectedPages;
    bool m_pendingZoom = false;
    QPointF m_pendingZoomPos;
    bool m_swallowUntilUp = false;
    bool m_scrubbing = false;
    bool m_twoFingerGesture = false;
    int m_lastReportedPage = -1;
    int m_lastReportedCount = -1;
    qint64 m_renderKey = 0;
    int m_layoutGen = 0;
    bool m_presentation = false;
    float m_presWheel = 0.f;
    float m_presPan = 0.f;
    bool m_fitWidthLockHeld = false;
    QVector<SearchHit> m_searchHits;
    int m_searchIndex = 0;
    QHash<int, QVector<int>> m_searchByPage;
    std::optional<float> m_pendingRestoreX;
    std::optional<int> m_pendingRestorePage;
    std::optional<float> m_pendingRestoreOffset;

    QHash<int, CachedPage> m_bitmaps;
    quint64 m_stamp = 0;
    qint64 m_cachedBytes = 0;
    QSet<int> m_inFlight;
    QHash<int, QVector<CharBox>> m_textCache;
    QHash<int, QVector<std::function<void(const QVector<CharBox> &)>>> m_textWaiters;
    QHash<int, QVector<PageLink>> m_linkCache;
    QSet<int> m_linksInFlight;

    QColor m_searchColor, m_searchCurrentColor, m_highlightPaintColor;

    bool m_selecting = false;
    bool m_pinching = false;
    bool m_twoFingerTracking = false;
    float m_twoFingerSpan = 0.f;
    QPointF m_twoFingerLastMid;
    float m_twoFingerScaleStart = 1.f;
    std::optional<SelectionRange> m_selection;
    std::optional<TapZoomSnapshot> m_tapZoomSnapshot;
    bool m_userZoomed = false;
    bool m_postDrawPending = false;

    MotionEvent m_curDown;
    bool m_hasCurDown = false;
    qint64 m_prevUpTime = 0;
    QPointF m_prevUpPos;
    bool m_hasPrevUp = false;
    bool m_alwaysInTapRegion = false;
    bool m_alwaysInBiggerTapRegion = false;
    bool m_inLongPress = false;
    bool m_isDoubleTapping = false;
    bool m_stillDown = false;
    bool m_deferConfirmTap = false;
    bool m_tapPending = false;
    QPointF m_lastFocus, m_downFocus;
    QTimer m_confirmTimer, m_longPressTimer, m_pendingZoomTimer, m_zoomEndTimer, m_flingTimer;
    struct Sample { qint64 t; QPointF p; };
    QVector<Sample> m_velocitySamples;
    bool m_mouseDown = false;
    QVector<Pointer> m_touchTracked;
    int m_wheelZoomPending = 0;

    struct AxisFling {
        bool active = false;
        float start = 0, dist = 0, minV = 0, maxV = 0;
        qint64 t0 = 0, duration = 0;
        float cur = 0;
    };
    AxisFling m_flingX, m_flingY;
    float m_pendingPanCarryX = 0, m_pendingPanCarryY = 0;
};

} // namespace papercut
