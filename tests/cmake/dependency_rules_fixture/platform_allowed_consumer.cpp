#include <rgsml/platform/windows/windows_resource_reader.hpp>

#include <type_traits>

static_assert(std::is_final_v<rgsml::platform::windows::WindowsResourceReader>);
static_assert(std::is_base_of_v<
    rgsml::core::IResourceReader,
    rgsml::platform::windows::WindowsResourceReader>);

int main()
{
    return rgsml::platform::windows::WindowsResourceReader::provider_id()
            == "rgsml.windows.local-file"
        ? 0
        : 1;
}
