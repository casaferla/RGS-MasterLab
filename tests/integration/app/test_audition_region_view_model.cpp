#include "audition_region_view_model.hpp"
#include "playback_transport_view_model.hpp"

#include <rgsml/core/audio_playback_service.hpp>

#include <QTest>
#include <QSignalSpy>

#include <array>
#include <memory>
#include <optional>
#include <thread>
#include <stdexcept>

namespace rgsml::tests {
namespace {

class RegionPlaybackFake final : public core::IAudioPlaybackService {
public:
    [[nodiscard]] core::Status prepare(const core::ResourceReference&) override
    {
        return core::Status::success();
    }
    [[nodiscard]] core::Status clear() override
    {
        state = core::PlaybackState::NO_SOURCE;
        duration.reset();
        loop.reset();
        return core::Status::success();
    }
    [[nodiscard]] core::Status play() override
    {
        ++playCalls;
        state = core::PlaybackState::PLAYING;
        return core::Status::success();
    }
    [[nodiscard]] core::Status pause() override
    {
        state = core::PlaybackState::PAUSED;
        return core::Status::success();
    }
    [[nodiscard]] core::Status stop() override
    {
        state = core::PlaybackState::STOPPED;
        position = core::FrameIndex{0};
        return core::Status::success();
    }
    [[nodiscard]] core::Status seek(core::FrameIndex requested) override
    {
        ++seekCalls;
        lastSeek = requested;
        if (failSeek) {
            return core::Status::failure(core::Error{
                core::ErrorCode::IoFailure, "Injected seek failure."});
        }
        position = requested;
        return core::Status::success();
    }
    [[nodiscard]] core::Status set_loop(
        std::optional<core::FrameRange> requested) override
    {
        ++setLoopCalls;
        lastLoopArgument = requested;
        if (failSetLoop) {
            return core::Status::failure(core::Error{
                core::ErrorCode::IoFailure, "Injected loop failure."});
        }
        loop = requested;
        if (positionAfterSetLoop) {
            position = *positionAfterSetLoop;
        }
        return core::Status::success();
    }
    [[nodiscard]] core::Result<core::PlaybackSnapshot> snapshot() const override
    {
        return core::Result<core::PlaybackSnapshot>::success(
            core::PlaybackSnapshot{state, position, duration, loop});
    }

