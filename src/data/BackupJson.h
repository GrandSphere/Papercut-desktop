#pragma once

#include "ViewerSettings.h"

#include <QString>
#include <QVector>
#include <optional>

namespace papercut {

class BackupJson {
public:
    static constexpr const char kFormat[] = "Papercut";
    static constexpr int kSchemaVersion = 1;

    enum class Kind { All, Settings, Bookmarks, Folio };

    struct Parsed {
        std::optional<ViewerSettings> settings;
        QString settingsJson;
        std::optional<QVector<PdfDocument>> documents;
        std::optional<QVector<Bookmark>> bookmarks;
    };

    static QString exportKind(Kind kind, const ViewerSettings &settings,
                              const QVector<PdfDocument> &documents,
                              const QVector<Bookmark> &bookmarks);
    static QString exportFolio(const QVector<FolioItem> &items);
    static Parsed parse(const QString &text);
    static ViewerSettings mergeSettings(const ViewerSettings &current, const QString &settingsJson);
    static QString settingsToJson(const ViewerSettings &settings);
};

} // namespace papercut
