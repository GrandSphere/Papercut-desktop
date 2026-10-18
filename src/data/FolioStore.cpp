#include "FolioStore.h"
#include "core/Io.h"

#include <QFile>
#include <QFileDevice>
#include <QFileInfo>
#include <QtDebug>

namespace papercut {

QDir FolioStore::dir() const {
    QDir d(filesDir_);
    d.mkpath(QStringLiteral("folio"));
    return QDir(d.filePath(QStringLiteral("folio")));
}

QString FolioStore::fileFor(qint64 id) const {
    return dir().filePath(QString::number(id));
}

bool FolioStore::exists(qint64 id) const {
    const QFileInfo info(fileFor(id));
    return info.exists() && info.size() > 0;
}

bool FolioStore::remove(qint64 id) const {
    const QString path = fileFor(id);
    if (!QFileInfo::exists(path)) return true;
    QFile f(path);
    f.setPermissions(f.permissions() | QFileDevice::WriteOwner | QFileDevice::WriteUser);
    return QFile::remove(path) || !QFileInfo::exists(path);
}

bool FolioStore::copyFrom(const QString &sourcePath, qint64 id) const {
    const QString dest = fileFor(id);
    const QString tmp = dest + QStringLiteral(".tmp");
    const QFileInfo src(sourcePath);
    if (!src.exists() || src.size() <= 0) {
        qWarning("folio copy: missing %s", qUtf8Printable(sourcePath));
        return false;
    }
    const QString absSrc = src.absoluteFilePath();
    if (QFileInfo(absSrc) == QFileInfo(QFileInfo(dest).absoluteFilePath()))
        return false;
    if (copyAtomically(dest, tmp, [absSrc]() -> QIODevice * { return new QFile(absSrc); }))
        return true;
    QFile::remove(tmp);
    QFile::remove(dest);
    if (QFile::copy(absSrc, dest) && QFileInfo(dest).size() > 0)
        return true;
    qWarning("folio copy: failed %s -> %s", qUtf8Printable(absSrc), qUtf8Printable(dest));
    return false;
}

} // namespace papercut
