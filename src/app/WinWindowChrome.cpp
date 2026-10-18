#include "WinWindowChrome.h"

#include <QAbstractNativeEventFilter>
#include <QColor>
#include <QGuiApplication>
#include <QQuickItem>
#include <QQuickWindow>
#include <QScreen>
#include <QSize>
#include <QWindow>
#include <algorithm>
#include <cmath>

#ifdef Q_OS_WIN
#include <dwmapi.h>
#include <windows.h>
#include <windowsx.h>
#ifndef DWMWA_BORDER_COLOR
#define DWMWA_BORDER_COLOR 34
#endif
#ifndef DWMWA_USE_IMMERSIVE_DARK_MODE
#define DWMWA_USE_IMMERSIVE_DARK_MODE 20
#endif
#ifndef DWMWA_VISIBLE_FRAME_BORDER_THICKNESS
#define DWMWA_VISIBLE_FRAME_BORDER_THICKNESS 37
#endif
#endif

namespace papercut {
#ifdef Q_OS_WIN

namespace {

constexpr int kCaptionButtonWidth = 46;
constexpr int kResizeBorder = 8;

class WinChrome final : public QAbstractNativeEventFilter {
public:
    explicit WinChrome(QQuickWindow *window) : m_window(window) {}

    ~WinChrome() override = default;

    HWND hwnd() const { return reinterpret_cast<HWND>(m_window->winId()); }

    void primeFrame() {
        if (m_initialFrameApplied) return;
        if (!hwnd()) return;
        m_initialFrameApplied = true;
        applyFrameStyle();
    }

    void onVisible() {
        if (!m_window || !m_window->isVisible()) return;
        onVisibilityChanged(m_window->visibility());
    }

    void onVisibilityChanged(QWindow::Visibility visibility) {
        if (!m_window || !m_window->isVisible()) return;
        if (!m_initialFrameApplied) {
            m_initialFrameApplied = true;
            applyFrameStyle();
            m_lastVisibility = visibility;
            return;
        }
        const bool wasZoomed = m_lastVisibility == QWindow::FullScreen
                               || m_lastVisibility == QWindow::Maximized;
        m_lastVisibility = visibility;
        if (visibility == QWindow::FullScreen || visibility == QWindow::Maximized) return;
        if (wasZoomed) applyFrameStyle();
    }

