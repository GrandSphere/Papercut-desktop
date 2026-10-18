#include "DocumentMagic.h"

#include <QFileInfo>
#include <cstring>
#include <initializer_list>

namespace papercut {
namespace {

bool startsWith(const QByteArray &head, int minSize, std::initializer_list<int> prefix) {
    if (head.size() < minSize) return false;
    int i = 0;
    for (int b : prefix) {
        if (static_cast<unsigned char>(head[i]) != static_cast<unsigned char>(b))
            return false;
        ++i;
    }
    return true;
}

int asciiLower(int c) {
    return (c >= 'A' && c <= 'Z') ? c + 32 : c;
}

bool startsWithCi(const QByteArray &head, int off, const char *ascii) {
    const int n = int(std::strlen(ascii));
    if (off < 0 || head.size() - off < n) return false;
    for (int i = 0; i < n; ++i) {
        if (asciiLower(static_cast<unsigned char>(head[off + i])) !=
            asciiLower(static_cast<unsigned char>(ascii[i])))
            return false;
    }
    return true;
}

bool containsCi(const QByteArray &head, const char *ascii) {
    const int n = int(std::strlen(ascii));
    if (n <= 0 || head.size() < n) return false;
    for (int i = 0; i <= head.size() - n; ++i) {
        if (startsWithCi(head, i, ascii)) return true;
    }
    return false;
}

int skipBomWs(const QByteArray &head) {
    int i = 0;
    if (head.size() >= 3 && startsWith(head, 3, {0xEF, 0xBB, 0xBF})) i = 3;
    while (i < head.size()) {
        const unsigned char c = static_cast<unsigned char>(head[i]);
        if (c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\f') ++i;
        else break;
    }
    return i;
}

bool looksLikeText(const QByteArray &head) {
    if (head.isEmpty()) return false;
    int odd = 0;
    for (int i = 0; i < head.size(); ++i) {
        const unsigned char c = static_cast<unsigned char>(head[i]);
        if (c == 0) return false;
        if (c < 32 && c != '\t' && c != '\n' && c != '\r' && c != '\f') ++odd;
    }
    return odd * 20 < head.size();
}

bool isZipDocumentHint(const QString &hint) {
    return hint == QLatin1String(DocumentMagic::EPUB) || hint == QLatin1String(DocumentMagic::XPS) ||
           hint == QLatin1String(DocumentMagic::OXPS) || hint == QLatin1String(DocumentMagic::CBZ) ||
           hint == QLatin1String(DocumentMagic::DOCX) || hint == QLatin1String(DocumentMagic::XLSX) ||
           hint == QLatin1String(DocumentMagic::PPTX) || hint == QLatin1String(DocumentMagic::HWPX);
}

} // namespace

QString DocumentMagic::shareType(const QString &magic) {
    if (magic == QLatin1String(CBZ_ALT) || magic == QLatin1String(ZIP))
        return QLatin1String(CBZ);
    return magic;
}

QString DocumentMagic::extensionFor(const QString &magic) {
    const QString t = fromMime(magic);
    if (t == QLatin1String(PDF)) return QStringLiteral("pdf");
    if (t == QLatin1String(EPUB)) return QStringLiteral("epub");
    if (t == QLatin1String(XPS)) return QStringLiteral("xps");
    if (t == QLatin1String(OXPS)) return QStringLiteral("oxps");
    if (t == QLatin1String(CBZ) || t == QLatin1String(CBZ_ALT) || t == QLatin1String(ZIP))
        return QStringLiteral("cbz");
    if (t == QLatin1String(JPEG)) return QStringLiteral("jpg");
    if (t == QLatin1String(PNG)) return QStringLiteral("png");
    if (t == QLatin1String(GIF)) return QStringLiteral("gif");
    if (t == QLatin1String(TIFF)) return QStringLiteral("tif");
    if (t == QLatin1String(BMP)) return QStringLiteral("bmp");
    if (t == QLatin1String(WEBP)) return QStringLiteral("webp");
    if (t == QLatin1String(SVG)) return QStringLiteral("svg");
    if (t == QLatin1String(TXT)) return QStringLiteral("txt");
    if (t == QLatin1String(MARKDOWN)) return QStringLiteral("md");
    if (t == QLatin1String(HTML)) return QStringLiteral("html");
    if (t == QLatin1String(DOCX)) return QStringLiteral("docx");
    if (t == QLatin1String(XLSX)) return QStringLiteral("xlsx");
    if (t == QLatin1String(PPTX)) return QStringLiteral("pptx");
    if (t == QLatin1String(HWPX)) return QStringLiteral("hwpx");
    if (t == QLatin1String(FB2)) return QStringLiteral("fb2");
    if (t == QLatin1String(MOBI)) return QStringLiteral("mobi");
    return QStringLiteral("bin");
}

QString DocumentMagic::fromHeader(const QByteArray &head) {
    if (startsWith(head, 5, {0x25, 0x50, 0x44, 0x46})) return QLatin1String(PDF);
    if (startsWith(head, 3, {0xFF, 0xD8, 0xFF})) return QLatin1String(JPEG);
    if (startsWith(head, 8, {0x89, 0x50, 0x4E, 0x47})) return QLatin1String(PNG);
    if (startsWith(head, 6, {0x47, 0x49, 0x46, 0x38, 0x37, 0x61}) ||
        startsWith(head, 6, {0x47, 0x49, 0x46, 0x38, 0x39, 0x61}))
        return QLatin1String(GIF);
    if (startsWith(head, 4, {0x49, 0x49, 0x2A, 0x00}) || startsWith(head, 4, {0x4D, 0x4D, 0x00, 0x2A}))
        return QLatin1String(TIFF);
    if (startsWith(head, 2, {0x42, 0x4D})) return QLatin1String(BMP);
    if (startsWith(head, 4, {0x52, 0x49, 0x46, 0x46}) && head.size() >= 12 &&
        startsWithCi(head, 8, "WEBP"))
        return QLatin1String(WEBP);
    if (startsWith(head, 2, {0xFF, 0xFE}) || startsWith(head, 2, {0xFE, 0xFF}))
        return QLatin1String(TXT);
    if (startsWith(head, 4, {0x50, 0x4B, 0x03, 0x04})) {
        const QString s = QString::fromLatin1(head);
        if (s.contains(QLatin1String("application/epub+zip")) ||
            s.contains(QLatin1String("mimetypeapplication/epub"))) {
            return QLatin1String(EPUB);
        }
        if (s.contains(QLatin1String("spreadsheetml")) || s.contains(QLatin1String("xl/")))
            return QLatin1String(XLSX);
        if (s.contains(QLatin1String("wordprocessingml")) || s.contains(QLatin1String("word/")))
            return QLatin1String(DOCX);
        if (s.contains(QLatin1String("presentationml")) || s.contains(QLatin1String("ppt/")))
            return QLatin1String(PPTX);
        if (s.contains(QLatin1String("FixedDocument")) ||
            s.contains(QLatin1String("vnd.ms-package.xps"))) {
            return QLatin1String(XPS);
        }
        return QLatin1String(ZIP);
    }
    const int off = skipBomWs(head);
    if (startsWithCi(head, off, "<svg") || (startsWithCi(head, off, "<?xml") && containsCi(head, "<svg")))
        return QLatin1String(SVG);
    if (startsWithCi(head, off, "<!doctype html") || startsWithCi(head, off, "<html"))
        return QLatin1String(HTML);
    return {};
}

QString DocumentMagic::fromName(const QString &name) {
    const QString ext = QFileInfo(name).suffix().trimmed().toLower();
    if (ext.isEmpty()) return {};
    if (ext == QLatin1String("pdf")) return QLatin1String(PDF);
    if (ext == QLatin1String("epub")) return QLatin1String(EPUB);
    if (ext == QLatin1String("xps")) return QLatin1String(XPS);
    if (ext == QLatin1String("oxps")) return QLatin1String(OXPS);
    if (ext == QLatin1String("cbz")) return QLatin1String(CBZ);
    if (ext == QLatin1String("zip")) return QLatin1String(ZIP);
    if (ext == QLatin1String("jpg") || ext == QLatin1String("jpeg")) return QLatin1String(JPEG);
    if (ext == QLatin1String("png")) return QLatin1String(PNG);
    if (ext == QLatin1String("gif")) return QLatin1String(GIF);
    if (ext == QLatin1String("tif") || ext == QLatin1String("tiff")) return QLatin1String(TIFF);
    if (ext == QLatin1String("bmp")) return QLatin1String(BMP);
    if (ext == QLatin1String("webp")) return QLatin1String(WEBP);
    if (ext == QLatin1String("svg")) return QLatin1String(SVG);
    if (ext == QLatin1String("txt") || ext == QLatin1String("text") || ext == QLatin1String("log"))
        return QLatin1String(TXT);
    if (ext == QLatin1String("md") || ext == QLatin1String("markdown")) return QLatin1String(MARKDOWN);
    if (ext == QLatin1String("html") || ext == QLatin1String("htm") || ext == QLatin1String("xhtml"))
        return QLatin1String(HTML);
    if (ext == QLatin1String("docx")) return QLatin1String(DOCX);
    if (ext == QLatin1String("xlsx")) return QLatin1String(XLSX);
    if (ext == QLatin1String("pptx")) return QLatin1String(PPTX);
    if (ext == QLatin1String("hwpx")) return QLatin1String(HWPX);
    if (ext == QLatin1String("fb2")) return QLatin1String(FB2);
    if (ext == QLatin1String("mobi") || ext == QLatin1String("azw") || ext == QLatin1String("azw3"))
        return QLatin1String(MOBI);
    return {};
}

QString DocumentMagic::fromMime(const QString &mime) {
    const QString t = mime.trimmed().toLower();
    if (t.isEmpty() || t == QLatin1String("application/octet-stream")) return {};
    if (t == QLatin1String(PDF) || t.startsWith(QLatin1String("application/pdf")))
        return QLatin1String(PDF);
    if (t == QLatin1String(EPUB)) return QLatin1String(EPUB);
    if (t == QLatin1String(XPS) || t == QLatin1String(OXPS)) return t;
    if (t == QLatin1String(CBZ) || t == QLatin1String(CBZ_ALT)) return QLatin1String(CBZ);
    if (t == QLatin1String(ZIP)) return QLatin1String(ZIP);
    if (t == QLatin1String(JPEG) || t == QLatin1String("image/jpg")) return QLatin1String(JPEG);
    if (t == QLatin1String(PNG)) return QLatin1String(PNG);
    if (t == QLatin1String(GIF)) return QLatin1String(GIF);
    if (t == QLatin1String(TIFF) || t == QLatin1String("image/tif")) return QLatin1String(TIFF);
    if (t == QLatin1String(BMP) || t == QLatin1String("image/x-ms-bmp")) return QLatin1String(BMP);
    if (t == QLatin1String(WEBP)) return QLatin1String(WEBP);
    if (t == QLatin1String(SVG)) return QLatin1String(SVG);
    if (t == QLatin1String(TXT) || t.startsWith(QLatin1String("text/plain")))
        return QLatin1String(TXT);
    if (t == QLatin1String(MARKDOWN) || t == QLatin1String("text/x-markdown"))
        return QLatin1String(MARKDOWN);
    if (t == QLatin1String(HTML) || t == QLatin1String("application/xhtml+xml") ||
        t.startsWith(QLatin1String("text/html")))
        return QLatin1String(HTML);
    if (t == QLatin1String(DOCX)) return QLatin1String(DOCX);
    if (t == QLatin1String(XLSX)) return QLatin1String(XLSX);
    if (t == QLatin1String(PPTX)) return QLatin1String(PPTX);
    if (t == QLatin1String(HWPX) || t == QLatin1String("application/haansofthwpx"))
        return QLatin1String(HWPX);
    if (t == QLatin1String(FB2) || t == QLatin1String("application/fictionbook+xml"))
        return QLatin1String(FB2);
    if (t == QLatin1String(MOBI) || t == QLatin1String("application/vnd.amazon.ebook"))
        return QLatin1String(MOBI);
    return t;
}

QString DocumentMagic::pick(const QByteArray &head, const QString &mime, const QString &name) {
    const QString fromHead = fromHeader(head);
    const QString fromN = fromName(name);
    const QString fromM = fromMime(mime);
    if (!fromHead.isEmpty() && fromHead != QLatin1String(ZIP)) return fromHead;
    if (fromHead == QLatin1String(ZIP)) {
        const QString hint = !fromN.isEmpty() ? fromN : fromM;
        if (isZipDocumentHint(hint)) return hint;
        return QLatin1String(CBZ);
    }
    if (!fromN.isEmpty()) return fromN;
    if (!fromM.isEmpty()) return fromM;
    if (!fromHead.isEmpty()) return fromHead;
    if (looksLikeText(head)) return QLatin1String(TXT);
    return {};
}

} // namespace papercut
