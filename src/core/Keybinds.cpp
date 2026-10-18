#include "Keybinds.h"

#include "AppPaths.h"

#include <QDir>
#include <QEvent>
#include <QFile>
#include <QFileInfo>
#include <QGuiApplication>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QKeyCombination>
#include <QKeyEvent>
#include <QKeySequence>
#include <QMetaObject>
#include <QMouseEvent>
#include <QRegularExpression>
#include <QSaveFile>
#include <QWindow>

namespace papercut {

Keybinds *Keybinds::s_instance = nullptr;

Keybinds *Keybinds::instance() { return s_instance; }

Keybinds *Keybinds::create() {
    if (s_instance) return s_instance;
    s_instance = new Keybinds();
    return s_instance;
}

Keybinds::Keybinds(QObject *parent) : QObject(parent) {
    loadDefaults();
    load();
    if (!QFileInfo::exists(AppPaths::keybinds())) save();
    m_saved = m_actions;
}

QVector<Keybinds::Action> Keybinds::catalog() {
    auto a = [](const char *id, const char *label, const char *group, const char *p, const char *s = "") {
        Action x;
        x.id = QString::fromLatin1(id);
        x.label = QString::fromLatin1(label);
        x.group = QString::fromLatin1(group);
        x.seq[0] = QString::fromLatin1(p);
        x.seq[1] = QString::fromLatin1(s);
        return x;
    };
    return {
        a("open", "Open", "Files", "Ctrl+O"),
        a("closeDocument", "Close document", "Files", "Ctrl+W"),
        a("quit", "Quit", "Files", "Ctrl+Q"),
        a("shareFull", "Share full", "Files", ""),
        a("sharePartial", "Share partial", "Files", "Ctrl+Shift+S"),
        a("viewer", "Viewer", "Screens", "Ctrl+D"),
        a("folio", "Folio", "Screens", "Ctrl+E"),
        a("bookmarks", "Bookmarks", "Screens", "Ctrl+B"),
        a("settings", "Settings", "Screens", "Ctrl+M"),
        a("combine", "Combine", "Screens", "Ctrl+U"),
        a("openSidebar", "Toggle sidebar", "Screens", "~"),
        a("toggleTitleBar", "Toggle title bar", "Screens", "Ctrl+Shift+T"),
        a("toggleMenuBar", "Toggle menu bar", "Screens", "F1"),
        a("fullScreen", "Full screen", "Screens", "F11"),
        a("togglePresentation", "Toggle presentation", "Screens", ""),
        a("grabWindow", "Grab window", "Screens", "Alt"),
        a("rotateLeft", "Rotate left", "Document", "Comma"),
        a("rotateRight", "Rotate right", "Document", "Period"),
        a("rotateAllLeft", "Rotate all left", "Document", "Ctrl+Comma"),
        a("rotateAllRight", "Rotate all right", "Document", "Ctrl+Period"),
        a("find", "Find", "Document", "/", "Ctrl+F"),
        a("colourMode", "Colour mode", "Document", "F2", "Ctrl+'"),
        a("zoomIn", "Zoom in", "Document", "Ctrl+WheelUp"),
        a("zoomOut", "Zoom out", "Document", "Ctrl+WheelDown"),
        a("fitWidth", "Fit width", "Document", "Ctrl+H"),
        a("copy", "Copy selection", "Document", "Ctrl+C"),
        a("selectAll", "Select all", "Document", "Ctrl+A"),
        a("goToPage", "Go to page", "Navigation", "Ctrl+G"),
        a("lockHScroll", "Toggle horizontal scrolling", "Navigation", ""),
        a("scrollUp", "Scroll up", "Navigation", "Up"),
        a("scrollDown", "Scroll down", "Navigation", "Down"),
        a("scrollLeft", "Scroll left", "Navigation", "Left"),
        a("scrollRight", "Scroll right", "Navigation", "Right"),
        a("pageDown", "Page down", "Navigation", "PgDown", "Space"),
        a("pageUp", "Page up", "Navigation", "PgUp"),
        a("goToStart", "Go to start", "Navigation", "Home"),
        a("goToEnd", "Go to end", "Navigation", "End"),
        a("bookmark1", "Bookmark 1", "Bookmarks", "Ctrl+1"),
        a("bookmark2", "Bookmark 2", "Bookmarks", "Ctrl+2"),
        a("bookmark3", "Bookmark 3", "Bookmarks", "Ctrl+3"),
        a("bookmark4", "Bookmark 4", "Bookmarks", "Ctrl+4"),
        a("bookmark5", "Bookmark 5", "Bookmarks", "Ctrl+5"),
        a("bookmark6", "Bookmark 6", "Bookmarks", "Ctrl+6"),
        a("bookmark7", "Bookmark 7", "Bookmarks", "Ctrl+7"),
        a("bookmark8", "Bookmark 8", "Bookmarks", "Ctrl+8"),
        a("bookmark9", "Bookmark 9", "Bookmarks", "Ctrl+9"),
        a("bookmark0", "Bookmark 10", "Bookmarks", "Ctrl+0"),
    };
}

void Keybinds::loadDefaults() { m_actions = catalog(); }

QString Keybinds::normalize(const QString &seq) {
    const QString t = seq.trimmed();
    if (t.isEmpty()) return {};
    if (t.contains(QLatin1String("Wheel"), Qt::CaseInsensitive)) {
        QStringList parts = t.split(QLatin1Char('+'), Qt::SkipEmptyParts);
        QStringList out;
        bool up = false, down = false;
        for (QString p : parts) {
            p = p.trimmed();
            if (p.compare(QLatin1String("Control"), Qt::CaseInsensitive) == 0
                || p.compare(QLatin1String("Ctrl"), Qt::CaseInsensitive) == 0)
                out << QStringLiteral("Ctrl");
            else if (p.compare(QLatin1String("Alt"), Qt::CaseInsensitive) == 0)
                out << QStringLiteral("Alt");
            else if (p.compare(QLatin1String("Shift"), Qt::CaseInsensitive) == 0)
                out << QStringLiteral("Shift");
            else if (p.compare(QLatin1String("Meta"), Qt::CaseInsensitive) == 0
                     || p.compare(QLatin1String("Win"), Qt::CaseInsensitive) == 0)
                out << QStringLiteral("Meta");
            else if (p.compare(QLatin1String("WheelUp"), Qt::CaseInsensitive) == 0
                     || p.compare(QLatin1String("Wheel Up"), Qt::CaseInsensitive) == 0)
                up = true;
            else if (p.compare(QLatin1String("WheelDown"), Qt::CaseInsensitive) == 0
                     || p.compare(QLatin1String("Wheel Down"), Qt::CaseInsensitive) == 0)
                down = true;
        }
        if (!up && !down) return {};
        out << (up ? QStringLiteral("WheelUp") : QStringLiteral("WheelDown"));
        return out.join(QLatin1Char('+'));
    }
    const QKeySequence ks(t, QKeySequence::PortableText);
    if (ks.isEmpty()) return t;
    QString out = ks.toString(QKeySequence::PortableText);
    if (out.endsWith(QLatin1Char(','))) {
        out.chop(1);
        if (out.endsWith(QLatin1Char('+'))) return out + QStringLiteral("Comma");
        return QStringLiteral("Comma");
    }
    if (out.endsWith(QLatin1Char('.'))) {
        out.chop(1);
        if (out.endsWith(QLatin1Char('+'))) return out + QStringLiteral("Period");
        return QStringLiteral("Period");
    }
    if (out == QLatin1Char('`') || out == QLatin1Char('~')) return QStringLiteral("~");
    if (out.endsWith(QLatin1String("+`"))) {
        out.chop(1);
        return out + QLatin1Char('~');
    }
    return out;
}

void Keybinds::load() {
    QFile f(AppPaths::keybinds());
    if (!f.open(QIODevice::ReadOnly)) return;
    const QJsonDocument doc = QJsonDocument::fromJson(f.readAll());
    if (!doc.isObject()) return;
    const QJsonObject o = doc.object();
    for (Action &a : m_actions) {
        if (!o.contains(a.id)) continue;
        const QJsonValue v = o.value(a.id);
        if (!v.isArray()) continue;
        const auto arr = v.toArray();
        if (arr.size() > 0) a.seq[0] = normalize(arr.at(0).toString());
        if (arr.size() > 1) a.seq[1] = normalize(arr.at(1).toString());
    }
}

void Keybinds::save() const {
    QDir().mkpath(AppPaths::configRoot());
    QJsonObject o;
    for (const Action &a : m_actions) {
        QJsonArray arr;
        arr.append(a.seq[0]);
        arr.append(a.seq[1]);
        o.insert(a.id, arr);
    }
    QSaveFile f(AppPaths::keybinds());
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        qWarning("papercut: could not write keybinds in the data directory");
        return;
    }
    f.write(QJsonDocument(o).toJson(QJsonDocument::Indented));
    f.commit();
}