    void applyFrameStyle() {
        if (!m_window) return;
        if (m_window->visibility() == QWindow::FullScreen) return;
        const HWND h = hwnd();
        if (!h) return;
        const bool zoomed = IsZoomed(h) || m_window->visibility() == QWindow::Maximized;
        const LONG prev = GetWindowLongW(h, GWL_STYLE);
        LONG style = prev;
        style &= ~WS_POPUP;
        if (!zoomed) style &= ~WS_MAXIMIZE;
        style |= WS_CAPTION | WS_THICKFRAME | WS_MAXIMIZEBOX | WS_MINIMIZEBOX | WS_SYSMENU;
        if (style != prev) SetWindowLongW(h, GWL_STYLE, style);
        const MARGINS margins{0, 0, 0, 1};
        DwmExtendFrameIntoClientArea(h, &margins);
        const QColor bg = m_window->color();
        if (bg.isValid()) {
            const COLORREF border = RGB(bg.red(), bg.green(), bg.blue());
            DwmSetWindowAttribute(h, DWMWA_BORDER_COLOR, &border, sizeof(border));
            const BOOL dark = bg.lightnessF() < 0.55f ? TRUE : FALSE;
            DwmSetWindowAttribute(h, DWMWA_USE_IMMERSIVE_DARK_MODE, &dark, sizeof(dark));
        }
        if (style != prev) {
            SetWindowPos(h, nullptr, 0, 0, 0, 0,
                         SWP_FRAMECHANGED | SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
        }
        if (bg.isValid()) {
            const COLORREF border = RGB(bg.red(), bg.green(), bg.blue());
            DwmSetWindowAttribute(h, DWMWA_BORDER_COLOR, &border, sizeof(border));
        }
        const int borderThickness = 0;
        DwmSetWindowAttribute(h, DWMWA_VISIBLE_FRAME_BORDER_THICKNESS, &borderThickness, sizeof(borderThickness));
    }

    bool nativeEventFilter(const QByteArray &eventType, void *message, qintptr *result) override {
        if (eventType != "windows_generic_MSG" || !m_window || !result) return false;
        auto *msg = static_cast<MSG *>(message);
        if (!msg || msg->hwnd != hwnd()) return false;
        switch (msg->message) {
        case WM_NCCALCSIZE:
            return handleNcCalcSize(msg, result);
        case WM_NCHITTEST:
            return handleNcHitTest(msg, result);
        case WM_GETMINMAXINFO:
            return handleGetMinMaxInfo(msg, result);
        case WM_NCACTIVATE:
            *result = TRUE;
            return true;
        case WM_EXITSIZEMOVE: {
            const bool zoomed = winWindowIsZoomed(m_window);
            if (!zoomed && m_window->visibility() != QWindow::Maximized)
                winWindowSyncNormalPlacement(m_window);
            return false;
        }
        default:
            return false;
        }
    }

private:
    RECT monitorWork(HWND h) const {
        MONITORINFO info{sizeof(info)};
        HMONITOR mon = MonitorFromWindow(h, MONITOR_DEFAULTTONEAREST);
        if (GetMonitorInfoW(mon, &info)) return info.rcWork;
        RECT rc{};
        SystemParametersInfoW(SPI_GETWORKAREA, 0, &rc, 0);
        return rc;
    }

    bool handleNcCalcSize(MSG *msg, qintptr *result) {
        if (!msg->wParam) {
            *result = 0;
            return true;
        }
        auto *params = reinterpret_cast<NCCALCSIZE_PARAMS *>(msg->lParam);
        if (IsZoomed(msg->hwnd)) params->rgrc[0] = monitorWork(msg->hwnd);
        *result = 0;
        return true;
    }

    bool handleGetMinMaxInfo(MSG *msg, qintptr *result) {
        DefWindowProcW(msg->hwnd, msg->message, msg->wParam, msg->lParam);
        auto *info = reinterpret_cast<MINMAXINFO *>(msg->lParam);
        const RECT work = monitorWork(msg->hwnd);
        const int pad = GetSystemMetrics(SM_CXPADDEDBORDER);
        const int bx = GetSystemMetrics(SM_CXFRAME) + pad;
        const int by = GetSystemMetrics(SM_CYFRAME) + pad;
        // Max size includes frame insets (maximize restore).
        info->ptMaxPosition.x = work.left - bx;
        info->ptMaxPosition.y = work.top - by;
        info->ptMaxSize.x = (work.right - work.left) + bx * 2;
        info->ptMaxSize.y = (work.bottom - work.top) + by * 2;
        *result = 0;
        return true;
    }

    QQuickItem *titleBar() const {
        if (!m_window) return nullptr;
        return m_window->findChild<QQuickItem *>(QStringLiteral("titleBarItem"));
    }

    bool handleNcHitTest(MSG *msg, qintptr *result) {
        if (m_window->visibility() == QWindow::FullScreen) {
            *result = HTCLIENT;
            return true;
        }
        const HWND h = msg->hwnd;
        POINT p{GET_X_LPARAM(msg->lParam), GET_Y_LPARAM(msg->lParam)};
        ScreenToClient(h, &p);
        RECT rc{};
        GetClientRect(h, &rc);
        const qreal dpr = std::max(m_window->devicePixelRatio(), 1.0);
        const int border = std::max(1, int(std::lround(kResizeBorder * dpr)));
        const bool canResize = m_window->visibility() != QWindow::FullScreen && !IsZoomed(h);
        if (canResize) {
            const bool left = p.x < border;
            const bool right = p.x >= rc.right - border;
            const bool top = p.y < border;
            const bool bottom = p.y >= rc.bottom - border;
            if (top && left) {
                *result = HTTOPLEFT;
                return true;
            }
            if (top && right) {
                *result = HTTOPRIGHT;
                return true;
            }
            if (bottom && left) {
                *result = HTBOTTOMLEFT;
                return true;
            }
            if (bottom && right) {
                *result = HTBOTTOMRIGHT;
                return true;
            }
            if (left) {
                *result = HTLEFT;
                return true;
            }
            if (right) {
                *result = HTRIGHT;
                return true;
            }
            if (top) {
                *result = HTTOP;
                return true;
            }
            if (bottom) {
                *result = HTBOTTOM;
                return true;
            }
        }
        QQuickItem *bar = titleBar();
        if (bar && bar->isVisible()) {
            const qreal lx = qreal(p.x) / dpr;
            const qreal ly = qreal(p.y) / dpr;
            const QPointF origin = bar->mapToScene(QPointF(0, 0));
            const qreal x0 = origin.x();
            const qreal y0 = origin.y();
            const qreal x1 = x0 + bar->width();
            const qreal y1 = y0 + bar->height();
            if (lx >= x0 && lx < x1 && ly >= y0 && ly < y1) {
                if (lx >= x1 - qreal(kCaptionButtonWidth * 3)) {
                    *result = HTCLIENT;
                    return true;
                }
                // Client hit tests so QML title bar always runs restore-under-cursor drag.
                *result = HTCLIENT;
                return true;
            }
        }
        *result = HTCLIENT;
        return true;
    }

    QQuickWindow *m_window = nullptr;
    bool m_initialFrameApplied = false;
    QWindow::Visibility m_lastVisibility = QWindow::Hidden;
};

} // namespace

void installWinWindowChrome(QQuickWindow *window) {
    if (!window) return;
    window->create();
    auto *chrome = new WinChrome(window);
    QGuiApplication::instance()->installNativeEventFilter(chrome);
    chrome->primeFrame();
    QObject::connect(window, &QWindow::visibilityChanged, window,
                     [chrome](QWindow::Visibility visibility) { chrome->onVisibilityChanged(visibility); });
    QObject::connect(window, &QWindow::visibleChanged, window,
                     [chrome](bool visible) { if (visible) chrome->onVisible(); });
    QObject::connect(window, &QObject::destroyed, QGuiApplication::instance(), [chrome] {
        QGuiApplication::instance()->removeNativeEventFilter(chrome);
        delete chrome;
    });
}

#else

void installWinWindowChrome(QQuickWindow *) {}

#endif

bool winWindowIsZoomed(QWindow *window) {
    if (!window) return false;
#ifdef Q_OS_WIN
    const HWND h = reinterpret_cast<HWND>(window->winId());
    if (h && IsZoomed(h)) return true;
#endif
    return window->visibility() == QWindow::Maximized;
}

void winWindowClearNativeZoom(QWindow *window) {
    if (!window) return;
#ifdef Q_OS_WIN
    const HWND h = reinterpret_cast<HWND>(window->winId());
    if (h && IsZoomed(h)) ShowWindow(h, SW_RESTORE);
#endif
    if (window->visibility() == QWindow::FullScreen || window->visibility() == QWindow::Maximized)
        window->showNormal();
    window->setVisibility(QWindow::Windowed);
}

void winWindowUnzoomToGeometry(QWindow *window, int x, int y, int w, int h) {
    if (!window) return;
    window->setProperty("wantFullscreen", false);
#ifdef Q_OS_WIN
    const HWND hwnd = reinterpret_cast<HWND>(window->winId());
    if (hwnd) {
        const qreal dpr = std::max(window->devicePixelRatio(), 1.0);
        WINDOWPLACEMENT wp{sizeof(wp)};
        GetWindowPlacement(hwnd, &wp);
        wp.flags = 0;
        wp.showCmd = SW_SHOWNORMAL;
        wp.rcNormalPosition = RECT{int(std::lround(x * dpr)), int(std::lround(y * dpr)),
                                   int(std::lround((x + w) * dpr)), int(std::lround((y + h) * dpr))};
        SetWindowPlacement(hwnd, &wp);
    }
#endif
    window->showNormal();
    window->setVisibility(QWindow::Windowed);
    window->setGeometry(x, y, w, h);
    winWindowSyncNormalPlacement(window);
}

void winWindowSyncNormalPlacement(QWindow *window) {
    if (!window) return;
#ifdef Q_OS_WIN
    const HWND hwnd = reinterpret_cast<HWND>(window->winId());
    if (!hwnd || IsZoomed(hwnd) || IsIconic(hwnd)) return;
    RECT rc{};
    GetWindowRect(hwnd, &rc);
    WINDOWPLACEMENT wp{sizeof(wp)};
    GetWindowPlacement(hwnd, &wp);
    wp.rcNormalPosition = rc;
    if (wp.showCmd == SW_SHOWMAXIMIZED) wp.showCmd = SW_SHOWNORMAL;
    SetWindowPlacement(hwnd, &wp);
#endif
}

} // namespace papercut
