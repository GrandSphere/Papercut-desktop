#include "AppServices.h"
#include "CombineController.h"
#include "FolioController.h"
#include "SingleInstance.h"
#include "ViewerController.h"
#include "Version.h"
#include "WinWindowChrome.h"
#include "cli/Cli.h"
#include "core/AppPaths.h"
#include "data/ViewerSettings.h"
#include "core/Keybinds.h"
#include "pdf/DocumentFont.h"
#include "pdf/DocumentMagic.h"
#include "ui/PdfStripItem.h"

#include <QColor>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QFont>
#include <QFontDatabase>
#include <QCoreApplication>
#include <QEventLoop>
#include <QGuiApplication>
#include <QIcon>
#include <QMetaObject>
#include <QScreen>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickStyle>
#include <QQuickWindow>
#include <QWindow>
#include <QtQml>
#include <algorithm>
#include <cstring>

#ifdef Q_OS_WIN
#include <dwmapi.h>
#include <io.h>
#include <fcntl.h>
#include <stdio.h>
#include <windows.h>
#ifndef DWMWA_WINDOW_CORNER_PREFERENCE
#define DWMWA_WINDOW_CORNER_PREFERENCE 33
#endif
#ifndef DWMWCP_ROUND
#define DWMWCP_ROUND 2
#endif
#ifndef DWMWA_USE_IMMERSIVE_DARK_MODE
#define DWMWA_USE_IMMERSIVE_DARK_MODE 20
#endif
#ifndef DWMWA_BORDER_COLOR
#define DWMWA_BORDER_COLOR 34
#endif
#ifndef DWMWA_CAPTION_COLOR
#define DWMWA_CAPTION_COLOR 35
#endif
#ifndef DWMWA_TEXT_COLOR
#define DWMWA_TEXT_COLOR 36
#endif
#ifndef DWMWA_CLOAK
#define DWMWA_CLOAK 13
#endif
#ifndef DWMWA_VISIBLE_FRAME_BORDER_THICKNESS
#define DWMWA_VISIBLE_FRAME_BORDER_THICKNESS 37
#endif
#endif

using namespace papercut;

namespace {

void applyStartupWindowGeometry(QWindow *window, const ViewerSettings &st) {
    const int w = std::max(ViewerSettings::MIN_WINDOW_W,
                           ViewerSettings::canonicalDefaultWindowWidth(st.defaultWindowWidth));
    const int h = std::max(ViewerSettings::MIN_WINDOW_H,
                           ViewerSettings::canonicalDefaultWindowHeight(st.defaultWindowHeight));
    QScreen *screen = window->screen();
    if (!screen) screen = QGuiApplication::primaryScreen();
    const QRect area = screen ? screen->availableGeometry() : QRect();
    const QString pos = ViewerSettings::canonicalStartupWindowPosition(st.startupWindowPosition);
    int x = 0;
    int y = 0;
    int gw = w;
    int gh = h;
    if (area.isEmpty()) {
        window->setGeometry(x, y, gw, gh);
        return;
    }
    if (pos == QLatin1String("SNAP_LEFT")) {
        gw = std::max(ViewerSettings::MIN_WINDOW_W, area.width() / 2);
        gh = std::max(ViewerSettings::MIN_WINDOW_H, area.height());
        x = area.x();
        y = area.y();
    } else if (pos == QLatin1String("SNAP_RIGHT")) {
        gw = std::max(ViewerSettings::MIN_WINDOW_W, area.width() / 2);
        gh = std::max(ViewerSettings::MIN_WINDOW_H, area.height());
        x = area.x() + area.width() - gw;
        y = area.y();
    } else {
        x = area.x() + std::max(0, (area.width() - w) / 2);
        y = area.y() + std::max(0, (area.height() - h) / 2);
        gw = w;
        gh = h;
    }
    window->setGeometry(x, y, gw, gh);
}

void applyStartupWindowState(QWindow *window, const ViewerSettings &st) {
    const QString pos = ViewerSettings::canonicalStartupWindowPosition(st.startupWindowPosition);
    if (pos == QLatin1String("MAXIMIZED"))
        window->showMaximized();
    else if (pos == QLatin1String("FULLSCREEN"))
        QMetaObject::invokeMethod(window, "beginStartupFullscreen", Qt::QueuedConnection);
}

} // namespace