Keybinds::Action *Keybinds::find(const QString &id) {
    for (Action &a : m_actions)
        if (a.id == id) return &a;
    return nullptr;
}

const Keybinds::Action *Keybinds::find(const QString &id) const {
    for (const Action &a : m_actions)
        if (a.id == id) return &a;
    return nullptr;
}

QVariantList Keybinds::actions() const {
    QVariantList out;
    QString prev;
    for (const Action &a : m_actions) {
        QVariantMap m;
        m.insert(QStringLiteral("id"), a.id);
        m.insert(QStringLiteral("label"), a.label);
        m.insert(QStringLiteral("group"), a.group);
        m.insert(QStringLiteral("header"), a.group != prev ? a.group : QString());
        m.insert(QStringLiteral("primary"), a.seq[0]);
        m.insert(QStringLiteral("secondary"), a.seq[1]);
        m.insert(QStringLiteral("primaryDisplay"), display(a.seq[0]));
        m.insert(QStringLiteral("secondaryDisplay"), display(a.seq[1]));
        out.append(m);
        prev = a.group;
    }
    return out;
}

QVariantList Keybinds::shortcutEntries() const {
    QVariantList out;
    for (const Action &a : m_actions) {
        if (isHoldAction(a.id)) continue;
        for (int i = 0; i < 2; ++i) {
            if (a.seq[i].isEmpty() || isWheelSequence(a.seq[i]) || usesWindowKeyEvent(a.seq[i])) continue;
            QVariantMap m;
            m.insert(QStringLiteral("id"), a.id);
            m.insert(QStringLiteral("sequence"), a.seq[i]);
            m.insert(QStringLiteral("repeat"), a.id.startsWith(QLatin1String("scroll")));
            out.append(m);
        }
    }
    return out;
}

