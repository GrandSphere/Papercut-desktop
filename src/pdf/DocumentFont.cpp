#include "DocumentFont.h"

#include "core/AppPaths.h"
#include "data/ViewerSettings.h"

#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QMutex>
#include <QMutexLocker>

#ifdef Q_OS_WIN
#include <windows.h>
#endif

#ifdef PAPERCUT_HAS_FONTCONFIG
#include <fontconfig/fontconfig.h>
#endif

namespace papercut {
namespace {

QMutex g_lock;
QHash<QString, QString> g_faceFiles;

QString cacheKey(const QString &family, int weight) {
    return family + QLatin1Char('\n') + QString::number(weight);
}

QString cssEscape(const QString &s) {
    QString o = s;
    o.replace(QLatin1Char('\\'), QStringLiteral("\\\\"));
    o.replace(QLatin1Char('"'), QStringLiteral("\\\""));
    return o;
}

QString cssUrlPath(const QString &path) {
    return QDir::fromNativeSeparators(QFileInfo(path).absoluteFilePath());
}

bool writeIfMissing(const QString &dest, const QByteArray &bytes) {
    if (bytes.size() < 100) return false;
    QDir().mkpath(QFileInfo(dest).absolutePath());
    if (QFileInfo(dest).size() == bytes.size()) return true;
    QFile out(dest);
    if (!out.open(QIODevice::WriteOnly | QIODevice::Truncate)) return false;
    return out.write(bytes) == bytes.size();
}

QString faceDest(const QString &tag, const QByteArray &bytes) {
    QString ext = QStringLiteral("ttf");
    if (bytes.startsWith("OTTO")) ext = QStringLiteral("otf");
    else if (bytes.startsWith("ttcf")) ext = QStringLiteral("ttc");
    const QString dest = AppPaths::temp().filePath(tag + QLatin1Char('.') + ext);
    if (!writeIfMissing(dest, bytes)) return {};
    return dest;
}

#ifdef Q_OS_WIN
QString windowsFaceFile(const QString &family, int weight) {
    LOGFONTW lf{};
    lf.lfHeight = -16;
    lf.lfWeight = weight;
    lf.lfCharSet = DEFAULT_CHARSET;
    lf.lfOutPrecision = OUT_TT_ONLY_PRECIS;
    lf.lfClipPrecision = CLIP_DEFAULT_PRECIS;
    lf.lfQuality = CLEARTYPE_QUALITY;
    lf.lfPitchAndFamily = DEFAULT_PITCH | FF_DONTCARE;
    const QString clipped = family.left(LF_FACESIZE - 1);
    clipped.toWCharArray(lf.lfFaceName);
    lf.lfFaceName[clipped.size()] = 0;

    const HFONT font = CreateFontIndirectW(&lf);
    if (!font) return {};
    const HDC dc = CreateCompatibleDC(nullptr);
    if (!dc) {
        DeleteObject(font);
        return {};
    }
    const HGDIOBJ old = SelectObject(dc, font);
    const DWORD n = GetFontData(dc, 0, 0, nullptr, 0);
    QByteArray bytes;
    if (n != GDI_ERROR && n > 100) {
        bytes.resize(int(n));
        if (GetFontData(dc, 0, 0, bytes.data(), n) != n) bytes.clear();
    }
    SelectObject(dc, old);
    DeleteDC(dc);
    DeleteObject(font);
    if (bytes.isEmpty()) return {};

    const QByteArray key = cacheKey(family, weight).toUtf8();
    const QString tag = QStringLiteral("pc-face-")
        + QString::fromLatin1(QCryptographicHash::hash(key, QCryptographicHash::Sha1).toHex().left(16));
    return faceDest(tag, bytes);
}
#endif

#ifdef PAPERCUT_HAS_FONTCONFIG
QString fontconfigFaceFile(const QString &family, int weight) {
    FcPattern *pat = FcPatternCreate();
    if (!pat) return {};
    const QByteArray utf = family.toUtf8();
    FcPatternAddString(pat, FC_FAMILY, reinterpret_cast<const FcChar8 *>(utf.constData()));
    FcPatternAddInteger(pat, FC_WEIGHT, FcWeightFromOpenType(weight));
    FcPatternAddInteger(pat, FC_SLANT, FC_SLANT_ROMAN);
    FcConfigSubstitute(nullptr, pat, FcMatchPattern);
    FcDefaultSubstitute(pat);
    FcResult result = FcResultNoMatch;
    FcPattern *match = FcFontMatch(nullptr, pat, &result);
    FcPatternDestroy(pat);
    if (!match) return {};
    FcChar8 *file = nullptr;
    QString path;
    if (FcPatternGetString(match, FC_FILE, 0, &file) == FcResultMatch && file)
        path = QString::fromUtf8(reinterpret_cast<const char *>(file));
    FcPatternDestroy(match);
    if (path.isEmpty() || !QFileInfo::exists(path)) return {};
    return path;
}
#endif

QString systemFaceFile(const QString &family, int weight) {
#ifdef Q_OS_WIN
    return windowsFaceFile(family, weight);
#elif defined(PAPERCUT_HAS_FONTCONFIG)
    return fontconfigFaceFile(family, weight);
#else
    Q_UNUSED(family);
    Q_UNUSED(weight);
    return {};
#endif
}

} // namespace

QString DocumentFont::ensureBundledRoboto() {
    QMutexLocker lock(&g_lock);
    const QString dest = AppPaths::temp().filePath(QStringLiteral("Roboto-Regular.ttf"));
    if (QFileInfo(dest).size() > 1000) return dest;
    QFile in(QStringLiteral(":/fonts/Roboto-Regular.ttf"));
    if (!in.open(QIODevice::ReadOnly)) return {};
    const QByteArray bytes = in.readAll();
    if (!writeIfMissing(dest, bytes)) return {};
    return dest;
}

QString DocumentFont::fileForFamily(const QString &family, int weight) {
    const QString canon = ViewerSettings::canonicalAppFontFamily(family);
    const int w = ViewerSettings::canonicalAppFontWeight(weight);
    const QString key = cacheKey(canon, w);
    {
        QMutexLocker lock(&g_lock);
        const auto it = g_faceFiles.constFind(key);
        if (it != g_faceFiles.cend()) return it.value();
    }

    QString path;
    if (canon.compare(QLatin1String("Roboto"), Qt::CaseInsensitive) == 0)
        path = ensureBundledRoboto();
    else
        path = systemFaceFile(canon, w);
    if (path.isEmpty()) path = ensureBundledRoboto();

    QMutexLocker lock(&g_lock);
    g_faceFiles.insert(key, path);
    return path;
}

static QString faceRule(const QString &family, int weight, const QString &file) {
    if (file.isEmpty()) return {};
    return QStringLiteral("@font-face { font-family: \"%1\"; font-weight: %2; src: url(\"%3\"); }\n")
        .arg(cssEscape(family), QString::number(weight), cssEscape(cssUrlPath(file)));
}

// Override MuPDF pre/code monospace.
static QString bodyRule(const QString &family, int weight) {
    return QStringLiteral(
               "body, p, div, span, li, td, th, h1, h2, h3, h4, h5, h6, pre, code {"
               " font-family: \"%1\", \"Roboto\", serif !important; font-weight: %2 !important; }\n")
        .arg(cssEscape(family), QString::number(weight));
}

QString DocumentFont::cssFor(const QString &family, int weight, const QStringList &faceFiles) {
    const QString canon = ViewerSettings::canonicalAppFontFamily(family);
    const int w = ViewerSettings::canonicalAppFontWeight(weight);
    QString css;
    QStringList unique;
    for (const QString &file : faceFiles) {
        if (file.isEmpty() || unique.contains(file)) continue;
        unique.append(file);
        css += faceRule(canon, w, file);
    }
    css += bodyRule(canon, w);
    return css;
}

QString DocumentFont::userCss(const QString &family, int weight) {
    const QString canon = ViewerSettings::canonicalAppFontFamily(family);
    const int w = ViewerSettings::canonicalAppFontWeight(weight);
    const QString roboto = ensureBundledRoboto();
    const QString chosen = fileForFamily(canon, w);
    QString css;
    css += faceRule(QStringLiteral("Roboto"), w, roboto);
    if (!chosen.isEmpty() && QFileInfo(chosen) != QFileInfo(roboto))
        css += faceRule(canon, w, chosen);
    css += bodyRule(canon, w);
    return css;
}

} // namespace papercut
