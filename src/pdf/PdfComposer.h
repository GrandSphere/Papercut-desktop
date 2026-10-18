#pragma once

#include <QList>
#include <QString>
#include <QStringList>

namespace papercut {

struct ComposeSource {
    QString path;
    QString magic;
};

class PdfComposer {
public:
    static void extractPages(const QString &srcPath, const QList<int> &pages, const QString &dest,
                             const QString &password = QString(), const QString &srcMagic = QString(),
                             const QList<int> &rotates = {});

    static void combine(const QList<ComposeSource> &sources, float fontScale, const QString &dest,
                        const QString &password = QString(), const QList<int> &rotates = {});
};

} // namespace papercut
