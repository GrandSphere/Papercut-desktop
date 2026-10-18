#include "BookmarkRepository.h"
#include "DocumentPaths.h"

#include <QSqlQuery>

namespace papercut {
namespace {

PdfDocument docFrom(const QSqlQuery &q) {
    return PdfDocument{
        q.value(0).toString(),
        q.value(1).toString(),
        q.value(2).toString(),
        q.value(3).toInt(),
        q.value(4).toFloat(),
        q.value(5).toFloat(),
    };
}

Bookmark markFrom(const QSqlQuery &q) {
    return Bookmark{
        q.value(0).toLongLong(),
        q.value(1).toString(),
        q.value(2).toString(),
        q.value(3).toInt(),
        q.value(4).toFloat(),
        q.value(5).toFloat(),
        q.value(6).toInt(),
    };
}

} // namespace

QVector<PdfDocument> BookmarkRepository::documents() const {
    QSqlQuery q(db_.db());
    q.exec(QStringLiteral("SELECT uri, displayName, displayPath, lastPageIndex, lastScrollXPdf, lastScrollYPdf "
                          "FROM documents ORDER BY displayName COLLATE NOCASE"));
    QVector<PdfDocument> out;
    while (q.next()) out.push_back(docFrom(q));
    return out;
}

QVector<Bookmark> BookmarkRepository::bookmarksFor(const QString &uri) const {
    QSqlQuery q(db_.db());
    q.prepare(QStringLiteral("SELECT id, documentUri, name, pageIndex, scrollXPdf, scrollYPdf, sortOrder "
                             "FROM bookmarks WHERE documentUri = ? ORDER BY sortOrder ASC, id ASC"));
    q.addBindValue(uri);
    q.exec();
    QVector<Bookmark> out;
    while (q.next()) out.push_back(markFrom(q));
    return out;
}

QVector<Bookmark> BookmarkRepository::allBookmarks() const {
    QSqlQuery q(db_.db());
    q.exec(QStringLiteral("SELECT id, documentUri, name, pageIndex, scrollXPdf, scrollYPdf, sortOrder "
                          "FROM bookmarks ORDER BY documentUri ASC, sortOrder ASC, id ASC"));
    QVector<Bookmark> out;
    while (q.next()) out.push_back(markFrom(q));
    return out;
}

std::optional<PdfDocument> BookmarkRepository::getDocument(const QString &uri) const {
    QSqlQuery q(db_.db());
    q.prepare(QStringLiteral("SELECT uri, displayName, displayPath, lastPageIndex, lastScrollXPdf, lastScrollYPdf "
                             "FROM documents WHERE uri = ?"));
    q.addBindValue(uri);
    q.exec();
    if (q.next()) return docFrom(q);
    return std::nullopt;
}

std::optional<PdfDocument> BookmarkRepository::findByNameAndPath(const QString &name, const QString &path) const {
    if (name.isEmpty()) return std::nullopt;
    QSqlQuery q(db_.db());
    q.prepare(QStringLiteral("SELECT uri, displayName, displayPath, lastPageIndex, lastScrollXPdf, lastScrollYPdf "
                             "FROM documents WHERE displayName = ? AND displayPath = ? LIMIT 1"));
    q.addBindValue(name);
    q.addBindValue(path);
    q.exec();
    if (q.next()) return docFrom(q);
    return std::nullopt;
}

PdfDocument BookmarkRepository::ensureDocument(const QString &uri, const QString &displayName,
                                               const QString &displayPath) {
    const PathLabel label = DocumentPaths::fromUriString(uri, displayName, displayPath);
    auto existing = getDocument(uri);
    if (existing) {
        if (existing->displayName != label.displayName || existing->displayPath != label.displayPath) {
            QSqlQuery q(db_.db());
            q.prepare(QStringLiteral("UPDATE documents SET displayName = ?, displayPath = ? WHERE uri = ?"));
            q.addBindValue(label.displayName);
            q.addBindValue(label.displayPath);
            q.addBindValue(uri);
            q.exec();
            existing->displayName = label.displayName;
            existing->displayPath = label.displayPath;
        }
        return *existing;
    }
    QSqlQuery q(db_.db());
    q.prepare(QStringLiteral("INSERT OR IGNORE INTO documents (uri, displayName, displayPath, lastPageIndex, "
                             "lastScrollXPdf, lastScrollYPdf) VALUES (?,?,?,0,0,0)"));
    q.addBindValue(uri);
    q.addBindValue(label.displayName);
    q.addBindValue(label.displayPath);
    q.exec();
    return getDocument(uri).value_or(PdfDocument{uri, label.displayName, label.displayPath});
}

void BookmarkRepository::saveLastPosition(const QString &uri, int page, float scrollX, float scrollY) {
    QSqlQuery q(db_.db());
    q.prepare(QStringLiteral("UPDATE documents SET lastPageIndex = ?, lastScrollXPdf = ?, lastScrollYPdf = ? WHERE uri = ?"));
    q.addBindValue(page);
    q.addBindValue(scrollX);
    q.addBindValue(scrollY);
    q.addBindValue(uri);
    q.exec();
    if (q.numRowsAffected() > 0) return;
    const PathLabel label = DocumentPaths::fromUriString(uri);
    QSqlQuery ins(db_.db());
    ins.prepare(QStringLiteral("INSERT OR IGNORE INTO documents (uri, displayName, displayPath, lastPageIndex, "
                               "lastScrollXPdf, lastScrollYPdf) VALUES (?,?,?,?,?,?)"));
    ins.addBindValue(uri);
    ins.addBindValue(label.displayName);
    ins.addBindValue(label.displayPath);
    ins.addBindValue(page);
    ins.addBindValue(scrollX);
    ins.addBindValue(scrollY);
    ins.exec();
}

int BookmarkRepository::maxSortOrder(const QString &uri) const {
    QSqlQuery q(db_.db());
    q.prepare(QStringLiteral("SELECT COALESCE(MAX(sortOrder), -1) FROM bookmarks WHERE documentUri = ?"));
    q.addBindValue(uri);
    q.exec();
    return q.next() ? q.value(0).toInt() : -1;
}

Bookmark BookmarkRepository::addBookmark(const QString &uri, const QString &name, int pageIndex,
                                         float scrollX, float scrollY) {
    const int order = maxSortOrder(uri) + 1;
    QSqlQuery q(db_.db());
    q.prepare(QStringLiteral("INSERT INTO bookmarks (documentUri, name, pageIndex, scrollXPdf, scrollYPdf, sortOrder) "
                             "VALUES (?,?,?,?,?,?)"));
    q.addBindValue(uri);
    q.addBindValue(name);
    q.addBindValue(pageIndex);
    q.addBindValue(scrollX);
    q.addBindValue(scrollY);
    q.addBindValue(order);
    q.exec();
    return Bookmark{q.lastInsertId().toLongLong(), uri, name, pageIndex, scrollX, scrollY, order};
}

void BookmarkRepository::updateBookmark(const Bookmark &bookmark) {
    QSqlQuery q(db_.db());
    q.prepare(QStringLiteral("UPDATE bookmarks SET documentUri = ?, name = ?, pageIndex = ?, scrollXPdf = ?, "
                             "scrollYPdf = ?, sortOrder = ? WHERE id = ?"));
    q.addBindValue(bookmark.documentUri);
    q.addBindValue(bookmark.name);
    q.addBindValue(bookmark.pageIndex);
    q.addBindValue(bookmark.scrollXPdf);
    q.addBindValue(bookmark.scrollYPdf);
    q.addBindValue(bookmark.sortOrder);
    q.addBindValue(bookmark.id);
    q.exec();
}

void BookmarkRepository::deleteBookmarks(const QVector<qint64> &ids) {
    if (ids.isEmpty()) return;
    db_.db().transaction();
    for (qint64 id : ids) {
        QSqlQuery q(db_.db());
        q.prepare(QStringLiteral("DELETE FROM bookmarks WHERE id = ?"));
        q.addBindValue(id);
        q.exec();
    }
    db_.db().commit();
}

void BookmarkRepository::reorder(const QVector<qint64> &orderedIds) {
    db_.db().transaction();
    for (int i = 0; i < orderedIds.size(); ++i) {
        QSqlQuery q(db_.db());
        q.prepare(QStringLiteral("UPDATE bookmarks SET sortOrder = ? WHERE id = ?"));
        q.addBindValue(i);
        q.addBindValue(orderedIds[i]);
        q.exec();
    }
    db_.db().commit();
}

void BookmarkRepository::deleteDocuments(const QVector<QString> &uris) {
    if (uris.isEmpty()) return;
    db_.db().transaction();
    for (const QString &uri : uris) {
        QSqlQuery q(db_.db());
        q.prepare(QStringLiteral("DELETE FROM documents WHERE uri = ?"));
        q.addBindValue(uri);
        q.exec();
    }
    db_.db().commit();
}

void BookmarkRepository::clearAllBookmarks() {
    QSqlQuery q(db_.db());
    q.exec(QStringLiteral("DELETE FROM bookmarks"));
}

QString BookmarkRepository::snapshot(BackupJson::Kind kind, const ViewerSettings &settings) const {
    const auto docs = kind == BackupJson::Kind::Settings ? QVector<PdfDocument>{} : documents();
    const auto marks = kind == BackupJson::Kind::Settings ? QVector<Bookmark>{} : allBookmarks();
    return BackupJson::exportKind(kind, settings, docs, marks);
}

void BookmarkRepository::importParsed(const BackupJson::Parsed &parsed) {
    db_.db().transaction();
    if (parsed.documents) {
        for (const PdfDocument &incoming : *parsed.documents) {
            auto existing = getDocument(incoming.uri);
            if (!existing) {
                QSqlQuery q(db_.db());
                q.prepare(QStringLiteral("INSERT OR REPLACE INTO documents (uri, displayName, displayPath, lastPageIndex, "
                                         "lastScrollXPdf, lastScrollYPdf) VALUES (?,?,?,?,?,?)"));
                q.addBindValue(incoming.uri);
                q.addBindValue(incoming.displayName);
                q.addBindValue(incoming.displayPath);
                q.addBindValue(incoming.lastPageIndex);
                q.addBindValue(incoming.lastScrollXPdf);
                q.addBindValue(incoming.lastScrollYPdf);
                q.exec();
            } else {
                QSqlQuery q(db_.db());
                q.prepare(QStringLiteral("UPDATE documents SET displayName = ?, displayPath = ?, lastPageIndex = ?, "
                                         "lastScrollXPdf = ?, lastScrollYPdf = ? WHERE uri = ?"));
                q.addBindValue(incoming.displayName.isEmpty() ? existing->displayName : incoming.displayName);
                q.addBindValue(incoming.displayPath.isEmpty() ? existing->displayPath : incoming.displayPath);
                q.addBindValue(incoming.lastPageIndex);
                q.addBindValue(incoming.lastScrollXPdf);
                q.addBindValue(incoming.lastScrollYPdf);
                q.addBindValue(incoming.uri);
                q.exec();
            }
        }
    }
    if (parsed.bookmarks) {
        for (const Bookmark &incoming : *parsed.bookmarks) {
            if (!getDocument(incoming.documentUri)) {
                const PathLabel label = DocumentPaths::fromUriString(incoming.documentUri);
                QSqlQuery q(db_.db());
                q.prepare(QStringLiteral("INSERT OR REPLACE INTO documents (uri, displayName, displayPath, lastPageIndex, "
                                         "lastScrollXPdf, lastScrollYPdf) VALUES (?,?,?,0,0,0)"));
                q.addBindValue(incoming.documentUri);
                q.addBindValue(label.displayName);
                q.addBindValue(label.displayPath);
                q.exec();
            }
            const int nextOrder = maxSortOrder(incoming.documentUri) + 1;
            QSqlQuery q(db_.db());
            q.prepare(QStringLiteral("INSERT INTO bookmarks (documentUri, name, pageIndex, scrollXPdf, scrollYPdf, sortOrder) "
                                     "VALUES (?,?,?,?,?,?)"));
            q.addBindValue(incoming.documentUri);
            q.addBindValue(incoming.name);
            q.addBindValue(incoming.pageIndex);
            q.addBindValue(incoming.scrollXPdf);
            q.addBindValue(incoming.scrollYPdf);
            q.addBindValue(nextOrder);
            q.exec();
        }
    }
    db_.db().commit();
}

} // namespace papercut
