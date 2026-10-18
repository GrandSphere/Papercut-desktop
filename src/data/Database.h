#pragma once

#include "ViewerSettings.h"

#include <QSqlDatabase>
#include <QString>
#include <QVector>

namespace papercut {

class Database {
public:
    bool open(const QString &path);
    QSqlDatabase &db() { return db_; }
    const QSqlDatabase &db() const { return db_; }

private:
    QSqlDatabase db_;
};

ViewerSettings settingsFromQuery(const class QSqlQuery &q);
void bindSettings(class QSqlQuery &q, const ViewerSettings &s);
ViewerSettings loadSettings(QSqlDatabase &db);
bool saveSettings(QSqlDatabase &db, const ViewerSettings &s);

} // namespace papercut
