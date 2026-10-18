#include "Cli.h"
#include "Version.h"
#include "core/AppPaths.h"
#include "core/ExitCodes.h"
#include "core/Io.h"
#include "data/BackupJson.h"
#include "data/BookmarkRepository.h"
#include "data/Database.h"
#include "data/DocumentPaths.h"
#include "data/FolioRepository.h"
#include "data/FolioStore.h"
#include "data/SettingsRepository.h"
#include "pdf/DocumentMagic.h"
#include "pdf/MuPdfEngine.h"
#include "pdf/PdfComposer.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QProcess>
#include <QTextStream>

#ifdef _WIN32
#include <fcntl.h>
#include <io.h>
#endif

using namespace papercut;

namespace {

QTextStream &errs() {
    static QTextStream s(stderr);
    return s;
}

void usage(QTextStream &out) {
    out << "Papercut " << PAPERCUT_VERSION << "\n"
        << "Usage: papercut [options] [file | - ...]\n"
        << "  A file, '-' or piped stdin counts as a document shared TO the app.\n"
        << "  --share [FILE|-]           Share FROM the app: write the document as PDF to stdout (or -o)\n"
        << "  --combine IN... [-o OUT]   Merge documents into one PDF (stdout when -o is omitted)\n"
        << "  --export-pages LIST IN [-o OUT]  Extract 1-based pages, e.g. 1,3-5\n"
        << "  -o, --output FILE|-        Output file; '-' is stdout\n"
        << "  --password PW              Password for protected input (or PAPERCUT_PASSWORD)\n"
        << "  --page N                   Open at 1-based page (GUI)\n"
        << "  --folio-add FILE...        Add documents to the Folio\n"
        << "  --export-backup FILE|-     Export settings + bookmarks as JSON\n"
        << "  --import-backup FILE|-     Import a backup JSON\n"
        << "  --headless                 Never open a window\n"
        << "  --help, --version\n"
        << "Exit codes: 0 ok, 1 usage, 2 open failed, 3 password, 4 unsupported, 5 write failed.\n";
    out.flush();
}

void binaryMode() {
#ifdef _WIN32
    _setmode(_fileno(stdin), _O_BINARY);
    _setmode(_fileno(stdout), _O_BINARY);
#endif
}

QByteArray readAllStdin() {
    QFile in;
    if (!in.open(stdin, QIODevice::ReadOnly)) return {};
    return in.readAll();
}

bool writeStdout(const QByteArray &bytes) {
    QFile out;
    if (!out.open(stdout, QIODevice::WriteOnly)) return false;
    const bool ok = out.write(bytes) == bytes.size();
    out.flush();
    return ok;
}

struct Options {
    bool help = false, version = false, headless = false;
    bool share = false, combine = false, folioAdd = false;
    QString exportPages, output, password, exportBackup, importBackup;
    bool hasOutput = false;
    int page = 0;
    QStringList inputs; // "-" = stdin
};

bool parseArgs(const QStringList &args, Options &o, QString &error) {
    auto value = [&](int &i, const QString &flag, QString &dest) {
        if (i + 1 >= args.size()) {
            error = flag + QStringLiteral(" needs a value");
            return false;
        }
        dest = args[++i];
        return true;
    };
    bool shareFileAllowed = false;
    for (int i = 1; i < args.size(); ++i) {
        const QString &a = args[i];
        if (a == QLatin1String("--help") || a == QLatin1String("-h")) o.help = true;
        else if (a == QLatin1String("--version")) o.version = true;
        else if (a == QLatin1String("--headless")) o.headless = true;
        else if (a == QLatin1String("--share")) { o.share = true; shareFileAllowed = true; }
        else if (a == QLatin1String("--combine")) o.combine = true;
        else if (a == QLatin1String("--folio-add")) o.folioAdd = true;
        else if (a == QLatin1String("-o") || a == QLatin1String("--output")) {
            if (!value(i, a, o.output)) return false;
            o.hasOutput = true;
        } else if (a == QLatin1String("--password")) {
            if (!value(i, a, o.password)) return false;
        } else if (a == QLatin1String("--export-pages")) {
            if (!value(i, a, o.exportPages)) return false;
        } else if (a == QLatin1String("--export-backup")) {
            if (!value(i, a, o.exportBackup)) return false;
        } else if (a == QLatin1String("--import-backup")) {
            if (!value(i, a, o.importBackup)) return false;
        } else if (a == QLatin1String("--page")) {
            QString v;
            if (!value(i, a, v)) return false;
            bool ok = false;
            o.page = v.toInt(&ok);
            if (!ok || o.page < 1) {
                error = QStringLiteral("--page needs a number >= 1");
                return false;
            }
        } else if (a.startsWith(QLatin1Char('-')) && a != QLatin1String("-")) {
            error = QStringLiteral("unknown option ") + a;
            return false;
        } else {
            o.inputs << a;
        }
    }
    Q_UNUSED(shareFileAllowed);
    if (o.password.isEmpty()) o.password = qEnvironmentVariable("PAPERCUT_PASSWORD");
    return true;
}

bool parsePageList(const QString &text, QList<int> &out) {
    for (const QString &part : text.split(QLatin1Char(','), Qt::SkipEmptyParts)) {
        const QString p = part.trimmed();
        const int dash = p.indexOf(QLatin1Char('-'), 1);
        bool ok1 = false, ok2 = true;
        if (dash > 0) {
            const int a = p.left(dash).toInt(&ok1);
            const int b = p.mid(dash + 1).toInt(&ok2);
            if (!ok1 || !ok2 || a < 1 || b < a) return false;
            for (int n = a; n <= b; ++n) out << n - 1;
        } else {
            const int a = p.toInt(&ok1);
            if (!ok1 || a < 1) return false;
            out << a - 1;
        }
    }
    return !out.isEmpty();
}

struct Spool {
    QStringList temps;
    ~Spool() {
        for (const QString &t : temps) QFile::remove(t);
    }
};

bool resolveInput(const QString &arg, Spool &spool, QString &path, QString &magic) {
    if (arg != QLatin1String("-")) {
        QFile f(arg);
        if (!f.open(QIODevice::ReadOnly)) return false;
        const QByteArray head = f.read(2048);
        magic = DocumentMagic::pick(head, QString(), arg);
        path = QFileInfo(arg).absoluteFilePath();
        return true;
    }
    const QByteArray bytes = readAllStdin();
    if (bytes.isEmpty()) return false;
    magic = DocumentMagic::pick(bytes.left(2048), QString(), QString());
    QDir().mkpath(AppPaths::temp().absolutePath());
    path = AppPaths::temp().filePath(QStringLiteral("stdin-%1-%2.%3")
                                         .arg(QCoreApplication::applicationPid())
                                         .arg(spool.temps.size())
                                         .arg(DocumentMagic::extensionFor(magic)));
    QFile out(path);
    if (!out.open(QIODevice::WriteOnly | QIODevice::Truncate) || out.write(bytes) != bytes.size()) return false;
    out.close();
    spool.temps << path;
    return true;
}

int codeFor(const QString &c) {
    if (c == QLatin1String("PASSWORD")) return ExitCode::Password;
    if (c == QLatin1String("NOT_PDF") || c == QLatin1String("No pages") || c == QLatin1String("No documents"))
        return ExitCode::NotSupported;
    if (c == QLatin1String("EXPORT")) return ExitCode::WriteFailed;
    return ExitCode::OpenFailed;
}

bool deliver(const QString &built, const Options &o) {
    if (!o.hasOutput || o.output == QLatin1String("-")) {
        QFile f(built);
        if (!f.open(QIODevice::ReadOnly)) return false;
        return writeStdout(f.readAll());
    }
    QFile::remove(o.output);
    return QFile::copy(built, o.output);
}

QString builtPath() {
    QDir().mkpath(AppPaths::temp().absolutePath());
    return AppPaths::temp().filePath(QStringLiteral("cli-out-%1.pdf").arg(QCoreApplication::applicationPid()));
}

int validate(const QString &path, const QString &password, float fontScale, int &pages, QString &mime) {
    MuPdfEngine engine;
    try {
        engine.open(path, fontScale, 0);
    } catch (const EngineError &e) {
        if (e.code() != QLatin1String("PASSWORD")) return codeFor(e.code());
        if (password.isEmpty() || !engine.authenticatePassword(password)) return ExitCode::Password;
    } catch (...) {
        return ExitCode::OpenFailed;
    }
    pages = engine.pageCount();
    mime = engine.mime();
    return ExitCode::Ok;
}

} // namespace

