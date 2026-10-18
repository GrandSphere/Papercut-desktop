#pragma once

#include "PdfModels.h"

#include <QString>
#include <QVector>

namespace papercut {

class TextFinder {
public:
    static QVector<SearchHit> find(const QVector<QVector<CharBox>> &pages, const QString &query);
};

} // namespace papercut
