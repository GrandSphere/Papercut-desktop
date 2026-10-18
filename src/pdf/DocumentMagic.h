#pragma once

#include <QByteArray>
#include <QString>
#include <QStringList>

namespace papercut {

class DocumentMagic {
public:
    static constexpr const char PDF[] = "application/pdf";
    static constexpr const char EPUB[] = "application/epub+zip";
    static constexpr const char XPS[] = "application/vnd.ms-xpsdocument";
    static constexpr const char OXPS[] = "application/oxps";
    static constexpr const char CBZ[] = "application/vnd.comicbook+zip";
    static constexpr const char CBZ_ALT[] = "application/x-cbz";
    static constexpr const char JPEG[] = "image/jpeg";
    static constexpr const char PNG[] = "image/png";
    static constexpr const char GIF[] = "image/gif";
    static constexpr const char TIFF[] = "image/tiff";
    static constexpr const char BMP[] = "image/bmp";
    static constexpr const char WEBP[] = "image/webp";
    static constexpr const char SVG[] = "image/svg+xml";
    static constexpr const char ZIP[] = "application/zip";
    static constexpr const char TXT[] = "text/plain";
    static constexpr const char MARKDOWN[] = "text/markdown";
    static constexpr const char HTML[] = "text/html";
    static constexpr const char DOCX[] =
        "application/vnd.openxmlformats-officedocument.wordprocessingml.document";
    static constexpr const char XLSX[] =
        "application/vnd.openxmlformats-officedocument.spreadsheetml.sheet";
    static constexpr const char PPTX[] =
        "application/vnd.openxmlformats-officedocument.presentationml.presentation";
    static constexpr const char HWPX[] = "application/vnd.hancom.hwpx";
    static constexpr const char FB2[] = "application/x-fictionbook+xml";
    static constexpr const char MOBI[] = "application/x-mobipocket-ebook";

    static QString shareType(const QString &magic);
    static QString extensionFor(const QString &magic);
    static QString pick(const QByteArray &head, const QString &mime, const QString &name);
    static QString fromHeader(const QByteArray &head);
    static QString fromName(const QString &name);
    static QString fromMime(const QString &mime);
};

} // namespace papercut
