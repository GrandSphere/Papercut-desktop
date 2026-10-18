#pragma once

#include "PdfModels.h"

#include <QVector>

namespace papercut {

class ColorRemapper {
public:
    static void remap(QVector<int> &pixels, int w, int h, int fg, int bg,
                      float imageBlend, const QVector<PdfRect> &imageRects,
                      float imageAlpha = 1.f);
};

} // namespace papercut
