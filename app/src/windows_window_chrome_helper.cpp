#include "windows_window_chrome_helper.hpp"

#ifdef _WIN32
#include <QCoreApplication>
#include <QPoint>
#include <QPointF>

#ifndef NOMINMAX
#define NOMINMAX
#endif

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#include <windows.h>
#include <windowsx.h>

namespace rgsml::app {

WindowsWindowChromeHelper::WindowsWindowChromeHelper(QQuickWindow* window)
    : window_(window)
{
    if (window_ != nullptr) {
        QCoreApplication::instance()->installNativeEventFilter(this);
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
    if (item != nullptr) {
        exclusions_.push_back(item);
    }
}

bool WindowsWindowChromeHelper::nativeEventFilter(
    const QByteArray& eventType,
    void* message,
    qintptr* result)
{
    Q_UNUSED(eventType);
    if (window_ == nullptr || message == nullptr || result == nullptr) {
        return false;
    }

    auto* msg = static_cast<MSG*>(message);
    if (msg->hwnd != reinterpret_cast<HWND>(window_->winId())) {
        return false;
    }

    if (msg->message == WM_NCHITTEST) {
        const int screenX = GET_X_LPARAM(msg->lParam);
        const int screenY = GET_Y_LPARAM(msg->lParam);
        const QPoint globalPos{screenX, screenY};
        const QPointF localPos = window_->mapFromGlobal(globalPos);

        // Header height is 48 logical pixels
        if (localPos.y() >= 0 && localPos.y() < 48 && localPos.x() >= 0 && localPos.x() < window_->width()) {
            // Check if globalPos falls within any interactive exclusion item
            for (const auto& item : exclusions_) {
                if (item != nullptr && item->isVisible() && item->isEnabled()) {
                    const QPointF itemLocal = item->mapFromGlobal(globalPos);
                    if (itemLocal.x() >= 0 && itemLocal.x() < item->width()
                        && itemLocal.y() >= 0 && itemLocal.y() < item->height()) {
                        return false; // Interactive control: let Qt handle normally
                    }
                }
            }
            // Point is in draggable non-interactive header: return HTCAPTION for native Windows titlebar semantics
            *result = HTCAPTION;
            return true;
        }
    }

    return false;
}

}  // namespace rgsml::app
#endif
