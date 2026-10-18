#pragma once

#include "EngineWorker.h"
#include "data/ViewerSettings.h"
#include "pdf/MuPdfEngine.h"
#include "pdf/PdfModels.h"
#include "ui/PdfStripItem.h"

#include <QList>
#include <QObject>
#include <QSet>
#include <QVariantList>
#include <QVariantMap>
#include <atomic>
#include <functional>
#include <optional>

namespace papercut {

class AppServices;

class DocumentController : public QObject {
    Q_OBJECT
    Q_PROPERTY(papercut::PdfStripItem *strip READ strip WRITE setStrip NOTIFY stripChanged)
    Q_PROPERTY(bool hasDocument READ hasDocument NOTIFY documentChanged)
    Q_PROPERTY(QVariantList outline READ outline NOTIFY documentChanged)
    Q_PROPERTY(bool hasOutline READ hasOutline NOTIFY documentChanged)
    Q_PROPERTY(int totalPages READ totalPages NOTIFY documentChanged)
    Q_PROPERTY(bool isPdf READ isPdf NOTIFY documentChanged)
    Q_PROPERTY(QString documentUri READ documentUri NOTIFY documentChanged)
    Q_PROPERTY(QVariantMap viewing READ viewing NOTIFY viewingChanged)
    Q_PROPERTY(bool lockSideScroll READ lockSideScroll NOTIFY viewingChanged)
    Q_PROPERTY(bool lockZoom READ lockZoom NOTIFY viewingChanged)
    Q_PROPERTY(bool originalColors READ originalColors NOTIFY viewingChanged)
    Q_PROPERTY(QString busy READ busy NOTIFY busyChanged)
    Q_PROPERTY(bool pageLoading READ pageLoading NOTIFY pageLoadingChanged)
    Q_PROPERTY(bool startupReady READ startupReady NOTIFY startupReadyChanged)
    Q_PROPERTY(bool passwordNeeded READ passwordNeeded NOTIFY passwordChanged)
    Q_PROPERTY(bool passwordWrong READ passwordWrong NOTIFY passwordChanged)
    Q_PROPERTY(int hitCount READ hitCount NOTIFY hitsChanged)
    Q_PROPERTY(int hitIndex READ hitIndex NOTIFY hitsChanged)
    Q_PROPERTY(bool searching READ searching NOTIFY searchingChanged)
    Q_PROPERTY(QVariantList hitList READ hitList NOTIFY hitsChanged)
    Q_PROPERTY(QVariantList bookmarks READ bookmarks NOTIFY bookmarksChanged)
    Q_PROPERTY(QString selectionText READ selectionText NOTIFY selectionChanged)
    Q_PROPERTY(int currentPage READ currentPage NOTIFY pageChanged)
    Q_PROPERTY(int visibleCount READ visibleCount NOTIFY pageChanged)
    Q_PROPERTY(QString zoomLabel READ zoomLabel NOTIFY zoomChanged)
    Q_PROPERTY(double zoomPercent READ zoomPercent NOTIFY zoomChanged)
    Q_PROPERTY(bool pageSelectMode READ pageSelectMode NOTIFY pageSelectChanged)
    Q_PROPERTY(QVariantList selectedPages READ selectedPagesList NOTIFY pageSelectChanged)
    Q_PROPERTY(bool presenting READ presenting NOTIFY presentingChanged)
    Q_PROPERTY(bool inFolio READ inFolio CONSTANT)

public:
    explicit DocumentController(QObject *parent = nullptr);
    ~DocumentController() override;

    PdfStripItem *strip() const { return m_strip; }
    void setStrip(PdfStripItem *strip);

