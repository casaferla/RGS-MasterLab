#pragma once

#ifdef _WIN32
#include <QAbstractNativeEventFilter>
#include <QQuickItem>
#include <QQuickWindow>

#include <vector>

namespace rgsml::app {

class WindowsWindowChromeHelper final : public QAbstractNativeEventFilter {
public:
    explicit WindowsWindowChromeHelper(QQuickWindow* window);
    ~WindowsWindowChromeHelper() override;

    WindowsWindowChromeHelper(const WindowsWindowChromeHelper&) = delete;
    WindowsWindowChromeHelper& operator=(const WindowsWindowChromeHelper&) = delete;

    void add_exclusion_item(QQuickItem* item);
    bool nativeEventFilter(const QByteArray& eventType, void* message, qintptr* result) override;

private:
    QQuickWindow* window_{nullptr};
    void* nativeWindowId_{nullptr};
    std::vector<QQuickItem*> exclusionItems_;
};

}  // namespace rgsml::app
#endif
