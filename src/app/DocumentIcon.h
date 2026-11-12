#pragma once

#include <QString>
#include <QUrl>

namespace papercut {

struct ViewerSettings;

// Per-user (HKCU) advertisement of Papercut's own ProgId / Applications key.
// Does not rewrite other apps' .pdf DefaultIcon or HKLM / UserChoice.
struct DocumentIcon {
    static constexpr const char kResource[] = ":/icons/papercut-document.png";
    static constexpr const char kProgId[] = "Papercut.Document";

    static QString icoPath();
    static QUrl previewUrl(const QString &customPath);
    static QString displayLabel(const QString &customPath);
    static QString importCustom(const QString &sourcePath);
    static void syncAssociation(const ViewerSettings &s);
};

} // namespace papercut
