#include "FolioRepository.h"

#include <QSqlError>
#include <QSqlQuery>
#include <QStringList>
#include <QtDebug>

namespace papercut {
namespace {

FolioItem itemFrom(const QSqlQuery &q) {
    FolioItem i;
    int n = 0;
    i.id = q.value(n++).toLongLong();
    i.uri = q.value(n++).toString();
    i.displayName = q.value(n++).toString();
    i.displayPath = q.value(n++).toString();
    i.sortOrder = q.value(n++).toInt();
    i.hidden = q.value(n++).toInt() != 0;
    i.isStartItem = q.value(n++).toInt() != 0;
    i.zoomMode = q.value(n++).toString();
    i.customZoomPercent = q.value(n++).toFloat();
    i.lockZoom = q.value(n++).toInt() != 0;
    i.lockSideScroll = q.value(n++).toInt() != 0;
    i.originalColors = q.value(n++).toInt() != 0;
    i.lastPageIndex = q.value(n++).toInt();
    i.lastScrollXPdf = q.value(n++).toFloat();
    i.lastScrollYPdf = q.value(n++).toFloat();
    i.localName = q.value(n++).toString();
    i.pageRotations = q.value(n++).toString();
    if (i.pageRotations.isEmpty()) i.pageRotations = QStringLiteral("[]");
    return i;
}

const char *kSelect =
    "SELECT id, uri, displayName, displayPath, sortOrder, hidden, isStartItem, zoomMode, customZoomPercent, "
    "lockZoom, lockSideScroll, originalColors, lastPageIndex, lastScrollXPdf, lastScrollYPdf, localName, "
    "pageRotations FROM folio_items";

} // namespace

QVector<FolioItem> FolioRepository::all() const {
    QSqlQuery q(db_.db());
    q.exec(QString(QLatin1String(kSelect)) + QStringLiteral(" ORDER BY sortOrder ASC, id ASC"));
    QVector<FolioItem> out;
    while (q.next()) out.push_back(itemFrom(q));
    return out;
}

std::optional<FolioItem> FolioRepository::getByUri(const QString &uri) const {
    QSqlQuery q(db_.db());
    q.prepare(QString(QLatin1String(kSelect)) + QStringLiteral(" WHERE uri = ? LIMIT 1"));
    q.addBindValue(uri);
    q.exec();
    if (q.next()) return itemFrom(q);
    return std::nullopt;
}

std::optional<FolioItem> FolioRepository::getById(qint64 id) const {
    QSqlQuery q(db_.db());
    q.prepare(QString(QLatin1String(kSelect)) + QStringLiteral(" WHERE id = ? LIMIT 1"));
    q.addBindValue(id);
    q.exec();
    if (q.next()) return itemFrom(q);
    return std::nullopt;
}

int FolioRepository::maxSortOrder() const {
    QSqlQuery q(db_.db());
    q.exec(QStringLiteral("SELECT COALESCE(MAX(sortOrder), -1) FROM folio_items"));
    return q.next() ? q.value(0).toInt() : -1;
}

std::optional<FolioItem> FolioRepository::add(const QString &uri, const QString &displayName,
                                              const QString &displayPath, const QString &sourcePath,
                                              const ViewerSettings *snapshot, int pageIndex, float scrollX,
                                              float scrollY) {
    auto existing = getByUri(uri);
    if (existing) {
        ensureLocal(*existing, sourcePath);
        return getById(existing->id).value_or(*existing);
    }
    const int sortOrder = maxSortOrder() + 1;
    QSqlQuery ins(db_.db());
    if (!ins.prepare(QStringLiteral(
            "INSERT INTO folio_items (uri, displayName, displayPath, sortOrder, hidden, isStartItem, "
            "zoomMode, customZoomPercent, lockZoom, lockSideScroll, originalColors, lastPageIndex, lastScrollXPdf, "
            "lastScrollYPdf, localName) VALUES (?,?,?,?,0,0,?,?,?,?,?,?,?,?,?)"))) {
        qWarning() << "folio insert prepare:" << ins.lastError().text();
        return std::nullopt;
    }
    ins.addBindValue(uri);
    ins.addBindValue(displayName);
    ins.addBindValue(displayPath);
    ins.addBindValue(sortOrder);
    ins.addBindValue(snapshot ? snapshot->zoomMode : QStringLiteral("FIT_WIDTH"));
    ins.addBindValue(snapshot ? snapshot->customZoomPercent : 100.f);
    ins.addBindValue(snapshot && snapshot->lockZoom ? 1 : 0);
    ins.addBindValue(snapshot ? (snapshot->lockSideScroll ? 1 : 0) : 1);
    ins.addBindValue(snapshot && snapshot->originalColors ? 1 : 0);
    ins.addBindValue(pageIndex);
    ins.addBindValue(scrollX);
    ins.addBindValue(scrollY);
    ins.addBindValue(QStringLiteral(""));
    if (!ins.exec()) {
        qWarning() << "folio insert:" << ins.lastError().text();
        return std::nullopt;
    }
    qint64 id = ins.lastInsertId().toLongLong();
    auto row = id > 0 ? getById(id) : std::nullopt;
    if (!row) row = getByUri(uri);
    if (!row) return std::nullopt;
    if (!store_.copyFrom(sourcePath, row->id)) {
        db_.db().transaction();
        QSqlQuery del(db_.db());
        del.prepare(QStringLiteral("DELETE FROM folio_items WHERE id = ?"));
        del.addBindValue(row->id);
        del.exec();
        reindex();
        db_.db().commit();
        store_.remove(row->id);
        return std::nullopt;
    }
    row->localName = QString::number(row->id);
    QSqlQuery upd(db_.db());
    upd.prepare(QStringLiteral("UPDATE folio_items SET localName = ? WHERE id = ?"));
    upd.addBindValue(row->localName);
    upd.addBindValue(row->id);
    upd.exec();
    return row;
}

bool FolioRepository::ensureLocal(const FolioItem &item, const QString &fallback) {
    if (store_.exists(item.id)) {
        if (item.localName.isEmpty()) {
            QSqlQuery q(db_.db());
            q.prepare(QStringLiteral("UPDATE folio_items SET localName = ? WHERE id = ?"));
            q.addBindValue(QString::number(item.id));
            q.addBindValue(item.id);
            q.exec();
        }
        return true;
    }
    QStringList sources;
    if (!fallback.isEmpty()) sources << fallback;
    if (!item.uri.isEmpty() && item.uri != fallback) sources << item.uri;
    for (const QString &src : sources) {
        if (store_.copyFrom(src, item.id)) {
            QSqlQuery q(db_.db());
            q.prepare(QStringLiteral("UPDATE folio_items SET localName = ? WHERE id = ?"));
            q.addBindValue(QString::number(item.id));
            q.addBindValue(item.id);
            q.exec();
            return true;
        }
    }
    return false;
}

void FolioRepository::saveSnapshot(qint64 id, const QString &zoomMode, float customZoomPercent, bool lockZoom,
                                   bool lockSideScroll, bool originalColors, int pageIndex, float scrollX,
                                   float scrollY, const QString &pageRotations) {
    QSqlQuery q(db_.db());
    q.prepare(QStringLiteral(
        "UPDATE folio_items SET zoomMode = ?, customZoomPercent = ?, lockZoom = ?, lockSideScroll = ?, "
        "originalColors = ?, lastPageIndex = ?, lastScrollXPdf = ?, lastScrollYPdf = ?, pageRotations = ? "
        "WHERE id = ?"));
    q.addBindValue(zoomMode);
    q.addBindValue(customZoomPercent);
    q.addBindValue(lockZoom ? 1 : 0);
    q.addBindValue(lockSideScroll ? 1 : 0);
    q.addBindValue(originalColors ? 1 : 0);
    q.addBindValue(pageIndex);
    q.addBindValue(scrollX);
    q.addBindValue(scrollY);
    q.addBindValue(pageRotations.isEmpty() ? QStringLiteral("[]") : pageRotations);
    q.addBindValue(id);
    q.exec();
}

void FolioRepository::saveRotations(qint64 id, const QString &pageRotations) {
    QSqlQuery q(db_.db());
    q.prepare(QStringLiteral("UPDATE folio_items SET pageRotations = ? WHERE id = ?"));
    q.addBindValue(pageRotations.isEmpty() ? QStringLiteral("[]") : pageRotations);
    q.addBindValue(id);
    q.exec();
}

void FolioRepository::setHidden(const QVector<qint64> &ids, bool hidden) {
    if (ids.isEmpty()) return;
    db_.db().transaction();
    for (qint64 id : ids) {
        QSqlQuery q(db_.db());
        q.prepare(QStringLiteral("UPDATE folio_items SET hidden = ? WHERE id = ?"));
        q.addBindValue(hidden ? 1 : 0);
        q.addBindValue(id);
        q.exec();
    }
    db_.db().commit();
}

void FolioRepository::setStartItem(qint64 id) {
    db_.db().transaction();
    QSqlQuery clear(db_.db());
    clear.exec(QStringLiteral("UPDATE folio_items SET isStartItem = 0"));
    QSqlQuery mark(db_.db());
    mark.prepare(QStringLiteral("UPDATE folio_items SET isStartItem = 1 WHERE id = ?"));
    mark.addBindValue(id);
    mark.exec();
    db_.db().commit();
}

void FolioRepository::remove(const QVector<qint64> &ids) {
    if (ids.isEmpty()) return;
    QVector<qint64> removable;
    for (qint64 id : ids) {
        if (store_.remove(id)) removable.append(id);
        else qWarning("folio: could not delete file of item %lld", static_cast<long long>(id));
    }
    db_.db().transaction();
    for (qint64 id : removable) {
        QSqlQuery q(db_.db());
        q.prepare(QStringLiteral("DELETE FROM folio_items WHERE id = ?"));
        q.addBindValue(id);
        q.exec();
    }
    reindex();
    db_.db().commit();
}

void FolioRepository::reorder(const QVector<qint64> &orderedIds) {
    db_.db().transaction();
    for (int i = 0; i < orderedIds.size(); ++i) {
        QSqlQuery q(db_.db());
        q.prepare(QStringLiteral("UPDATE folio_items SET sortOrder = ? WHERE id = ?"));
        q.addBindValue(i);
        q.addBindValue(orderedIds[i]);
        q.exec();
    }
    db_.db().commit();
}

void FolioRepository::reindex() {
    const auto items = all();
    for (int i = 0; i < items.size(); ++i) {
        if (items[i].sortOrder == i) continue;
        QSqlQuery q(db_.db());
        q.prepare(QStringLiteral("UPDATE folio_items SET sortOrder = ? WHERE id = ?"));
        q.addBindValue(i);
        q.addBindValue(items[i].id);
        q.exec();
    }
}

} // namespace papercut
