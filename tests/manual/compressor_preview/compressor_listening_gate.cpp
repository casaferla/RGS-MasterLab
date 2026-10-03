#include <rgsml/audio/audio_buffer.hpp>
#include <rgsml/audio/wav_reader.hpp>
#include <rgsml/core/uuid.hpp>
#include <rgsml/dsp/compressor_parameters.hpp>
#include <rgsml/dsp/module_registry.hpp>
#include <rgsml/dsp/processing_chain.hpp>
#include <rgsml/platform/windows/windows_resource_reader.hpp>
#include <rgsml/render/render_preview.hpp>
#include <rgsml/render/render_request.hpp>

#include <QAudioDevice>
#include <QAudioFormat>
#include <QAudioSink>
#include <QBuffer>
#include <QCommandLineOption>
#include <QCommandLineParser>
#include <QCoreApplication>
#include <QEventLoop>
#include <QFileInfo>
#include <QMediaDevices>
#include <QTextStream>
#include <QThread>
#include <QTimer>

#include <algorithm>
#include <bit>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <string>
#include <vector>

namespace {

[[nodiscard]] rgsml::core::FrameCount frame_count(std::int64_t value)
{
    return *rgsml::core::FrameCount::create(value).value();
}

[[nodiscard]] rgsml::core::FrameRange frame_range(std::int64_t end)
{
    return *rgsml::core::FrameRange::create(
        rgsml::core::FrameIndex{0}, rgsml::core::FrameIndex{end}).value();
}

[[nodiscard]] rgsml::dsp::ModuleInstanceId listening_instance_id()
{
    auto uuid = rgsml::core::Uuid::parse("55000000-0000-0000-0000-000000000001");
    return *rgsml::dsp::ModuleInstanceId::from_uuid(*uuid.value()).value();
}

[[nodiscard]] std::vector<std::uint64_t> sample_bits(
    rgsml::audio::AudioBufferView view)
{
    std::vector<std::uint64_t> result;
    for (std::size_t channel = 0; channel < view.format().channel_count(); ++channel) {
        const auto samples = *view.channel(channel).value();
        for (const auto sample : samples) {
            result.push_back(std::bit_cast<std::uint64_t>(sample));
        }
    }
    return result;
}

[[nodiscard]] bool parse_chunks(const QString& text, std::vector<std::int64_t>& chunks)
{
    for (const auto& item : text.split(',', Qt::SkipEmptyParts)) {
        bool ok = false;
        const auto value = item.toLongLong(&ok);
        if (!ok || value <= 0 || !rgsml::core::FrameCount::create(value)) {
            return false;
        }
        chunks.push_back(value);
    }
    return !chunks.empty();
}

[[nodiscard]] rgsml::core::Result<rgsml::render::RenderResult> render_compressor(
    rgsml::audio::AudioBufferView source,
    const rgsml::dsp::ModuleRegistry& registry,
    const rgsml::dsp::ProcessingChain& chain,
    const rgsml::dsp::ModuleInstanceId& id,
    const rgsml::dsp::CompressorParameters& parameters,
    std::int64_t chunk_frames)
{
    auto request = rgsml::render::RenderRequest::create(
        source,
        frame_range(source.frame_count().value()),
        chain,
        {{id, parameters}},
        frame_count(chunk_frames));
    if (!request) {
        return rgsml::core::Result<rgsml::render::RenderResult>::failure(*request.error());
    }
    return rgsml::render::render_preview(*request.value(), registry);
}

[[nodiscard]] QByteArray interleaved_device_bytes(
    rgsml::audio::AudioBufferView view,
    QAudioFormat::SampleFormat sample_format)
{
    QByteArray bytes;
    const auto bytes_per_sample = sample_format == QAudioFormat::Float
        ? sizeof(float)
        : sizeof(std::int16_t);
    bytes.reserve(static_cast<qsizetype>(
        view.frame_count().value()
        * static_cast<std::int64_t>(view.format().channel_count())
        * static_cast<std::int64_t>(bytes_per_sample)));
    for (std::int64_t frame = 0; frame < view.frame_count().value(); ++frame) {
        for (std::size_t channel = 0; channel < view.format().channel_count(); ++channel) {
            const auto sample = (*view.channel(channel).value())[static_cast<std::size_t>(frame)];
            if (sample_format == QAudioFormat::Float) {
                const auto value = static_cast<float>(sample);
                bytes.append(reinterpret_cast<const char*>(&value), sizeof(value));
            } else {
                const auto bounded = std::clamp(sample, -1.0, 1.0);
                const auto value = static_cast<std::int16_t>(std::lround(bounded * 32'767.0));
                bytes.append(reinterpret_cast<const char*>(&value), sizeof(value));
            }
        }
    }
    return bytes;
}

[[nodiscard]] bool play_result(
    rgsml::audio::AudioBufferView view,
    QTextStream& output)
{
    const auto device = QMediaDevices::defaultAudioOutput();
    if (device.isNull()) {
        output << "LISTENING_ERROR=NO_DEFAULT_OUTPUT\n";
        return false;
    }
    QAudioFormat format;
    format.setSampleRate(static_cast<int>(view.format().sample_rate().value()));
    format.setChannelConfig(
        view.format().channel_count() == 1U
            ? QAudioFormat::ChannelConfigMono
            : QAudioFormat::ChannelConfigStereo);
    format.setSampleFormat(QAudioFormat::Float);
    if (!device.isFormatSupported(format)) {
        format.setSampleFormat(QAudioFormat::Int16);
    }
    if (!device.isFormatSupported(format)) {
        output << "LISTENING_ERROR=EXACT_RATE_OUTPUT_UNSUPPORTED\n";
        return false;
    }

    auto bytes = interleaved_device_bytes(view, format.sampleFormat());
    QBuffer stream{&bytes};
    if (!stream.open(QIODevice::ReadOnly)) {
        output << "LISTENING_ERROR=BUFFER_OPEN_FAILED\n";
        return false;
    }
    QAudioSink sink{device, format};
    QEventLoop loop;
    bool completed = false;
    QObject::connect(&sink, &QAudioSink::stateChanged, &loop, [&](QAudio::State state) {
        if (state == QAudio::IdleState) {
            completed = true;
            loop.quit();
        } else if (state == QAudio::StoppedState && sink.error() != QAudio::NoError) {
            loop.quit();
        }
    });
    QTimer::singleShot(120'000, &loop, &QEventLoop::quit);
    sink.start(&stream);
    loop.exec();
    sink.stop();
    if (!completed || sink.error() != QAudio::NoError) {
        output << "LISTENING_ERROR=PLAYBACK_FAILED\n";
        return false;
    }
    return true;
}

}  // namespace

int main(int argc, char* argv[])
{
    QCoreApplication application{argc, argv};
    QCoreApplication::setApplicationName(QStringLiteral("rgsml_compressor_listening_gate"));
    QCommandLineParser parser;
    parser.addHelpOption();
    const QCommandLineOption source_option{
        QStringList{QStringLiteral("source")},
        QStringLiteral("Absolute local Source WAV path."),
        QStringLiteral("path")};
    const QCommandLineOption presets_option{
        QStringList{QStringLiteral("presets")},
        QStringLiteral("Comma-separated audition preset names (default,peak,rms_fast,dual_mono,mix50)."),
        QStringLiteral("presets"),
        QStringLiteral("default,peak,dual_mono")};
    const QCommandLineOption chunks_option{
        QStringList{QStringLiteral("chunks")},
        QStringLiteral("Comma-separated maximum block sizes."),
        QStringLiteral("frames"),
        QStringLiteral("64,257")};
    parser.addOptions({source_option, presets_option, chunks_option});
    parser.process(application);
    QTextStream output{stdout};
    if (!parser.isSet(source_option)) {
        output << "LISTENING_ERROR=SOURCE_REQUIRED\n";
        return 2;
    }

    std::vector<std::int64_t> chunks;
    if (!parse_chunks(parser.value(chunks_option), chunks) || chunks.size() < 2U) {
        output << "LISTENING_ERROR=INVALID_ARGUMENTS\n";
        return 3;
    }

    const auto absolute_path = QFileInfo{parser.value(source_option)}.absoluteFilePath();
    const auto path_utf8 = absolute_path.toUtf8();
    auto reference = rgsml::platform::windows::WindowsResourceReader::make_read_reference(
        std::string_view{path_utf8.constData(), static_cast<std::size_t>(path_utf8.size())},
        "Listening Source WAV");
    if (!reference) {
        output << "LISTENING_ERROR=SOURCE_REFERENCE_REJECTED\n";
        return 4;
    }
    auto resource = rgsml::platform::windows::WindowsResourceReader::open_read_only(
        std::move(*reference.value()));
    if (!resource) {
        output << "LISTENING_ERROR=SOURCE_OPEN_FAILED\n";
        return 5;
    }
    std::unique_ptr<rgsml::core::IResourceReader> resource_reader =
        std::move(*resource.value());
    auto wav = rgsml::audio::WavReader::open(std::move(resource_reader));
    if (!wav) {
        output << "LISTENING_ERROR=WAV_REJECTED\n";
        return 6;
    }
    auto source = rgsml::audio::AudioBuffer::create(
        (*wav.value())->info().audio_format(),
        rgsml::audio::FrameDomainId::SOURCE_PROCESSING_RATE,
        rgsml::core::FrameIndex{0},
        (*wav.value())->info().frame_count());
    if (!source) {
        output << "LISTENING_ERROR=SOURCE_ALLOCATION_FAILED\n";
        return 7;
    }
    auto decoded = (*wav.value())->read_frames(
        rgsml::core::FrameIndex{0}, source.value()->mutable_view());
    if (!decoded || decoded.value()->value() != source.value()->view().frame_count().value()) {
        output << "LISTENING_ERROR=SOURCE_DECODE_FAILED\n";
        return 8;
    }
    static_cast<void>((*wav.value())->close());

    auto registry = rgsml::dsp::ModuleRegistry::create_dsp_package_v1();
    auto chain = rgsml::dsp::ProcessingChain::create(
        *registry.value(),
        {rgsml::dsp::ProcessingStage::MASTER, rgsml::dsp::ChainSegment::MANUAL});
    const auto id = listening_instance_id();
    if (!chain.value()->add(id, "rgsml.dsp.compressor", 0)) {
        output << "LISTENING_ERROR=CHAIN_CREATION_FAILED\n";
        return 10;
    }

    output << "RGSML_COMPRESSOR_LISTENING_GATE\n";
    output << "SOURCE_RATE=" << source.value()->view().format().sample_rate().value() << "\n";
    output << "CHANNELS=" << source.value()->view().format().channel_count() << "\n";
    output << "NO_HIDDEN_NORMALIZATION=TRUE\n";
    output << "LEVEL_MATCH=DISABLED\n";
    output << "CHUNK_PARTITION_BIT_IDENTITY=PENDING\n";
    output.flush();

    const auto preset_str = parser.value(presets_option);
    const auto preset_list = preset_str.split(',', Qt::SkipEmptyParts);

    for (const auto& pr_name : preset_list) {
        rgsml::dsp::CompressorParameters comp_params = *rgsml::dsp::CompressorParameters::create_default().value();
        if (pr_name == QStringLiteral("peak")) {
            comp_params = *rgsml::dsp::CompressorParameters::create(
                rgsml::dsp::CompressorDetectorMode::PEAK, rgsml::dsp::CompressorChannelLink::LINKED_MAX,
                -18.0, 4.0, 0.0, 1.0, 50.0, 50.0, 5.0, 100.0, 0.0).value();
        } else if (pr_name == QStringLiteral("rms_fast")) {
            comp_params = *rgsml::dsp::CompressorParameters::create(
                rgsml::dsp::CompressorDetectorMode::RMS, rgsml::dsp::CompressorChannelLink::LINKED_MAX,
                -18.0, 4.0, 6.0, 10.0, 100.0, 20.0, 5.0, 100.0, 0.0).value();
        } else if (pr_name == QStringLiteral("dual_mono")) {
            comp_params = *rgsml::dsp::CompressorParameters::create(
                rgsml::dsp::CompressorDetectorMode::RMS, rgsml::dsp::CompressorChannelLink::DUAL_MONO,
                -18.0, 4.0, 6.0, 30.0, 200.0, 50.0, 5.0, 100.0, 0.0).value();
        } else if (pr_name == QStringLiteral("mix50")) {
            comp_params = *rgsml::dsp::CompressorParameters::create(
                rgsml::dsp::CompressorDetectorMode::RMS, rgsml::dsp::CompressorChannelLink::LINKED_MAX,
                -18.0, 4.0, 6.0, 30.0, 200.0, 50.0, 5.0, 50.0, 0.0).value();
        }

        auto reference_render = render_compressor(
            source.value()->view(), *registry.value(), *chain.value(), id,
            comp_params, chunks.front());
        if (!reference_render) {
            output << "LISTENING_ERROR=RENDER_FAILED\n";
            return 11;
        }

        const auto reference_bits = sample_bits(reference_render.value()->view());
        for (std::size_t index = 1; index < chunks.size(); ++index) {
            auto candidate = render_compressor(
                source.value()->view(), *registry.value(), *chain.value(), id,
                comp_params, chunks[index]);
            if (!candidate || sample_bits(candidate.value()->view()) != reference_bits) {
                output << "LISTENING_ERROR=CHUNK_PARTITION_MISMATCH\n";
                return 12;
            }
        }

        output << "AUDITION_PRESET=" << pr_name << "\n";
        output.flush();
        if (!play_result(reference_render.value()->view(), output)) {
            return 13;
        }
        QThread::msleep(750);
    }

    output << "CHUNK_PARTITION_BIT_IDENTITY=PASS\n";
    output << "AUTOMATED_HARNESS_RESULT=PASS\n";
    output << "HUMAN_LISTENING_RESULT=REQUIRED\n";
    return 0;
}
