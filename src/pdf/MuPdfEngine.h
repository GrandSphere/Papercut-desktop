#pragma once

#include "PdfModels.h"

#include <QByteArray>
#include <QHash>
#include <QImage>
#include <QString>
#include <QVector>
#include <mutex>
#include <optional>
#include <stdexcept>
#include <utility>

struct pc_engine;

namespace papercut {

class EngineError : public std::runtime_error {
public:
    explicit EngineError(const QString &code) : std::runtime_error(code.toStdString()), m_code(code) {}
    const QString &code() const { return m_code; }

private:
    QString m_code;
};

// MuPDF shim; EngineWorker only.
class MuPdfEngine {
public:
    MuPdfEngine();
    ~MuPdfEngine();
    MuPdfEngine(const MuPdfEngine &) = delete;
    MuPdfEngine &operator=(const MuPdfEngine &) = delete;

    void open(const QString &path, float fontScale, int startPage = 0, const QString &userCss = {});
    bool authenticatePassword(const QString &password);
    void cancelPassword();
    void close();
    void relayout(float fontScale, const QString &userCss);

    bool isOpened() const;
    bool isReflowable() const;
    QString mime() const;
    QVector<PageInfo> pages() const;
    int pageCount() const;

    std::optional<PageInfo> ensurePageSize(int index);
    QVector<PageInfo> measureRemainingPages();

    QImage render(int index, int targetWidth, int fg, int bg, float imageBlend,
                  bool originalColors = false, float imageAlpha = 1.f, int extraCw = 0);

    QVector<OutlineItem> outline();
    std::optional<std::pair<int, std::optional<float>>> resolveUri(const QString &uri);
    PageContent pageContent(int index);

    // URI page: #page=N, ?page=N, #N.
    static std::optional<int> pageFromUri(const QString &uri);

private:
    PageContent contentLocked(int index);
    void finishOpenLocked();
    void closeLocked();
    void ensureMeasuredLocked(int index);
    PageInfo measureLocked(int index);

    mutable std::recursive_mutex m_mutex;
    pc_engine *m_engine = nullptr;
    bool m_awaitingPassword = false;
    int m_startPage = 0;
    float m_em = 12.f;
    QByteArray m_userCss;
    QString m_mime;
    QVector<PageInfo> m_pages;
    QVector<bool> m_measured;
    int m_measuredCount = 0;
    QHash<int, PageContent> m_content;
    std::optional<QVector<OutlineItem>> m_outline;
};

} // namespace papercut
