#include "waveform_item.hpp"
#include "waveform_presentation.hpp"
#include "internal/waveform_viewport.hpp"

#include <rgsml/audio/wav_reader.hpp>

#include "../../unit/audio/wav_test_support.hpp"

#include <QGuiApplication>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QQuickWindow>
#include <QTest>
#include <QWheelEvent>

#include <cstdint>
#include <memory>
#include <optional>
#include <vector>

namespace rgsml::tests {
namespace {

class TestWaveformItem final : public ui::WaveformItem {
public:
    using ui::WaveformItem::commit_pending_seek;
    using ui::WaveformItem::focusOutEvent;
    using ui::WaveformItem::keyPressEvent;
    using ui::WaveformItem::mouseDoubleClickEvent;
    using ui::WaveformItem::mouseMoveEvent;
    using ui::WaveformItem::mousePressEvent;
    using ui::WaveformItem::mouseReleaseEvent;
    using ui::WaveformItem::mouseUngrabEvent;
    using ui::WaveformItem::wheelEvent;
};

[[nodiscard]] std::shared_ptr<const audio::WaveformSummary> summary()
{
    using namespace wav_support;
    std::vector<std::int64_t> codes(10'000U);
    for (std::size_t index = 0; index < codes.size(); ++index) {
        codes[index] = static_cast<std::int64_t>((index * 257U) % 65'536U) - 32'768;
    }
    auto reader = audio::WavReader::open(memory_reader(
        make_wav(1U, 16U, 1U, 48'000U, pcm_payload(codes, 16U)),
        std::make_shared<ReaderControl>()));
    Q_ASSERT(reader);
    auto result = audio::build_waveform_summary(**reader.value());
    Q_ASSERT(result);
    return std::make_shared<audio::WaveformSummary>(std::move(*result.value()));
}

void send_mouse(
    TestWaveformItem& item,
    QEvent::Type type,
    double x,
    Qt::MouseButton button,
    Qt::MouseButtons buttons,
    Qt::KeyboardModifiers modifiers = Qt::NoModifier)
{
    const QPointF position{x, 50.0};
    QMouseEvent event{
        type, position, position, position, button, buttons, modifiers};
    switch (type) {
    case QEvent::MouseButtonPress:
        item.mousePressEvent(&event);
        break;
    case QEvent::MouseMove:
        item.mouseMoveEvent(&event);
        break;
    case QEvent::MouseButtonRelease:
        item.mouseReleaseEvent(&event);
        break;
    case QEvent::MouseButtonDblClick:
        item.mouseDoubleClickEvent(&event);
        break;
    default:
        break;
    }
}

}  // namespace

class WaveformItemInteractionTest final : public QObject {
    Q_OBJECT

private slots:
    void thresholdSeekPanRegionWheelAndKeyboard();
    void forwardAndReverseRegionCreationMatch();
    void canonicalKeyboardZoomIn();
};

void WaveformItemInteractionTest::thresholdSeekPanRegionWheelAndKeyboard()
{
    QQuickWindow window;
    window.resize(640, 240);
    TestWaveformItem item;
    item.setParentItem(window.contentItem());
    item.setWidth(640.0);
    item.setHeight(200.0);
    ui::WaveformPresentation presentation;
    item.set_presentation(&presentation);

    int seekCalls = 0;
    int regionCommits = 0;
    core::FrameIndex lastSeek{0};
    std::optional<core::FrameRange> committed;
    presentation.set_seek_handler([&](core::FrameIndex frame) {
        ++seekCalls;
        lastSeek = frame;
        return core::Status::success();
    });
    presentation.set_region_commit_handler([&](core::FrameRange region) {
        ++regionCommits;
        committed = region;
        presentation.set_displayed_region(region);
        return core::Status::success();
    });
    presentation.publish_ready(summary());
    QVERIFY(presentation.can_navigate());

    const auto dpr = window.devicePixelRatio();
    const auto logical = [dpr](double physical) { return physical / dpr + 0.01; };

    send_mouse(item, QEvent::MouseButtonPress, logical(100), Qt::LeftButton, Qt::LeftButton);
    send_mouse(item, QEvent::MouseMove, logical(103), Qt::NoButton, Qt::LeftButton);
    send_mouse(item, QEvent::MouseButtonRelease, logical(103), Qt::LeftButton, Qt::NoButton);
    QCOMPARE(seekCalls, 0);
    item.commit_pending_seek();
    QCOMPARE(seekCalls, 1);
    QVERIFY(lastSeek.value() >= 0 && lastSeek.value() < 10'000);

    send_mouse(
        item, QEvent::MouseButtonPress, logical(100), Qt::LeftButton,
        Qt::LeftButton, Qt::ShiftModifier);
    send_mouse(
        item, QEvent::MouseMove, logical(104), Qt::NoButton,
        Qt::LeftButton, Qt::ShiftModifier);
    send_mouse(
        item, QEvent::MouseButtonRelease, logical(104), Qt::LeftButton,
        Qt::NoButton, Qt::ShiftModifier);
    QCOMPARE(regionCommits, 1);
    QVERIFY(committed.has_value());
    QVERIFY(committed->begin().value() < committed->end().value());

    send_mouse(
        item, QEvent::MouseButtonPress, logical(200), Qt::LeftButton,
        Qt::LeftButton, Qt::ShiftModifier);
    send_mouse(
        item, QEvent::MouseMove, logical(205), Qt::NoButton,
        Qt::LeftButton, Qt::ShiftModifier);
    send_mouse(
        item, QEvent::MouseButtonRelease, logical(205), Qt::LeftButton,
        Qt::NoButton, Qt::ShiftModifier);
    QCOMPARE(regionCommits, 2);

    send_mouse(
        item, QEvent::MouseButtonPress, logical(250), Qt::LeftButton,
        Qt::LeftButton, Qt::ShiftModifier);
    send_mouse(
        item, QEvent::MouseMove, logical(280), Qt::NoButton,
        Qt::LeftButton, Qt::ShiftModifier);
    send_mouse(
        item, QEvent::MouseButtonRelease, logical(280), Qt::LeftButton,
        Qt::NoButton, Qt::ShiftModifier);
    QCOMPARE(regionCommits, 3);
    const auto physicalWidth = ui::internal::waveform_physical_width(640.0, dpr);
    auto startHandle = presentation.physical_pixel_boundary(
        committed->begin().value(), physicalWidth);
    send_mouse(item, QEvent::MouseButtonPress, logical(startHandle), Qt::LeftButton, Qt::LeftButton);
    send_mouse(item, QEvent::MouseMove, logical(startHandle + 8), Qt::NoButton, Qt::LeftButton);
    send_mouse(item, QEvent::MouseButtonRelease, logical(startHandle + 8), Qt::LeftButton, Qt::NoButton);
    QCOMPARE(regionCommits, 4);
    auto endHandle = presentation.physical_pixel_boundary(
        committed->end().value(), physicalWidth);
    send_mouse(item, QEvent::MouseButtonPress, logical(endHandle), Qt::LeftButton, Qt::LeftButton);
    send_mouse(item, QEvent::MouseMove, logical(endHandle - 8), Qt::NoButton, Qt::LeftButton);
    send_mouse(item, QEvent::MouseButtonRelease, logical(endHandle - 8), Qt::LeftButton, Qt::NoButton);
    QCOMPARE(regionCommits, 5);

    const auto beforeFitRegion = presentation.visible_range();
    const auto regionMiddle = (presentation.physical_pixel_boundary(
        committed->begin().value(), physicalWidth)
        + presentation.physical_pixel_boundary(
            committed->end().value(), physicalWidth)) / 2;

    const auto regionBeforeFillSeek = committed;
    const auto seeksBeforeFillClick = seekCalls;
    send_mouse(item, QEvent::MouseButtonPress, logical(regionMiddle), Qt::LeftButton, Qt::LeftButton);
    send_mouse(item, QEvent::MouseButtonRelease, logical(regionMiddle), Qt::LeftButton, Qt::NoButton);
    QCOMPARE(seekCalls, seeksBeforeFillClick);
    item.commit_pending_seek();
    QCOMPARE(seekCalls, seeksBeforeFillClick + 1);
    QCOMPARE(committed, regionBeforeFillSeek);
    QCOMPARE(
        lastSeek.value(),
        presentation.physical_seek_frame(regionMiddle, physicalWidth).value());

    const auto seeksBeforeRegionDoubleClick = seekCalls;
    send_mouse(item, QEvent::MouseButtonPress, logical(regionMiddle), Qt::LeftButton, Qt::LeftButton);
    send_mouse(item, QEvent::MouseButtonRelease, logical(regionMiddle), Qt::LeftButton, Qt::NoButton);
    send_mouse(item, QEvent::MouseButtonPress, logical(regionMiddle), Qt::LeftButton, Qt::LeftButton);
    send_mouse(item, QEvent::MouseButtonDblClick, logical(regionMiddle), Qt::LeftButton, Qt::LeftButton);
    send_mouse(item, QEvent::MouseButtonRelease, logical(regionMiddle), Qt::LeftButton, Qt::NoButton);
    item.commit_pending_seek();
    QCOMPARE(seekCalls, seeksBeforeRegionDoubleClick);
    QVERIFY(presentation.visible_range() != beforeFitRegion);
    presentation.fitSource();

    const auto visibleBeforeHandleDoubleClick = presentation.visible_range();
    startHandle = presentation.physical_pixel_boundary(
        committed->begin().value(), physicalWidth);
    const auto seeksBeforeHandleDoubleClick = seekCalls;
    send_mouse(item, QEvent::MouseButtonPress, logical(startHandle), Qt::LeftButton, Qt::LeftButton);
    send_mouse(item, QEvent::MouseButtonRelease, logical(startHandle), Qt::LeftButton, Qt::NoButton);
    send_mouse(item, QEvent::MouseButtonDblClick, logical(startHandle), Qt::LeftButton, Qt::LeftButton);
    send_mouse(item, QEvent::MouseButtonRelease, logical(startHandle), Qt::LeftButton, Qt::NoButton);
    item.commit_pending_seek();
    QCOMPARE(seekCalls, seeksBeforeHandleDoubleClick);
    QVERIFY(presentation.visible_range() != visibleBeforeHandleDoubleClick);
    presentation.fitSource();

    QVERIFY(presentation.zoom_at(true, physicalWidth / 2, physicalWidth));
    QVERIFY(!presentation.full_fit());
    const auto seeksBeforeBackgroundDoubleClick = seekCalls;
    send_mouse(item, QEvent::MouseButtonPress, logical(1), Qt::LeftButton, Qt::LeftButton);
    send_mouse(item, QEvent::MouseButtonRelease, logical(1), Qt::LeftButton, Qt::NoButton);
    QCOMPARE(seekCalls, seeksBeforeBackgroundDoubleClick);
    send_mouse(item, QEvent::MouseButtonDblClick, logical(1), Qt::LeftButton, Qt::LeftButton);
    send_mouse(item, QEvent::MouseButtonRelease, logical(1), Qt::LeftButton, Qt::NoButton);
    item.commit_pending_seek();
    QCOMPARE(seekCalls, seeksBeforeBackgroundDoubleClick);
    QVERIFY(presentation.full_fit());

    const auto beforeWheel = presentation.visible_range();
    QWheelEvent wheel{
        QPointF{320.0, 50.0}, QPointF{320.0, 50.0}, QPoint{}, QPoint{0, 120},
        Qt::NoButton, Qt::ControlModifier, Qt::NoScrollPhase, false};
    item.wheelEvent(&wheel);
    const auto afterWheel = presentation.visible_range();
    QVERIFY(afterWheel.end().value() - afterWheel.begin().value()
            < beforeWheel.end().value() - beforeWheel.begin().value());
    const auto beforeShiftWheel = presentation.visible_range();
    QWheelEvent shiftWheel{
        QPointF{320.0, 50.0}, QPointF{320.0, 50.0}, QPoint{}, QPoint{0, -120},
        Qt::NoButton, Qt::ShiftModifier, Qt::NoScrollPhase, false};
    item.wheelEvent(&shiftWheel);
    QVERIFY(presentation.visible_range() != beforeShiftWheel);
    QWheelEvent excessiveWheel{
        QPointF{320.0, 50.0}, QPointF{320.0, 50.0}, QPoint{}, QPoint{0, 1'200},
        Qt::NoButton, Qt::ControlModifier, Qt::NoScrollPhase, false};
    item.wheelEvent(&excessiveWheel);
    QCOMPARE(item.discarded_wheel_steps(), qulonglong{2});

    const auto beforePan = presentation.visible_range();
    send_mouse(item, QEvent::MouseButtonPress, logical(300), Qt::LeftButton, Qt::LeftButton);
    send_mouse(item, QEvent::MouseMove, logical(304), Qt::NoButton, Qt::LeftButton);
    send_mouse(item, QEvent::MouseButtonRelease, logical(304), Qt::LeftButton, Qt::NoButton);
    QCOMPARE(seekCalls, seeksBeforeHandleDoubleClick);
    QVERIFY(presentation.visible_range() != beforePan || beforePan.begin().value() == 0);

    QKeyEvent panRight{QEvent::KeyPress, Qt::Key_Right, Qt::AltModifier};
    item.keyPressEvent(&panRight);
    QVERIFY(panRight.isAccepted());
    QKeyEvent fitSource{QEvent::KeyPress, Qt::Key_Home, Qt::NoModifier};
    item.keyPressEvent(&fitSource);
    QVERIFY(presentation.full_fit());

    send_mouse(
        item, QEvent::MouseButtonPress, logical(150), Qt::LeftButton,
        Qt::LeftButton, Qt::ShiftModifier);
    send_mouse(
        item, QEvent::MouseMove, logical(155), Qt::NoButton,
        Qt::LeftButton, Qt::ShiftModifier);
    QVERIFY(presentation.candidate_region().has_value());
    QKeyEvent escape{QEvent::KeyPress, Qt::Key_Escape, Qt::NoModifier};
    item.keyPressEvent(&escape);
    QVERIFY(!presentation.candidate_region().has_value());
    QCOMPARE(regionCommits, 5);

    send_mouse(
        item, QEvent::MouseButtonPress, logical(200), Qt::LeftButton,
        Qt::LeftButton, Qt::ShiftModifier);
    send_mouse(
        item, QEvent::MouseMove, logical(205), Qt::NoButton,
        Qt::LeftButton, Qt::ShiftModifier);
    QVERIFY(presentation.candidate_region().has_value());
    presentation.publish_building();
    QVERIFY(!presentation.candidate_region().has_value());
    QCOMPARE(regionCommits, 5);
}

void WaveformItemInteractionTest::forwardAndReverseRegionCreationMatch()
{
    QQuickWindow window;
    window.resize(640, 240);
    TestWaveformItem item;
    item.setParentItem(window.contentItem());
    item.setWidth(640.0);
    item.setHeight(200.0);
    ui::WaveformPresentation presentation;
    item.set_presentation(&presentation);
    std::optional<core::FrameRange> committed;
    presentation.set_region_commit_handler([&](core::FrameRange region) {
        committed = region;
        presentation.set_displayed_region(region);
        return core::Status::success();
    });
    presentation.publish_ready(summary());

    const auto dpr = window.devicePixelRatio();
    const auto logical = [dpr](double physical) { return physical / dpr + 0.01; };
    const auto create = [&](std::int64_t from, std::int64_t to) {
        send_mouse(
            item, QEvent::MouseButtonPress, logical(from),
            Qt::LeftButton, Qt::LeftButton, Qt::ShiftModifier);
        send_mouse(
            item, QEvent::MouseMove, logical(to),
            Qt::NoButton, Qt::LeftButton, Qt::ShiftModifier);
        send_mouse(
            item, QEvent::MouseButtonRelease, logical(to),
            Qt::LeftButton, Qt::NoButton, Qt::ShiftModifier);
    };
    create(100, 200);
    QVERIFY(committed.has_value());
    const auto forward = *committed;
    create(200, 100);
    QVERIFY(committed.has_value());
    const auto reverse = *committed;
    QCOMPARE(reverse, forward);
}

void WaveformItemInteractionTest::canonicalKeyboardZoomIn()
{
    QQuickWindow window;
    window.resize(640, 240);
    TestWaveformItem item;
    item.setParentItem(window.contentItem());
    item.setWidth(640.0);
    item.setHeight(200.0);
    ui::WaveformPresentation presentation;
    item.set_presentation(&presentation);
    presentation.publish_ready(summary());

    const auto fullFit = presentation.visible_range();
    QKeyEvent zoomIn{
        QEvent::KeyPress, Qt::Key_Plus, Qt::ControlModifier};
    item.keyPressEvent(&zoomIn);
    QVERIFY(zoomIn.isAccepted());
    const auto zoomed = presentation.visible_range();
    QVERIFY(zoomed.end().value() - zoomed.begin().value()
            < fullFit.end().value() - fullFit.begin().value());
}

}  // namespace rgsml::tests

int main(int argc, char* argv[])
{
    QGuiApplication application(argc, argv);
    rgsml::tests::WaveformItemInteractionTest test;
    return QTest::qExec(&test, argc, argv);
}

#include "test_waveform_item_interaction.moc"
