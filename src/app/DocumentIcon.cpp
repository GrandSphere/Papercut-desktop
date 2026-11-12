#include "DocumentIcon.h"

#include "core/AppPaths.h"
#include "data/ViewerSettings.h"

#include <QBuffer>
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QIODevice>
#include <QImage>
#include <QList>
#include <QPainter>
#include <QStandardPaths>
#include <QStringList>
#include <QUrl>

#ifdef Q_OS_WIN
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <shlobj.h>
#include <string>
#elif !defined(Q_OS_MACOS)
#include <QProcess>
#include <QTextStream>
#endif

namespace papercut {
namespace {

QImage loadIconImage(const QString &source) {
    QImage img(source);
    return img;
}

#if !defined(Q_OS_MACOS)
QImage squareIcon(const QImage &src, int size) {
    QImage square(size, size, QImage::Format_ARGB32);
    square.fill(Qt::transparent);
    const QImage scaled = src.scaled(size, size, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    QPainter p(&square);
    p.drawImage((size - scaled.width()) / 2, (size - scaled.height()) / 2, scaled);
    p.end();
    return square;
}

QString associatedExe() {
    const QFileInfo bin(QCoreApplication::applicationFilePath());
#ifdef Q_OS_WIN
    const QFileInfo launch(bin.dir().filePath(QStringLiteral("../papercut.exe")));
#else
    const QFileInfo launch(bin.dir().filePath(QStringLiteral("../papercut")));
#endif
    if (launch.exists()) {
        const QString a = launch.canonicalFilePath();
        const QString b = bin.canonicalFilePath();
        if (!a.isEmpty() && a.compare(b, Qt::CaseInsensitive) != 0)
            return QDir::toNativeSeparators(a);
    }
    return QDir::toNativeSeparators(bin.canonicalFilePath());
}

QString iconSource(const ViewerSettings &s) {
    if (!s.documentIconPath.trimmed().isEmpty() && QFileInfo::exists(s.documentIconPath))
        return s.documentIconPath;
    return QString::fromLatin1(DocumentIcon::kResource);
}
#endif

#ifdef Q_OS_WIN

void put16(QByteArray &b, quint16 v) {
    b.append(char(v & 0xff));
    b.append(char((v >> 8) & 0xff));
}

void put32(QByteArray &b, quint32 v) {
    b.append(char(v & 0xff));
    b.append(char((v >> 8) & 0xff));
    b.append(char((v >> 16) & 0xff));
    b.append(char((v >> 24) & 0xff));
}

bool writeIcoFromImage(const QString &dest, const QImage &src) {
    if (src.isNull()) return false;
    static const int kSizes[] = {16, 24, 32, 48, 64, 128, 256};
    QList<QByteArray> pngs;
    pngs.reserve(7);
    for (int s : kSizes) {
        QByteArray out;
        QBuffer buf(&out);
        buf.open(QIODevice::WriteOnly);
        squareIcon(src, s).save(&buf, "PNG");
        pngs.append(out);
    }

    QByteArray ico;
    put16(ico, 0);
    put16(ico, 1);
    put16(ico, quint16(pngs.size()));
    int offset = 6 + 16 * pngs.size();
    for (int i = 0; i < pngs.size(); ++i) {
        const int dim = kSizes[i];
        ico.append(char(dim >= 256 ? 0 : dim));
        ico.append(char(dim >= 256 ? 0 : dim));
        ico.append(char(0));
        ico.append(char(0));
        put16(ico, 1);
        put16(ico, 32);
        put32(ico, quint32(pngs[i].size()));
        put32(ico, quint32(offset));
        offset += pngs[i].size();
    }
    for (const QByteArray &png : pngs) ico.append(png);

    QDir().mkpath(QFileInfo(dest).absolutePath());
    QFile f(dest);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate)) return false;
    return f.write(ico) == ico.size();
}

bool writeIco(const QString &dest, const QString &source) {
    if (source.endsWith(QLatin1String(".ico"), Qt::CaseInsensitive) && QFile::exists(source)
        && QFileInfo(source).canonicalFilePath() != QFileInfo(dest).canonicalFilePath()) {
        QFile::remove(dest);
        if (QFile::copy(source, dest)) return true;
    }
    const QImage img = loadIconImage(source);
    if (img.isNull()) return false;
    return writeIcoFromImage(dest, img);
}

void writeSz(const QString &sub, const wchar_t *name, const QString &value) {
    HKEY key = nullptr;
    if (RegCreateKeyExW(HKEY_CURRENT_USER, reinterpret_cast<LPCWSTR>(sub.utf16()), 0, nullptr,
                        REG_OPTION_NON_VOLATILE, KEY_SET_VALUE, nullptr, &key, nullptr)
        != ERROR_SUCCESS)
        return;
    const std::wstring w = value.toStdWString();
    RegSetValueExW(key, name, 0, REG_SZ, reinterpret_cast<const BYTE *>(w.c_str()),
                   DWORD((w.size() + 1) * sizeof(wchar_t)));
    RegCloseKey(key);
}

void deleteTree(const QString &sub) {
    RegDeleteTreeW(HKEY_CURRENT_USER, reinterpret_cast<LPCWSTR>(sub.utf16()));
}

bool keyExists(const QString &sub) {
    HKEY key = nullptr;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, reinterpret_cast<LPCWSTR>(sub.utf16()), 0, KEY_READ, &key)
        != ERROR_SUCCESS)
        return false;
    RegCloseKey(key);
    return true;
}

