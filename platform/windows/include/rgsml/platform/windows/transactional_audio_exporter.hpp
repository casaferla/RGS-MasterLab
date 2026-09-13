#pragma once

#include <rgsml/audio/audio_buffer_view.hpp>
#include <rgsml/audio/audio_format.hpp>
#include <rgsml/audio/wav_format.hpp>
#include <rgsml/core/frame_time.hpp>
#include <rgsml/core/resource_reference.hpp>
#include <rgsml/core/result.hpp>

#include <cstdint>
#include <string>

namespace rgsml::platform::windows {

enum class ExportCollisionPolicy : std::uint8_t {
    FAIL_IF_EXISTS,
};

enum class ExportVerification : std::uint8_t {
    VERIFIED,
};

class TransactionalAudioExportRequest final {
public:
    [[nodiscard]] static core::Result<TransactionalAudioExportRequest> create(
        core::ResourceReference source,
        audio::AudioBufferView payload,
        core::ResourceReference destination,
        audio::WavSampleFormat sampleFormat,
        ExportCollisionPolicy collisionPolicy);

    [[nodiscard]] const core::ResourceReference& source() const noexcept;
    [[nodiscard]] audio::AudioBufferView payload() const noexcept;
    [[nodiscard]] const core::ResourceReference& destination() const noexcept;
    [[nodiscard]] audio::WavSampleFormat sample_format() const noexcept;
    [[nodiscard]] ExportCollisionPolicy collision_policy() const noexcept;

private:
    TransactionalAudioExportRequest(
        core::ResourceReference source,
        audio::AudioBufferView payload,
        core::ResourceReference destination,
        audio::WavSampleFormat sampleFormat,
        ExportCollisionPolicy collisionPolicy) noexcept;

    core::ResourceReference source_;
    audio::AudioBufferView payload_;
    core::ResourceReference destination_;
    audio::WavSampleFormat sample_format_;
    ExportCollisionPolicy collision_policy_;
};

class TransactionalAudioExportResult final {
public:
    [[nodiscard]] const core::ResourceReference& destination() const noexcept;
    [[nodiscard]] audio::WavContainerKind container_kind() const noexcept;
    [[nodiscard]] audio::WavSampleFormat sample_format() const noexcept;
    [[nodiscard]] const audio::AudioFormat& audio_format() const noexcept;
    [[nodiscard]] core::FrameCount frame_count() const noexcept;
    [[nodiscard]] std::uint64_t exact_file_size_bytes() const noexcept;
    [[nodiscard]] const std::string& encoded_file_sha256() const noexcept;
    [[nodiscard]] const std::string& decoded_audio_sha256() const noexcept;
    [[nodiscard]] ExportVerification verification() const noexcept;

private:
    friend class TransactionalAudioExporter;

    TransactionalAudioExportResult(
        core::ResourceReference destination,
        audio::WavContainerKind containerKind,
        audio::WavSampleFormat sampleFormat,
        audio::AudioFormat audioFormat,
        core::FrameCount frameCount,
        std::uint64_t exactFileSizeBytes,
        std::string encodedFileSha256,
        std::string decodedAudioSha256) noexcept;

    core::ResourceReference destination_;
    audio::WavContainerKind container_kind_;
    audio::WavSampleFormat sample_format_;
    audio::AudioFormat audio_format_;
    core::FrameCount frame_count_;
    std::uint64_t exact_file_size_bytes_;
    std::string encoded_file_sha256_;
    std::string decoded_audio_sha256_;
};

class TransactionalAudioExporter final {
public:
    [[nodiscard]] static core::Result<TransactionalAudioExportResult>
    export_new_file(const TransactionalAudioExportRequest& request);
};

}  // namespace rgsml::platform::windows