namespace {

QString spoolStdin() {
    QFile in;
    if (!in.open(stdin, QIODevice::ReadOnly)) return {};
    const QByteArray bytes = in.readAll();
    if (bytes.isEmpty()) return {};
    const QString mime = DocumentMagic::pick(bytes.left(2048), QString(), QString());
    const QString ext = DocumentMagic::extensionFor(mime);
    QDir().mkpath(AppPaths::temp().absolutePath());
    const QString path = AppPaths::temp().filePath(QStringLiteral("stdin-%1.%2").arg(QCoreApplication::applicationPid()).arg(ext));
    QFile out(path);
    if (!out.open(QIODevice::WriteOnly | QIODevice::Truncate) || out.write(bytes) != bytes.size()) return {};
    return path;
}

void applyNativeTitleBar(QWindow *window, const QColor &bg, const QColor &fg) {
    if (!window) return;
#ifdef Q_OS_WIN
    const HWND hwnd = reinterpret_cast<HWND>(window->winId());
    if (!hwnd) return;
    static HWND lastHwnd = nullptr;
    static QRgb lastBg = 0, lastFg = 0;
    const bool unchanged = hwnd == lastHwnd && bg.rgba() == lastBg && fg.rgba() == lastFg;
    lastHwnd = hwnd;
    lastBg = bg.rgba();
    lastFg = fg.rgba();
    if (unchanged) return;
    const BOOL dark = bg.lightnessF() < 0.55 ? TRUE : FALSE;
    DwmSetWindowAttribute(hwnd, 19, &dark, sizeof(dark)); // Win10 dark mode attr 19
    DwmSetWindowAttribute(hwnd, DWMWA_USE_IMMERSIVE_DARK_MODE, &dark, sizeof(dark));
    const COLORREF caption = RGB(bg.red(), bg.green(), bg.blue());
    const COLORREF text = RGB(fg.red(), fg.green(), fg.blue());
    DwmSetWindowAttribute(hwnd, DWMWA_CAPTION_COLOR, &caption, sizeof(caption));
    DwmSetWindowAttribute(hwnd, DWMWA_TEXT_COLOR, &text, sizeof(text));
    DwmSetWindowAttribute(hwnd, DWMWA_BORDER_COLOR, &caption, sizeof(caption));
    const int corners = DWMWCP_ROUND;
    DwmSetWindowAttribute(hwnd, DWMWA_WINDOW_CORNER_PREFERENCE, &corners, sizeof(corners));
    const int borderThickness = 0;
    DwmSetWindowAttribute(hwnd, DWMWA_VISIBLE_FRAME_BORDER_THICKNESS, &borderThickness, sizeof(borderThickness));
    SetWindowPos(hwnd, nullptr, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE | SWP_FRAMECHANGED); // Win10 caption refresh
    DwmSetWindowAttribute(hwnd, DWMWA_BORDER_COLOR, &caption, sizeof(caption));
#else
    Q_UNUSED(bg);
    Q_UNUSED(fg);
#endif
}

bool isCliRequest(int argc, char **argv) {
    for (int i = 1; i < argc; ++i) {
        const char *a = argv[i];
        if (!a || std::strcmp(a, "-") == 0) continue;
        if (std::strcmp(a, "--help") == 0 || std::strcmp(a, "-h") == 0 || std::strcmp(a, "--version") == 0
            || std::strcmp(a, "--share") == 0 || std::strcmp(a, "--combine") == 0
            || std::strcmp(a, "--export-pages") == 0 || std::strcmp(a, "--folio-add") == 0
            || std::strcmp(a, "--export-backup") == 0 || std::strcmp(a, "--import-backup") == 0
            || std::strcmp(a, "--headless") == 0 || std::strcmp(a, "-o") == 0
            || std::strcmp(a, "--output") == 0)
            return true;
    }
    return false;
}

#ifdef Q_OS_WIN
bool handleMissing(DWORD which) {
    const HANDLE h = GetStdHandle(which);
    return h == nullptr || h == INVALID_HANDLE_VALUE;
}

void attachParentConsole() {
    const bool needOut = handleMissing(STD_OUTPUT_HANDLE);
    const bool needErr = handleMissing(STD_ERROR_HANDLE);
    const bool needIn = handleMissing(STD_INPUT_HANDLE);
    if (!needOut && !needErr && !needIn) return;
    if (!AttachConsole(ATTACH_PARENT_PROCESS)) return;
    FILE *fp = nullptr;
    if (needOut) fp = freopen("CONOUT$", "w", stdout);
    if (needErr) fp = freopen("CONOUT$", "w", stderr);
    if (needIn) fp = freopen("CONIN$", "r", stdin);
    Q_UNUSED(fp);
}
#endif

} // namespace