namespace papercut {

int runCli() {
    binaryMode();

    QTextStream out(stdout);
    Options o;
    QString error;
    const QStringList args = QCoreApplication::arguments();
    if (!parseArgs(args, o, error)) {
        errs() << "papercut: " << error << "\n";
        usage(errs());
        return ExitCode::Usage;
    }
    if (o.help) {
        usage(out);
        return ExitCode::Ok;
    }
    if (o.version) {
        out << PAPERCUT_VERSION << '\n';
        return ExitCode::Ok;
    }

    Database db;
    AppPaths::migrateLegacyLayout();
    QDir().mkpath(AppPaths::dataRoot());
    QDir().mkpath(AppPaths::configRoot());
    if (!db.open(AppPaths::database())) {
        errs() << "papercut: cannot open database\n";
        return ExitCode::WriteFailed;
    }
    SettingsRepository settings;
    settings.migrateFromDatabaseIfEmpty(db);
    AppPaths::setFilesRootOverride(settings.get().filesRoot);
    BookmarkRepository bookmarks(db);
    Spool spool;

    if (!o.exportBackup.isEmpty()) {
        const QString json = bookmarks.snapshot(BackupJson::Kind::All, settings.get());
        if (o.exportBackup == QLatin1String("-")) return writeStdout(json.toUtf8()) ? 0 : ExitCode::WriteFailed;
        QFile f(o.exportBackup);
        if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate) || f.write(json.toUtf8()) < 0)
            return ExitCode::WriteFailed;
        return ExitCode::Ok;
    }
    if (!o.importBackup.isEmpty()) {
        QByteArray bytes;
        if (o.importBackup == QLatin1String("-")) {
            bytes = readAllStdin();
        } else {
            QFile f(o.importBackup);
            if (!f.open(QIODevice::ReadOnly)) return ExitCode::OpenFailed;
            bytes = f.readAll();
        }
        const BackupJson::Parsed parsed = BackupJson::parse(QString::fromUtf8(bytes));
        if (!parsed.settings && !parsed.documents && !parsed.bookmarks) {
            errs() << "papercut: not a Papercut backup\n";
            return ExitCode::NotSupported;
        }
        bookmarks.importParsed(parsed);
        if (!parsed.settingsJson.isEmpty())
            settings.update([&](ViewerSettings cur) { return BackupJson::mergeSettings(cur, parsed.settingsJson); });
        return ExitCode::Ok;
    }

    const float fontScale = 1.f;

    if (o.combine) {
        QList<ComposeSource> sources;
        for (const QString &in : o.inputs) {
            QString path, magic;
            if (!resolveInput(in, spool, path, magic)) {
                errs() << "papercut: cannot read " << in << "\n";
                return ExitCode::OpenFailed;
            }
            sources.append({path, magic});
        }
        const QString built = builtPath();
        try {
            PdfComposer::combine(sources, fontScale, built, o.password);
        } catch (const EngineError &e) {
            errs() << "papercut: " << e.code() << "\n";
            QFile::remove(built);
            return codeFor(e.code());
        }
        const bool ok = deliver(built, o);
        QFile::remove(built);
        return ok ? ExitCode::Ok : ExitCode::WriteFailed;
    }

    if (!o.exportPages.isEmpty()) {
        QList<int> pages;
        if (!parsePageList(o.exportPages, pages) || o.inputs.size() != 1) {
            errs() << "papercut: --export-pages LIST needs exactly one input\n";
            return ExitCode::Usage;
        }
        QString path, magic;
        if (!resolveInput(o.inputs[0], spool, path, magic)) return ExitCode::OpenFailed;
        const QString built = builtPath();
        try {
            PdfComposer::extractPages(path, pages, built, o.password, magic);
        } catch (const EngineError &e) {
            errs() << "papercut: " << e.code() << "\n";
            QFile::remove(built);
            return codeFor(e.code());
        }
        const bool ok = deliver(built, o);
        QFile::remove(built);
        return ok ? ExitCode::Ok : ExitCode::WriteFailed;
    }

    if (o.share) {
        if (o.inputs.size() != 1) {
            errs() << "papercut: --share needs exactly one input (file or -)\n";
            return ExitCode::Usage;
        }
        QString path, magic;
        if (!resolveInput(o.inputs[0], spool, path, magic)) return ExitCode::OpenFailed;
        const QString built = builtPath();
        if (magic == QLatin1String(DocumentMagic::PDF) && o.password.isEmpty()) {
            QFile::remove(built);
            if (!QFile::copy(path, built)) return ExitCode::WriteFailed;
        } else {
            try {
                PdfComposer::combine({{path, magic}}, fontScale, built, o.password);
            } catch (const EngineError &e) {
                errs() << "papercut: " << e.code() << "\n";
                QFile::remove(built);
                return codeFor(e.code());
            }
        }
        const bool ok = deliver(built, o);
        QFile::remove(built);
        return ok ? ExitCode::Ok : ExitCode::WriteFailed;
    }

    if (o.folioAdd) {
        FolioStore store(AppPaths::filesRoot());
        FolioRepository folio(db, store);
        int failed = 0;
        for (const QString &in : o.inputs) {
            QString path, magic;
            if (!resolveInput(in, spool, path, magic)) {
                ++failed;
                continue;
            }
            const PathLabel label = DocumentPaths::label(path);
            if (!folio.add(path, label.displayName, label.displayPath, path)) {
                errs() << "papercut: could not add " << path << " to Folio\n";
                ++failed;
            }
        }
        return failed ? ExitCode::WriteFailed : ExitCode::Ok;
    }

    if (!o.headless) {
        usage(errs());
        return ExitCode::Usage;
    }
    if (o.inputs.size() != 1) {
        errs() << "papercut: expected one document\n";
        return ExitCode::Usage;
    }
    QString path, magic;
    if (!resolveInput(o.inputs[0], spool, path, magic)) {
        errs() << "papercut: cannot read " << o.inputs[0] << "\n";
        return ExitCode::OpenFailed;
    }
    int pages = 0;
    QString mime;
    const int rc = validate(path, o.password, fontScale, pages, mime);
    if (rc != ExitCode::Ok) {
        errs() << "papercut: " << (rc == ExitCode::Password ? "password required" : "cannot open") << "\n";
        return rc;
    }
    const PathLabel label = DocumentPaths::label(path);
    bookmarks.ensureDocument(path, label.displayName, label.displayPath);
    if (settings.get().restoreLastPdf) {
        settings.update([&](ViewerSettings st) {
            st.lastPdfUri = path;
            return st;
        });
    }
    out << "pages=" << pages << " mime=" << mime << '\n';
    return ExitCode::Ok;
}

} // namespace papercut
