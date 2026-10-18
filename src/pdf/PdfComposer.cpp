#include "PdfComposer.h"

#include "DocumentMagic.h"
#include "EngineConstants.h"
#include "MuPdfEngine.h"
#include "pc_mupdf.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <vector>

namespace papercut {

namespace {

QString detectMagic(const ComposeSource &s) {
    if (!s.magic.isEmpty()) return s.magic;
    QFile f(s.path);
    QByteArray head;
    if (f.open(QIODevice::ReadOnly)) head = f.read(1024);
    return DocumentMagic::pick(head, QString(), QFileInfo(s.path).fileName());
}

void throwFor(int rc) {
    switch (rc) {
    case PC_OK: return;
    case PC_PASSWORD: throw EngineError(QStringLiteral("PASSWORD"));
    case PC_NOT_PDF: throw EngineError(QStringLiteral("NOT_PDF"));
    default: throw EngineError(QStringLiteral("EXPORT"));
    }
}

void run(const QList<ComposeSource> &sources, const int *pages, int npages, float em,
         const QString &password, const QString &dest, const int *rotates, int nrotates) {
    QDir().mkpath(QFileInfo(dest).absolutePath());
    const QString tmpDir = QFileInfo(dest).absolutePath();

    std::vector<QByteArray> paths, magics;
    for (const ComposeSource &s : sources) {
        paths.push_back(s.path.toUtf8());
        magics.push_back(detectMagic(s).toUtf8());
    }
    std::vector<const char *> pp, mp;
    for (size_t i = 0; i < paths.size(); ++i) {
        pp.push_back(paths[i].constData());
        mp.push_back(magics[i].constData());
    }
    const QByteArray pw = password.toUtf8();
    const QByteArray d = dest.toUtf8();
    const QByteArray t = tmpDir.toUtf8();
    const int rc = pc_compose(pp.data(), mp.data(), int(pp.size()), pages, npages, em,
                              password.isNull() ? nullptr : pw.constData(), d.constData(), t.constData(),
                              rotates, nrotates);
    throwFor(rc);
}

} // namespace

void PdfComposer::extractPages(const QString &srcPath, const QList<int> &pages, const QString &dest,
                               const QString &password, const QString &srcMagic, const QList<int> &rotates) {
    if (pages.isEmpty()) throw EngineError(QStringLiteral("No pages"));
    ComposeSource s{srcPath, srcMagic};
    run({s}, pages.constData(), int(pages.size()), 12.f, password, dest,
        rotates.isEmpty() ? nullptr : rotates.constData(), int(rotates.size()));
}

void PdfComposer::combine(const QList<ComposeSource> &sources, float fontScale, const QString &dest,
                          const QString &password, const QList<int> &rotates) {
    if (sources.isEmpty()) throw EngineError(QStringLiteral("No documents"));
    run(sources, nullptr, 0, layoutEm(fontScale), password, dest,
        rotates.isEmpty() ? nullptr : rotates.constData(), int(rotates.size()));
}

} // namespace papercut