    bool hasDocument() const { return m_doc.valid; }
    QVariantList outline() const;
    bool hasOutline() const { return !m_doc.outline.isEmpty(); }
    int totalPages() const { return m_doc.pages.size(); }
    bool isPdf() const { return m_doc.valid && m_doc.mime == QLatin1String("application/pdf"); }
    QString documentUri() const { return m_doc.uri; }
    QVariantMap viewing() const;
    bool lockSideScroll() const;
    bool lockZoom() const;
    bool originalColors() const;
    QString busy() const { return m_busy; }
    bool pageLoading() const { return m_pageLoading; }
    bool startupReady() const { return m_startupReady; }
    bool passwordNeeded() const { return m_passwordNeeded; }
    bool passwordWrong() const { return m_passwordWrong; }
    int hitCount() const { return m_hits.size(); }
    int hitIndex() const { return m_hitIndex; }
    bool searching() const { return m_searching; }
    QVariantList hitList() const;
    QVariantList bookmarks() const;
    QString selectionText() const { return m_selectionText; }
    int currentPage() const { return m_currentPage; }
    int visibleCount() const { return m_visibleCount; }
    QString zoomLabel() const;
    double zoomPercent() const;
    bool pageSelectMode() const { return m_pageSelectMode; }
    QVariantList selectedPagesList() const;
    bool presenting() const { return m_presenting; }
    virtual bool inFolio() const { return false; }

    virtual ViewerSettings viewingSettings() const = 0;

    Q_INVOKABLE void search(const QString &query);
    Q_INVOKABLE void selectHit(int index);
    Q_INVOKABLE void nextHit();
    Q_INVOKABLE void prevHit();

    Q_INVOKABLE void goToPageIndex(int index, double y = -1);
    Q_INVOKABLE void setScrubbing(bool on);
    Q_INVOKABLE void fitWidth();
    Q_INVOKABLE void setCenterHorizontally(bool on);
    Q_INVOKABLE void centerHorizontally();
    Q_INVOKABLE void setZoomPercent(double percent);
    Q_INVOKABLE void setLockZoom(bool on);
    Q_INVOKABLE void setLockSideScroll(bool on);
    Q_INVOKABLE void setPresenting(bool on);
    Q_INVOKABLE void togglePresentation();
    Q_INVOKABLE void stepPage(int delta);
    Q_INVOKABLE void rotateCurrent(int deltaCw);
    Q_INVOKABLE void rotateAll(int deltaCw);
    virtual void setOriginalColors(bool on);
    Q_INVOKABLE void toggleOriginalColors() { setOriginalColors(!originalColors()); }
    Q_INVOKABLE void refreshView();

    Q_INVOKABLE QVariantMap readingPosition() const;
    Q_INVOKABLE void addBookmark(const QString &name, int pageIndex, double scrollX, double scrollY);
    Q_INVOKABLE void updateBookmark(const QVariantMap &bookmark);
    Q_INVOKABLE void deleteBookmarks(const QVariantList &ids);
    Q_INVOKABLE void reorderBookmarks(const QVariantList &ids);
    Q_INVOKABLE void jumpBookmark(const QVariantMap &bookmark);

    Q_INVOKABLE void submitPassword(const QString &password);
    Q_INVOKABLE void cancelPassword();

    Q_INVOKABLE void clearSelection();
    Q_INVOKABLE void selectAll();
    Q_INVOKABLE void copySelection();
    Q_INVOKABLE void startPageSelect();
    Q_INVOKABLE void togglePage(int page);
    Q_INVOKABLE void setSelectedPages(const QVariantList &pages);
    Q_INVOKABLE void exitPageSelect();
    Q_INVOKABLE void shareSelectedPages(bool storeRotation = true);
    Q_INVOKABLE void applyFitWidthForPageSelect();
    Q_INVOKABLE void exportPartial(const QVariantList &pages, bool storeRotation = true);
    Q_INVOKABLE void shareFull(bool storeRotation = true);
    Q_INVOKABLE virtual void closeDocument();
    Q_INVOKABLE bool releaseIfUsing(const QStringList &paths);
    Q_INVOKABLE void resetOverlays();
    Q_INVOKABLE virtual QString shareSourcePath() const = 0;
    Q_INVOKABLE QVariantList pageSizes() const;

signals:
    void stripChanged();
    void documentChanged();
    void viewingChanged();
    void busyChanged();
    void pageLoadingChanged();
    void startupReadyChanged();
    void passwordChanged();
    void hitsChanged();
    void searchingChanged();
    void bookmarksChanged();
    void selectionChanged();
    void pageChanged();
    void zoomChanged();
    void pageSelectChanged();
    void presentingChanged();
    void toast(const QString &message);
    void exportReady(const QString &path);
    void menuRequested();
    void pageTapped(int page);

protected:
    struct DocState {
        bool valid = false;
        QString uri;
        QVector<PageInfo> pages;
        QVector<OutlineItem> outline;
        QString mime = QStringLiteral("application/pdf");
        bool reflowable = false;
    };
    struct OpenData {
        QVector<PageInfo> pages;
        QString mime;
        bool reflowable = false;
    };
    struct Restore {
        bool pending = false;
        int page = 0;
        float x = 0.f;
        float offset = 0.f;
    };

