#pragma once

#include <QString>
#include <QStringList>

namespace papercut {

struct DocumentFont {
    static QString ensureBundledRoboto();
    static QString fileForFamily(const QString &family, int weight);
    static QString cssFor(const QString &family, int weight, const QStringList &faceFiles);
    static QString userCss(const QString &family, int weight);
};

} // namespace papercut
