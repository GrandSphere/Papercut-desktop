#include "AppPaths.h"

#include <QCoreApplication>
#include <QFile>
#include <QFileInfo>
#include <QStandardPaths>

namespace papercut {
namespace {

QString g_filesRootOverride;

QString xdgHome(const char *env, const QString &fallbackUnderHome) {
    const QByteArray v = qgetenv(env);
    if (!v.isEmpty()) return QString::fromLocal8Bit(v);
    return QDir::homePath() + fallbackUnderHome;
}

QString roamingAppData() {
#ifdef Q_OS_WIN
    QString roaming = QString::fromLocal8Bit(qgetenv("APPDATA"));
    if (roaming.isEmpty())
        roaming = QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation);
    return roaming;
#else
    return {};
#endif
}

QString papercutBaseWin() {
    return QDir(roamingAppData()).filePath(QStringLiteral("grandsphere/papercut"));
}

void copyFileIfMissing(const QString &from, const QString &to) {
    if (from.isEmpty() || to.isEmpty() || !QFileInfo::exists(from) || QFileInfo::exists(to)) return;
    QDir().mkpath(QFileInfo(to).absolutePath());
    QFile::copy(from, to);
}

void copyDirFilesIfMissing(const QDir &from, const QDir &to) {
    if (!from.exists()) return;
    to.mkpath(QStringLiteral("."));
    for (const QFileInfo &fi : from.entryInfoList(QDir::Files | QDir::NoDotAndDotDot)) {
        const QString dest = to.filePath(fi.fileName());
        if (!QFileInfo::exists(dest)) QFile::copy(fi.absoluteFilePath(), dest);
    }
}

} // namespace

QString AppPaths::dataRoot() {
    const QByteArray override = qgetenv("PAPERCUT_DATA_DIR");
    if (!override.isEmpty()) return QString::fromLocal8Bit(override);
#ifdef Q_OS_WIN
    return QDir(papercutBaseWin()).filePath(QStringLiteral("files"));
#else
    return QDir(xdgHome("XDG_DATA_HOME", QStringLiteral("/.local/share")))
        .filePath(QStringLiteral("grandsphere/papercut"));
#endif
}

QString AppPaths::configRoot() {
    const QByteArray override = qgetenv("PAPERCUT_CONFIG_DIR");
    if (!override.isEmpty()) return QString::fromLocal8Bit(override);
    if (!qgetenv("PAPERCUT_DATA_DIR").isEmpty())
        return QDir(dataRoot()).filePath(QStringLiteral("config"));
#ifdef Q_OS_WIN
    return QDir(papercutBaseWin()).filePath(QStringLiteral("config"));
#else
    return QDir(xdgHome("XDG_CONFIG_HOME", QStringLiteral("/.config")))
        .filePath(QStringLiteral("grandsphere/papercut"));
#endif
}

QString AppPaths::filesRoot() {
    if (!g_filesRootOverride.trimmed().isEmpty()) return QDir::cleanPath(g_filesRootOverride);
    return dataRoot();
}

void AppPaths::setFilesRootOverride(const QString &path) {
    g_filesRootOverride = path.trimmed();
}

QDir AppPaths::temp() {
    QDir dir(filesRoot());
    dir.mkpath(QStringLiteral("temp files"));
    return QDir(dir.filePath(QStringLiteral("temp files")));
}

QDir AppPaths::folio() {
    QDir dir(filesRoot());
    dir.mkpath(QStringLiteral("folio"));
    return QDir(dir.filePath(QStringLiteral("folio")));
}

QString AppPaths::database() { return QDir(dataRoot()).filePath(QLatin1String(kDatabase)); }

QString AppPaths::keybinds() { return QDir(configRoot()).filePath(QLatin1String(kKeybinds)); }

QString AppPaths::settingsFile() { return QDir(configRoot()).filePath(QLatin1String(kSettings)); }

QString AppPaths::exportPdf() { return temp().filePath(QStringLiteral("papercut-export.pdf")); }

QString AppPaths::jsonShare(const QString &name) { return temp().filePath(name); }

void AppPaths::migrateLegacyLayout() {
    if (!qgetenv("PAPERCUT_DATA_DIR").isEmpty()) return;
#ifdef Q_OS_WIN
    const QString legacy = papercutBaseWin();
    if (legacy.isEmpty()) return;
    copyFileIfMissing(QDir(legacy).filePath(QLatin1String(kDatabase)), database());
    copyFileIfMissing(QDir(legacy).filePath(QLatin1String(kKeybinds)), keybinds());
    copyDirFilesIfMissing(QDir(QDir(legacy).filePath(QStringLiteral("Files/folio"))), folio());
    copyDirFilesIfMissing(QDir(QDir(legacy).filePath(QStringLiteral("tempFiles"))), temp());
    copyDirFilesIfMissing(QDir(QDir(legacy).filePath(QStringLiteral("Files"))), QDir(dataRoot()));
#else
    const QString data = dataRoot();
    copyFileIfMissing(QDir(data).filePath(QLatin1String(kKeybinds)), keybinds());
#endif
}

} // namespace papercut
