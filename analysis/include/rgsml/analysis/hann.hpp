#ifndef RGSML_ANALYSIS_HANN_HPP
#define RGSML_ANALYSIS_HANN_HPP

#include <cstddef>
#include <vector>

namespace rgsml::analysis {

struct HannWindow final {
    std::vector<double> window;
    double sum_w{0.0};
    double sum_w2{0.0};

    static HannWindow create(std::size_t windowSize);
};

}  // namespace rgsml::analysis

#endif  // RGSML_ANALYSIS_HANN_HPP
