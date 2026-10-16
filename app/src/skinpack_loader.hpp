#pragma once

#include <QObject>
#include <QColor>
#include <QImage>
#include <QQuickImageProvider>
#include <QReadWriteLock>
#include <QUrl>

#include <memory>

namespace rgsml::app {

struct SkinImageStore final {
    mutable QReadWriteLock lock;
    QImage transport;
};

class SkinImageProvider final : public QQuickImageProvider {
public:
    explicit SkinImageProvider(std::shared_ptr<SkinImageStore> store);
    QImage requestImage(const QString& id, QSize* size, const QSize& requestedSize) override;
private:
    std::shared_ptr<SkinImageStore> store_;
};

class SkinPackLoader final : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool active READ active NOTIFY changed)
    Q_PROPERTY(QString skinName READ skinName NOTIFY changed)
    Q_PROPERTY(QString errorString READ errorString NOTIFY changed)
    Q_PROPERTY(int revision READ revision NOTIFY changed)
    Q_PROPERTY(QColor backgroundColor READ backgroundColor NOTIFY changed)
    Q_PROPERTY(QColor borderColor READ borderColor NOTIFY changed)
public:
    explicit SkinPackLoader(std::shared_ptr<SkinImageStore> store, QObject* parent = nullptr);
    bool active() const noexcept { return active_; }
    QString skinName() const { return name_; }
    QString errorString() const { return error_; }
    int revision() const noexcept { return revision_; }
    QColor backgroundColor() const { return backgroundColor_; }
    QColor borderColor() const { return borderColor_; }
    Q_INVOKABLE bool openFromUrl(const QUrl& fileUrl);
    Q_INVOKABLE void reset();
signals:
    void changed();
private:
    bool reject(const QString& reason);
    std::shared_ptr<SkinImageStore> store_;
    bool active_ = false;
    QString name_ = QStringLiteral("RGS Original");
    QString error_;
    int revision_ = 0;
    QColor backgroundColor_ = QColor(QStringLiteral("#0F1820"));
    QColor borderColor_ = QColor(QStringLiteral("#2A3947"));
};

} // namespace rgsml::app