void Keybinds::setCapturing(bool on) {
    if (m_capturing == on) return;
    m_capturing = on;
    emit capturingChanged();
}

QString Keybinds::sequence(const QString &id, int slot) const {
    const Action *a = find(id);
    if (!a) return {};
    return a->seq[slot ? 1 : 0];
}

QString Keybinds::actionForWheel(int modifiers, bool up) const {
    const QString seq = sequenceFromWheel(modifiers, up);
    for (const Action &a : m_actions) {
        for (int i = 0; i < 2; ++i) {
            if (!a.seq[i].isEmpty() && a.seq[i].compare(seq, Qt::CaseInsensitive) == 0) return a.id;
        }
    }
    return {};
}

void Keybinds::setBinding(const QString &id, int slot, const QString &seq) {
    Action *target = find(id);
    if (!target) return;
    const QString n = normalize(seq);
    slot = slot ? 1 : 0;
    if (!n.isEmpty()) {
        for (Action &a : m_actions) {
            for (int i = 0; i < 2; ++i) {
                if (&a == target && i == slot) continue;
                if (a.seq[i].compare(n, Qt::CaseInsensitive) == 0) a.seq[i].clear();
            }
        }
    }
    target->seq[slot] = n;
    m_dirty = true;
    emit changed();
}

void Keybinds::clearBinding(const QString &id, int slot) { setBinding(id, slot, {}); }

void Keybinds::resetDefaults() {
    loadDefaults();
    m_dirty = true;
    emit changed();
}

void Keybinds::beginEdit() {
    m_saved = m_actions;
    m_dirty = false;
}

void Keybinds::revert() {
    m_actions = m_saved;
    m_dirty = false;
    emit changed();
}

void Keybinds::commit() {
    save();
    m_saved = m_actions;
    m_dirty = false;
}

bool Keybinds::matchesFilter(const QString &haystack, const QString &query) const {
    const QString q = query.trimmed();
    if (q.isEmpty()) return true;
    static const QRegularExpression ws(QStringLiteral("\\s+"));
    const QStringList tokens = q.split(ws, Qt::SkipEmptyParts);
    for (const QString &tok : tokens) {
        if (!haystack.contains(tok, Qt::CaseInsensitive)) return false;
    }
    return true;
}

