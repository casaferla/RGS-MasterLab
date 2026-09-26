#include "windows_window_chrome_helper.hpp"

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#include <windows.h>
#include <windowsx.h>

#include <QCoreApplication>
#include <QPointF>

namespace rgsml::app {

WindowsWindowChromeHelper::WindowsWindowChromeHelper(QQuickWindow* window)
    : window_(window)
{
    if (window_ != nullptr) {
        nativeWindowId_ = reinterpret_cast<void*>(window_->winId());
        QCoreApplication::instance()->installNativeEventFilter(this);

        const HWND hwnd = static_cast<HWND>(nativeWindowId_);
        if (hwnd != nullptr) {
            const LONG style = GetWindowLongW(hwnd, GWL_STYLE);
            SetWindowLongW(hwnd, GWL_STYLE, style | WS_THICKFRAME | WS_MAXIMIZEBOX | WS_CAPTION | WS_SYSMENU);
            SetWindowPos(hwnd, nullptr, 0, 0, 0, 0,
                         SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_FRAMECHANGED);
        }
    }
}

WindowsWindowChromeHelper::~WindowsWindowChromeHelper()
{
    if (QCoreApplication::instance() != nullptr) {
        QCoreApplication::instance()->removeNativeEventFilter(this);
    }
}

void WindowsWindowChromeHelper::add_exclusion_item(QQuickItem* item)
{
    if (item != nullptr && std::find(exclusionItems_.begin(), exclusionItems_.end(), item) == exclusionItems_.end()) {
        exclusionItems_.push_back(item);
    }
}

bool WindowsWindowChromeHelper::nativeEventFilter(
    const QByteArray& eventType,
    void* message,
    qintptr* result)
{
    if (window_ == nullptr || nativeWindowId_ == nullptr) {
        return false;
    }

    MSG* msg = static_cast<MSG*>(message);
    if (msg == nullptr || msg->hwnd != static_cast<HWND>(nativeWindowId_)) {
        return false;
    }

    if (msg->message == WM_NCCALCSIZE && msg->wParam == TRUE) {
        *result = 0;
        return true;
    }

    if (msg->message == WM_NCHITTEST) {
        const POINT globalPt{ GET_X_LPARAM(msg->lParam), GET_Y_LPARAM(msg->lParam) };
        RECT windowRect{};
        GetWindowRect(msg->hwnd, &windowRect);

        const int borderThickness = GetSystemMetrics(SM_CXSIZEFRAME) + GetSystemMetrics(SM_CXPADDEDBORDER);
        const bool top = globalPt.y >= windowRect.top && globalPt.y < windowRect.top + borderThickness;
        const bool bottom = globalPt.y < windowRect.bottom && globalPt.y >= windowRect.bottom - borderThickness;
        const bool left = globalPt.x >= windowRect.left && globalPt.x < windowRect.left + borderThickness;
        const bool right = globalPt.x < windowRect.right && globalPt.x >= windowRect.right - borderThickness;

        if (top && left) { *result = HTTOPLEFT; return true; }
        if (top && right) { *result = HTTOPRIGHT; return true; }
        if (bottom && left) { *result = HTBOTTOMLEFT; return true; }
        if (bottom && right) { *result = HTBOTTOMRIGHT; return true; }
        if (top) { *result = HTTOP; return true; }
        if (bottom) { *result = HTBOTTOM; return true; }
        if (left) { *result = HTLEFT; return true; }
        if (right) { *result = HTRIGHT; return true; }

        // Non-interactive header region returns false so QML headerMouseArea startSystemMove() handles move authority
    }

    return false;
}

}  // namespace rgsml::app
#endif
