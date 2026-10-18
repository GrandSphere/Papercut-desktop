#include "MuPdfEngine.h"

#include "ColorRemapper.h"
#include "DocumentMagic.h"
#include "EngineConstants.h"
#include "pc_mupdf.h"

#include <QFile>
#include <QFileInfo>
#include <QRegularExpression>
#include <algorithm>
#include <cmath>
#include <cstring>

namespace papercut {

namespace {

const QRegularExpression &pageParam() {
    static const QRegularExpression re(QStringLiteral("[#?&]page=(\\d+)"));
    return re;
}

const QRegularExpression &pageFragment() {
    static const QRegularExpression re(QStringLiteral("^#(\\d+)$"));
    return re;
}

} // namespace

MuPdfEngine::MuPdfEngine() {
    m_engine = pc_new();
    if (!m_engine) throw EngineError(QStringLiteral("INIT"));
}

MuPdfEngine::~MuPdfEngine() {
    std::lock_guard lock(m_mutex);
    if (m_engine) {
        pc_free(m_engine);
        m_engine = nullptr;
    }
}

std::optional<int> MuPdfEngine::pageFromUri(const QString &uri) {
    if (uri.trimmed().isEmpty()) return std::nullopt;
    QRegularExpressionMatch m = pageParam().match(uri);
    if (!m.hasMatch()) m = pageFragment().match(uri);
    if (!m.hasMatch()) return std::nullopt;
    bool ok = false;
    const int n = m.captured(1).toInt(&ok);
    if (!ok) return std::nullopt;
    if (n - 1 < 0) return std::nullopt;
    return n - 1;
}

void MuPdfEngine::open(const QString &path, float fontScale, int startPage, const QString &userCss) {
    std::lock_guard lock(m_mutex);
    closeLocked();
    m_em = layoutEm(fontScale);
    m_startPage = startPage;
    m_userCss = userCss.toUtf8();
    pc_set_user_css(m_engine, m_userCss.constData());

    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) throw EngineError(QStringLiteral("OPEN"));
    const QByteArray head = f.read(1024);
    f.close();
    m_mime = DocumentMagic::pick(head, QString(), QFileInfo(path).fileName());

    const QByteArray pathUtf8 = path.toUtf8();
    const QByteArray mimeUtf8 = m_mime.toUtf8();
    const int rc = pc_open(m_engine, pathUtf8.constData(), mimeUtf8.isEmpty() ? nullptr : mimeUtf8.constData());
    if (rc == PC_FAIL) throw EngineError(QStringLiteral("OPEN"));
    if (rc == PC_PASSWORD) {
        m_awaitingPassword = true;
        throw EngineError(QStringLiteral("PASSWORD"));
    }
    m_awaitingPassword = false;
    finishOpenLocked();
}

bool MuPdfEngine::authenticatePassword(const QString &password) {
    std::lock_guard lock(m_mutex);
    if (!pc_is_open(m_engine)) return false;
    if (!pc_authenticate(m_engine, password.toUtf8().constData())) return false;
    m_awaitingPassword = false;
    finishOpenLocked();
    return true;
}

void MuPdfEngine::cancelPassword() {
    std::lock_guard lock(m_mutex);
    if (m_awaitingPassword) closeLocked();
}

void MuPdfEngine::relayout(float fontScale, const QString &userCss) {
    std::lock_guard lock(m_mutex);
    if (!m_engine || !pc_is_open(m_engine) || !pc_is_reflowable(m_engine)) return;
    m_em = layoutEm(fontScale);
    m_userCss = userCss.toUtf8();
    pc_set_user_css(m_engine, m_userCss.constData());
    finishOpenLocked();
}

bool MuPdfEngine::isReflowable() const {
    std::lock_guard lock(m_mutex);
    return m_engine && pc_is_reflowable(m_engine);
}

