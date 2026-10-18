#pragma once

#include <QString>

namespace papercut {

enum class ZoomMode { FitPage, FitWidth, Custom };

inline ZoomMode zoomModeFromStorage(const QString &value) {
    if (value == QLatin1String("FIT_PAGE")) return ZoomMode::FitPage;
    if (value == QLatin1String("CUSTOM")) return ZoomMode::Custom;
    return ZoomMode::FitWidth;
}

inline QString zoomModeToStorage(ZoomMode mode) {
    switch (mode) {
    case ZoomMode::FitPage: return QStringLiteral("FIT_PAGE");
    case ZoomMode::Custom: return QStringLiteral("CUSTOM");
    case ZoomMode::FitWidth:
    default: return QStringLiteral("FIT_WIDTH");
    }
}

} // namespace papercut
