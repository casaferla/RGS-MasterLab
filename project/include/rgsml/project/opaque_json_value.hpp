#pragma once

#include <rgsml/core/result.hpp>

#include <string>
#include <string_view>

namespace rgsml::project {

// A validated, canonical semantic JSON value. Vendor types never cross this
// boundary. Object key order is semantic-free; array order remains significant.
class OpaqueJsonValue final {
public:
    [[nodiscard]] static core::Result<OpaqueJsonValue> parse(std::string_view utf8);
    [[nodiscard]] static OpaqueJsonValue empty_array() { return OpaqueJsonValue{"[]"}; }
    [[nodiscard]] static OpaqueJsonValue empty_object() { return OpaqueJsonValue{"{}"}; }
    [[nodiscard]] const std::string& canonical_utf8() const noexcept { return bytes_; }
    [[nodiscard]] bool operator==(const OpaqueJsonValue&) const = default;

private:
    explicit OpaqueJsonValue(std::string bytes) : bytes_(std::move(bytes)) {}
    std::string bytes_;
};

}  // namespace rgsml::project
