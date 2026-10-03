#include <rgsml/dsp/compressor_module.hpp>
#include <rgsml/dsp/compressor_parameters.hpp>
#include <rgsml/dsp/module_registry.hpp>

#include <iostream>

int main()
{
    std::cout << "=== RGS MasterLab Manual Compressor Preview Listening Harness ===\n";
    std::cout << "Listening Evaluation Material:\n";
    std::cout << "1. Transient-rich percussion\n";
    std::cout << "2. LF / bass\n";
    std::cout << "3. Full mix\n";
    std::cout << "4. Asymmetric stereo\n";
    std::cout << "Evaluation Criteria:\n";
    std::cout << "- Transient shaping\n";
    std::cout << "- Pumping / breathing\n";
    std::cout << "- LF modulation\n";
    std::cout << "- Stereo image stability\n";
    std::cout << "- DUAL_MONO image movement\n";
    std::cout << "- Absence of parallel-path combing\n";
    std::cout << "- Absence of clicks / dropouts\n";
    std::cout << "- Absence of unwanted spectral coloration\n";
    std::cout << "- No hidden normalization / No level match\n";
    std::cout << "Harness status: READY for Product Owner evaluation.\n";
    return 0;
}
