#include "CombineController.h"

#include "AppServices.h"
#include "core/AppPaths.h"
#include "core/ErrorMessages.h"
#include "data/DocumentPaths.h"
#include "pdf/MuPdfEngine.h"
#include "pdf/PdfComposer.h"

#include <QFileInfo>
#include <QMetaObject>
#include <QPointer>
#include <thread>

namespace papercut {

void CombineController::addPaths(const QStringList &paths) {
    for (const QString &p : paths) {
        const PathLabel label = DocumentPaths::label(p);
        QVariantMap m;
        m.insert(QStringLiteral("path"), p);
        m.insert(QStringLiteral("displayName"), label.displayName);
        m.insert(QStringLiteral("displayPath"), label.displayPath);
        m_entries.append(m);
    }
    emit entriesChanged();
}

void CombineController::setEntries(const QVariantList &entries) {
    m_entries = entries;
    emit entriesChanged();
}

void CombineController::removeAt(int index) {
    if (index < 0 || index >= m_entries.size()) return;
    m_entries.removeAt(index);
    emit entriesChanged();
}

void CombineController::clear() {
    m_entries.clear();
    emit entriesChanged();
}

void CombineController::move(int from, int to) {
    const int n = int(m_entries.size());
    if (from < 0 || from >= n || to < 0 || to >= n || from == to) return;
    m_entries.move(from, to);
    emit entriesChanged();
}

void CombineController::combine() {
    if (!m_busy.isEmpty()) return;
    if (m_entries.isEmpty()) {
        emit toast(errorMessage(QStringLiteral("No documents")));
        return;
    }
    QList<ComposeSource> sources;
    for (const QVariant &e : m_entries) {
        const QString path = e.toMap().value(QStringLiteral("path")).toString();
        if (!QFileInfo::exists(path)) {
            emit toast(errorMessage(QStringLiteral("OPEN")));
            return;
        }
        sources.append({path, QString()});
    }
    m_busy = QStringLiteral("Building PDF");
    emit busyChanged();
    const QString dest = AppPaths::exportPdf();
    const QPointer<CombineController> self(this);
    std::thread([self, sources, dest] {
        QString err;
        try {
            PdfComposer::combine(sources, 1.f, dest);
        } catch (const EngineError &e) {
            err = e.code();
        } catch (...) {
            err = QStringLiteral("EXPORT");
        }
        if (!self) return;
        QMetaObject::invokeMethod(self.data(), [self, dest, err] {
            if (!self) return;
            self->m_busy.clear();
            emit self->busyChanged();
            if (err.isEmpty()) emit self->exportReady(dest);
            else emit self->toast(errorMessage(err));
        }, Qt::QueuedConnection);
    }).detach();
}

} // namespace papercut
