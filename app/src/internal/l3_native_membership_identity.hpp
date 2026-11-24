#pragma once
// Exact structural population evidence, independent of measured values
// and source identity. Only actual native producers append their real
// grid positions and validity/gate flags. A digest is not a QTP approval.
#include <rgsml/core/sha256.hpp>
#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>

namespace rgsml::app::l3_automatch::m05m06_real {
class NativeMembershipIdentity final {
public:
    explicit NativeMembershipIdentity(std::string_view policy) noexcept {
        append(static_cast<std::uint64_t>(policy.size()));
        digest_.update(std::as_bytes(
            std::span<const char>{policy.data(),policy.size()}));
    }
    void append(std::uint64_t value) noexcept {
        std::array<std::byte,8> bytes{};
        for(unsigned i=0;i<8U;++i)
            bytes[i]=static_cast<std::byte>((value>>(8U*i))&0xffU);
        digest_.update(bytes);
    }
    [[nodiscard]] std::string finish() const {
        auto snapshot=digest_;
        return core::sha256_hex(snapshot.finalize());
    }
private:
    core::Sha256 digest_;
};
} // namespace rgsml::app::l3_automatch::m05m06_real
