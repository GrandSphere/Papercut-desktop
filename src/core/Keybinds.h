#pragma once

#include <QObject>
#include <QPointer>
#include <QString>
#include <QVariantList>
#include <QVector>

class QEvent;
class QKeyEvent;
class QWindow;

namespace papercut {

class Keybinds : public QObject {
    Q_OBJECT
    Q_PROPERTY(QVariantList actions READ actions NOTIFY changed)
    Q_PROPERTY(QVariantList shortcutEntries READ shortcutEntries NOTIFY changed)
    Q_PROPERTY(bool capturing READ capturing WRITE setCapturing NOTIFY capturingChanged)

public:
    struct Action {
        QString id;
        QString label;
        QString group;
        QString seq[2];
    };

    static Keybinds *instance();
    static Keybinds *create();

    QVariantList actions() const;
    QVariantList shortcutEntries() const;

    bool capturing() const { return m_capturing; }
    void setCapturing(bool on);

    QString sequence(const QString &id, int slot) const;
    QString actionForWheel(int modifiers, bool up) const;

    Q_INVOKABLE void setBinding(const QString &id, int slot, const QString &seq);
    Q_INVOKABLE void clearBinding(const QString &id, int slot);
    Q_INVOKABLE void resetDefaults();
    Q_INVOKABLE void beginEdit();
    Q_INVOKABLE void revert();
    Q_INVOKABLE void commit();
    Q_INVOKABLE bool matchesFilter(const QString &haystack, const QString &query) const;
    Q_INVOKABLE QString display(const QString &seq) const;
    Q_INVOKABLE QString sequenceFromKey(int key, int modifiers) const;
    Q_INVOKABLE QString sequenceFromWheel(int modifiers, bool up) const;
    Q_INVOKABLE bool isWheelSequence(const QString &seq) const;
    Q_INVOKABLE QString actionForKey(int key, int modifiers) const;
    Q_INVOKABLE bool usesWindowKeyEvent(const QString &seq) const;
    Q_INVOKABLE bool isHoldAction(const QString &id) const;
    Q_INVOKABLE QString modifierSequence(int key) const;
    Q_INVOKABLE void watchWindow(QObject *window);

    bool eventFilter(QObject *watched, QEvent *event) override;

signals:
    void changed();
    void capturingChanged();

private:
    explicit Keybinds(QObject *parent = nullptr);
    void loadDefaults();
    void load();
    void save() const;
    Action *find(const QString &id);
    const Action *find(const QString &id) const;
    static QVector<Action> catalog();
    static QString normalize(const QString &seq);
    QString grabSequence() const;
    bool grabPressMatches(const QKeyEvent *e) const;
    bool grabReleaseMatches(const QKeyEvent *e) const;

    QVector<Action> m_actions;
    QVector<Action> m_saved;
    bool m_capturing = false;
    mutable bool m_dirty = false;
    bool m_grabHeld = false;
    QPointer<QWindow> m_window;
    static Keybinds *s_instance;
};

} // namespace papercut
