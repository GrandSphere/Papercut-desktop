#pragma once

#include <QObject>
#include <QStringList>
#include <QVariantList>

namespace papercut {

class CombineController : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString busy READ busy NOTIFY busyChanged)
    Q_PROPERTY(QVariantList entries READ entries NOTIFY entriesChanged)

public:
    explicit CombineController(QObject *parent = nullptr) : QObject(parent) {}

    QString busy() const { return m_busy; }
    QVariantList entries() const { return m_entries; }

    Q_INVOKABLE void addPaths(const QStringList &paths);
    Q_INVOKABLE void setEntries(const QVariantList &entries);
    Q_INVOKABLE void removeAt(int index);
    Q_INVOKABLE void move(int from, int to);
    Q_INVOKABLE void clear();
    Q_INVOKABLE void combine();

signals:
    void busyChanged();
    void entriesChanged();
    void toast(const QString &message);
    void exportReady(const QString &path);

private:
    QString m_busy;
    QVariantList m_entries;
};

} // namespace papercut
