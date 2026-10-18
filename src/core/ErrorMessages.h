#pragma once

#include <QString>

namespace papercut {

inline QString errorMessage(const QString &code) {
    if (code == QLatin1String("FOLIO_ADDED")) return QStringLiteral("Added to Folio");
    if (code == QLatin1String("FOLIO_COPY")) return QStringLiteral("Could not add to Folio");
    if (code == QLatin1String("NOT_PDF")) return QStringLiteral("PDF files only");
    if (code == QLatin1String("PASSWORD")) return QStringLiteral("This PDF is password protected");
    if (code == QLatin1String("EXPORT")) return QStringLiteral("Could not build PDF");
    return QStringLiteral("Could not open PDF");
}

} // namespace papercut