QString Keybinds::display(const QString &seq) const {
    if (seq.isEmpty()) return QStringLiteral("None");
    QString s = seq;
    s.replace(QStringLiteral("WheelUp"), QStringLiteral("Wheel Up"));
    s.replace(QStringLiteral("WheelDown"), QStringLiteral("Wheel Down"));
    s.replace(QStringLiteral("Comma"), QStringLiteral(","));
    s.replace(QStringLiteral("Period"), QStringLiteral("."));
    s.replace(QStringLiteral("PgDown"), QStringLiteral("Page Down"));
    s.replace(QStringLiteral("PgUp"), QStringLiteral("Page Up"));
    s.replace(QLatin1Char('+'), QStringLiteral(" + "));
    return s;
}

QString Keybinds::sequenceFromKey(int key, int modifiers) const {
    if (key == Qt::Key_Escape || key == Qt::Key_unknown) return {};
    if (key == Qt::Key_Control || key == Qt::Key_Shift || key == Qt::Key_Alt || key == Qt::Key_Meta
        || key == Qt::Key_AltGr)
        return {};
    if (key == Qt::Key_QuoteLeft || key == Qt::Key_AsciiTilde) {
        QStringList parts;
        const auto mods = static_cast<Qt::KeyboardModifiers>(modifiers) & ~Qt::KeypadModifier;
        if (mods & Qt::ControlModifier) parts << QStringLiteral("Ctrl");
        if (mods & Qt::AltModifier) parts << QStringLiteral("Alt");
        if (mods & Qt::MetaModifier) parts << QStringLiteral("Meta");
        parts << QStringLiteral("~");
        return parts.join(QLatin1Char('+'));
    }
    const QKeyCombination combo(static_cast<Qt::KeyboardModifiers>(modifiers) & ~Qt::KeypadModifier,
                                static_cast<Qt::Key>(key));
    const QString s = QKeySequence(combo).toString(QKeySequence::PortableText);
    return normalize(s);
}

QString Keybinds::sequenceFromWheel(int modifiers, bool up) const {
    QStringList parts;
    const auto mods = static_cast<Qt::KeyboardModifiers>(modifiers);
    if (mods & Qt::ControlModifier) parts << QStringLiteral("Ctrl");
    if (mods & Qt::AltModifier) parts << QStringLiteral("Alt");
    if (mods & Qt::ShiftModifier) parts << QStringLiteral("Shift");
    if (mods & Qt::MetaModifier) parts << QStringLiteral("Meta");
    parts << (up ? QStringLiteral("WheelUp") : QStringLiteral("WheelDown"));
    return parts.join(QLatin1Char('+'));
}

bool Keybinds::isWheelSequence(const QString &seq) const {
    return seq.contains(QLatin1String("Wheel"), Qt::CaseInsensitive);
}

QString Keybinds::actionForKey(int key, int modifiers) const {
    const QString seq = sequenceFromKey(key, modifiers);
    if (seq.isEmpty()) return {};
    for (const Action &a : m_actions) {
        for (int i = 0; i < 2; ++i) {
            if (!a.seq[i].isEmpty() && a.seq[i].compare(seq, Qt::CaseInsensitive) == 0) return a.id;
        }
    }
    return {};
}

bool Keybinds::usesWindowKeyEvent(const QString &seq) const {
    if (seq.contains(QLatin1String("Comma")) || seq.contains(QLatin1String("Period"))) return true;
    if (seq.contains(QLatin1Char('~')) || seq.contains(QLatin1Char('`'))) return true;
    return seq == QLatin1String("Alt") || seq == QLatin1String("Ctrl") || seq == QLatin1String("Shift")
        || seq == QLatin1String("Meta");
}

bool Keybinds::isHoldAction(const QString &id) const { return id == QLatin1String("grabWindow"); }

QString Keybinds::modifierSequence(int key) const {
    if (key == Qt::Key_Alt || key == Qt::Key_AltGr) return QStringLiteral("Alt");
    if (key == Qt::Key_Control) return QStringLiteral("Ctrl");
    if (key == Qt::Key_Shift) return QStringLiteral("Shift");
    if (key == Qt::Key_Meta) return QStringLiteral("Meta");
    return {};
}

