#include <rgsml/platform/windows/windows_resource_reader.hpp>
#include <rgsml/platform/windows/windows_resource_writer.hpp>
#include <rgsml/platform/windows/transactional_audio_exporter.hpp>

#include <type_traits>

static_assert(std::is_final_v<rgsml::platform::windows::WindowsResourceReader>);
static_assert(std::is_base_of_v<
    rgsml::core::IResourceReader,
    rgsml::platform::windows::WindowsResourceReader>);
static_assert(std::is_base_of_v<
    rgsml::core::IResourceWriter,
    rgsml::platform::windows::WindowsResourceWriter>);
static_assert(std::is_final_v<
    rgsml::platform::windows::TransactionalAudioExporter>);

int main()
{
    return rgsml::platform::windows::WindowsResourceReader::provider_id()
            == "rgsml.windows.local-file"
        ? 0
        : 1;
}
