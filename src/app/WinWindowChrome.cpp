#include "WinWindowChrome.h"

#include <QAbstractNativeEventFilter>
#include <QGuiApplication>
#include <QMetaObject>
#include <QQuickItem>
#include <QQuickWindow>
#include <QRect>
#include <QTimer>
#include <algorithm>
#include <cmath>

#ifdef Q_OS_WIN
#include <dwmapi.h>
#include <windows.h>
#include <windowsx.h>
#endif

namespace papercut {
#ifdef Q_OS_WIN

namespace {

constexpr int kCaptionButtonWidth = 46;
constexpr int kResizeBorder = 8;

class WinChrome final : public QAbstractNativeEventFilter {
public:
    explicit WinChrome(QQuickWindow *window) : m_window(window) {}

    HWND hwnd() const { return reinterpret_cast<HWND>(m_window->winId()); }

    void applyFrameStyle() {
        if (!m_window) return;
        if (!m_restoring && m_window->visibility() == QWindow::FullScreen) return;
        const HWND h = hwnd();
        if (!h) return;
        LONG style = GetWindowLongW(h, GWL_STYLE);
        style &= ~(WS_POPUP | WS_MAXIMIZE);
        style |= WS_CAPTION | WS_THICKFRAME | WS_MAXIMIZEBOX | WS_MINIMIZEBOX | WS_SYSMENU | WS_VISIBLE;
        SetWindowLongW(h, GWL_STYLE, style);
        const MARGINS margins{0, 0, 0, 1};
        DwmExtendFrameIntoClientArea(h, &margins);
        SetWindowPos(h, nullptr, 0, 0, 0, 0,
                     SWP_FRAMECHANGED | SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
    }

    void rememberWindowed() {
        if (!m_window || m_restoring) return;
        if (m_window->visibility() != QWindow::Windowed) return;
        const HWND h = hwnd();
        if (!h || IsZoomed(h) || IsIconic(h)) return;
        if (GetWindowLongW(h, GWL_STYLE) & WS_POPUP) return;
        const QRect g = m_window->geometry();
        if (g.width() < 360 || g.height() < 480) return;
        const RECT work = monitorWork(h);
        const int workW = work.right - work.left;
        const int workH = work.bottom - work.top;
        if (g.width() >= workW - 8 && g.height() >= workH - 8) return;
        m_saved = g;
    }

    void leaveMaximizedOrFullscreen() {
        if (!m_window) return;
        m_restoring = true;
        m_window->setProperty("wantFullscreen", false);
        m_window->setProperty("restoringWindowed", true);
        QMetaObject::invokeMethod(m_window, "restoreWindowed");
        m_window->setWindowState(Qt::WindowNoState);
        m_window->showNormal();
        m_window->setVisibility(QWindow::Windowed);
        applyFrameStyle();
        if (m_saved.isValid() && m_saved.width() >= 360 && m_saved.height() >= 480) {
            m_window->setGeometry(m_saved);
            const HWND h = hwnd();
            if (h) {
                SetWindowPos(h, nullptr, m_saved.x(), m_saved.y(), m_saved.width(), m_saved.height(),
                             SWP_FRAMECHANGED | SWP_NOZORDER | SWP_NOACTIVATE);
                ShowWindow(h, SW_RESTORE);
            }
        }
        QTimer::singleShot(0, m_window, [this] {
            m_restoring = false;
            m_window->setProperty("restoringWindowed", false);
        });
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
        case WM_NCLBUTTONDOWN:
            return handleNcLButtonDown(msg, result);
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
        // Fullscreen: skip HTCAPTION (blocks QML restore).
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
                // Title buttons: HTCLIENT.
                if (lx >= x1 - qreal(kCaptionButtonWidth * 3)) {
                    *result = HTCLIENT;
                    return true;
                }
                *result = HTCAPTION;
                return true;
            }
        }
        *result = HTCLIENT;
        return true;
    }

    bool handleNcLButtonDown(MSG *msg, qintptr *result) {
        if (msg->wParam != HTCAPTION) return false;
        const auto vis = m_window->visibility();
        if (vis != QWindow::FullScreen && vis != QWindow::Maximized) return false;
        QTimer::singleShot(0, m_window, [this] {
            leaveMaximizedOrFullscreen();
            QTimer::singleShot(48, m_window, [this] {
                if (m_window->visibility() == QWindow::FullScreen
                    || m_window->visibility() == QWindow::Maximized)
                    leaveMaximizedOrFullscreen();
                m_window->startSystemMove();
            });
        });
        *result = 0;
        return true;
    }

    QQuickWindow *m_window = nullptr;
    QRect m_saved;
    bool m_restoring = false;
};

} // namespace

void installWinWindowChrome(QQuickWindow *window) {
    if (!window) return;
    auto *chrome = new WinChrome(window);
    QGuiApplication::instance()->installNativeEventFilter(chrome);
    QObject::connect(window, &QWindow::visibilityChanged, window, [chrome] { chrome->applyFrameStyle(); });
    const auto remember = [chrome] { chrome->rememberWindowed(); };
    QObject::connect(window, &QWindow::xChanged, window, remember);
    QObject::connect(window, &QWindow::yChanged, window, remember);
    QObject::connect(window, &QWindow::widthChanged, window, remember);
    QObject::connect(window, &QWindow::heightChanged, window, remember);
    QObject::connect(window, &QObject::destroyed, QGuiApplication::instance(), [chrome] {
        QGuiApplication::instance()->removeNativeEventFilter(chrome);
        delete chrome;
    });
    chrome->applyFrameStyle();
    chrome->rememberWindowed();
}

#else

void installWinWindowChrome(QQuickWindow *) {}

#endif
} // namespace papercut
