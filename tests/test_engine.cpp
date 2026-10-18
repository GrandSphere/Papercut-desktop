#include "pdf/MuPdfEngine.h"
#include "pdf/PdfComposer.h"

#include <QByteArray>
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QImage>
#include <QTemporaryDir>
#include <cstdio>
#include <cstdlib>

using namespace papercut;

static int g_fail = 0;
#define CHECK(cond)                                                                      \
    do {                                                                                 \
        if (!(cond)) {                                                                   \
            std::fprintf(stderr, "FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond);         \
            ++g_fail;                                                                    \
        }                                                                                \
    } while (0)

static QByteArray makePdf(int pages, const QByteArray &text) {
    QByteArray out = "%PDF-1.4\n";
    QList<int> offsets;
    auto add = [&](const QByteArray &obj) {
        offsets << int(out.size());
        out += QByteArray::number(offsets.size()) + " 0 obj\n" + obj + "\nendobj\n";
    };
    add("<< /Type /Catalog /Pages 2 0 R >>");
    QByteArray kids;
    for (int i = 0; i < pages; ++i) kids += QByteArray::number(4 + i * 2) + " 0 R ";
    add("<< /Type /Pages /Kids [" + kids + "] /Count " + QByteArray::number(pages) + " >>");
    add("<< /Type /Font /Subtype /Type1 /BaseFont /Helvetica >>");
    for (int i = 0; i < pages; ++i) {
        const QByteArray content = "BT /F1 24 Tf 20 100 Td (" + text + " " + QByteArray::number(i + 1) + ") Tj ET";
        add("<< /Type /Page /Parent 2 0 R /MediaBox [0 0 200 150] /Resources << /Font << /F1 3 0 R >> >> /Contents "
            + QByteArray::number(5 + i * 2) + " 0 R >>");
        add("<< /Length " + QByteArray::number(content.size()) + " >>\nstream\n" + content + "\nendstream");
    }
    const int xref = int(out.size());
    out += "xref\n0 " + QByteArray::number(offsets.size() + 1) + "\n0000000000 65535 f \n";
    for (int o : offsets) out += QByteArray::number(o).rightJustified(10, '0') + " 00000 n \n";
    out += "trailer\n<< /Size " + QByteArray::number(offsets.size() + 1) + " /Root 1 0 R /Info << /Title (Secret) >> >>\n";
    out += "startxref\n" + QByteArray::number(xref) + "\n%%EOF\n";
    return out;
}

static void writeFile(const QString &path, const QByteArray &data) {
    QFile f(path);
    CHECK(f.open(QIODevice::WriteOnly));
    f.write(data);
}

int main(int argc, char **argv) {
    QCoreApplication app(argc, argv);
    QTemporaryDir dir;
    CHECK(dir.isValid());
    const QString a = dir.filePath("a.pdf");
    const QString b = dir.filePath("b.pdf");
    writeFile(a, makePdf(3, "Hello"));
    writeFile(b, makePdf(2, "World"));

    MuPdfEngine eng;
    CHECK(!eng.isOpened());
    eng.open(a, 1.f, 1);
    CHECK(eng.isOpened());
    CHECK(eng.pageCount() == 3);
    CHECK(eng.mime() == "application/pdf");
    const PageInfo p0 = eng.ensurePageSize(0).value();
    CHECK(p0.width == 200.f && p0.height == 150.f);
    CHECK(eng.measureRemainingPages().size() == 3);

    const QImage img = eng.render(0, 400, int(0xFF000000u), int(0xFFFFFFFFu), 0.f, true);
    CHECK(img.width() == 400 && img.height() == 300);
    int dark = 0;
    for (int y = 0; y < img.height(); ++y)
        for (int x = 0; x < img.width(); ++x)
            if (qRed(img.pixel(x, y)) < 80) ++dark;
    CHECK(dark > 100);
    CHECK(qRed(img.pixel(0, 0)) == 255);

    const QImage inv = eng.render(0, 400, int(0xFFFFFFFFu), int(0xFF000000u), 0.f, false);
    CHECK(qRed(inv.pixel(0, 0)) < 20);
    int bright = 0;
    for (int y = 0; y < inv.height(); ++y)
        for (int x = 0; x < inv.width(); ++x)
            if (qRed(inv.pixel(x, y)) > 200) ++bright;
    CHECK(bright > 100);

    // Pixel budget clamps huge requests.
    const QImage big = eng.render(0, 4096, int(0xFF000000u), int(0xFFFFFFFFu), 0.f, true);
    CHECK(qint64(big.width()) * big.height() <= 8000000LL + 4096);

    const QImage rot = eng.render(0, 400, int(0xFF000000u), int(0xFFFFFFFFu), 0.f, true, 1.f, 90);
    CHECK(rot.width() == 400 && rot.height() > 500);

    const PageContent c = eng.pageContent(0);
    QString text;
    for (const CharBox &ch : c.chars) text += ch.c;
    CHECK(text.contains("Hello 1"));
    CHECK(c.chars.first().contains(c.chars.first().ulx + 0.1f, c.chars.first().uly + 0.1f)
          || c.chars.size() > 0);

    CHECK(eng.outline().isEmpty());
    CHECK(MuPdfEngine::pageFromUri("#page=7").value() == 6);
    CHECK(MuPdfEngine::pageFromUri("#3").value() == 2);
    CHECK(!MuPdfEngine::pageFromUri("http://x").has_value());
    CHECK(eng.resolveUri("#page=2").value().first == 1);
    CHECK(!eng.resolveUri("https://example.com").has_value());

    eng.close();
    CHECK(!eng.isOpened());

    bool threw = false;
    try {
        eng.open(dir.filePath("nope.pdf"), 1.f);
    } catch (const EngineError &e) {
        threw = e.code() == "OPEN";
    }
    CHECK(threw);

    const QString out = dir.filePath("out/combined.pdf");
    PdfComposer::combine({{a, {}}, {b, {}}}, 1.f, out);
    MuPdfEngine ce;
    ce.open(out, 1.f);
    CHECK(ce.pageCount() == 5);
    ce.close();
    {
        QFile f(out);
        f.open(QIODevice::ReadOnly);
        const QByteArray bytes = f.readAll();
        CHECK(!bytes.contains("Secret"));
        CHECK(!bytes.contains("/ID"));
    }

    const QString ex = dir.filePath("ex.pdf");
    PdfComposer::extractPages(a, {2, 0}, ex);
    ce.open(ex, 1.f);
    CHECK(ce.pageCount() == 2);
    QString t0;
    for (const CharBox &ch : ce.pageContent(0).chars) t0 += ch.c;
    CHECK(t0.contains("Hello 3"));
    ce.close();

    const QString exr = dir.filePath("ex-rot.pdf");
    PdfComposer::extractPages(a, {0}, exr, QString(), QString(), {90});
    ce.open(exr, 1.f);
    const PageInfo rp = ce.ensurePageSize(0).value();
    CHECK(rp.width == 150.f && rp.height == 200.f);
    ce.close();

    threw = false;
    try {
        PdfComposer::extractPages(a, {}, ex);
    } catch (const EngineError &e) {
        threw = e.code() == "No pages";
    }
    CHECK(threw);

    // PNG merges via document writer, not raw embed.
    QImage png(64, 48, QImage::Format_RGB32);
    png.fill(Qt::red);
    const QString pngPath = dir.filePath("pic.png");
    png.save(pngPath);
    const QString out2 = dir.filePath("mixed.pdf");
    PdfComposer::combine({{a, {}}, {pngPath, {}}}, 1.f, out2);
    ce.open(out2, 1.f);
    CHECK(ce.pageCount() == 4);
    ce.close();

    const QString txt = dir.filePath("hello.txt");
    writeFile(txt, "Hello from text\n");
    MuPdfEngine te;
    te.open(txt, 1.f);
    CHECK(te.isOpened());
    CHECK(te.pageCount() >= 1);
    CHECK(te.mime() == "text/plain");
    const QImage timg = te.render(0, 200, int(0xFF000000u), int(0xFFFFFFFFu), 0.f, true);
    CHECK(timg.width() > 0 && timg.height() > 0);
    te.close();

    const QString md = dir.filePath("hello.md");
    writeFile(md, "# Hello\n\nMarkdown body.\n");
    te.open(md, 1.f);
    CHECK(te.pageCount() >= 1);
    CHECK(te.mime() == "text/markdown");
    te.close();

    const QString withTxt = dir.filePath("with-txt.pdf");
    PdfComposer::combine({{a, {}}, {txt, {}}}, 1.f, withTxt);
    ce.open(withTxt, 1.f);
    CHECK(ce.pageCount() >= 4);
    ce.close();

    if (g_fail == 0) std::puts("ok");
    return g_fail == 0 ? 0 : 1;
}
