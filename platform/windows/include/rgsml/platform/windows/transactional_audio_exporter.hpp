#pragma once

#include <rgsml/audio/audio_buffer_view.hpp>
#include <rgsml/audio/audio_buffer.hpp>
#include <rgsml/audio/audio_format.hpp>
#include <rgsml/audio/wav_format.hpp>
#include <rgsml/core/frame_time.hpp>
#include <rgsml/core/resource_reference.hpp>
#include <rgsml/core/result.hpp>

#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <utility>

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

// PREPRODUCTION M18 only. The product owner of M17 must supply an
// authorization object which keeps source/project/output ownership and
// epoch stable across the atomic Windows commit AND final readback.
// No default implementation, and no implicit GUI or Export Anyway access.
class M18ExportCommitLease {
public:
    virtual ~M18ExportCommitLease() = default;
    // Called with lease held immediately before publishing the candidate.
    [[nodiscard]] virtual core::Status revalidate_current() const = 0;
};

using M18AcquireCommitLease = std::function<
    core::Result<std::unique_ptr<M18ExportCommitLease>>()>;

class M18TransactionalAudioExportRequest final {
public:
    [[nodiscard]] static core::Result<M18TransactionalAudioExportRequest> create(
        core::ResourceReference source,
        std::shared_ptr<const audio::AudioBuffer> immutableVerifiedMaster,
        core::ResourceReference destination,
        audio::WavSampleFormat sampleFormat,
        std::optional<std::uint64_t> pcm24DitherSeed,
        double ceilingDbTp,
        ExportCollisionPolicy collisionPolicy,
        M18AcquireCommitLease acquireCommitLease);

    [[nodiscard]] const core::ResourceReference& source() const noexcept
    {return source_;}
    [[nodiscard]] const core::ResourceReference& destination() const noexcept
    {return destination_;}
    [[nodiscard]] audio::AudioBufferView payload() const noexcept
    {return verified_master_->view();}
    [[nodiscard]] audio::WavSampleFormat sample_format() const noexcept
    {return sample_format_;}
    [[nodiscard]] std::optional<std::uint64_t> dither_seed() const noexcept
    {return dither_seed_;}
    [[nodiscard]] double ceiling_db_tp() const noexcept
    {return ceiling_db_tp_;}
    [[nodiscard]] ExportCollisionPolicy collision_policy() const noexcept
    {return collision_policy_;}
    [[nodiscard]] const M18AcquireCommitLease& commit_lease_factory() const noexcept
    {return commit_lease_factory_;}

private:
    M18TransactionalAudioExportRequest(
        core::ResourceReference source,
        std::shared_ptr<const audio::AudioBuffer> master,
        core::ResourceReference destination,
        audio::WavSampleFormat format,
        std::optional<std::uint64_t> seed,
        double ceilingDbTp,ExportCollisionPolicy policy,
        M18AcquireCommitLease factory) noexcept
        :source_(std::move(source)),verified_master_(std::move(master)),
         destination_(std::move(destination)),sample_format_(format),
         dither_seed_(seed),ceiling_db_tp_(ceilingDbTp),
         collision_policy_(policy),commit_lease_factory_(std::move(factory)) {}

    core::ResourceReference source_;
    std::shared_ptr<const audio::AudioBuffer> verified_master_;
    core::ResourceReference destination_;
    audio::WavSampleFormat sample_format_;
    std::optional<std::uint64_t> dither_seed_;
    double ceiling_db_tp_;
    ExportCollisionPolicy collision_policy_;
    M18AcquireCommitLease commit_lease_factory_;
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

    // Explicit M18-only transaction. The app has NO caller for this API:
    // first establish a genuine M17 owner + epoch locking commit lease.
    [[nodiscard]] static core::Result<TransactionalAudioExportResult>
    export_new_file(const M18TransactionalAudioExportRequest& request);

private:
    [[nodiscard]] static core::Result<TransactionalAudioExportResult>
    export_impl(
        const core::ResourceReference& source,
        audio::AudioBufferView payload,
        const core::ResourceReference& destination,
        audio::WavSampleFormat format,
        ExportCollisionPolicy policy,
        std::optional<std::uint64_t> ditherSeed,
        std::optional<double> ceilingDbTp,
        const M18AcquireCommitLease* commitLeaseFactory);
};

}  // namespace rgsml::platform::windows
