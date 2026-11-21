#pragma once
// M18 nonpublishing adapter to the CANONICAL core IResourceWriter byte port.
// The Windows TransactionalAudioExporter owns path safety, staging, commit,
// readback and cancellation; this adapter owns NONE of those responsibilities.
#include "m18_streaming_wav.hpp"
#include <rgsml/core/resource_io.hpp>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <span>

namespace rgsml::audio::internal {

enum class M18CoreSinkFailure : std::uint8_t {
    NONE, WRITER_ERROR, ZERO_PROGRESS, INVALID_TRANSFER, EXCEPTION
};

class M18CoreResourceByteSink final : public M18ByteSink {
public:
    explicit M18CoreResourceByteSink(core::IResourceWriter& writer) noexcept
        : writer_(writer) {}

    [[nodiscard]] bool write_all(
        std::span<const std::uint8_t> bytes) noexcept override
    {
        if (failure_!=M18CoreSinkFailure::NONE)return false;
        if (bytes.empty())return true;
        try {
            const auto asBytes=std::as_bytes(bytes);
            std::size_t offset=0U;
            while(offset<asBytes.size()) {
                const auto outcome=writer_.write(asBytes.subspan(offset));
                if (!outcome ||outcome.value()==nullptr) {
                    failure_=M18CoreSinkFailure::WRITER_ERROR;
                    return false;
                }
                const auto written=*outcome.value();
                if (written==0U) {
                    failure_=M18CoreSinkFailure::ZERO_PROGRESS;
                    return false;
                }
                if (written>asBytes.size()-offset) {
                    failure_=M18CoreSinkFailure::INVALID_TRANSFER;
                    return false;
                }
                if (bytes_accepted_>
                    std::numeric_limits<std::uint64_t>::max()-written) {
                    failure_=M18CoreSinkFailure::INVALID_TRANSFER;
                    return false;
                }
                offset+=written;
                bytes_accepted_+=written;
            }
            return true;
        } catch(...) {
            failure_=M18CoreSinkFailure::EXCEPTION;
            return false;
        }
    }

    [[nodiscard]] M18CoreSinkFailure failure() const noexcept
    { return failure_; }
    [[nodiscard]] std::uint64_t bytes_accepted() const noexcept
    { return bytes_accepted_; }

private:
    core::IResourceWriter& writer_;
    M18CoreSinkFailure failure_{M18CoreSinkFailure::NONE};
    std::uint64_t bytes_accepted_{0};
};
} // namespace rgsml::audio::internal
