#include "ViewerController.h"

#include "AppServices.h"
#include "core/AppPaths.h"
#include "core/Io.h"

#include <QFile>
#include <QFileInfo>

namespace papercut {

namespace {

ViewerSettings asDark(ViewerSettings s) {
    s.originalColors = false;
    s.imageColorBlend = s.darkImageBlend;
    s.imageAlpha = s.darkImageAlpha;
    return s;
}

} // namespace

ViewerController::ViewerController(QObject *parent) : DocumentController(parent) {
    m_startupDark = m_app->settings().alwaysDarkMode;
}

ViewerSettings ViewerController::viewingSettings() const {
    const ViewerSettings s = m_app->settings();
    return (m_startupDark && s.alwaysDarkMode) ? asDark(s) : s;
}

void ViewerController::setOriginalColors(bool on) {
    m_startupDark = false;
    DocumentController::setOriginalColors(on);
}

void ViewerController::mutateView(const std::function<ViewerSettings(ViewerSettings)> &transform) {
    m_app->updateSettings(transform);
    pushViewingToStrip();
}

QString ViewerController::exportSource() const { return !m_doc.uri.isEmpty() ? m_doc.uri : m_sourceUri; }

void ViewerController::clearPendingPassword() {
    m_pendingPath.clear();
    m_pendingFromLast = false;
}

void ViewerController::resumeAfterPassword(const OpenData &data) {
    if (m_pendingPath.isEmpty()) return;
    const QString path = m_pendingPath;
    const bool fromLast = m_pendingFromLast;
    clearPendingPassword();
    clearPasswordPrompt();
    completeOpen(path, fromLast, data);
}

void ViewerController::closeDocument() {
    if (!m_doc.valid) return;
    const auto p = readingPosition();
    saveReadingPosition(p.value(QStringLiteral("page")).toInt(), p.value(QStringLiteral("scrollX")).toDouble(),
                        p.value(QStringLiteral("scrollY")).toDouble());
    ++m_openGen;
    DocumentController::closeDocument();
}

QString ViewerController::documentName() const {
    if (m_sourceUri.isEmpty()) return QString();
    const auto doc = m_app->bookmarks().getDocument(m_sourceUri);
    if (doc && !doc->displayName.isEmpty()) return doc->displayName;
    return DocumentPaths::label(m_sourceUri).displayName;
}

bool ViewerController::isLastPdfCopy(const QString &) const { return false; }

PathLabel ViewerController::documentLabel(const QString &path, bool lastPdf) const {
    const ViewerSettings s = m_app->settings();
    const QString labelPath = (lastPdf && s.lastPdfUri && !s.lastPdfUri->isEmpty()) ? *s.lastPdfUri : path;
    return DocumentPaths::label(labelPath);
}

std::optional<PdfDocument> ViewerController::resolveLastPdfDocument(const ViewerSettings &s) const {
    if (s.lastPdfUri && !s.lastPdfUri->trimmed().isEmpty() && !isLastPdfCopy(*s.lastPdfUri))
        return m_app->bookmarks().getDocument(*s.lastPdfUri);
    return std::nullopt;
}

std::optional<PdfDocument> ViewerController::resolveStoredDocument(const QString &path, bool fromLastPdf,
                                                                   const PathLabel &label) const {
    std::optional<PdfDocument> found;
    if (fromLastPdf || isLastPdfCopy(path)) found = resolveLastPdfDocument(m_app->settingsRepo().get());
    else found = m_app->bookmarks().getDocument(path);
    if (found) return found;
    return m_app->bookmarks().findByNameAndPath(label.displayName, label.displayPath);
}

void ViewerController::copyLastPdf(const QString &) {}

void ViewerController::savePrevious(int page, float x, float y) {
    const QString key = m_sourceUri;
    if (key.isEmpty()) return;
    m_app->bookmarks().saveLastPosition(key, page, x, y);
    if (m_app->settingsRepo().get().restoreLastPdf) {
        m_app->updateSettings([&](ViewerSettings s) {
            s.lastPageIndex = page;
            s.lastScrollXPdf = x;
            s.lastScrollYPdf = y;
            return s;
        });
    }
}

int ViewerController::beginOpen() {
    const int gen = ++m_openGen;
    cancelAllRenders();
    setDocument(std::nullopt);
    resetSearchState();
    setPageLoading(true);
    return gen;
}

void ViewerController::openWithPrevious(const QString &path, int previousPage, double previousScrollX,
                                        double previousScrollY, const QString &password) {
    savePrevious(previousPage, float(previousScrollX), float(previousScrollY));
    open(path, true, password);
}

void ViewerController::open(const QString &path, bool reportError, const QString &password) {
    const int gen = beginOpen();
    const ViewerSettings s = m_app->settingsRepo().get();
    const bool fromLast = isLastPdfCopy(path);
    const PathLabel label = documentLabel(path, fromLast);
    const auto stored = resolveStoredDocument(path, fromLast, label);
    const int startPage = s.restoreLastPosition && stored ? stored->lastPageIndex : 0;
    openOnWorker(gen, path, startPage, fromLast, reportError, false, password);
}

void ViewerController::openOnWorker(int generation, const QString &path, int startPage, bool fromLastPdf,
                                    bool reportError, bool startup, const QString &password) {
    const float scale = float(m_app->fontScale());
    const QString css = m_app->documentUserCss();
    m_worker.post([=, this] {
        QString code;
        OpenData data;
        try {
            openDocumentOnEngine(path, startPage, scale, css);
            data = captureOpenData();
        } catch (const EngineError &e) {
            code = e.code();
        } catch (...) {
            code = QStringLiteral("OPEN");
        }
        if (code == QLatin1String("PASSWORD") && !password.isEmpty()) {
            try {
                if (m_engine.authenticatePassword(password)) {
                    data = captureOpenData();
                    code.clear();
                }
            } catch (...) {
            }
        }
        postToUi([=, this] {
            if (generation != m_openGen) return;
            if (code.isEmpty()) {
                completeOpen(path, fromLastPdf, data);
            } else if (code == QLatin1String("PASSWORD")) {
                m_pendingPath = path;
                m_pendingFromLast = startup;
                showPasswordPrompt();
            } else {
                setDocument(std::nullopt);
                setPageLoading(false);
                if (reportError && !startup) emitError(QStringLiteral("OPEN"));
            }
            if (startup) markStartupReady();
        });
    });
}

void ViewerController::openPreview(const QString &path) {
    const int gen = beginOpen();
    const float scale = float(m_app->fontScale());
    const QString css = m_app->documentUserCss();
    m_worker.post([=, this] {
        bool ok = true;
        OpenData data;
        try {
            openDocumentOnEngine(path, 0, scale, css);
            data = captureOpenData();
        } catch (...) {
            ok = false;
        }
        postToUi([=, this] {
            if (gen != m_openGen) return;
            if (ok) {
                DocState d;
                d.valid = true;
                d.uri = path;
                d.pages = data.pages;
                d.mime = data.mime;
                d.reflowable = data.reflowable;
                setDocument(d);
                setPageLoading(false);
                fillPagesAndOutline();
            } else {
                setDocument(std::nullopt);
                setPageLoading(false);
                emitError(QStringLiteral("OPEN"));
            }
        });
    });
}

void ViewerController::completeOpen(const QString &path, bool fromLastPdf, const OpenData &data) {
    const int generation = m_openGen;
    const ViewerSettings s = m_app->settingsRepo().get();
    const bool lastPdf = fromLastPdf || isLastPdfCopy(path);
    const PathLabel label = documentLabel(path, lastPdf);
    const auto stored = resolveStoredDocument(path, lastPdf, label);
    QString key;
    if (stored) {
        key = stored->uri;
    } else if (lastPdf) {
        if (s.lastPdfUri && !s.lastPdfUri->isEmpty() && !isLastPdfCopy(*s.lastPdfUri)) key = *s.lastPdfUri;
    } else {
        key = path;
    }
    std::optional<PdfDocument> doc;
    if (!key.trimmed().isEmpty()) doc = m_app->bookmarks().ensureDocument(key, label.displayName, label.displayPath);
    if (generation != m_openGen) return;
    setSourceUri(doc ? doc->uri : QString());
    const bool restorePos = s.restoreLastPosition;
    stashRestore(restorePos ? (doc ? doc->lastPageIndex : s.lastPageIndex) : 0,
                 restorePos ? (doc ? doc->lastScrollXPdf : s.lastScrollXPdf) : 0.f,
                 restorePos ? (doc ? doc->lastScrollYPdf : s.lastScrollYPdf) : 0.f, restorePos);
    setPageLoading(true);
    DocState d;
    d.valid = true;
    d.uri = path;
    d.pages = data.pages;
    d.mime = data.mime;
    d.reflowable = data.reflowable;
    setDocument(d);
    resetSearchState();
    if (s.restoreLastPdf) {
        const QString lastKey = doc ? doc->uri : path;
        m_app->updateSettings([&](ViewerSettings st) {
            st.lastPdfUri = lastKey;
            return st;
        });
    }
    fillPagesAndOutline();
}

void ViewerController::maybeRestoreLast(bool hasDocumentIntent) {
    if (hasDocumentIntent || m_restoredLast) {
        m_restoredLast = true;
        markStartupReady();
        return;
    }
    m_restoredLast = true;
    const ViewerSettings s = m_app->settingsRepo().get();
    if (s.folioEnabled && s.openFolioOnStartup) {
        emit launchFolio();
        markStartupReady();
        return;
    }
    if (!s.restoreLastPdf) {
        markStartupReady();
        return;
    }
    if (!s.lastPdfUri || s.lastPdfUri->trimmed().isEmpty()) {
        markStartupReady();
        return;
    }
    const QString file = *s.lastPdfUri;
    const QFileInfo fi(file);
    if (!fi.exists() || fi.size() <= 0) {
        markStartupReady();
        return;
    }
    const auto stored = resolveLastPdfDocument(s);
    const int startPage = s.restoreLastPosition ? (stored ? stored->lastPageIndex : s.lastPageIndex) : 0;
    const int gen = ++m_openGen;
    setPageLoading(true);
    openOnWorker(gen, file, startPage, false, false, true);
}

void ViewerController::saveReadingPosition(int page, double scrollX, double scrollY) {
    if (!m_doc.valid) return;
    if (!m_sourceUri.isEmpty())
        m_app->bookmarks().saveLastPosition(m_sourceUri, page, float(scrollX), float(scrollY));
    m_app->updateSettings([&](ViewerSettings s) {
        s.lastPageIndex = page;
        s.lastScrollXPdf = float(scrollX);
        s.lastScrollYPdf = float(scrollY);
        return s;
    });
}

bool ViewerController::alreadyInFolio() const {
    if (m_sourceUri.isEmpty()) return false;
    return m_app->folio().getByUri(m_sourceUri).has_value();
}

void ViewerController::addCurrentToFolio() {
    const QString key = m_sourceUri;
    if (key.isEmpty()) return;
    const QString source = !m_doc.uri.isEmpty() ? m_doc.uri : key;
    const PathLabel label = DocumentPaths::label(key);
    const ViewerSettings snap = viewingSettings();
    const auto added = m_app->folio().add(key, label.displayName, label.displayPath, source, &snap);
    emitError(added ? QStringLiteral("FOLIO_ADDED") : QStringLiteral("FOLIO_COPY"));
}

} // namespace papercut
