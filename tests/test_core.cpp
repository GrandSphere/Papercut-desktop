#include "Version.h"
#include "core/AppPaths.h"
#include "core/Keybinds.h"
#include "core/ErrorMessages.h"
#include "core/ExitCodes.h"
#include "data/BackupJson.h"
#include "data/BookmarkRepository.h"
#include "data/Database.h"
#include "data/DocumentPaths.h"
#include "data/SettingsRepository.h"
#include "data/ViewerSettings.h"
#include "pdf/ColorRemapper.h"
#include "pdf/DocumentFont.h"
#include "pdf/DocumentMagic.h"
#include "pdf/EngineConstants.h"
#include "pdf/PdfModels.h"
#include "pdf/TextFinder.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QTemporaryDir>
#include <QVector>
#include <cmath>
#include <cstdio>

static int fails = 0;

static void check(bool cond, const char *msg) {
    if (!cond) {
        std::fprintf(stderr, "FAIL: %s\n", msg);
        ++fails;
    }
}

int main(int argc, char **argv) {
    QCoreApplication app(argc, argv);

    using namespace papercut;

    check(!QStringLiteral(PAPERCUT_VERSION).isEmpty(), "version");
    check(PAPERCUT_VERSION_CODE > 0, "version code");
    {
        ViewerSettings a, b;
        b.lastPageIndex = 9;
        b.lastScrollYPdf = 12.f;
        b.lastFolioItemId = 3;
        b.lastPdfUri = QStringLiteral("x.pdf");
        check(a.appearanceEqual(b), "session fields are not appearance");
        check(!(a == b), "session fields still differ");
        b.lockSideScroll = true;
        b.lockZoom = true;
        b.zoomMode = QStringLiteral("FIT_PAGE");
        b.customZoomPercent = 140.f;
        b.originalColors = true;
        check(a.appearanceEqual(b), "live view prefs are not appearance");
        check(!(a == b), "live view prefs still differ");
        b.rememberLastZoom = true;
        check(!a.appearanceEqual(b), "rememberLastZoom is appearance");
        ViewerSettings c, d;
        d.startupZoomMode = QStringLiteral("FIT_PAGE");
        check(!c.appearanceEqual(d), "startupZoomMode is appearance");
        d = ViewerSettings{};
        d.startupZoomPercent = 150.f;
        check(!c.appearanceEqual(d), "startupZoomPercent is appearance");
    }
    {
        ViewerSettings s;
        s.originalColors = true;
        s.startupZoomMode = QStringLiteral("FIT_PAGE");
        const QString json = BackupJson::settingsToJson(s);
        check(json.contains(QStringLiteral("\"originalColors\": true")), "json writes originalColors");
        check(BackupJson::mergeSettings(ViewerSettings{}, json).originalColors, "json roundtrip originalColors");
        check(BackupJson::mergeSettings(ViewerSettings{}, QStringLiteral("{\"originalColors\": 1}")).originalColors,
              "json originalColors int");
    }
    check(layoutEm(1.f) == 12.f, "layoutEm 1");
    check(layoutEm(0.5f) == 6.f, "layoutEm 0.5");
    check(layoutEm(2.f) == 24.f, "layoutEm 2");
    check(layoutEm(0.1f) == layoutEm(0.3f), "layoutEm clamp low");
    check(layoutEm(4.f) == 36.f, "layoutEm clamp high");

    QByteArray pdf = QByteArray("%PDF-1.4");
    check(DocumentMagic::pick(pdf, {}, {}) == QLatin1String(DocumentMagic::PDF), "magic pdf");
    QByteArray jpeg;
    jpeg.append(char(0xFF));
    jpeg.append(char(0xD8));
    jpeg.append(char(0xFF));
    check(DocumentMagic::pick(jpeg, {}, QStringLiteral("x.bin")) == QLatin1String(DocumentMagic::JPEG), "magic jpeg");
    check(DocumentMagic::fromName(QStringLiteral("book.epub")) == QLatin1String(DocumentMagic::EPUB), "name epub");
    check(DocumentMagic::fromName(QStringLiteral("notes.txt")) == QLatin1String(DocumentMagic::TXT), "name txt");
    check(DocumentMagic::fromName(QStringLiteral("readme.md")) == QLatin1String(DocumentMagic::MARKDOWN), "name md");
    check(DocumentMagic::fromName(QStringLiteral("sheet.xlsx")) == QLatin1String(DocumentMagic::XLSX), "name xlsx");
    check(DocumentMagic::shareType(QLatin1String(DocumentMagic::ZIP)) == QLatin1String(DocumentMagic::CBZ), "share zip");
    check(DocumentMagic::shareType({}).isEmpty(), "share empty");
    check(DocumentMagic::extensionFor(QLatin1String(DocumentMagic::TXT)) == QStringLiteral("txt"), "ext txt");
    check(DocumentMagic::extensionFor({}) == QStringLiteral("bin"), "ext unknown");
    check(DocumentMagic::pick(QByteArray("hello world\n"), {}, {}) == QLatin1String(DocumentMagic::TXT),
          "text fallback");
    check(DocumentMagic::pick(QByteArray("hello world\n"), {}, {}) != QLatin1String(DocumentMagic::PDF),
          "text not pdf");
    {
        const QString css = DocumentFont::cssFor(
            QStringLiteral("Segoe UI"), 500, {QStringLiteral("C:/temp files/Segoe.ttf")});
        check(css.contains(QLatin1String("@font-face")), "css font-face");
        check(css.contains(QLatin1String("font-family: \"Segoe UI\"")), "css family");
        check(css.contains(QLatin1String("url(\"")), "css quoted url");
        check(css.contains(QLatin1String("temp files/Segoe.ttf")), "css space in path");
        check(css.contains(QLatin1String("font-weight: 500")), "css weight");
        check(css.contains(QLatin1String("pre, code")), "css txt pre");
    }
    {
        QByteArray zip;
        zip.append("PK");
        zip.append(char(0x03));
        zip.append(char(0x04));
        zip.append("[Content_Types].xml");
        check(DocumentMagic::pick(zip, {}, QStringLiteral("sheet.xlsx")) ==
                  QLatin1String(DocumentMagic::XLSX),
              "xlsx not xps");
        check(DocumentMagic::pick(zip, {}, {}) != QLatin1String(DocumentMagic::XPS), "content types not xps");
        check(DocumentMagic::pick(zip, {}, {}) != QLatin1String(DocumentMagic::PDF), "zip not pdf");
    }
    {
        QByteArray bin;
        bin.append(char(0));
        bin.append(char(1));
        bin.append(char(2));
        check(DocumentMagic::pick(bin, {}, {}).isEmpty(), "binary unknown");
        check(DocumentMagic::pick(bin, {}, {}) != QLatin1String(DocumentMagic::PDF), "binary not pdf");
    }

    // Kotlin parity: lum 0/1 -> fg/bg, float trunc toward 0.
    QVector<int> px{static_cast<int>(0xFF000000), static_cast<int>(0xFFFFFFFF)};
    ColorRemapper::remap(px, 2, 1, 0xFF101010, 0xFFE0E0E0, 0.f, {}, 1.f);
    check((px[0] & 0xFFFFFF) == 0x101010, "remap black -> fg");
    check((px[1] & 0xFFFFFF) == 0xE0E0E0, "remap white -> bg");

    ViewerSettings s;
    s.doubleTapMenu = true;
    s.singleTapMenu = true;
    s = s.withExclusiveMenuTaps();
    check(!s.singleTapMenu, "exclusive menu taps");
    s.folioEnabled = false;
    s.openFolioOnStartup = true;
    s = s.withExclusiveStartup();
    check(!s.openFolioOnStartup, "exclusive startup folio");

    QVector<QVector<CharBox>> pages(1);
    const QString hello = QStringLiteral("hello world");
    for (QChar c : hello) {
        CharBox b;
        b.c = c;
        pages[0].push_back(b);
    }
    const auto hits = TextFinder::find(pages, QStringLiteral("hello"));
    check(hits.size() == 1, "find hello count");
    if (!hits.isEmpty()) {
        check(hits[0].startIndex == 0 && hits[0].endIndex == 4, "find hello span");
        check(hits[0].snippet == QStringLiteral("hello"), "find snippet");
    }

    const QByteArray priorDataDir = qgetenv("PAPERCUT_DATA_DIR");
    qunsetenv("PAPERCUT_DATA_DIR");
    const QString root = AppPaths::root();
    check(root.contains(QStringLiteral("grandsphere")), "data dir grandsphere");
    check(root.contains(QStringLiteral("papercut")), "data dir papercut");

    check(DocumentPaths::isUgly(QStringLiteral("content://x")), "ugly content");
    check(!DocumentPaths::isUgly(QStringLiteral("Downloads")), "not ugly Downloads");
    const auto lab = DocumentPaths::fromUriString(QStringLiteral("C:/Users/me/Documents/book.pdf"));
    check(lab.displayName == QStringLiteral("book.pdf"), "path name");

    check(errorMessage(QStringLiteral("PASSWORD")).contains(QStringLiteral("password")), "err password");

    {
        PageInfo p;
        p.width = 100;
        p.height = 200;
        const PageInfo r90 = displayPage(p, 90);
        check(r90.width == 200 && r90.height == 100, "display 90 swap");
        const PageInfo r0 = displayPage(p, 0);
        check(r0.width == 100 && r0.height == 200, "display 0 keep");
        const XY tl = rotateContentPoint(0, 0, p, 90);
        check(std::abs(tl.x - 200.f) < 0.01f && std::abs(tl.y) < 0.01f, "rotate 90 origin");
        check(normRotate(-90) == 270, "norm -90");
        check(normRotate(450) == 90, "norm 450");
    }

    ViewerSettings orig;
    orig.fgColor = 0x112233;
    orig.folioEnabled = true;
    const QString json = BackupJson::exportKind(BackupJson::Kind::Settings, orig, {}, {});
    check(json.contains(QStringLiteral("\"format\": \"Papercut\"")), "backup format");
    const auto parsed = BackupJson::parse(json);
    check(!parsed.settingsJson.isEmpty(), "backup settings present");
    const ViewerSettings merged = BackupJson::mergeSettings(ViewerSettings{}, parsed.settingsJson);
    check(merged.fgColor == 0x112233, "backup fg");
    check(merged.folioEnabled, "backup folioEnabled");

    QTemporaryDir tmp;
    Database db;
    check(db.open(tmp.filePath(QStringLiteral("Papercut.db"))), "db open");
    {
        QTemporaryDir cfg;
        qputenv("PAPERCUT_DATA_DIR", QFile::encodeName(cfg.path()));
        SettingsRepository settings;
        check(settings.get().fontScale == 1.f, "default fontScale");
        settings.migrateFromDatabaseIfEmpty(db);
        check(QFile::exists(AppPaths::settingsFile()), "settings.json created on launch");
        settings.update([](ViewerSettings s) {
            s.lockZoom = true;
            s.windowPaddingV = 7;
            s.rememberLastZoom = true;
            s.startupZoomMode = QStringLiteral("FIT_PAGE");
            s.startupZoomPercent = 175.f;
            s.zoomMode = QStringLiteral("CUSTOM");
            s.customZoomPercent = 130.f;
            s.originalColors = true;
            return s;
        });
        check(settings.get().lockZoom, "settings update");
        check(QFileInfo(AppPaths::settingsFile()).fileName() == QStringLiteral("settings.json"), "settings json name");
        check(QFile::exists(AppPaths::settingsFile()), "settings.json written");
        check(!AppPaths::settingsFile().startsWith(AppPaths::database().left(AppPaths::database().lastIndexOf('/'))
                                                   + QStringLiteral("/Papercut")),
              "settings not db");
        SettingsRepository reloaded;
        check(reloaded.get().windowPaddingV == 7, "settings reload from json");
        check(reloaded.get().rememberLastZoom, "rememberLastZoom reload");
        check(reloaded.get().startupZoomMode == QStringLiteral("FIT_PAGE"), "startupZoomMode reload");
        check(std::abs(reloaded.get().startupZoomPercent - 175.f) < 0.1f, "startupZoomPercent reload");
        check(reloaded.get().zoomMode == QStringLiteral("CUSTOM"), "zoomMode reload");
        check(std::abs(reloaded.get().customZoomPercent - 130.f) < 0.1f, "customZoomPercent reload");
        check(reloaded.get().originalColors, "originalColors reload");
        qunsetenv("PAPERCUT_DATA_DIR");
    }
    BookmarkRepository books(db);
    books.ensureDocument(QStringLiteral("file:///C:/tmp/a.pdf"), QStringLiteral("a.pdf"), QStringLiteral("tmp"));
    books.saveLastPosition(QStringLiteral("file:///C:/tmp/a.pdf"), 3, 1.f, 2.f);
    const auto stored = books.getDocument(QStringLiteral("file:///C:/tmp/a.pdf"));
    check(stored && stored->lastPageIndex == 3, "save last position");
    books.addBookmark(QStringLiteral("file:///C:/tmp/a.pdf"), QStringLiteral("here"), 3, 1.f, 2.f);
    check(books.bookmarksFor(QStringLiteral("file:///C:/tmp/a.pdf")).size() == 1, "add bookmark");

    {
        QTemporaryDir data;
        qputenv("PAPERCUT_DATA_DIR", QFile::encodeName(data.path()));
        check(QDir::cleanPath(AppPaths::root()) == QDir::cleanPath(data.path()), "keybinds data dir override");
        check(QFileInfo(AppPaths::keybinds()).fileName() == QStringLiteral("keybinds.json"), "keybinds filename");
        check(!AppPaths::keybinds().startsWith(QCoreApplication::applicationDirPath()), "keybinds not in app dir");
        auto *kb = Keybinds::create();
        check(kb->sequence(QStringLiteral("open"), 0) == QStringLiteral("Ctrl+O"), "default open");
        check(kb->sequence(QStringLiteral("rotateLeft"), 0) == QStringLiteral("Comma"), "rotate left comma");
        check(kb->sequence(QStringLiteral("rotateRight"), 0) == QStringLiteral("Period"), "rotate right period");
        check(kb->actionForKey(Qt::Key_Comma, 0) == QStringLiteral("rotateLeft"), "comma key rotates left");
        check(kb->actionForKey(Qt::Key_Period, int(Qt::ControlModifier)) == QStringLiteral("rotateAllRight"),
              "ctrl period rotates all right");
        check(kb->actionForKey(Qt::Key_QuoteLeft, 0) == QStringLiteral("openSidebar"), "tilde key toggles sidebar");
        check(kb->display(QStringLiteral("Comma")) == QStringLiteral(","), "display comma");
        check(kb->display(QStringLiteral("Ctrl+Period")) == QStringLiteral("Ctrl + ."), "display ctrl period");
        check(kb->sequence(QStringLiteral("find"), 0) == QStringLiteral("/"), "default find slash");
        check(kb->sequence(QStringLiteral("zoomIn"), 0) == QStringLiteral("Ctrl+WheelUp"), "default zoom wheel");
        check(kb->matchesFilter(QStringLiteral("abc def ghi jkl mno pqr stu vwxyz"), QStringLiteral("ab yz")), "fuzzy words ab yz");
        check(kb->matchesFilter(QStringLiteral("abc xyz"), QStringLiteral("ab yz")), "fuzzy abc xyz");
        check(kb->matchesFilter(QStringLiteral("abayz"), QStringLiteral("ab yz")), "fuzzy abayz");
        check(kb->matchesFilter(QStringLiteral("abc xyz"), QStringLiteral("yz ab")), "fuzzy order independent");
        check(!kb->matchesFilter(QStringLiteral("acb xyz"), QStringLiteral("ab yz")), "fuzzy not acb xyz");
        check(!kb->matchesFilter(QStringLiteral("abc xy z"), QStringLiteral("ab yz")), "fuzzy not abc xy z");
        check(kb->sequence(QStringLiteral("closeDocument"), 0) == QStringLiteral("Ctrl+W"), "ctrl w close");
        check(!kb->sequence(QStringLiteral("sharePartial"), 0).isEmpty(), "share partial default");
        check(kb->sequence(QStringLiteral("grabWindow"), 0) == QStringLiteral("Alt"), "grab window alt");
        check(kb->sequence(QStringLiteral("toggleMenuBar"), 0) == QStringLiteral("F1"), "toggle menu bar F1");
        check(kb->sequence(QStringLiteral("openSidebar"), 0) == QStringLiteral("~"), "toggle sidebar tilde");
        check(kb->sequence(QStringLiteral("colourMode"), 0) == QStringLiteral("F2"), "colour mode F2");
        check(kb->sequence(QStringLiteral("selectAll"), 0) == QStringLiteral("Ctrl+A"), "select all ctrl a");
        check(QFile::exists(AppPaths::keybinds()), "keybinds.json created on launch");
        kb->beginEdit();
        kb->setBinding(QStringLiteral("open"), 0, QStringLiteral("Ctrl+Alt+O"));
        kb->revert();
        check(kb->sequence(QStringLiteral("open"), 0) == QStringLiteral("Ctrl+O"), "revert drops unsaved");
        kb->setBinding(QStringLiteral("open"), 0, QStringLiteral("Ctrl+Shift+O"));
        check(kb->sequence(QStringLiteral("open"), 0) == QStringLiteral("Ctrl+Shift+O"), "set open binding");
        kb->commit();
        check(QFile::exists(AppPaths::keybinds()), "keybinds.json in data dir");
        QFile kf(AppPaths::keybinds());
        check(kf.open(QIODevice::ReadOnly), "read keybinds.json");
        const QByteArray body = kf.readAll();
        check(body.contains("Ctrl+Shift+O"), "persisted binding");
        kb->setBinding(QStringLiteral("bookmarks"), 0, QStringLiteral("Ctrl+Shift+O"));
        check(kb->sequence(QStringLiteral("open"), 0).isEmpty(), "conflict steals previous");
        check(kb->sequence(QStringLiteral("bookmarks"), 0) == QStringLiteral("Ctrl+Shift+O"), "conflict new owner");
        if (priorDataDir.isEmpty()) qunsetenv("PAPERCUT_DATA_DIR");
        else qputenv("PAPERCUT_DATA_DIR", priorDataDir);
    }

    if (fails) {
        std::fprintf(stderr, "%d checks failed\n", fails);
        return 1;
    }
    std::fprintf(stdout, "ok\n");
    return 0;
}
