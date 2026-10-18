#pragma once

#include <QDir>
#include <QString>

namespace papercut {

class FolioStore {
public:
    explicit FolioStore(const QString &filesDir) : filesDir_(filesDir) {}
    void setFilesDir(const QString &filesDir) { filesDir_ = filesDir; }

    QDir dir() const;
    QString fileFor(qint64 id) const;
    bool exists(qint64 id) const;
    bool remove(qint64 id) const; // false when open/locked on Windows
    bool copyFrom(const QString &sourcePath, qint64 id) const;

private:
    QString filesDir_;
};

} // namespace papercut