    virtual void mutateView(const std::function<ViewerSettings(ViewerSettings)> &transform) = 0;
    virtual QString exportSource() const = 0;
    virtual void resumeAfterPassword(const OpenData &data) = 0;
    virtual void clearPendingPassword() {}
    virtual void onFolioSwipe(int) {}
    virtual void onRotationsChanged() {}
    virtual bool clearViewWhenNoDocument() const { return true; }
    virtual QString sourceKey() const { return m_sourceUri; }
    void setRotationsJson(const QString &json);
    QString rotationsJson() const;

    void setDocument(const std::optional<DocState> &doc);
    void setPageLoading(bool on);
    void setBusy(const QString &text);
    void markStartupReady();
    void emitError(const QString &code);
    void resetSearchState();
    void cancelAllRenders();
    void stashRestore(int page, float scrollX, float offsetOnPage, bool enabled = true);
    void showPasswordPrompt();
    void clearPasswordPrompt();
    void fillPagesAndOutline();
    void pushViewingToStrip();
    void setSourceUri(const QString &uri);
    void refreshBookmarks();
    void postToUi(std::function<void()> fn);
    void followLink(const QString &uri, bool external);
    bool openedOnWorker() { return m_engine.isOpened(); }
    void openDocumentOnEngine(const QString &path, int startPage, float fontScale, const QString &userCss);
    OpenData captureOpenData();
    void relayoutReflowable();

    MuPdfEngine m_engine;
    EngineWorker m_worker;
    AppServices *m_app = nullptr;
    PdfStripItem *m_strip = nullptr;
    DocState m_doc;
    QString m_sourceUri;
    Restore m_restore;
    QString m_loadedUri;
    bool m_loadedAny = false;

private:
    void syncStrip();
    void onPageRequested(int index, int width, bool urgent);
    void setPinchZoom(double percent);
    void setZoomMode(ZoomMode mode);
    void setCustomZoomInternal(double percent);
    void applySearchToStrip();
    QVariantMap bookmarkToMap(const Bookmark &b) const;
    int extraRotate(int page) const;
    QVector<PageInfo> displayPages() const;
    void rebuildDisplayPages();
    void reloadStripPages(bool invalidate);
    void rotatePages(int deltaCw, bool all);
    bool hasRotation() const;
    QList<int> rotatesFor(const QList<int> &pages) const;

    QString m_busy;
    bool m_pageLoading = false;
    bool m_startupReady = false;
    bool m_passwordNeeded = false;
    bool m_passwordWrong = false;
    QVector<SearchHit> m_hits;
    int m_hitIndex = 0;
    bool m_searching = false;
    std::atomic<int> m_searchGen{0};
    QVector<Bookmark> m_bookmarks;
    QString m_selectionText;
    int m_currentPage = 1;
    int m_visibleCount = 0;
    bool m_pageSelectMode = false;
    QVector<int> m_selectedPages;
    QVector<PageInfo> m_origPages;
    QVector<int> m_pageRotate;
    bool m_presenting = false;
    int m_reflowGen = 0;
    QString m_laidFamily;
    int m_laidWeight = 0;
    float m_laidScale = 0.f;
};

} // namespace papercut
