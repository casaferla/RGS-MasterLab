#pragma once

#include <cstdint>

namespace rgsml::platform::windows::internal {

enum class ExportFailurePoint : std::uint8_t {
    NONE,
    CANDIDATE_CREATE,
    WRITER_IO,
    VALIDATION,
    ENCODED_CHECKSUM,
    DECODED_CHECKSUM,
    COMMIT,
    DESTINATION_READBACK,
    CLEANUP,
};

// Tests are serialized. Production observes NONE unless a private test sets a
// point for the current thread.
void set_export_failure_point_for_test(ExportFailurePoint point) noexcept;

}  // namespace rgsml::platform::windows::internal
