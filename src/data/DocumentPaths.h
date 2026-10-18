#pragma once

#include <QString>
#include <QUrl>
#include <optional>

namespace papercut {

struct PathLabel {
    QString displayName;
    QString displayPath;
};

class DocumentPaths {
public:
    static PathLabel label(const QString &filePath);
    static PathLabel fromUriString(const QString &uriString,
                                   const QString &storedName = {},
                                   const QString &storedPath = {});
    static bool isUgly(const QString &value);
    static bool needsRebuild(const QString &path, const QString &name);
    static QString fullyDecode(const QString &text);
};

} // namespace papercut