const QStringList &documentExts() {
    static const QStringList k{QStringLiteral(".pdf"), QStringLiteral(".epub"), QStringLiteral(".xps"),
                               QStringLiteral(".oxps"), QStringLiteral(".cbz"), QStringLiteral(".fb2"),
                               QStringLiteral(".mobi")};
    return k;
}

void notifyAssocChanged() { SHChangeNotify(SHCNE_ASSOCCHANGED, SHCNF_IDLIST, nullptr, nullptr); }

void advertiseWindows(const QString &exe, const QString &icoNative, bool useIcon) {
    const QString prog = QStringLiteral("Software\\Classes\\") + QLatin1String(DocumentIcon::kProgId);
    const QString app = QStringLiteral("Software\\Classes\\Applications\\") + QFileInfo(exe).fileName();
    const QString progIcon = prog + QStringLiteral("\\DefaultIcon");
    const QString appIcon = app + QStringLiteral("\\DefaultIcon");
    const bool hadIcon = keyExists(progIcon) || keyExists(appIcon);

    if (!useIcon) {
        if (!hadIcon) return;
        deleteTree(progIcon);
        deleteTree(appIcon);
        notifyAssocChanged();
        return;
    }

    writeSz(prog, nullptr, QStringLiteral("Papercut document"));
    writeSz(prog + QStringLiteral("\\shell\\open\\command"), nullptr,
            QStringLiteral("\"%1\" \"%2\"").arg(exe, QStringLiteral("%1")));
    writeSz(progIcon, nullptr, icoNative + QStringLiteral(",0"));
    for (const QString &ext : documentExts()) {
        writeSz(QStringLiteral("Software\\Classes\\") + ext + QStringLiteral("\\OpenWithProgids"),
                L"Papercut.Document", QString());
    }

    writeSz(app, L"FriendlyAppName", QStringLiteral("Papercut"));
    writeSz(app + QStringLiteral("\\shell\\open\\command"), nullptr,
            QStringLiteral("\"%1\" \"%2\"").arg(exe, QStringLiteral("%1")));
    writeSz(appIcon, nullptr, icoNative + QStringLiteral(",0"));
    for (const QString &ext : documentExts())
        writeSz(app + QStringLiteral("\\SupportedTypes"), reinterpret_cast<const wchar_t *>(ext.utf16()), QString());

    notifyAssocChanged();
}

#elif !defined(Q_OS_MACOS)

const QStringList &documentMimes() {
    static const QStringList k{
        QStringLiteral("application/pdf"),
        QStringLiteral("application/epub+zip"),
        QStringLiteral("application/vnd.ms-xpsdocument"),
        QStringLiteral("application/oxps"),
        QStringLiteral("application/vnd.comicbook+zip"),
        QStringLiteral("application/x-cbz"),
        QStringLiteral("application/x-fictionbook+xml"),
        QStringLiteral("application/x-mobipocket-ebook")};
    return k;
}

QString desktopPath() {
    const QString apps = QStandardPaths::writableLocation(QStandardPaths::ApplicationsLocation);
    QDir().mkpath(apps);
    return QDir(apps).filePath(QStringLiteral("papercut.desktop"));
}

