#pragma once
// Native Phase6 inputs may be borrowed by the synchronous analyzer,
// but an M13 worker must own every gate and eligibility byte.
#include "l3_product_native_one_pass.hpp"
#include <optional>
#include <span>
#include <utility>
#include <vector>

namespace rgsml::app::l3_automatch::m13_frozen {
class M13OwnedNativeSpec final {
public:
    [[nodiscard]] static std::optional<M13OwnedNativeSpec> capture(
        ProductNativeOnePassSpec spec)
    {
        const auto& support=spec.source_support;
        if(!spec.current_full_phase6_source_and_grid||
           spec.exact_m20_fft_size<2U||spec.spectral_bands.empty()||
           support.selected_native_frame_sink||support.selected_stereo_frame_sink)
            return std::nullopt;
        return M13OwnedNativeSpec{std::move(spec)};
    }
    // Borrow only for the duration of the synchronous numerical call.
    // No pointer stored in this value refers to its former owner.
    [[nodiscard]] ProductNativeOnePassSpec borrow() const {
        auto out=spec_;
        out.source_support.spectral_frame_mask=spectral_mask_;
        out.source_support.retained_width_blocks=width_blocks_;
        out.source_support.authoritative_band_eligibility=band_eligibility_;
        return out;
    }
private:
    template<class T>
    [[nodiscard]] static std::vector<T> copy_span(std::span<const T> source) {
        if(source.empty())return {};
        return {source.begin(),source.end()};
    }
    explicit M13OwnedNativeSpec(ProductNativeOnePassSpec spec)
        :spec_(std::move(spec)),
         spectral_mask_(copy_span(spec_.source_support.spectral_frame_mask)),
         width_blocks_(copy_span(spec_.source_support.retained_width_blocks)),
         band_eligibility_(copy_span(spec_.source_support.authoritative_band_eligibility))
    {
        spec_.source_support.spectral_frame_mask={};
        spec_.source_support.retained_width_blocks={};
        spec_.source_support.authoritative_band_eligibility={};
    }
    ProductNativeOnePassSpec spec_;
    std::vector<std::uint8_t> spectral_mask_;
    std::vector<analysis::TonalStereoRetainedInterval> width_blocks_;
    std::vector<std::uint8_t> band_eligibility_;
};
} // namespace rgsml::app::l3_automatch::m13_frozen
