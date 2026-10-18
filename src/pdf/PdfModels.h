#pragma once

#include <QString>
#include <QVector>
#include <algorithm>
#include <optional>
#include <utility>

namespace papercut {

struct PageInfo {
    float width = 0;
    float height = 0;
    float x0 = 0;
    float y0 = 0;
};

inline int normRotate(int deg) {
    deg %= 360;
    if (deg < 0) deg += 360;
    return deg;
}

inline PageInfo displayPage(const PageInfo &p, int extraCw) {
    extraCw = normRotate(extraCw);
    PageInfo o = p;
    if (extraCw == 90 || extraCw == 270) std::swap(o.width, o.height);
    if (extraCw != 0) {
        o.x0 = 0;
        o.y0 = 0;
    }
    return o;
}

struct XY {
    float x = 0;
    float y = 0;
};

inline XY rotateContentPoint(float x, float y, const PageInfo &orig, int extraCw) {
    const float lx = x - orig.x0, ly = y - orig.y0;
    switch (normRotate(extraCw)) {
    case 90: return {orig.height - ly, lx};
    case 180: return {orig.width - lx, orig.height - ly};
    case 270: return {ly, orig.width - lx};
    default: return {lx, ly};
    }
}

struct PdfRect {
    float left = 0;
    float top = 0;
    float right = 0;
    float bottom = 0;
};

struct CharBox {
    QChar c;
    float ulx = 0, uly = 0, urx = 0, ury = 0;
    float llx = 0, lly = 0, lrx = 0, lry = 0;

    bool contains(float x, float y) const {
        const float minX = std::min({ulx, urx, llx, lrx});
        const float maxX = std::max({ulx, urx, llx, lrx});
        const float minY = std::min({uly, ury, lly, lry});
        const float maxY = std::max({uly, ury, lly, lry});
        return x >= minX && x <= maxX && y >= minY && y <= maxY;
    }
};

struct PageLink {
    PdfRect bounds;
    QString uri;
    bool isExternal = false;
};

struct OutlineItem {
    QString title;
    int pageIndex = 0;
    std::optional<float> y;
    int depth = 0;
};

struct PageContent {
    QVector<CharBox> chars;
    QVector<PdfRect> imageRects;
    QVector<PageLink> links;
};

inline CharBox rotateCharBox(const CharBox &b, const PageInfo &orig, int extraCw) {
    const XY ul = rotateContentPoint(b.ulx, b.uly, orig, extraCw);
    const XY ur = rotateContentPoint(b.urx, b.ury, orig, extraCw);
    const XY ll = rotateContentPoint(b.llx, b.lly, orig, extraCw);
    const XY lr = rotateContentPoint(b.lrx, b.lry, orig, extraCw);
    CharBox o = b;
    o.ulx = ul.x;
    o.uly = ul.y;
    o.urx = ur.x;
    o.ury = ur.y;
    o.llx = ll.x;
    o.lly = ll.y;
    o.lrx = lr.x;
    o.lry = lr.y;
    return o;
}

inline PdfRect rotatePdfRect(const PdfRect &r, const PageInfo &orig, int extraCw) {
    const XY a = rotateContentPoint(r.left, r.top, orig, extraCw);
    const XY b = rotateContentPoint(r.right, r.bottom, orig, extraCw);
    const XY c = rotateContentPoint(r.left, r.bottom, orig, extraCw);
    const XY d = rotateContentPoint(r.right, r.top, orig, extraCw);
    return {std::min({a.x, b.x, c.x, d.x}), std::min({a.y, b.y, c.y, d.y}),
            std::max({a.x, b.x, c.x, d.x}), std::max({a.y, b.y, c.y, d.y})};
}

inline PageContent rotatePageContent(const PageContent &c, const PageInfo &orig, int extraCw) {
    if (normRotate(extraCw) == 0) return c;
    PageContent o;
    o.chars.reserve(c.chars.size());
    for (const CharBox &b : c.chars) o.chars.append(rotateCharBox(b, orig, extraCw));
    o.imageRects.reserve(c.imageRects.size());
    for (const PdfRect &r : c.imageRects) o.imageRects.append(rotatePdfRect(r, orig, extraCw));
    o.links.reserve(c.links.size());
    for (PageLink link : c.links) {
        link.bounds = rotatePdfRect(link.bounds, orig, extraCw);
        o.links.append(link);
    }
    return o;
}

struct SearchHit {
    int startPage = 0;
    int startIndex = 0;
    int endPage = 0;
    int endIndex = 0;
    QString snippet;
};

} // namespace papercut
