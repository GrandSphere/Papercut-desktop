#include "DocumentController.h"

#include "AppServices.h"
#include "core/AppPaths.h"
#include "core/ErrorMessages.h"
#include "pdf/PdfComposer.h"
#include "pdf/TextFinder.h"

#include <QFileInfo>
#include <algorithm>
#include <memory>

namespace papercut {

static QSet<int> toSet(const QVector<int> &v) { return QSet<int>(v.begin(), v.end()); }

static QString encodeRotations(const QVector<int> &r) {
    QString s = QStringLiteral("[");
    for (int i = 0; i < r.size(); ++i) {
        if (i) s += QLatin1Char(',');
        s += QString::number(normRotate(r[i]));
    }
    s += QLatin1Char(']');
    return s;
}

static QVector<int> decodeRotations(const QString &json, int n) {
    QVector<int> out(n, 0);
    if (n <= 0) return out;
    QString t = json.trimmed();
    if (t.startsWith(QLatin1Char('['))) t = t.mid(1);
    if (t.endsWith(QLatin1Char(']'))) t.chop(1);
    int i = 0;
    for (const QString &p : t.split(QLatin1Char(','))) {
        if (i >= n) break;
        const QString bit = p.trimmed();
        if (bit.isEmpty()) continue;
        out[i++] = normRotate(bit.toInt());
    }
    return out;
}

DocumentController::DocumentController(QObject *parent) : QObject(parent), m_app(AppServices::instance()) {
    connect(m_app, &AppServices::settingsChanged, this, [this] {
        pushViewingToStrip();
        relayoutReflowable();
    });
}

DocumentController::~DocumentController() {
    m_searchGen++;
}

void DocumentController::setStrip(PdfStripItem *strip) {
    if (m_strip == strip) return;
    if (m_strip) {
        if (m_doc.valid && m_loadedAny)
            stashRestore(m_strip->currentPageIndex(), m_strip->currentScrollXPdf(), m_strip->currentOffsetOnPage());
        m_strip->disconnect(this);
        m_worker.cancelAllRenders();
        m_loadedAny = false;
        if (!m_selectionText.isEmpty()) {
            m_selectionText.clear();
            emit selectionChanged();
        }
    }
    m_strip = strip;
    if (m_strip) {
        connect(m_strip, &PdfStripItem::pageRequested, this, &DocumentController::onPageRequested);
        connect(m_strip, &PdfStripItem::cancelRendersRequested, this,
                [this](int first, int last) { m_worker.cancelRendersExcept(first, last); });
        connect(m_strip, &PdfStripItem::textRequested, this, [this](int page) {
            m_worker.post([this, page] {
                QVector<CharBox> chars;
                try {
                    chars = m_engine.pageContent(page).chars;
                } catch (...) {
                }
                postToUi([this, page, chars] {
                    QVector<CharBox> out = chars;
                    if (page >= 0 && page < m_origPages.size() && extraRotate(page))
                        out = rotatePageContent(PageContent{chars, {}, {}}, m_origPages[page], extraRotate(page)).chars;
                    if (m_strip) m_strip->setPageText(page, out);
                });
            });
        });
        connect(m_strip, &PdfStripItem::linksRequested, this, [this](int page) {
            m_worker.post([this, page] {
                QVector<PageLink> links;
                try {
                    links = m_engine.pageContent(page).links;
                } catch (...) {
                }
                postToUi([this, page, links] {
                    QVector<PageLink> out = links;
                    if (page >= 0 && page < m_origPages.size() && extraRotate(page)) {
                        PageContent c;
                        c.links = links;
                        out = rotatePageContent(c, m_origPages[page], extraRotate(page)).links;
                    }
                    if (m_strip) m_strip->setPageLinks(page, out);
                });
            });
        });
        connect(m_strip, &PdfStripItem::pinchEnded, this, [this](float p) {
            setPinchZoom(p);
            emit zoomChanged();
        });
        connect(m_strip, &PdfStripItem::tapZoomed, this, [this](int mode, float percent) {
            const ZoomMode m = static_cast<ZoomMode>(mode);
            if (m == ZoomMode::Custom) setCustomZoomInternal(percent);
            else setZoomMode(m);
            emit zoomChanged();
        });
        connect(m_strip, &PdfStripItem::lockSideScrollChanged, this, [this](bool) { emit viewingChanged(); });
        connect(m_strip, &PdfStripItem::selectionTextChanged, this, [this](const QString &text) {
            m_selectionText = text;
            emit selectionChanged();
        });
        connect(m_strip, &PdfStripItem::visiblePageChanged, this, [this](int index, int count) {
            m_currentPage = count == 0 ? 1 : index + 1;
            m_visibleCount = count;
            emit pageChanged();
            emit zoomChanged();
        });
        connect(m_strip, &PdfStripItem::doubleTapped, this, &DocumentController::menuRequested);
        connect(m_strip, &PdfStripItem::linkTapped, this, &DocumentController::followLink);
        connect(m_strip, &PdfStripItem::pageTapped, this, &DocumentController::pageTapped);
        connect(m_strip, &PdfStripItem::folioSwiped, this, [this](int delta) { onFolioSwipe(delta); });
        m_strip->setFolioPaging(inFolio());
        m_strip->setSettings(viewingSettings());
        m_loadedAny = false;
        m_strip->setPageSelect(m_pageSelectMode, toSet(m_selectedPages));
        if (m_doc.valid) syncStrip();
        else m_strip->setDocument({});
        applySearchToStrip();
    }
    emit stripChanged();
    emit viewingChanged();
}

void DocumentController::syncStrip() {
    if (!m_strip) return;
    if (!m_doc.valid) {
        if (!clearViewWhenNoDocument()) {
            m_loadedAny = false;
        } else if (m_loadedAny) {
            m_loadedAny = false;
            m_strip->setDocument({});
        }
        return;
    }
    if (!m_loadedAny || m_doc.uri != m_loadedUri) {
        m_loadedUri = m_doc.uri;
        m_loadedAny = true;
        if (m_restore.pending) {
            m_restore.pending = false;
            m_strip->pendingGoToPage(m_restore.page, m_restore.offset, m_restore.x);
        }
        m_strip->setDocument(m_doc.pages);
    } else {
        m_strip->replacePageSizes(m_doc.pages);
    }
}

void DocumentController::pushViewingToStrip() {
    if (m_strip) m_strip->setSettings(viewingSettings());
    emit viewingChanged();
    emit zoomChanged();
}

void DocumentController::postToUi(std::function<void()> fn) {
    QMetaObject::invokeMethod(this, std::move(fn), Qt::QueuedConnection);
}

void DocumentController::openDocumentOnEngine(const QString &path, int startPage, float fontScale,
                                              const QString &userCss) {
    m_engine.open(path, fontScale, startPage, userCss);
}

DocumentController::OpenData DocumentController::captureOpenData() {
    OpenData data;
    data.pages = m_engine.pages();
    data.mime = m_engine.mime();
    data.reflowable = m_engine.isReflowable();
    return data;
}

void DocumentController::relayoutReflowable() {
    if (!m_doc.valid || !m_doc.reflowable || !m_loadedAny) return;
    const QString family = m_app->uiFontFamily();
    const int weight = m_app->uiFontWeight();
    const float scale = float(m_app->fontScale());
    if (family == m_laidFamily && weight == m_laidWeight && qFuzzyCompare(scale + 1.f, m_laidScale + 1.f)) return;
    m_laidFamily = family;
    m_laidWeight = weight;
    m_laidScale = scale;
    const QString css = m_app->documentUserCss();
    const int page = m_strip ? m_strip->currentPageIndex() : (m_currentPage - 1);
    const int gen = ++m_reflowGen;
    m_worker.post([this, scale, css, page, gen] {
        try {
            m_engine.relayout(scale, css);
        } catch (...) {
            return;
        }
        const QVector<PageInfo> pages = m_engine.pages();
        postToUi([this, pages, page, gen] {
            if (gen != m_reflowGen || !m_doc.valid) return;
            m_doc.pages = pages;
            m_origPages = pages;
            m_pageRotate = QVector<int>(pages.size(), 0);
            reloadStripPages(true);
            if (m_strip && !pages.isEmpty())
                m_strip->goToPage(std::clamp(page, 0, int(pages.size()) - 1));
        });
    });
}

void DocumentController::setDocument(const std::optional<DocState> &doc) {
    const QString prevUri = m_doc.valid ? m_doc.uri : QString();
    if (doc) {
        m_doc = *doc;
        m_doc.valid = true;
        if (m_doc.uri != prevUri) {
            m_origPages = m_doc.pages;
            m_pageRotate.clear();
            if (m_presenting) {
                m_presenting = false;
                if (m_strip) m_strip->setPresentation(false);
                emit presentingChanged();
            }
        }
        m_laidFamily = m_app->uiFontFamily();
        m_laidWeight = m_app->uiFontWeight();
        m_laidScale = float(m_app->fontScale());
    } else {
        m_doc = DocState{};
        m_origPages.clear();
        m_pageRotate.clear();
        m_laidFamily.clear();
        m_laidWeight = 0;
        m_laidScale = 0.f;
        if (m_presenting) {
            m_presenting = false;
            if (m_strip) m_strip->setPresentation(false);
            emit presentingChanged();
        }
    }
    syncStrip();
    emit documentChanged();
    emit viewingChanged();
}

void DocumentController::setPageLoading(bool on) {
    if (m_pageLoading == on) return;
    m_pageLoading = on;
    emit pageLoadingChanged();
}

void DocumentController::setBusy(const QString &text) {
    if (m_busy == text) return;
    m_busy = text;
    emit busyChanged();
}

void DocumentController::markStartupReady() {
    if (m_startupReady) return;
    m_startupReady = true;
    emit startupReadyChanged();
}

void DocumentController::emitError(const QString &code) { emit toast(errorMessage(code)); }

void DocumentController::resetSearchState() {
    m_searchGen++;
    m_hits.clear();
    m_hitIndex = 0;
    if (m_searching) {
        m_searching = false;
        emit searchingChanged();
    }
    applySearchToStrip();
    emit hitsChanged();
}

void DocumentController::cancelAllRenders() { m_worker.cancelAllRenders(); }

void DocumentController::stashRestore(int page, float scrollX, float offsetOnPage, bool enabled) {
    m_restore.page = page;
    m_restore.x = enabled ? scrollX : 0.f;
    m_restore.offset = enabled ? offsetOnPage : 0.f;
    m_restore.pending = enabled;
}

void DocumentController::showPasswordPrompt() {
    setPageLoading(false);
    m_passwordWrong = false;
    m_passwordNeeded = true;
    emit passwordChanged();
}

void DocumentController::clearPasswordPrompt() {
    m_passwordNeeded = false;
    m_passwordWrong = false;
    emit passwordChanged();
}

void DocumentController::setSourceUri(const QString &uri) {
    m_sourceUri = uri;
    refreshBookmarks();
}

QVariantMap DocumentController::viewing() const {
    const ViewerSettings s = viewingSettings();
    QVariantMap m;
    m.insert(QStringLiteral("bgColor"), m_app->argb(s.bgColor));
    m.insert(QStringLiteral("fgColor"), m_app->argb(s.fgColor));
    m.insert(QStringLiteral("highlightColor"), m_app->argb(s.highlightColor));
    m.insert(QStringLiteral("lockSideScroll"),
             (m_strip && m_strip->hasDocument()) ? m_strip->lockSideScroll() : s.lockSideScroll);
    m.insert(QStringLiteral("centerHorizontally"), s.centerHorizontally);
    m.insert(QStringLiteral("lockZoom"), s.lockZoom);
    m.insert(QStringLiteral("originalColors"), s.originalColors);
    m.insert(QStringLiteral("zoomMode"), s.zoomMode);
    m.insert(QStringLiteral("customZoomPercent"), double(s.customZoomPercent));
    m.insert(QStringLiteral("effectivePercent"), double(s.effectivePercent()));
    m.insert(QStringLiteral("doubleTapMenu"), s.doubleTapMenu);
    m.insert(QStringLiteral("singleTapMenu"), s.singleTapMenu);
    return m;
}

bool DocumentController::lockSideScroll() const {
    if (m_strip && m_strip->hasDocument()) return m_strip->lockSideScroll();
    return viewingSettings().lockSideScroll;
}

bool DocumentController::lockZoom() const { return viewingSettings().lockZoom; }

bool DocumentController::originalColors() const { return viewingSettings().originalColors; }

QString DocumentController::zoomLabel() const {
    const ViewerSettings s = viewingSettings();
    const ZoomMode mode = m_strip && m_strip->hasDocument() ? m_strip->zoomMode() : s.effectiveMode();
    if (mode == ZoomMode::FitWidth) return QStringLiteral("Width");
    if (mode == ZoomMode::FitPage || m_presenting) return QStringLiteral("Page");
    return QString::number(int(std::lround(zoomPercent()))) + QLatin1Char('%');
}

double DocumentController::zoomPercent() const {
    if (m_strip && m_strip->hasDocument()) return m_strip->currentZoomPercent();
    return viewingSettings().effectivePercent();
}

void DocumentController::refreshView() {
    emit zoomChanged();
    emit pageChanged();
}

void DocumentController::setZoomMode(ZoomMode mode) {
    mutateView([mode](ViewerSettings s) {
        const bool unlock = s.autoDisablePan && s.lockSideScroll && mode != ZoomMode::FitWidth;
        s.zoomMode = zoomModeToStorage(mode);
        if (mode == ZoomMode::FitWidth) s.lockSideScroll = true;
        else if (unlock) s.lockSideScroll = false;
        return s;
    });
}

void DocumentController::setPinchZoom(double percent) {
    mutateView([percent](ViewerSettings s) {
        const float p = std::clamp(float(percent), 25.f, 400.f);
        const bool unlock = s.autoDisablePan && s.lockSideScroll;
        if (s.rememberLastZoom) {
            s.zoomMode = zoomModeToStorage(ZoomMode::Custom);
            s.customZoomPercent = p;
        }
        if (unlock) s.lockSideScroll = false;
        return s;
    });
}

void DocumentController::setCustomZoomInternal(double percent) {
    mutateView([percent](ViewerSettings s) {
        const float p = std::clamp(float(percent), 25.f, 400.f);
        const bool zoomChanged = s.zoomMode != QLatin1String("CUSTOM") || std::abs(s.customZoomPercent - p) > 0.4f;
        const bool unlock = s.autoDisablePan && s.lockSideScroll && zoomChanged;
        s.zoomMode = zoomModeToStorage(ZoomMode::Custom);
        s.customZoomPercent = p;
        if (unlock) s.lockSideScroll = false;
        return s;
    });
}

void DocumentController::goToPageIndex(int index, double y) {
    if (!m_strip) return;
    m_strip->goToPage(index, y >= 0 ? std::optional<float>(float(y)) : std::nullopt);
}

void DocumentController::setScrubbing(bool on) {
    if (!m_strip) return;
    m_strip->setScrubbing(on);
}

void DocumentController::fitWidth() {
    if (!m_doc.valid || !m_strip) {
        emit toast(QStringLiteral("Open a PDF first"));
        return;
    }
    if (m_presenting) setPresenting(false);
    m_strip->applyFitWidth();
    setZoomMode(ZoomMode::FitWidth);
    emit zoomChanged();
}

void DocumentController::setCenterHorizontally(bool on) {
    m_app->updateSettings([on](ViewerSettings s) {
        s.centerHorizontally = on;
        return s;
    });
    mutateView([on](ViewerSettings s) {
        s.centerHorizontally = on;
        return s;
    });
    if (on && m_strip) m_strip->recenterX();
}

void DocumentController::centerHorizontally() {
    if (!m_doc.valid || !m_strip) {
        emit toast(QStringLiteral("Open a PDF first"));
        return;
    }
    setCenterHorizontally(true);
}

void DocumentController::setZoomPercent(double percent) {
    if (!m_doc.valid || !m_strip) {
        emit toast(QStringLiteral("Open a PDF first"));
        return;
    }
    if (m_presenting) setPresenting(false);
    m_strip->applyCustomZoom(float(percent));
    setCustomZoomInternal(percent);
    emit zoomChanged();
}

void DocumentController::setLockZoom(bool on) {
    mutateView([on](ViewerSettings s) {
        s.lockZoom = on;
        return s;
    });
    emit viewingChanged();
}

void DocumentController::setLockSideScroll(bool on) {
    if (m_strip) m_strip->setLockSideScroll(on);
    mutateView([on](ViewerSettings s) {
        s.lockSideScroll = on;
        return s;
    });
    emit viewingChanged();
}

void DocumentController::setOriginalColors(bool on) {
    mutateView([on](ViewerSettings s) {
        s.originalColors = on;
        return s;
    });
    emit viewingChanged();
}

int DocumentController::extraRotate(int page) const {
    if (page < 0 || page >= m_pageRotate.size()) return 0;
    return normRotate(m_pageRotate[page]);
}

QVector<PageInfo> DocumentController::displayPages() const {
    QVector<PageInfo> out;
    out.reserve(m_origPages.size());
    for (int i = 0; i < m_origPages.size(); ++i)
        out.append(displayPage(m_origPages[i], extraRotate(i)));
    return out;
}

void DocumentController::rebuildDisplayPages() {
    if (m_origPages.isEmpty()) return;
    if (m_pageRotate.size() != m_origPages.size()) {
        QVector<int> next(m_origPages.size(), 0);
        const int n = std::min(m_pageRotate.size(), m_origPages.size());
        for (int i = 0; i < n; ++i) next[i] = m_pageRotate[i];
        m_pageRotate = next;
    }
    m_doc.pages = displayPages();
}

void DocumentController::reloadStripPages(bool invalidate) {
    if (!m_strip || !m_loadedAny) return;
    if (invalidate) m_strip->invalidatePages();
    m_strip->replacePageSizes(m_doc.pages);
    if (m_presenting) m_strip->applyFitPage();
}

void DocumentController::setRotationsJson(const QString &json) {
    if (m_origPages.isEmpty()) m_origPages = m_doc.pages;
    m_pageRotate = decodeRotations(json, m_origPages.size());
    rebuildDisplayPages();
    reloadStripPages(hasRotation());
}

QString DocumentController::rotationsJson() const { return encodeRotations(m_pageRotate); }

bool DocumentController::hasRotation() const {
    for (int r : m_pageRotate) {
        if (normRotate(r) != 0) return true;
    }
    return false;
}

QList<int> DocumentController::rotatesFor(const QList<int> &pages) const {
    QList<int> out;
    out.reserve(pages.size());
    for (int p : pages) out.append(extraRotate(p));
    return out;
}

void DocumentController::rotatePages(int deltaCw, bool all) {
    if (!m_doc.valid) return;
    if (m_origPages.isEmpty()) m_origPages = m_doc.pages;
    rebuildDisplayPages();
    if (m_pageRotate.isEmpty()) return;
    if (all) {
        for (int &r : m_pageRotate) r = normRotate(r + deltaCw);
    } else {
        const int i = m_currentPage - 1;
        if (i < 0 || i >= m_pageRotate.size()) return;
        m_pageRotate[i] = normRotate(m_pageRotate[i] + deltaCw);
    }
    rebuildDisplayPages();
    reloadStripPages(true);
    onRotationsChanged();
}

void DocumentController::rotateCurrent(int deltaCw) { rotatePages(deltaCw, false); }

void DocumentController::rotateAll(int deltaCw) { rotatePages(deltaCw, true); }

void DocumentController::setPresenting(bool on) {
    if (!m_doc.valid) on = false;
    if (m_presenting == on) return;
    m_presenting = on;
    if (m_strip) {
        m_strip->setPresentation(on);
        if (!on) m_strip->setSettings(viewingSettings());
    }
    emit presentingChanged();
    emit zoomChanged();
}

void DocumentController::togglePresentation() {
    if (!m_doc.valid) {
        emit toast(QStringLiteral("Open a PDF first"));
        return;
    }
    setPresenting(!m_presenting);
}

void DocumentController::stepPage(int delta) {
    if (m_strip) m_strip->stepPage(delta);
}

void DocumentController::onPageRequested(int index, int width, bool urgent) {
    if (!m_strip) return;
    const qint64 key = m_strip->paintKey();
    const ViewerSettings paint = viewingSettings();
    const int rot = extraRotate(index);
    m_worker.postRender(index, urgent, [this, index, width, key, paint, rot] {
        try {
            QImage img = m_engine.render(index, width, paint.fgColor, paint.bgColor, paint.imageColorBlend,
                                         paint.originalColors, paint.imageAlpha, rot);
            postToUi([this, index, width, key, img] {
                setPageLoading(false);
                if (m_strip) m_strip->setPageImage(index, width, img, key);
            });
        } catch (...) {
        }
    });
}

void DocumentController::fillPagesAndOutline() {
    if (!m_doc.valid) return;
    const QString uri = m_doc.uri;
    m_worker.post([this, uri] {
        QVector<PageInfo> pages;
        QVector<OutlineItem> outline;
        try {
            pages = m_engine.measureRemainingPages();
        } catch (...) {
            pages = m_engine.pages();
        }
        try {
            outline = m_engine.outline();
        } catch (...) {
        }
        postToUi([this, uri, pages, outline] {
            if (!m_doc.valid || m_doc.uri != uri) return;
            m_origPages = pages;
            rebuildDisplayPages();
            m_doc.outline = outline;
            syncStrip();
            emit documentChanged();
        });
    });
}

QVariantList DocumentController::outline() const {
    QVariantList list;
    for (const OutlineItem &o : m_doc.outline) {
        QVariantMap m;
        m.insert(QStringLiteral("title"), o.title);
        m.insert(QStringLiteral("pageIndex"), o.pageIndex);
        m.insert(QStringLiteral("y"), o.y ? QVariant(double(*o.y)) : QVariant(-1.0));
        m.insert(QStringLiteral("hasY"), o.y.has_value());
        m.insert(QStringLiteral("depth"), o.depth);
        list.append(m);
    }
    return list;
}

void DocumentController::followLink(const QString &raw, bool external) {
    const QString text = raw.trimmed();
    if (text.isEmpty()) return;
    const QString lower = text.toLower();
    QString openable;
    if (lower.startsWith(QLatin1String("tel:")) || lower.startsWith(QLatin1String("callto:"))) {
        QString number;
        for (const QChar c : text.mid(text.indexOf(QLatin1Char(':')) + 1))
            if (c.isDigit() || c == QLatin1Char('+')) number += c;
        if (!number.isEmpty()) openable = QStringLiteral("tel:") + number;
    } else if (lower.startsWith(QLatin1String("mailto:")) || lower.startsWith(QLatin1String("http://"))
               || lower.startsWith(QLatin1String("https://")) || lower.startsWith(QLatin1String("sms:"))
               || lower.startsWith(QLatin1String("smsto:"))) {
        openable = text;
    } else if (lower.startsWith(QLatin1String("www."))) {
        openable = QStringLiteral("https://") + text;
    }
    if (!openable.isEmpty()) {
        m_app->openUrl(openable);
        return;
    }
    if (external) {
        m_app->openUrl(text);
        return;
    }
    m_worker.post([this, text] {
        const auto dest = m_engine.resolveUri(text);
        if (!dest) return;
        const int page = dest->first;
        const std::optional<float> y = dest->second;
        postToUi([this, page, y] {
            if (m_strip) m_strip->goToPage(page, y);
        });
    });
}

QVariantList DocumentController::hitList() const {
    QVariantList list;
    for (const SearchHit &h : m_hits) {
        QVariantMap m;
        m.insert(QStringLiteral("snippet"), h.snippet);
        m.insert(QStringLiteral("page"), h.startPage);
        list.append(m);
    }
    return list;
}

void DocumentController::applySearchToStrip() {
    if (m_strip) m_strip->setSearchHits(m_hits, m_hitIndex);
}

void DocumentController::search(const QString &query) {
    const int gen = ++m_searchGen;
    if (query.trimmed().isEmpty()) {
        m_hits.clear();
        m_hitIndex = 0;
        if (m_searching) {
            m_searching = false;
            emit searchingChanged();
        }
        applySearchToStrip();
        emit hitsChanged();
        return;
    }
    m_searching = true;
    emit searchingChanged();
    if (!m_doc.valid) {
        m_hits.clear();
        m_hitIndex = 0;
        m_searching = false;
        emit searchingChanged();
        applySearchToStrip();
        emit hitsChanged();
        return;
    }
    const int n = m_doc.pages.size();
    if (n == 0) {
        m_hits.clear();
        m_searching = false;
        emit searchingChanged();
        applySearchToStrip();
        emit hitsChanged();
        return;
    }
    auto acc = std::make_shared<QVector<QVector<CharBox>>>(n);
    auto step = std::make_shared<std::function<void(int)>>();
    std::weak_ptr<std::function<void(int)>> weakStep = step;
    *step = [this, gen, n, acc, query, weakStep](int i) {
        auto self = weakStep.lock();
        if (!self) return;
        m_worker.postBackground([this, gen, n, acc, query, self, i] {
            if (gen != m_searchGen.load()) return;
            try {
                (*acc)[i] = m_engine.pageContent(i).chars;
            } catch (...) {
            }
            if (i + 1 < n) {
                (*self)(i + 1);
                return;
            }
            const QVector<SearchHit> found = TextFinder::find(*acc, query);
            postToUi([this, gen, found] {
                if (gen != m_searchGen.load()) return;
                m_hits = found;
                m_hitIndex = 0;
                m_searching = false;
                emit searchingChanged();
                applySearchToStrip();
                emit hitsChanged();
            });
        });
    };
    (*step)(0);
}
void DocumentController::selectHit(int index) {
    const int n = m_hits.size();
    if (n == 0) return;
    m_hitIndex = std::clamp(index, 0, n - 1);
    applySearchToStrip();
    emit hitsChanged();
}

void DocumentController::nextHit() {
    const int n = m_hits.size();
    if (n == 0) return;
    m_hitIndex = (m_hitIndex + 1 + n) % n;
    applySearchToStrip();
    emit hitsChanged();
}

void DocumentController::prevHit() {
    const int n = m_hits.size();
    if (n == 0) return;
    m_hitIndex = (m_hitIndex - 1 + n) % n;
    applySearchToStrip();
    emit hitsChanged();
}

QVariantMap DocumentController::bookmarkToMap(const Bookmark &b) const {
    QVariantMap m;
    m.insert(QStringLiteral("id"), b.id);
    m.insert(QStringLiteral("documentUri"), b.documentUri);
    m.insert(QStringLiteral("name"), b.name);
    m.insert(QStringLiteral("pageIndex"), b.pageIndex);
    m.insert(QStringLiteral("scrollXPdf"), double(b.scrollXPdf));
    m.insert(QStringLiteral("scrollYPdf"), double(b.scrollYPdf));
    m.insert(QStringLiteral("sortOrder"), b.sortOrder);
    return m;
}

QVariantList DocumentController::bookmarks() const {
    QVariantList list;
    for (const Bookmark &b : m_bookmarks) list.append(bookmarkToMap(b));
    return list;
}

void DocumentController::refreshBookmarks() {
    const QString key = sourceKey();
    m_bookmarks = key.isEmpty() ? QVector<Bookmark>() : m_app->bookmarks().bookmarksFor(key);
    emit bookmarksChanged();
}

QVariantMap DocumentController::readingPosition() const {
    QVariantMap m;
    m.insert(QStringLiteral("page"), m_strip ? m_strip->currentPageIndex() : 0);
    m.insert(QStringLiteral("scrollX"), m_strip ? double(m_strip->currentScrollXPdf()) : 0.0);
    m.insert(QStringLiteral("scrollY"), m_strip ? double(m_strip->currentOffsetOnPage()) : 0.0);
    return m;
}

void DocumentController::addBookmark(const QString &name, int pageIndex, double scrollX, double scrollY) {
    const QString key = sourceKey();
    if (key.isEmpty()) return;
    m_app->bookmarks().addBookmark(key, name, pageIndex, float(scrollX), float(scrollY));
    refreshBookmarks();
}

void DocumentController::updateBookmark(const QVariantMap &m) {
    Bookmark b;
    b.id = m.value(QStringLiteral("id")).toLongLong();
    b.documentUri = m.value(QStringLiteral("documentUri")).toString();
    b.name = m.value(QStringLiteral("name")).toString();
    b.pageIndex = m.value(QStringLiteral("pageIndex")).toInt();
    b.scrollXPdf = m.value(QStringLiteral("scrollXPdf")).toFloat();
    b.scrollYPdf = m.value(QStringLiteral("scrollYPdf")).toFloat();
    b.sortOrder = m.value(QStringLiteral("sortOrder")).toInt();
    m_app->bookmarks().updateBookmark(b);
    refreshBookmarks();
}

void DocumentController::deleteBookmarks(const QVariantList &ids) {
    QVector<qint64> v;
    for (const QVariant &i : ids) v.append(i.toLongLong());
    m_app->bookmarks().deleteBookmarks(v);
    refreshBookmarks();
}

void DocumentController::reorderBookmarks(const QVariantList &ids) {
    QVector<qint64> v;
    for (const QVariant &i : ids) v.append(i.toLongLong());
    m_app->bookmarks().reorder(v);
    refreshBookmarks();
}

void DocumentController::jumpBookmark(const QVariantMap &m) {
    if (!m_strip) return;
    m_strip->goToPageOffset(m.value(QStringLiteral("pageIndex")).toInt(), m.value(QStringLiteral("scrollYPdf")).toFloat(),
                            m.value(QStringLiteral("scrollXPdf")).toFloat());
}

void DocumentController::submitPassword(const QString &password) {
    m_worker.post([this, password] {
        bool ok = false;
        try {
            ok = m_engine.authenticatePassword(password);
        } catch (...) {
        }
        OpenData data;
        if (ok) data = captureOpenData();
        postToUi([this, ok, data] {
            if (!ok) {
                m_passwordWrong = true;
                emit passwordChanged();
                return;
            }
            resumeAfterPassword(data);
        });
    });
}

void DocumentController::cancelPassword() {
    m_worker.post([this] { m_engine.cancelPassword(); });
    clearPendingPassword();
    clearPasswordPrompt();
    setPageLoading(false);
    setDocument(std::nullopt);
}

void DocumentController::clearSelection() {
    if (m_strip) m_strip->clearSelection();
    m_selectionText.clear();
    emit selectionChanged();
}

void DocumentController::selectAll() {
    if (m_strip) m_strip->selectAll();
}

void DocumentController::copySelection() {
    if (m_selectionText.isEmpty()) return;
    m_app->copyText(m_selectionText);
    emit toast(QStringLiteral("Copied"));
}

QVariantList DocumentController::selectedPagesList() const {
    QVariantList list;
    for (int p : m_selectedPages) list.append(p);
    return list;
}

void DocumentController::closeDocument() {
    if (!m_doc.valid) return;
    cancelAllRenders();
    m_worker.post([this] { m_engine.close(); });
    setSourceUri({});
    setDocument(std::nullopt);
    setPageLoading(false);
}

// Close doc before delete (Windows lock).
bool DocumentController::releaseIfUsing(const QStringList &paths) {
    if (!m_doc.valid) return false;
    const QString open = QFileInfo(QUrl(m_doc.uri).isLocalFile() ? QUrl(m_doc.uri).toLocalFile() : m_doc.uri).absoluteFilePath();
    for (const QString &p : paths) {
        if (QFileInfo(p).absoluteFilePath().compare(open, Qt::CaseInsensitive) != 0) continue;
        closeDocument();
        m_worker.drainControl();
        return true;
    }
    return false;
}

void DocumentController::startPageSelect() {
    if (m_presenting) setPresenting(false);
    m_pageSelectMode = true;
    m_selectedPages.clear();
    if (m_strip) m_strip->setPageSelect(true, {});
    emit pageSelectChanged();
}

void DocumentController::togglePage(int page) {
    if (m_selectedPages.contains(page)) m_selectedPages.removeAll(page);
    else m_selectedPages.append(page);
    if (m_strip) m_strip->setPageSelect(m_pageSelectMode, toSet(m_selectedPages));
    emit pageSelectChanged();
}

void DocumentController::setSelectedPages(const QVariantList &pages) {
    m_selectedPages.clear();
    for (const QVariant &p : pages) m_selectedPages.append(p.toInt());
    if (m_strip) m_strip->setPageSelect(m_pageSelectMode, toSet(m_selectedPages));
    emit pageSelectChanged();
}

void DocumentController::applyFitWidthForPageSelect() {
    if (m_strip) m_strip->applyFitWidthForPageSelect();
}

void DocumentController::exitPageSelect() {
    const bool was = m_pageSelectMode;
    m_pageSelectMode = false;
    m_selectedPages.clear();
    if (m_strip) m_strip->setPageSelect(false, {});
    Q_UNUSED(was);
    emit pageSelectChanged();
}

void DocumentController::shareSelectedPages(bool storeRotation) {
    if (!m_selectedPages.isEmpty()) {
        QVariantList l;
        for (int p : m_selectedPages) l.append(p);
        exportPartial(l, storeRotation);
    }
}

void DocumentController::exportPartial(const QVariantList &pages, bool storeRotation) {
    const QString source = exportSource();
    if (source.isEmpty()) return;
    QList<int> list;
    for (const QVariant &p : pages) list.append(p.toInt());
    const QList<int> rots = (storeRotation && hasRotation()) ? rotatesFor(list) : QList<int>{};
    setBusy(QStringLiteral("Building PDF"));
    m_worker.post([this, source, list, rots] {
        QString dest = AppPaths::exportPdf();
        QString err;
        try {
            PdfComposer::extractPages(source, list, dest, QString(), QString(), rots);
        } catch (const EngineError &e) {
            err = e.code();
        } catch (...) {
            err = QStringLiteral("EXPORT");
        }
        postToUi([this, dest, err] {
            setBusy(QString());
            if (err.isEmpty()) {
                exitPageSelect();
                emit exportReady(dest);
            } else {
                emitError(err);
            }
        });
    });
}

void DocumentController::shareFull(bool storeRotation) {
    const QString source = shareSourcePath();
    if (source.isEmpty()) {
        emit toast(QStringLiteral("Open a PDF first"));
        return;
    }
    const bool bake = storeRotation && hasRotation();
    if (!bake && isPdf()) {
        emit exportReady(source);
        return;
    }
    QList<int> idxs, rots;
    if (bake) {
        for (int i = 0; i < totalPages(); ++i) {
            idxs.append(i);
            rots.append(extraRotate(i));
        }
    }
    setBusy(QStringLiteral("Building PDF"));
    const bool pdf = isPdf();
    m_worker.post([this, source, bake, idxs, rots, pdf] {
        QString dest = AppPaths::exportPdf();
        QString err;
        try {
            if (bake && pdf) {
                PdfComposer::extractPages(source, idxs, dest, QString(), QString(), rots);
            } else if (bake) {
                PdfComposer::combine({{source, QString()}}, 1.f, dest, QString(), rots);
            } else {
                PdfComposer::combine({{source, QString()}}, 1.f, dest);
            }
        } catch (const EngineError &e) {
            err = e.code();
        } catch (...) {
            err = QStringLiteral("EXPORT");
        }
        postToUi([this, dest, err] {
            setBusy(QString());
            if (err.isEmpty()) emit exportReady(dest);
            else emitError(err);
        });
    });
}

QVariantList DocumentController::pageSizes() const {
    QVariantList list;
    for (const PageInfo &p : m_doc.pages) {
        QVariantMap m;
        m.insert(QStringLiteral("width"), double(p.width));
        m.insert(QStringLiteral("height"), double(p.height));
        list.append(m);
    }
    return list;
}

void DocumentController::resetOverlays() {
    search(QString());
    clearSelection();
}

} // namespace papercut
