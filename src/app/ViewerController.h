#pragma once

#include "DocumentController.h"
#include "data/DocumentPaths.h"

namespace papercut {

class ViewerController : public DocumentController {
    Q_OBJECT
    Q_PROPERTY(bool startupDark READ startupDark NOTIFY viewingChanged)

public:
    explicit ViewerController(QObject *parent = nullptr);

    bool startupDark() const { return m_startupDark; }
    ViewerSettings viewingSettings() const override;
    void setOriginalColors(bool on) override;
    QString shareSourcePath() const override { return exportSource(); }

    Q_INVOKABLE void open(const QString &path, bool reportError = true, const QString &password = {});
    Q_INVOKABLE void openWithPrevious(const QString &path, int previousPage, double previousScrollX,
                                      double previousScrollY, const QString &password = {});
    Q_INVOKABLE void openPreview(const QString &path);
    Q_INVOKABLE void maybeRestoreLast(bool hasDocumentIntent);
    void closeDocument() override;
    Q_INVOKABLE void saveReadingPosition(int page, double scrollX, double scrollY);
    Q_INVOKABLE bool alreadyInFolio() const;
    Q_INVOKABLE void addCurrentToFolio();
    Q_INVOKABLE QString documentName() const;

signals:
    void launchFolio();

protected:
    void mutateView(const std::function<ViewerSettings(ViewerSettings)> &transform) override;
    QString exportSource() const override;
    void resumeAfterPassword(const OpenData &data) override;
    void clearPendingPassword() override;

private:
    int beginOpen();
    void openOnWorker(int generation, const QString &path, int startPage, bool fromLastPdf, bool reportError,
                      bool startup, const QString &password = {});
    void completeOpen(const QString &path, bool fromLastPdf, const OpenData &data);
    bool isLastPdfCopy(const QString &path) const;
    PathLabel documentLabel(const QString &path, bool lastPdf) const;
    std::optional<PdfDocument> resolveLastPdfDocument(const ViewerSettings &s) const;
    std::optional<PdfDocument> resolveStoredDocument(const QString &path, bool fromLastPdf,
                                                     const PathLabel &label) const;
    void copyLastPdf(const QString &path);
    void savePrevious(int page, float x, float y);

    bool m_startupDark = false;
    int m_openGen = 0;
    bool m_restoredLast = false;
    QString m_pendingPath;
    bool m_pendingFromLast = false;
};

} // namespace papercut
