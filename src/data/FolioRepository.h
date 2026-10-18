#pragma once

#include "Database.h"
#include "FolioStore.h"

#include <QString>
#include <QVector>
#include <optional>

namespace papercut {

class FolioRepository {
public:
    FolioRepository(Database &db, FolioStore &store) : db_(db), store_(store) {}

    QVector<FolioItem> all() const;
    std::optional<FolioItem> getByUri(const QString &uri) const;
    std::optional<FolioItem> getById(qint64 id) const;
    QString fileFor(const FolioItem &item) const { return store_.fileFor(item.id); }
    std::optional<FolioItem> add(const QString &uri, const QString &displayName, const QString &displayPath,
                                 const QString &sourcePath, const ViewerSettings *snapshot = nullptr,
                                 int pageIndex = 0, float scrollX = 0, float scrollY = 0);
    bool ensureLocal(const FolioItem &item, const QString &fallback = {});
    void saveSnapshot(qint64 id, const QString &zoomMode, float customZoomPercent, bool lockZoom,
                      bool lockSideScroll, bool originalColors, int pageIndex, float scrollX, float scrollY,
                      const QString &pageRotations = QStringLiteral("[]"));
    void saveRotations(qint64 id, const QString &pageRotations);
    void setHidden(const QVector<qint64> &ids, bool hidden);
    void setStartItem(qint64 id);
    void remove(const QVector<qint64> &ids);
    void reorder(const QVector<qint64> &orderedIds);

private:
    int maxSortOrder() const;
    void reindex();
    Database &db_;
    FolioStore &store_;
};

} // namespace papercut
