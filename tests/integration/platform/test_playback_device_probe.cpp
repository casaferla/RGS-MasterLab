#include <QAudioDevice>
#include <QAudioFormat>
#include <QAudioSink>
#include <QCoreApplication>
#include <QMediaDevices>
#include <QTextStream>

#include <array>
#include <optional>
#include <QString>

namespace {

struct RateCapabilities final {
    int rate;
    bool float32;
    bool pcm16;
};

struct Selection final {
    int rate;
    QAudioFormat::SampleFormat format;
    bool srcApplied;
};

[[nodiscard]] QAudioFormat format_for(
    int rate,
    QAudioFormat::SampleFormat sampleFormat)
{
    QAudioFormat format;
    format.setSampleRate(rate);
    format.setChannelConfig(QAudioFormat::ChannelConfigStereo);
    format.setSampleFormat(sampleFormat);
    return format;
}

[[nodiscard]] QString sanitized_label(const QAudioDevice& device)
{
    const auto description = device.description();
    if (description.contains(
            QStringLiteral("High Definition Audio Device"),
            Qt::CaseInsensitive)) {
        return QStringLiteral("HEADPHONES_HIGH_DEFINITION_AUDIO");
    }
    if (description.contains(QStringLiteral("LG"), Qt::CaseInsensitive)) {
        return QStringLiteral("LG_DISPLAY_AUDIO");
    }
    return QStringLiteral("OTHER_OUTPUT");
}

[[nodiscard]] std::optional<Selection> select_candidate(
    int sourceRate,
    const RateCapabilities& exact,
    const RateCapabilities& paired)
{
    if (exact.float32) {
        return Selection{sourceRate, QAudioFormat::Float, false};
    }
    if (exact.pcm16) {
        return Selection{sourceRate, QAudioFormat::Int16, false};
    }
    if (paired.float32) {
        return Selection{paired.rate, QAudioFormat::Float, true};
    }
    if (paired.pcm16) {
        return Selection{paired.rate, QAudioFormat::Int16, true};
    }
    return std::nullopt;
}

}  // namespace

int main(int argc, char* argv[])
{
    QCoreApplication application(argc, argv);
    QTextStream output{stdout};
    const auto outputs = QMediaDevices::audioOutputs();
    const auto defaultOutput = QMediaDevices::defaultAudioOutput();
    output << "RGSML_PLAYBACK_DEVICE_PROBE\n";
    output << "OUTPUT_COUNT=" << outputs.size() << "\n";
    output << "DEFAULT_OUTPUT_PRESENT="
           << (!defaultOutput.isNull() ? "true" : "false") << "\n";
    output << "SECOND_OUTPUT_AVAILABLE="
           << (outputs.size() > 1 ? "true" : "false") << "\n";
    for (qsizetype index = 0; index < outputs.size(); ++index) {
        output << "OUTPUT_LABEL_" << index << "="
               << sanitized_label(outputs[index]) << "\n";
    }
    if (defaultOutput.isNull()
        || defaultOutput.mode() != QAudioDevice::Output) {
        output << "RESULT=NO_DEFAULT_OUTPUT\n";
        return 2;
    }

    std::array<RateCapabilities, 2> matrix{};
    for (std::size_t index = 0; index < matrix.size(); ++index) {
        const int rate = index == 0U ? 44100 : 48000;
        const auto floatFormat = format_for(rate, QAudioFormat::Float);
        const auto pcm16Format = format_for(rate, QAudioFormat::Int16);
        matrix[index] = RateCapabilities{
            rate,
            defaultOutput.isFormatSupported(floatFormat),
            defaultOutput.isFormatSupported(pcm16Format),
        };
        output << "DEFAULT_OUTPUT_" << rate << "_STEREO_FLOAT32="
               << (matrix[index].float32 ? "true" : "false") << "\n";
        output << "DEFAULT_OUTPUT_" << rate << "_STEREO_PCM16="
               << (matrix[index].pcm16 ? "true" : "false") << "\n";
    }

    for (std::size_t index = 0; index < matrix.size(); ++index) {
        const auto selected = select_candidate(
            matrix[index].rate, matrix[index], matrix[1U - index]);
        if (!selected) {
            output << "SOURCE_" << matrix[index].rate
                   << "_RESULT=EXACT_AND_PAIRED_UNSUPPORTED\n";
            output << "RESULT=EXACT_AND_PAIRED_UNSUPPORTED\n";
            return 3;
        }
        output << "SOURCE_" << matrix[index].rate
               << "_SELECTED_RATE=" << selected->rate << "\n";
        output << "SOURCE_" << matrix[index].rate << "_SELECTED_FORMAT="
               << (selected->format == QAudioFormat::Float
                       ? "FLOAT32"
                       : "PCM16")
               << "\n";
        output << "SOURCE_" << matrix[index].rate << "_SRC_APPLIED="
               << (selected->srcApplied ? "true" : "false") << "\n";

        QAudioSink routeProbe{
            defaultOutput, format_for(selected->rate, selected->format)};
        if (routeProbe.isNull()) {
            output << "RESULT=SINK_CONSTRUCTION_FAILED\n";
            return 4;
        }
    }
    output << "RESULT=PASS\n";
    return 0;
}
