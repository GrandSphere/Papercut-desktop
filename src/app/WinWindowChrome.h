#pragma once

class QQuickWindow;
class QWindow;

namespace papercut {

// Win: native frame, QML title bar.
void installWinWindowChrome(QQuickWindow *window);
bool winWindowIsZoomed(QWindow *window);
void winWindowClearNativeZoom(QWindow *window);
void winWindowUnzoomToGeometry(QWindow *window, int x, int y, int w, int h);
void winWindowSyncNormalPlacement(QWindow *window);

} // namespace papercut