int main(int argc, char **argv) {
    if (isCliRequest(argc, argv)) {
#ifdef Q_OS_WIN
        attachParentConsole();
#endif
        QCoreApplication app(argc, argv);
        QCoreApplication::setApplicationName(QStringLiteral("papercut"));
        QCoreApplication::setOrganizationName(QStringLiteral("grandsphere"));
        QCoreApplication::setApplicationVersion(QStringLiteral(PAPERCUT_VERSION));
        return runCli();
    }

    QGuiApplication app(argc, argv);
    QGuiApplication::setApplicationName(QStringLiteral("papercut"));
    QGuiApplication::setOrganizationName(QStringLiteral("grandsphere"));
    QGuiApplication::setApplicationVersion(QStringLiteral(PAPERCUT_VERSION));
    QQuickStyle::setStyle(QStringLiteral("Basic"));
    auto loadFamily = [](const QString &path) {
        const int id = QFontDatabase::addApplicationFont(path);
        if (id < 0) return QString();
        const QStringList families = QFontDatabase::applicationFontFamilies(id);
        return families.isEmpty() ? QString() : families.first();
    };
    AppServices::registerUiFont(QStringLiteral("roboto"),
                                loadFamily(QStringLiteral(":/fonts/Roboto-Regular.ttf")));
    AppServices::setIconFontFamily(loadFamily(QStringLiteral(":/fonts/MaterialSymbolsOutlined.ttf")));

    QString file, password;
    int page = 0;
    const QStringList args = QCoreApplication::arguments();
    for (int i = 1; i < args.size(); ++i) {
        const QString &a = args[i];
        if (a == QLatin1String("--page") && i + 1 < args.size()) page = args[++i].toInt();
        else if (a == QLatin1String("--password") && i + 1 < args.size()) password = args[++i];
        else if (a == QLatin1String("-")) file = spoolStdin();
        else if (!a.startsWith(QLatin1Char('-')) && file.isEmpty()) file = QFileInfo(a).absoluteFilePath();
    }

    AppServices *services = AppServices::create();
#ifdef Q_OS_WIN
    const QString robotoFile = DocumentFont::ensureBundledRoboto();
    if (!robotoFile.isEmpty())
        AddFontResourceExW(reinterpret_cast<LPCWSTR>(robotoFile.utf16()), FR_PRIVATE, nullptr);
#endif
    auto applyUiFont = [services] {
        const QString family = services->uiFontFamily();
        if (family.isEmpty()) return;
        QFont ui(family);
        ui.setWeight(QFont::Weight(services->uiFontWeight()));
        QGuiApplication::setFont(ui);
    };
    applyUiFont();
    QObject::connect(services, &AppServices::settingsChanged, &app, applyUiFont);

    SingleInstance *instance = nullptr;
    if (!services->settings().allowMultipleInstances) {
        if (SingleInstance::forward({file, page, password})) return 0;
        instance = new SingleInstance(&app);
        if (!instance->listen()) qWarning("papercut: could not listen for further launches");
    }
    if (instance) {
        QObject::connect(instance, &SingleInstance::requestReceived, services,
                         [services](const SingleInstance::Request &r) {
                             emit services->fileRequested(r.file, r.page, r.password);
                         });
    }

    qmlRegisterSingletonInstance("Papercut", 1, 0, "App", services);
    qmlRegisterSingletonInstance("Papercut", 1, 0, "Keybinds", Keybinds::create());
    qmlRegisterType<PdfStripItem>("Papercut", 1, 0, "PdfStripItem");
    qmlRegisterType<ViewerController>("Papercut", 1, 0, "ViewerController");
    qmlRegisterType<FolioController>("Papercut", 1, 0, "FolioController");
    qmlRegisterType<CombineController>("Papercut", 1, 0, "CombineController");
    qmlRegisterUncreatableType<DocumentController>("Papercut", 1, 0, "DocumentController", QStringLiteral("abstract"));

    QQmlApplicationEngine engine;
    QVariantMap launch;
    launch.insert(QStringLiteral("file"), file);
    launch.insert(QStringLiteral("page"), page);
    launch.insert(QStringLiteral("password"), password);
    engine.rootContext()->setContextProperty(QStringLiteral("Launch"), launch);
    QObject::connect(&engine, &QQmlApplicationEngine::objectCreationFailed, &app, [] { QCoreApplication::exit(1); },
                     Qt::QueuedConnection);
    QObject::connect(
        &engine, &QQmlApplicationEngine::objectCreated, &app,
        [services](QObject *obj, const QUrl &) {
            auto *window = qobject_cast<QQuickWindow *>(obj);
            if (!window) return;
            auto apply = [window, services] {
                applyNativeTitleBar(window, services->bgColor(), services->appFontColor());
                const QString id = ViewerSettings::canonicalAppIcon(services->settings().appIcon);
                QIcon icon(QStringLiteral(":/icons/%1.ico").arg(id));
                if (icon.isNull())
                    icon = QIcon(QStringLiteral(":/icons/papercut-dark-off.ico"));
                window->setIcon(icon);
                QGuiApplication::setWindowIcon(icon);
            };
            QObject::connect(window, &QWindow::visibleChanged, window, [apply, window] {
                if (window->isVisible()) apply();
            });
            QObject::connect(services, &AppServices::settingsChanged, window, apply);
            window->setColor(services->bgColor());
            window->create();
#ifdef Q_OS_WIN
            HWND hwnd = reinterpret_cast<HWND>(window->winId());
            BOOL cloak = TRUE;
            if (hwnd)
                DwmSetWindowAttribute(hwnd, DWMWA_CLOAK, &cloak, sizeof(cloak));
            installWinWindowChrome(window);
            apply();
            window->update();
            QCoreApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
            if (hwnd) {
                BOOL uncloak = FALSE;
                DwmSetWindowAttribute(hwnd, DWMWA_CLOAK, &uncloak, sizeof(uncloak));
                apply();
            }
#else
            installWinWindowChrome(window);
            apply();
            window->update();
            QCoreApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
#endif
            const ViewerSettings startupSettings = services->settings();
            applyStartupWindowGeometry(window, startupSettings);
            window->show();
            applyStartupWindowState(window, startupSettings);
        });
    engine.load(QUrl(QStringLiteral("qrc:/qml/Main.qml")));
    return app.exec();
}
