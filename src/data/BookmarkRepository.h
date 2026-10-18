#pragma once

#include "BackupJson.h"
#include "Database.h"

#include <QSqlDatabase>
#include <QString>
#include <QVector>

namespace papercut {

class BookmarkRepository {
public:
    explicit BookmarkRepository(Database &db) : db_(db) {}

    QVector<PdfDocument> documents() const;
    QVector<Bookmark> bookmarksFor(const QString &uri) const;
    QVector<Bookmark> allBookmarks() const;
    std::optional<PdfDocument> getDocument(const QString &uri) const;
    std::optional<PdfDocument> findByNameAndPath(const QString &name, const QString &path) const;
    PdfDocument ensureDocument(const QString &uri, const QString &displayName, const QString &displayPath);
    void saveLastPosition(const QString &uri, int page, float scrollX, float scrollY);
    Bookmark addBookmark(const QString &uri, const QString &name, int pageIndex, float scrollX, float scrollY);
    void updateBookmark(const Bookmark &bookmark);
    void deleteBookmarks(const QVector<qint64> &ids);
    void reorder(const QVector<qint64> &orderedIds);
    void deleteDocuments(const QVector<QString> &uris);
    void clearAllBookmarks();
    QString snapshot(BackupJson::Kind kind, const ViewerSettings &settings) const;
    void importParsed(const BackupJson::Parsed &parsed);

private:
    int maxSortOrder(const QString &uri) const;
    Database &db_;
};

} // namespace papercut
