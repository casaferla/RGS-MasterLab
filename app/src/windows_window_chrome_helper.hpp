#pragma once

#ifdef _WIN32
#include <QAbstractNativeEventFilter>
#include <QPointer>
#include <QQuickItem>
#include <QQuickWindow>

#include <vector>

namespace rgsml::app {

class WindowsWindowChromeHelper final : public QAbstractNativeEventFilter {
public:
    explicit WindowsWindowChromeHelper(QQuickWindow* window);
    ~WindowsWindowChromeHelper() override;

    bool nativeEventFilter(
        const QByteArray& eventType,
        void* message,
        qintptr* result) override;

    void add_exclusion_item(QQuickItem* item);

private:
    QPointer<QQuickWindow> window_;
    quintptr nativeWindowId_{0};
    std::vector<QPointer<QQuickItem>> exclusions_;
};

}  // namespace rgsml::app
#endif
