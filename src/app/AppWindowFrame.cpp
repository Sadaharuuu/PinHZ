#include "AppWindowFrame.h"
#include <QApplication>
#include <QEvent>
#include <QLibrary>
#include <QPalette>
#include <QPointer>
#include <QWidget>
#include <QVariantMap>

#ifdef Q_OS_WIN
#include <qt_windows.h>
#endif

namespace {
class FrameWatcher : public QObject {
public:
    explicit FrameWatcher(QObject *parent) : QObject(parent) {}

protected:
    bool eventFilter(QObject *object, QEvent *event) override {
        if (event->type() == QEvent::WinIdChange || event->type() == QEvent::Show ||
            event->type() == QEvent::WindowStateChange) {
            if (auto *window = qobject_cast<QWidget *>(object))
                AppWindowFrame::apply(window);
        }
        return false;
    }
};
} // namespace

bool AppWindowFrame::apply(QWidget *window) {
#ifdef Q_OS_WIN
    if (QApplication::platformName() != "windows" || !window || !window->isWindow() ||
        !window->internalWinId() || (window->windowFlags() & Qt::FramelessWindowHint))
        return false;
    const auto type = window->windowType();
    if (type != Qt::Window && type != Qt::Dialog && type != Qt::Tool)
        return false;

    // Resolve dynamically so old Windows versions and Qt's older MinGW SDK still work.
    using SetAttribute = HRESULT(WINAPI *)(HWND, DWORD, LPCVOID, DWORD);
    static QLibrary dwm("dwmapi");
    static const auto setAttribute =
        reinterpret_cast<SetAttribute>(dwm.resolve("DwmSetWindowAttribute"));
    if (!setAttribute)
        return false;
    const HWND handle = reinterpret_cast<HWND>(window->internalWinId());
    const auto palette = qApp->palette();
    const QColor background = palette.color(QPalette::Base);
    const QColor foreground = palette.color(QPalette::WindowText);
    const QColor border = palette.color(QPalette::Mid);
    const BOOL dark = background.lightness() < 128;
    const COLORREF captionColor = RGB(background.red(), background.green(), background.blue());
    const COLORREF textColor = RGB(foreground.red(), foreground.green(), foreground.blue());
    const COLORREF borderColor = RGB(border.red(), border.green(), border.blue());
    const DWORD roundCorners = 2; // DWMWCP_ROUND; Windows manages maximized/snapped corners.
    // Documented DWMWINDOWATTRIBUTE values, absent from the bundled MinGW 5.3 headers.
    QVariantMap applied;
    const auto set = [&](DWORD attribute, LPCVOID value, DWORD size, const QString &name,
                         const QVariant &record) {
        const HRESULT result = setAttribute(handle, attribute, value, size);
        if (SUCCEEDED(result))
            applied.insert(name, record);
    };
    set(20, &dark, sizeof(dark), "dark", bool(dark));
    set(33, &roundCorners, sizeof(roundCorners), "corners", uint(roundCorners));
    set(34, &borderColor, sizeof(borderColor), "border", border.name());
    set(35, &captionColor, sizeof(captionColor), "caption", background.name());
    set(36, &textColor, sizeof(textColor), "text", foreground.name());
    // Record settings only after DWM accepts them; caption colors are not read back.
    window->setProperty("pinHzNativeFrame", applied);
    // Unsupported attributes are ignored; the standard system frame remains usable.
    RedrawWindow(handle, nullptr, nullptr, RDW_FRAME | RDW_INVALIDATE);
    return !applied.isEmpty();
#else
    Q_UNUSED(window);
    return false;
#endif
}

void AppWindowFrame::refresh() {
    if (QApplication::platformName() != "windows")
        return;
    static QPointer<FrameWatcher> watcher;
    if (!watcher) {
        watcher = new FrameWatcher(qApp);
        qApp->installEventFilter(watcher);
    }
    for (QWidget *window : qApp->topLevelWidgets())
        apply(window);
}
