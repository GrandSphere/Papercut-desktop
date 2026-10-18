#include "Io.h"

#include <QFile>
#include <QFileInfo>
#include <QScopedPointer>

namespace papercut {

bool copyAtomically(const QString &dest, const QString &tmp, const std::function<QIODevice *()> &open) {
    QFile::remove(tmp);
    bool copied = false;
    {
        QScopedPointer<QIODevice> input(open());
        if (input && input->open(QIODevice::ReadOnly)) {
            QFile out(tmp);
            if (out.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
                copied = out.write(input->readAll()) >= 0;
            }
        }
    }
    if (copied && QFileInfo(tmp).size() > 0) {
        QFile::remove(dest);
        if (QFile::rename(tmp, dest) && QFileInfo(dest).size() > 0)
            return true;
    }
    QFile::remove(tmp);
    return false;
}

} // namespace papercut