QString pngPath() { return QDir(AppPaths::configRoot()).filePath(QStringLiteral("document.png")); }

bool writePng(const QString &dest, const QString &source) {
    const QImage img = loadIconImage(source);
    if (img.isNull()) return false;
    QDir().mkpath(QFileInfo(dest).absolutePath());
    QFile::remove(dest);
    return squareIcon(img, 256).save(dest, "PNG");
}

void advertiseXdg(const QString &exe, const QString &png, bool useIcon) {
    QFile f(desktopPath());
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) return;
    QTextStream out(&f);
    out << QStringLiteral("[Desktop Entry]\n");
    out << QStringLiteral("Type=Application\n");
    out << QStringLiteral("Name=Papercut\n");
    out << QStringLiteral("Exec=\"%1\" %f\n").arg(exe);
    out << QStringLiteral("Terminal=false\n");
    out << QStringLiteral("Categories=Office;Viewer;\n");
    out << QStringLiteral("StartupWMClass=papercut\n");
    out << QStringLiteral("MimeType=%1;\n").arg(documentMimes().join(QLatin1Char(';')));
    if (useIcon && !png.isEmpty()) out << QStringLiteral("Icon=%1\n").arg(png);
    f.close();

    const QString apps = QFileInfo(desktopPath()).absolutePath();
    QProcess::startDetached(QStringLiteral("update-desktop-database"), {apps});
}

#endif

QString customSourceDest(const QString &sourcePath) {
    const QString ext = QFileInfo(sourcePath).suffix().toLower();
    return QDir(AppPaths::configRoot())
        .filePath(QStringLiteral("document-icon-source.") + (ext.isEmpty() ? QStringLiteral("png") : ext));
}

} // namespace

QString DocumentIcon::icoPath() { return QDir(AppPaths::configRoot()).filePath(QStringLiteral("document.ico")); }

QUrl DocumentIcon::previewUrl(const QString &customPath) {
    if (!customPath.trimmed().isEmpty() && QFileInfo::exists(customPath))
        return QUrl::fromLocalFile(QFileInfo(customPath).absoluteFilePath());
    return QUrl(QStringLiteral("qrc") + QLatin1String(kResource));
}

QString DocumentIcon::displayLabel(const QString &customPath) {
    if (customPath.trimmed().isEmpty()) return QStringLiteral("Default");
    return QFileInfo(customPath).fileName();
}

QString DocumentIcon::importCustom(const QString &sourcePath) {
    const QFileInfo src(sourcePath);
    if (!src.exists() || !src.isFile()) return {};
    const QImage probe = loadIconImage(src.absoluteFilePath());
    const bool ico = src.suffix().compare(QLatin1String("ico"), Qt::CaseInsensitive) == 0;
    if (probe.isNull() && !ico) return {};
    QDir().mkpath(AppPaths::configRoot());
    const QString dest = customSourceDest(src.absoluteFilePath());
    if (QFileInfo(src.absoluteFilePath()).canonicalFilePath() == QFileInfo(dest).canonicalFilePath())
        return dest;
    for (const QFileInfo &fi :
         QDir(AppPaths::configRoot()).entryInfoList({QStringLiteral("document-icon-source.*")}, QDir::Files))
        QFile::remove(fi.absoluteFilePath());
    if (!QFile::copy(src.absoluteFilePath(), dest)) return {};
    return dest;
}

void DocumentIcon::syncAssociation(const ViewerSettings &s) {
#ifdef Q_OS_WIN
    const QString exe = associatedExe();
    if (exe.isEmpty()) return;
    QString ico = icoPath();
    if (s.useDocumentIcon) {
        if (!writeIco(ico, iconSource(s))) ico.clear();
    }
    advertiseWindows(exe, QDir::toNativeSeparators(ico), s.useDocumentIcon && !ico.isEmpty());
#elif !defined(Q_OS_MACOS)
    const QString exe = associatedExe();
    if (exe.isEmpty()) return;
    QString png = pngPath();
    if (s.useDocumentIcon) {
        if (!writePng(png, iconSource(s))) png.clear();
    } else {
        QFile::remove(png);
        png.clear();
    }
    advertiseXdg(exe, png, s.useDocumentIcon && !png.isEmpty());
#else
    Q_UNUSED(s);
#endif
}

} // namespace papercut
