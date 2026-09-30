#ifndef RGSML_ANALYSIS_SPECTRUM_SNAPSHOT_HPP
#define RGSML_ANALYSIS_SPECTRUM_SNAPSHOT_HPP

#include <cstddef>
#include <cstdint>
#include <vector>

namespace rgsml::analysis {

struct SpectrumSnapshot final {
    bool valid{false};
    std::uint64_t stream_generation{0};
    std::uint64_t analysis_epoch{0};
    std::uint64_t sequence_number{0};
    std::uint32_t sample_rate_hz{44100};
    std::size_t point_count{0};
    std::vector<double> frequencies_hz;
    std::vector<double> dbfs_powers;
};

}  // namespace rgsml::analysis

#endif  // RGSML_ANALYSIS_SPECTRUM_SNAPSHOT_HPP