namespace {

bool isModifierKey(int key) {
    return key == Qt::Key_Alt || key == Qt::Key_AltGr || key == Qt::Key_Control || key == Qt::Key_Shift
        || key == Qt::Key_Meta;
}

bool parseGrab(const QString &seq, Qt::KeyboardModifiers *mods, int *key) {
    *mods = Qt::NoModifier;
    *key = 0;
    if (seq.isEmpty()) return false;
    if (seq == QLatin1String("Alt")) {
        *mods = Qt::AltModifier;
        *key = Qt::Key_Alt;
        return true;
    }
    if (seq == QLatin1String("Ctrl")) {
        *mods = Qt::ControlModifier;
        *key = Qt::Key_Control;
        return true;
    }
    if (seq == QLatin1String("Shift")) {
        *mods = Qt::ShiftModifier;
        *key = Qt::Key_Shift;
        return true;
    }
    if (seq == QLatin1String("Meta")) {
        *mods = Qt::MetaModifier;
        *key = Qt::Key_Meta;
        return true;
    }
    QString portable = seq;
    portable.replace(QLatin1String("Comma"), QStringLiteral(","));
    portable.replace(QLatin1String("Period"), QStringLiteral("."));
    const QKeySequence ks(portable, QKeySequence::PortableText);
    if (ks.count() < 1) return false;
    const QKeyCombination c = ks[0];
    *mods = c.keyboardModifiers();
    *key = int(c.key());
    return *key != 0;
}

} // namespace

QString Keybinds::grabSequence() const {
    const Action *a = find(QStringLiteral("grabWindow"));
    if (!a) return {};
    if (!a->seq[0].isEmpty()) return a->seq[0];
    return a->seq[1];
}

bool Keybinds::grabPressMatches(const QKeyEvent *e) const {
    Qt::KeyboardModifiers mods = Qt::NoModifier;
    int key = 0;
    if (!parseGrab(grabSequence(), &mods, &key)) return false;
    if (!(e->key() == key || (key == Qt::Key_Alt && e->key() == Qt::Key_AltGr))) return false;
    if (isModifierKey(key)) return true;
    return (e->modifiers() & mods) == mods;
}

bool Keybinds::grabReleaseMatches(const QKeyEvent *e) const {
    Qt::KeyboardModifiers mods = Qt::NoModifier;
    int key = 0;
    if (!parseGrab(grabSequence(), &mods, &key)) return false;
    if (e->key() == key || (key == Qt::Key_Alt && e->key() == Qt::Key_AltGr)) return true;
    if ((mods & Qt::ControlModifier) && e->key() == Qt::Key_Control) return true;
    if ((mods & Qt::AltModifier) && (e->key() == Qt::Key_Alt || e->key() == Qt::Key_AltGr)) return true;
    if ((mods & Qt::ShiftModifier) && e->key() == Qt::Key_Shift) return true;
    if ((mods & Qt::MetaModifier) && e->key() == Qt::Key_Meta) return true;
    return false;
}

void Keybinds::watchWindow(QObject *window) {
    auto *w = qobject_cast<QWindow *>(window);
    if (!w) return;
    m_window = w;
    QGuiApplication::instance()->installEventFilter(this);
    QObject::connect(m_window, &QWindow::activeChanged, this, [this]() {
        if (m_window && !m_window->isActive()) m_grabHeld = false;
    });
}

bool Keybinds::eventFilter(QObject *watched, QEvent *event) {
    Q_UNUSED(watched);
    if (m_capturing || !m_window) return false;
    const auto type = event->type();
    if (type == QEvent::KeyPress || type == QEvent::ShortcutOverride || type == QEvent::KeyRelease) {
        if (QGuiApplication::focusWindow() != m_window) return false;
        const auto *ke = static_cast<QKeyEvent *>(event);
        if (ke->isAutoRepeat()) return false;
        if (type == QEvent::KeyRelease) {
            if (grabReleaseMatches(ke)) m_grabHeld = false;
        } else if (grabPressMatches(ke)) {
            m_grabHeld = true;
        }
        return false;
    }
    if (type == QEvent::MouseButtonPress && m_grabHeld) {
        const auto *me = static_cast<QMouseEvent *>(event);
        if (me->button() != Qt::LeftButton) return false;
        QWindow *over = QGuiApplication::topLevelAt(me->globalPosition().toPoint());
        if (over != m_window && QGuiApplication::focusWindow() != m_window) return false;
        const QPointF g = me->globalPosition();
        if (!QMetaObject::invokeMethod(m_window, "beginSystemMoveFromZoomed", Q_ARG(QVariant, g.x()),
                                       Q_ARG(QVariant, g.y()), Q_ARG(QVariant, true)))
            m_window->startSystemMove();
        return true;
    }
    return false;
}

} // namespace papercut
