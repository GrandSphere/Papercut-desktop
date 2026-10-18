#include "DocumentPaths.h"

#include <QDir>
#include <QFileInfo>
#include <QRegularExpression>
#include <QUrl>

namespace papercut {
namespace {

const QRegularExpression &downloadDir() {
    static const QRegularExpression re(QStringLiteral("(?i)^Download/"));
    return re;
}
const QRegularExpression &downloadOnly() {
    static const QRegularExpression re(QStringLiteral("(?i)^Download$"));
    return re;
}
const QRegularExpression &sdRoot() {
    static const QRegularExpression re(QStringLiteral("(?i)^/?storage/[0-9A-F]{4}-[0-9A-F]{4}/"));
    return re;
}
const QRegularExpression &sdMid() {
    static const QRegularExpression re(QStringLiteral("(?i)(?:^|/)storage/[0-9A-F]{4}-[0-9A-F]{4}/"));
    return re;
}

bool looksLikeFileName(const QString &value) {
    const QString t = value.trimmed();
    return !t.isEmpty() && !t.contains(QLatin1Char('/')) && !DocumentPaths::isUgly(t);
}

QString fileNameOf(const QString &value) {
    QString cleaned = DocumentPaths::fullyDecode(value);
    cleaned.replace(QLatin1Char('\\'), QLatin1Char('/'));
    cleaned = cleaned.section(QLatin1Char('/'), -1);
    cleaned = cleaned.section(QLatin1Char(':'), -1);
    return looksLikeFileName(cleaned) ? cleaned : QString();
}

QString stripStorageRoot(QString path) {
    path = path.trimmed();
    while (path.startsWith(QLatin1Char('/'))) path.remove(0, 1);
    const QStringList markers{
        QStringLiteral("storage/emulated/0/"),
        QStringLiteral("/storage/emulated/0/"),
        QStringLiteral("sdcard/"),
        QStringLiteral("/sdcard/"),
    };
    for (const QString &marker : markers) {
        QString needle = marker;
        while (needle.startsWith(QLatin1Char('/'))) needle.remove(0, 1);
        const int idx = path.indexOf(needle, 0, Qt::CaseInsensitive);
        if (idx >= 0)
            return path.mid(idx + needle.size());
    }
    auto m = sdRoot().match(path);
    if (m.hasMatch())
        return path.mid(m.capturedEnd());
    m = sdMid().match(path);
    if (m.hasMatch())
        return path.mid(m.capturedEnd());
    return path;
}

QString folderOf(const QString &raw, const QString &name) {
    QString path = DocumentPaths::fullyDecode(raw);
    path.replace(QLatin1Char('\\'), QLatin1Char('/'));
    path = path.section(QStringLiteral("://"), -1);
    if (path.contains(QLatin1Char(':')) && !path.startsWith(QLatin1Char('/'))) {
        const QString after = path.section(QLatin1Char(':'), 1);
        if (!after.isEmpty()) path = after;
    }
    path = stripStorageRoot(path);
    while (path.startsWith(QLatin1Char('/'))) path.remove(0, 1);
    while (path.endsWith(QLatin1Char('/'))) path.chop(1);
    if (auto m = downloadDir().match(path); m.hasMatch())
        path.replace(m.capturedStart(), m.capturedLength(), QStringLiteral("Downloads/"));
    if (auto m = downloadOnly().match(path); m.hasMatch())
        path.replace(m.capturedStart(), m.capturedLength(), QStringLiteral("Downloads"));
    if (path.isEmpty() || DocumentPaths::isUgly(path)) return {};
    const QString last = path.section(QLatin1Char('/'), -1);
    if (looksLikeFileName(name) && last.compare(name, Qt::CaseInsensitive) == 0) {
        path = path.contains(QLatin1Char('/')) ? path.section(QLatin1Char('/'), 0, -2) : QString();
    } else if (looksLikeFileName(last) && last.contains(QLatin1Char('.'))) {
        path = path.contains(QLatin1Char('/')) ? path.section(QLatin1Char('/'), 0, -2) : QString();
    }
    while (path.startsWith(QLatin1Char('/'))) path.remove(0, 1);
    while (path.endsWith(QLatin1Char('/'))) path.chop(1);
    if (path.isEmpty() || path.compare(name, Qt::CaseInsensitive) == 0 || DocumentPaths::isUgly(path))
        return {};
    return path;
}

QString fileNameFromUrl(const QUrl &uri) {
    const QString fromSeg = fileNameOf(DocumentPaths::fullyDecode(uri.fileName()));
    if (!fromSeg.isEmpty()) return fromSeg;
    const QString fromPath = fileNameOf(DocumentPaths::fullyDecode(uri.path()));
    if (!fromPath.isEmpty()) return fromPath;
    return fileNameOf(DocumentPaths::fullyDecode(uri.toString()));
}

} // namespace

QString DocumentPaths::fullyDecode(const QString &text) {
    QString current = text;
    for (int i = 0; i < 4; ++i) {
        if (!current.contains(QLatin1Char('%'))) return current;
        const QString next = QUrl::fromPercentEncoding(current.toUtf8());
        if (next == current) return current;
        current = next;
    }
    return current;
}

bool DocumentPaths::isUgly(const QString &value) {
    const QString t = value.trimmed();
    if (t.isEmpty()) return true;
    if (t.contains(QLatin1Char('%'))) return true;
    if (t.contains(QStringLiteral("://"))) return true;
    if (t.startsWith(QStringLiteral("content:"), Qt::CaseInsensitive)) return true;
    if (t.startsWith(QStringLiteral("file:"), Qt::CaseInsensitive)) return true;
    if (t.contains(QStringLiteral("com.android.providers"))) return true;
    if (t.contains(QStringLiteral("/storage/emulated/"), Qt::CaseInsensitive)) return true;
    if (t.startsWith(QStringLiteral("/storage/"), Qt::CaseInsensitive)) return true;
    return false;
}

bool DocumentPaths::needsRebuild(const QString &path, const QString &name) {
    if (isUgly(path)) return true;
    const QString folder = folderOf(path, name);
    if (folder.isEmpty()) return true;
    if (folder.compare(name, Qt::CaseInsensitive) == 0) return true;
    return false;
}

PathLabel DocumentPaths::label(const QString &filePath) {
    const QFileInfo info(filePath);
    const QString name = looksLikeFileName(info.fileName()) ? info.fileName() : QStringLiteral("document.pdf");
    const QString folder = folderOf(QDir::toNativeSeparators(info.absolutePath()), name);
    const QString path = folder.isEmpty() ? info.absolutePath() : folder;
    return {name, isUgly(path) ? QString() : path};
}

PathLabel DocumentPaths::fromUriString(const QString &uriString, const QString &storedName,
                                       const QString &storedPath) {
    const QUrl uri(fullyDecode(uriString));
    QString name;
    if (!storedName.isEmpty() && !isUgly(storedName) && looksLikeFileName(storedName))
        name = storedName;
    else if (uri.isValid())
        name = fileNameFromUrl(uri);
    if (name.isEmpty())
        name = fileNameOf(fullyDecode(storedName.isEmpty() ? uriString : storedName));
    if (name.isEmpty())
        name = QStringLiteral("document.pdf");

    QString path;
    if (!storedPath.isEmpty() && !needsRebuild(storedPath, name))
        path = folderOf(storedPath, name);
    if (path.isEmpty() && uri.isValid() && uri.isLocalFile())
        path = folderOf(uri.toLocalFile(), name);
    if (path.isEmpty())
        path = folderOf(fullyDecode(uriString), name);
    return {name, path};
}

} // namespace papercut