void MuPdfEngine::finishOpenLocked() {
    if (pc_is_reflowable(m_engine)) {
        pc_set_user_css(m_engine, m_userCss.constData());
        const float w = m_mime == QLatin1String(DocumentMagic::XLSX) ? kSpreadsheetWidth : kReflowWidth;
        const float h = m_mime == QLatin1String(DocumentMagic::XLSX) ? kSpreadsheetHeight : kReflowHeight;
        pc_layout(m_engine, w, h, m_em);
    }
    const int count = std::max(0, pc_page_count(m_engine));
    m_pages.clear();
    m_content.clear();
    m_outline.reset();
    m_measured = QVector<bool>(count, false);
    m_measuredCount = 0;
    if (count <= 0) return;
    const int start = std::clamp(m_startPage, 0, count - 1);
    const PageInfo seed = measureLocked(start);
    m_pages = QVector<PageInfo>(count, seed);
    m_measured[start] = true;
    ++m_measuredCount;
}

PageInfo MuPdfEngine::measureLocked(int index) {
    float b[4];
    if (!pc_page_bounds(m_engine, index, b)) throw EngineError(QStringLiteral("PAGE"));
    PageInfo info;
    info.width = b[2] - b[0];
    info.height = b[3] - b[1];
    info.x0 = b[0];
    info.y0 = b[1];
    return info;
}

void MuPdfEngine::ensureMeasuredLocked(int index) {
    if (index >= 0 && index < m_measured.size() && !m_measured[index]) {
        m_pages[index] = measureLocked(index);
        m_measured[index] = true;
        ++m_measuredCount;
    }
}

void MuPdfEngine::close() {
    std::lock_guard lock(m_mutex);
    closeLocked();
}

void MuPdfEngine::closeLocked() {
    m_content.clear();
    m_outline.reset();
    m_pages.clear();
    m_measured.clear();
    m_measuredCount = 0;
    if (m_engine) pc_close(m_engine);
    m_awaitingPassword = false;
}

bool MuPdfEngine::isOpened() const {
    std::lock_guard lock(m_mutex);
    return m_engine && pc_is_open(m_engine);
}

QString MuPdfEngine::mime() const {
    std::lock_guard lock(m_mutex);
    return m_mime;
}

QVector<PageInfo> MuPdfEngine::pages() const {
    std::lock_guard lock(m_mutex);
    return m_pages;
}

int MuPdfEngine::pageCount() const {
    std::lock_guard lock(m_mutex);
    return m_pages.size();
}

std::optional<PageInfo> MuPdfEngine::ensurePageSize(int index) {
    std::lock_guard lock(m_mutex);
    if (!pc_is_open(m_engine) || index < 0 || index >= m_pages.size()) return std::nullopt;
    ensureMeasuredLocked(index);
    return m_pages[index];
}

QVector<PageInfo> MuPdfEngine::measureRemainingPages() {
    std::lock_guard lock(m_mutex);
    if (!pc_is_open(m_engine)) return m_pages;
    if (m_measuredCount < m_pages.size()) {
        for (int i = 0; i < m_pages.size(); ++i) ensureMeasuredLocked(i);
    }
    return m_pages;
}

QImage MuPdfEngine::render(int index, int targetWidth, int fg, int bg, float imageBlend,
                           bool originalColors, float imageAlpha, int extraCw) {
    std::lock_guard lock(m_mutex);
    if (!pc_is_open(m_engine)) throw EngineError(QStringLiteral("No document"));
    ensureMeasuredLocked(index);
    if (index < 0 || index >= m_pages.size()) throw EngineError(QStringLiteral("No document"));
    const PageInfo orig = m_pages[index];
    const PageInfo info = displayPage(orig, extraCw);
    float usedScale = float(std::clamp(targetWidth, 1, 4096)) / std::max(info.width, 1.f);
    const int width0 = std::max(1, int(info.width * usedScale));
    const int height0 = std::max(1, int(info.height * usedScale));
    if (static_cast<long long>(width0) * height0 > kMaxRenderPixels) {
        usedScale *= float(std::sqrt(double(kMaxRenderPixels) / (double(width0) * double(height0))));
    }

    uint32_t *raw = nullptr;
    int w = 0, h = 0;
    if (!pc_render(m_engine, index, usedScale, -orig.x0 * usedScale, -orig.y0 * usedScale, &raw, &w, &h, extraCw))
        throw EngineError(QStringLiteral("RENDER"));

    QVector<int> pixels(w * h);
    std::memcpy(pixels.data(), raw, size_t(w) * size_t(h) * sizeof(int));
    pc_free_buf(raw);

    if (!originalColors) {
        QVector<PdfRect> imageRects;
        if (imageBlend > 0.001f || imageAlpha < 0.999f) {
            const PageContent c = contentLocked(index);
            imageRects.reserve(c.imageRects.size());
            for (const PdfRect &src : c.imageRects) {
                const PdfRect r = rotatePdfRect(src, orig, extraCw);
                imageRects.append({r.left * usedScale, r.top * usedScale, r.right * usedScale, r.bottom * usedScale});
            }
        }
        ColorRemapper::remap(pixels, w, h, fg, bg, imageBlend, imageRects, imageAlpha);
    }

    QImage image(w, h, QImage::Format_RGB32);
    for (int y = 0; y < h; ++y)
        std::memcpy(image.scanLine(y), pixels.constData() + size_t(y) * size_t(w), size_t(w) * sizeof(int));
    return image;
}