    core::PlaybackState state{core::PlaybackState::STOPPED};
    core::FrameIndex position{0};
    std::optional<core::FrameCount> duration{
        *core::FrameCount::create(1'000).value()};
    std::optional<core::FrameRange> loop;
    std::optional<core::FrameRange> lastLoopArgument;
    std::optional<core::FrameIndex> positionAfterSetLoop;
    core::FrameIndex lastSeek{0};
    int playCalls{0};
    int seekCalls{0};
    int setLoopCalls{0};
    bool failSeek{false};
    bool failSetLoop{false};
};

[[nodiscard]] core::FrameRange range(std::int64_t start, std::int64_t end)
{
    return *core::FrameRange::create(
        core::FrameIndex{start}, core::FrameIndex{end}).value();
}

}  // namespace

class AuditionRegionViewModelTest final : public QObject {
    Q_OBJECT

private slots:
    void exactRegionAndTimeEditing();
    void segmentedTimeLongHoursValidationAndTiesEven();
    void explicitSeekIsSingleAndPositionExactAcrossLoopStates();
    void loopTransactionsAndRepositioning();
    void failureAndSourceReplacementRemainTruthful();
    void m18RegionAndLoopBusyForeignThreadAndReentrantCannotMutate();
    void m18InvalidOrUnchangedRegionNeverConsumesProductEpoch();
    void m18NestedRegionMutationDuringChangedSignalIsRejected();
};

void AuditionRegionViewModelTest::
m18RegionAndLoopBusyForeignThreadAndReentrantCannotMutate()
{
    auto service=std::make_unique<RegionPlaybackFake>();
    auto* observed=service.get();
    app::PlaybackTransportViewModel transport{std::move(service)};
    app::AuditionRegionViewModel model{&transport};
    model.source_committed(
        *core::FrameCount::create(1000).value(),
        *core::SampleRate::create(48000).value());
    model.set_waveform_ready(true);
    const auto original=range(10,100);
    const auto replacement=range(20,90);
    QVERIFY(model.set_region(original));
    QVERIFY(model.set_loop_enabled(true));
    QCOMPARE(model.region(),std::optional{original});
    const auto priorLoopCalls=observed->setLoopCalls;
    const auto priorSeekCalls=observed->seekCalls;
    QSignalSpy changed{&model,&app::AuditionRegionViewModel::changed};
    std::size_t attempts=0;
    model.set_private_m18_region_mutation_admission(
        [&](const char* name)->core::Status {
            if(name)++attempts;
            return core::Status::failure(core::Error{
                core::ErrorCode::InvalidState,"M18_REGION_EPOCH_BUSY"});
        });
    QVERIFY(!model.set_region(replacement));
    QVERIFY(!model.set_start(core::FrameIndex{15}));
    QVERIFY(!model.set_end_exclusive(core::FrameIndex{95}));
    QVERIFY(!model.set_loop_enabled(false));
    QVERIFY(!model.clear_region());
    QCOMPARE(attempts,std::size_t{5});
    QCOMPARE(model.region(),std::optional{original});
    QVERIFY(model.loop_enabled());
    QCOMPARE(observed->setLoopCalls,priorLoopCalls);
    QCOMPARE(observed->seekCalls,priorSeekCalls);
    QCOMPARE(changed.count(),0);
    // Same-loop no-op doesn't burn an epoch or disturb playback.
    QVERIFY(model.set_loop_enabled(true));
    QCOMPARE(attempts,std::size_t{5});
    // A foreign worker may not touch any Qt playback/Region field.
    bool deniedForeign=false;
    std::thread worker([&] {
        const auto r=model.set_region(replacement);
        deniedForeign=!r &&r.error()->code()==core::ErrorCode::InvalidState;
        model.nudgeStartForward();
        model.requestClearRegion();
        model.requestLoopEnabled(false);
        model.commitEndSegments(
            QStringLiteral("00"),QStringLiteral("00"),
            QStringLiteral("00"),QStringLiteral("100000000"));
    });
    worker.join();
    QVERIFY(deniedForeign);
    QCOMPARE(model.region(),std::optional{original});
    QVERIFY(model.loop_enabled());
    QCOMPARE(attempts,std::size_t{5});
    QCOMPARE(changed.count(),0);
    // A source-relative seek is not a Project, Region, loop or PCM
    // identity change; owner-thread navigation remains usable during M18.
    QVERIFY(model.seek(core::FrameIndex{42}));
    QCOMPARE(observed->seekCalls,priorSeekCalls+1);
    QCOMPARE(observed->position.value(),std::int64_t{42});
    QCOMPARE(attempts,std::size_t{5});

    bool nestedDenied=false;
    model.set_private_m18_region_mutation_admission(
        [&](const char*)->core::Status {
            const auto nested=model.clear_region();
            nestedDenied=!nested
                &&nested.error()->code()==core::ErrorCode::InvalidState;
            return core::Status::failure(core::Error{
                core::ErrorCode::InvalidState,"M18_REGION_OUTER_DENIED"});
        });
    QVERIFY(!model.set_region(replacement));
    QVERIFY(nestedDenied);
    QCOMPARE(model.region(),std::optional{original});
    QCOMPARE(changed.count(),0);
    model.set_private_m18_region_mutation_admission(
        [](const char*)->core::Status {
            throw std::runtime_error("M18_REGION_CALLBACK_THROW");
        });
    QVERIFY(!model.clear_region());
    QCOMPARE(observed->setLoopCalls,priorLoopCalls);
    QCOMPARE(model.region(),std::optional{original});
    // Future Product gate uninstalled: frozen M17 behavior is unchanged.
    model.set_private_m18_region_mutation_admission({});
    QVERIFY(model.set_loop_enabled(false));
    QVERIFY(model.set_region(replacement));
    QCOMPARE(model.region(),std::optional{replacement});
    QVERIFY(model.clear_region());
    QVERIFY(!model.has_region());
}

void AuditionRegionViewModelTest::
m18InvalidOrUnchangedRegionNeverConsumesProductEpoch()
{
    auto fake=std::make_unique<RegionPlaybackFake>();
    auto* observed=fake.get();
    app::PlaybackTransportViewModel transport{std::move(fake)};
    app::AuditionRegionViewModel model{&transport};
    model.source_committed(
        *core::FrameCount::create(1000).value(),
        *core::SampleRate::create(48000).value());
    model.set_waveform_ready(true);
    const auto current=range(10,200);
    QVERIFY(model.set_region(current));
    QVERIFY(model.set_loop_enabled(true));
    const auto loopBefore=observed->setLoopCalls;
    std::size_t admissionCalls=0;
    model.set_private_m18_region_mutation_admission(
        [&](const char*)->core::Status {
            ++admissionCalls;
            return core::Status::failure(core::Error{
                core::ErrorCode::InvalidState,"M18_BUSY"});
        });
    // Same region and same loop state are true no-ops in M18 mode.
    QVERIFY(model.set_region(current));
    QVERIFY(model.set_loop_enabled(true));
    QCOMPARE(admissionCalls,std::size_t{0});
    QCOMPARE(observed->setLoopCalls,loopBefore);
    QCOMPARE(model.region(),std::optional{current});
    const auto invalid=range(900,1200);
    QVERIFY(!model.set_region(invalid));
    QCOMPARE(admissionCalls,std::size_t{0});
    QVERIFY(model.loop_enabled());
    QCOMPARE(model.region(),std::optional{current});
    // A different valid selection must ask exactly once and be rejected.
    QVERIFY(!model.set_region(range(20,190)));
    QCOMPARE(admissionCalls,std::size_t{1});
    QCOMPARE(model.region(),std::optional{current});
    // No REGION operation is needed for a source-relative playback seek.
    QVERIFY(model.seek(core::FrameIndex{44}));
    QCOMPARE(admissionCalls,std::size_t{1});
    QCOMPARE(observed->position.value(),std::int64_t{44});
    // An unavailable loop cannot consume a Product epoch just to report
    // invalid selection, even while a worker owns the immutable Master.
    model.set_private_m18_region_mutation_admission({});
    QVERIFY(model.set_loop_enabled(false));
    QVERIFY(model.clear_region());
    model.set_private_m18_region_mutation_admission(
        [&](const char*)->core::Status {
            ++admissionCalls;
            return core::Status::failure(core::Error{
                core::ErrorCode::InvalidState,"M18_BUSY"});
        });
    QVERIFY(!model.set_loop_enabled(true));
    QCOMPARE(admissionCalls,std::size_t{1});
    QVERIFY(!model.loop_enabled());
    QVERIFY(!model.has_region());
}

void AuditionRegionViewModelTest::
m18NestedRegionMutationDuringChangedSignalIsRejected()
{
    auto fake=std::make_unique<RegionPlaybackFake>();
    auto* observed=fake.get();
    app::PlaybackTransportViewModel transport{std::move(fake)};
    app::AuditionRegionViewModel model{&transport};
    model.source_committed(
        *core::FrameCount::create(1000).value(),
        *core::SampleRate::create(48000).value());
    model.set_waveform_ready(true);
    QVERIFY(model.set_region(range(20,120)));
    std::size_t admitted=0;
    model.set_private_m18_region_mutation_admission(
        [&](const char*)->core::Status {
            ++admitted;
            return core::Status::success();
        });
    std::size_t callbacks=0;
    std::size_t nestedDenied=0;
    QObject::connect(&model,&app::AuditionRegionViewModel::changed,
        &model,[&] {
            ++callbacks;
            // A synchronous listener cannot change the Region again
            // until the originally admitted publication is complete.
            const auto attempt=model.clear_region();
            if(!attempt &&attempt.error()->code()
                ==core::ErrorCode::InvalidState)++nestedDenied;
        });
    QVERIFY(model.set_region(range(30,110)));
    QCOMPARE(model.region(),std::optional{range(30,110)});
    QCOMPARE(admitted,std::size_t{1});
    QVERIFY(callbacks>=1);
    QCOMPARE(nestedDenied,callbacks);
    QCOMPARE(observed->setLoopCalls,0);
    QVERIFY(model.set_loop_enabled(true));
    QVERIFY(model.loop_enabled());
    QCOMPARE(admitted,std::size_t{2});
    QCOMPARE(nestedDenied,callbacks);
    QVERIFY(model.clear_region());
    QVERIFY(!model.has_region());
    QCOMPARE(admitted,std::size_t{3});
    QCOMPARE(nestedDenied,callbacks);
    // RAII sentinel is released after every operation, so the new
    // independent edit can still be admitted on the same owner thread.
    QVERIFY(model.set_region(range(40,100)));
    QCOMPARE(admitted,std::size_t{4});
    QCOMPARE(model.region(),std::optional{range(40,100)});
}

void AuditionRegionViewModelTest::exactRegionAndTimeEditing()
{
    auto service = std::make_unique<RegionPlaybackFake>();
    auto* observed = service.get();
    app::PlaybackTransportViewModel transport{std::move(service)};
    app::AuditionRegionViewModel model{&transport};
    model.source_committed(
        *core::FrameCount::create(1'000).value(),
        *core::SampleRate::create(10).value());
    model.set_waveform_ready(true);

    QVERIFY(!model.has_region());
    QVERIFY(!model.can_loop());
    QVERIFY(model.set_region(range(2, 8)));
    QVERIFY(model.has_region());
    QCOMPARE(model.region(), std::optional{range(2, 8)});
    QCOMPARE(observed->setLoopCalls, 0);
    QCOMPARE(observed->seekCalls, 0);
    QCOMPARE(model.start_frame_text(), QStringLiteral("2"));
    QCOMPARE(model.end_frame_text(), QStringLiteral("8"));

    model.commitStartSegments(
        QStringLiteral("0"), QStringLiteral("0"),
        QStringLiteral("0"), QStringLiteral("35"));
    QCOMPARE(model.region(), std::optional{range(4, 8)});
    model.commitEndSegments(
        QStringLiteral("00"), QStringLiteral("00"),
        QStringLiteral("00"), QStringLiteral("75"));
    QCOMPARE(model.region(), std::optional{range(4, 8)});
    model.commitStartSegments(
        QStringLiteral("00"), QStringLiteral("00"),
        QStringLiteral("00"), QStringLiteral("15"));
    QCOMPARE(model.region(), std::optional{range(2, 8)});
    QCOMPARE(model.start_hours(), QStringLiteral("00"));
    QCOMPARE(model.start_minutes(), QStringLiteral("00"));
    QCOMPARE(model.start_seconds(), QStringLiteral("00"));
    QCOMPARE(model.start_fraction(), QStringLiteral("200000000"));

    const auto preserved = model.region();
    model.commitEndSegments(
        QStringLiteral("00"), QStringLiteral("00"),
        QStringLiteral("00"), QStringLiteral("1"));
    QCOMPARE(model.region(), preserved);
    QVERIFY(!model.error_message().isEmpty());
    model.commitStartSegments(
        QStringLiteral("x"), QStringLiteral("00"),
        QStringLiteral("00"), QStringLiteral("0"));
    QCOMPARE(model.region(), preserved);
    QVERIFY(!model.error_message().isEmpty());

    QVERIFY(model.set_start(core::FrameIndex{-100}));
    QCOMPARE(model.region(), std::optional{range(0, 8)});
    QVERIFY(model.set_end_exclusive(core::FrameIndex{5'000}));
    QCOMPARE(model.region(), std::optional{range(0, 1'000)});
    QVERIFY(model.set_start(core::FrameIndex{999}));
    QCOMPARE(model.region(), std::optional{range(999, 1'000)});
}

void AuditionRegionViewModelTest::segmentedTimeLongHoursValidationAndTiesEven()
{
    auto service = std::make_unique<RegionPlaybackFake>();
    app::PlaybackTransportViewModel transport{std::move(service)};
    app::AuditionRegionViewModel model{&transport};
    model.source_committed(
        *core::FrameCount::create(4'000'000).value(),
        *core::SampleRate::create(10).value());
    model.set_waveform_ready(true);
    QVERIFY(model.set_region(range(0, 4'000'000)));

    model.commitStartSegments(
        QStringLiteral("1"), QStringLiteral("2"),
        QStringLiteral("3"), QStringLiteral("5"));
    QCOMPARE(model.region(), std::optional{range(37'235, 4'000'000)});
    QCOMPARE(model.start_hours(), QStringLiteral("01"));
    QCOMPARE(model.start_minutes(), QStringLiteral("02"));
    QCOMPARE(model.start_seconds(), QStringLiteral("03"));
    QCOMPARE(model.start_fraction(), QStringLiteral("500000000"));

    model.commitStartSegments(
        QStringLiteral("10"), QStringLiteral("00"),
        QStringLiteral("00"), QStringLiteral("0"));
    QCOMPARE(model.start_hours(), QStringLiteral("10"));
    model.commitStartSegments(
        QStringLiteral("100"), QStringLiteral("00"),
        QStringLiteral("00"), QStringLiteral("0"));
    QCOMPARE(model.region(), std::optional{range(3'600'000, 4'000'000)});
    QCOMPARE(model.start_hours(), QStringLiteral("100"));

    const auto preserved = model.region();
    const auto expectRejected = [&](const QString& hours,
                                    const QString& minutes,
                                    const QString& seconds,
                                    const QString& fraction) {
        model.commitStartSegments(hours, minutes, seconds, fraction);
        QCOMPARE(model.region(), preserved);
        QVERIFY(!model.error_message().isEmpty());
    };
    expectRejected(
        QStringLiteral("100"), QStringLiteral("60"),
        QStringLiteral("00"), QStringLiteral("0"));
    expectRejected(
        QStringLiteral("100"), QStringLiteral("00"),
        QStringLiteral("60"), QStringLiteral("0"));
    expectRejected(
        QStringLiteral("100"), QStringLiteral("00"),
        QStringLiteral("00"), QString{});
    expectRejected(
        QStringLiteral("100"), QStringLiteral("00"),
        QStringLiteral("00"), QStringLiteral("1234567890"));
    expectRejected(
        QStringLiteral("+100"), QStringLiteral("00"),
        QStringLiteral("00"), QStringLiteral("0"));
    expectRejected(
        QString(64, QLatin1Char('9')), QStringLiteral("00"),
        QStringLiteral("00"), QStringLiteral("0"));

    model.source_committed(
        *core::FrameCount::create(1'000).value(),
        *core::SampleRate::create(10).value());
    model.set_waveform_ready(true);
    QVERIFY(model.set_region(range(0, 10)));
    model.commitStartSegments(
        QStringLiteral("00"), QStringLiteral("00"),
        QStringLiteral("00"), QStringLiteral("05"));
    QCOMPARE(model.region(), std::optional{range(0, 10)});
    model.commitStartSegments(
        QStringLiteral("00"), QStringLiteral("00"),
        QStringLiteral("00"), QStringLiteral("15"));
    QCOMPARE(model.region(), std::optional{range(2, 10)});
    model.commitStartSegments(
        QStringLiteral("00"), QStringLiteral("00"),
        QStringLiteral("00"), QStringLiteral("050"));
    QCOMPARE(model.region(), std::optional{range(0, 10)});
    model.commitStartSegments(
        QStringLiteral("00"), QStringLiteral("00"),
        QStringLiteral("00"), QStringLiteral("5"));
    QCOMPARE(model.region(), std::optional{range(5, 10)});

    model.commitEndSegments(
        QStringLiteral("00"), QStringLiteral("01"),
        QStringLiteral("40"), QStringLiteral("0"));
    QCOMPARE(model.region(), std::optional{range(5, 1'000)});
    QCOMPARE(model.end_frame_text(), QStringLiteral("1000"));
    QVERIFY(model.set_region(range(999, 1'000)));
    QCOMPARE(model.region()->end().value(), std::int64_t{1'000});

    model.source_committed(
        *core::FrameCount::create(2'000'000'000).value(),
        *core::SampleRate::create(1'000'000'000).value());
    model.set_waveform_ready(true);
    QVERIFY(model.set_region(range(0, 2'000'000'000)));
    model.commitStartSegments(
        QStringLiteral("00"), QStringLiteral("00"),
        QStringLiteral("00"), QStringLiteral("123456789"));
    QCOMPARE(
        model.region(),
        std::optional{range(123'456'789, 2'000'000'000)});
    QCOMPARE(model.start_fraction(), QStringLiteral("123456789"));
}

void AuditionRegionViewModelTest::explicitSeekIsSingleAndPositionExactAcrossLoopStates()
{
    const std::array states{
        core::PlaybackState::STOPPED,
        core::PlaybackState::PAUSED,
        core::PlaybackState::PLAYING,
    };
    const std::array targets{
        std::int64_t{50}, std::int64_t{150}, std::int64_t{250}};

    for (const bool loopEnabled : {false, true}) {
        auto service = std::make_unique<RegionPlaybackFake>();
        auto* observed = service.get();
        app::PlaybackTransportViewModel transport{std::move(service)};
        app::AuditionRegionViewModel model{&transport};
        model.source_committed(
            *core::FrameCount::create(1'000).value(),
            *core::SampleRate::create(48'000).value());
        model.set_waveform_ready(true);
        QVERIFY(model.set_region(range(100, 200)));
        if (loopEnabled) {
            observed->position = core::FrameIndex{150};
            QVERIFY(model.set_loop_enabled(true));
        }
        const auto expectedRegion = model.region();
        const auto expectedLoop = observed->loop;
        const auto playCallsBefore = observed->playCalls;

        for (const auto state : states) {
            for (const auto target : targets) {
                observed->state = state;
                const auto seeksBefore = observed->seekCalls;
                QVERIFY(model.seek(core::FrameIndex{target}));
                QCOMPARE(observed->seekCalls, seeksBefore + 1);
                QCOMPARE(observed->lastSeek.value(), target);
                QCOMPARE(observed->position.value(), target);
                QCOMPARE(observed->state, state);
                QCOMPARE(observed->loop, expectedLoop);
                QCOMPARE(model.region(), expectedRegion);
                QCOMPARE(observed->playCalls, playCallsBefore);
            }
        }
    }
}

void AuditionRegionViewModelTest::loopTransactionsAndRepositioning()
{
    auto service = std::make_unique<RegionPlaybackFake>();
    auto* observed = service.get();
    app::PlaybackTransportViewModel transport{std::move(service)};
    app::AuditionRegionViewModel model{&transport};
    model.source_committed(
        *core::FrameCount::create(1'000).value(),
        *core::SampleRate::create(48'000).value());
    model.set_waveform_ready(true);

    QVERIFY(!model.set_loop_enabled(true));
    QCOMPARE(observed->setLoopCalls, 0);
    QVERIFY(model.set_region(range(100, 200)));
    observed->position = core::FrameIndex{0};
    QVERIFY(model.set_loop_enabled(true));
    QCOMPARE(observed->setLoopCalls, 1);
    QCOMPARE(observed->lastLoopArgument, std::optional{range(100, 200)});
    QCOMPARE(observed->seekCalls, 1);
    QCOMPARE(observed->lastSeek.value(), std::int64_t{100});
    QVERIFY(model.loop_enabled());
    QCOMPARE(observed->playCalls, 0);

    observed->position = core::FrameIndex{150};
    QVERIFY(model.set_region(range(120, 220)));
    QCOMPARE(observed->setLoopCalls, 2);
    QCOMPARE(observed->seekCalls, 1);
    observed->position = core::FrameIndex{220};
    QVERIFY(model.set_region(range(120, 220)));
    QCOMPARE(observed->setLoopCalls, 3);
    QCOMPARE(observed->seekCalls, 2);

    observed->state = core::PlaybackState::PLAYING;
    observed->position = core::FrameIndex{500};
    observed->positionAfterSetLoop = core::FrameIndex{350};
    QVERIFY(model.set_region(range(300, 400)));
    QCOMPARE(observed->setLoopCalls, 4);
    QCOMPARE(observed->seekCalls, 3);
    QCOMPARE(observed->lastSeek.value(), std::int64_t{300});

    observed->position = core::FrameIndex{350};
    observed->positionAfterSetLoop = core::FrameIndex{250};
    QVERIFY(model.set_region(range(300, 400)));
    QCOMPARE(observed->setLoopCalls, 5);
    QCOMPARE(observed->seekCalls, 3);
    observed->positionAfterSetLoop.reset();

    observed->position = core::FrameIndex{250};
    QVERIFY(model.set_region(range(300, 400)));
    QCOMPARE(observed->setLoopCalls, 6);
    QCOMPARE(observed->seekCalls, 4);
    QCOMPARE(observed->lastSeek.value(), std::int64_t{300});

    QVERIFY(model.set_loop_enabled(false));
    QCOMPARE(observed->setLoopCalls, 7);
    QVERIFY(!observed->lastLoopArgument.has_value());
    QVERIFY(!model.loop_enabled());
    QVERIFY(model.clear_region());
    QVERIFY(!model.has_region());
}

void AuditionRegionViewModelTest::failureAndSourceReplacementRemainTruthful()
{
    auto service = std::make_unique<RegionPlaybackFake>();
    auto* observed = service.get();
    app::PlaybackTransportViewModel transport{std::move(service)};
    app::AuditionRegionViewModel model{&transport};
    model.source_committed(
        *core::FrameCount::create(1'000).value(),
        *core::SampleRate::create(44'100).value());
    model.set_waveform_ready(true);
    QVERIFY(model.set_region(range(100, 200)));
    observed->position = core::FrameIndex{100};
    QVERIFY(model.set_loop_enabled(true));

    const auto previous = model.region();
    observed->failSetLoop = true;
    QVERIFY(!model.set_region(range(300, 400)));
    QCOMPARE(model.region(), previous);
    QVERIFY(model.loop_enabled());
    QVERIFY(!model.error_message().isEmpty());

    QVERIFY(!model.clear_region());
    QCOMPARE(model.region(), previous);
    QVERIFY(model.loop_enabled());
    observed->failSetLoop = false;

    observed->position = core::FrameIndex{0};
    observed->failSeek = true;
    QVERIFY(!model.set_region(range(120, 220)));
    QCOMPARE(model.region(), std::optional{range(120, 220)});
    QVERIFY(model.loop_enabled());
    QVERIFY(model.error_message().contains(QStringLiteral("armed")));

    model.source_committed(
        *core::FrameCount::create(500).value(),
        *core::SampleRate::create(48'000).value());
    QVERIFY(!model.has_region());
    QVERIFY(!model.loop_enabled());
    QVERIFY(!model.controls_enabled());
}

}  // namespace rgsml::tests

QTEST_APPLESS_MAIN(rgsml::tests::AuditionRegionViewModelTest)

#include "test_audition_region_view_model.moc"
