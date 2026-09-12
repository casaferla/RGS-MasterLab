#include <rgsml/dsp/module_registry.hpp>
#include <rgsml/dsp/processing_chain.hpp>

#include <cstddef>

int main()
{
    auto registry = rgsml::dsp::ModuleRegistry::create_dsp_package_v1();
    return registry.value() != nullptr
            && registry.value()->descriptors().size() == std::size_t{11}
            && registry.value()->factory_count() == std::size_t{0}
        ? 0
        : 1;
}