QVector<OutlineItem> MuPdfEngine::outline() {
    std::lock_guard lock(m_mutex);
    if (!pc_is_open(m_engine)) return {};
    if (m_outline) return *m_outline;
    QVector<OutlineItem> out;
    pc_outline_item *items = nullptr;
    int count = 0;
    if (pc_outline(m_engine, &items, &count)) {
        out.reserve(count);
        for (int i = 0; i < count; ++i) {
            const pc_outline_item &it = items[i];
            OutlineItem o;
            const QString title = QString::fromUtf8(it.title);
            o.title = title.trimmed().isEmpty() ? QStringLiteral("Untitled") : title;
            int page = it.page;
            if (page < 0) page = pageFromUri(QString::fromUtf8(it.uri ? it.uri : "")).value_or(-1);
            o.pageIndex = page;
            if (it.has_y) o.y = it.y;
            o.depth = it.depth;
            out.append(o);
        }
    }
    pc_outline_free(items, count);
    m_outline = out;
    return out;
}

std::optional<std::pair<int, std::optional<float>>> MuPdfEngine::resolveUri(const QString &uri) {
    std::lock_guard lock(m_mutex);
    if (!pc_is_open(m_engine)) return std::nullopt;
    if (const auto p = pageFromUri(uri)) {
        if (*p >= 0 && *p < m_pages.size()) return std::make_pair(*p, std::optional<float>());
    }
    const QByteArray u = uri.toUtf8();
    if (pc_is_external(m_engine, u.constData())) return std::nullopt;
    int page = -1, hasY = 0;
    float y = 0;
    if (!pc_resolve_uri(m_engine, u.constData(), &page, &hasY, &y)) return std::nullopt;
    return std::make_pair(page, hasY ? std::optional<float>(y) : std::nullopt);
}

PageContent MuPdfEngine::pageContent(int index) {
    std::lock_guard lock(m_mutex);
    return contentLocked(index);
}

PageContent MuPdfEngine::contentLocked(int index) {
    if (const auto it = m_content.constFind(index); it != m_content.constEnd()) return *it;
    if (!pc_is_open(m_engine)) return {};
    pc_content raw;
    if (!pc_content_load(m_engine, index, &raw)) throw EngineError(QStringLiteral("CONTENT"));
    PageContent c;
    c.chars.reserve(raw.nchars);
    for (int i = 0; i < raw.nchars; ++i) {
        const pc_char &s = raw.chars[i];
        CharBox b;
        b.c = QChar(s.c <= 0xFFFFu ? ushort(s.c) : ushort(0xFFFD));
        b.ulx = s.q[0];
        b.uly = s.q[1];
        b.urx = s.q[2];
        b.ury = s.q[3];
        b.llx = s.q[4];
        b.lly = s.q[5];
        b.lrx = s.q[6];
        b.lry = s.q[7];
        c.chars.append(b);
    }
    for (int i = 0; i < raw.nimages; ++i)
        c.imageRects.append({raw.images[i].r[0], raw.images[i].r[1], raw.images[i].r[2], raw.images[i].r[3]});
    for (int i = 0; i < raw.nlinks; ++i) {
        PageLink l;
        l.bounds = {raw.links[i].r[0], raw.links[i].r[1], raw.links[i].r[2], raw.links[i].r[3]};
        l.uri = QString::fromUtf8(raw.links[i].uri);
        l.isExternal = raw.links[i].external != 0;
        c.links.append(l);
    }
    pc_content_free(&raw);
    m_content.insert(index, c);
    return c;
}

} // namespace papercut
