#include <rgsml/audio/source_resource.hpp>

#include <rgsml/audio/wav_reader.hpp>

#include <new>
#include <utility>

namespace rgsml::audio {

core::Result<SourceResource> SourceResource::probe(
    std::unique_ptr<core::IResourceReader> reader)
{
    if (!reader) {
        return core::Result<SourceResource>::failure(core::Error{
            core::ErrorCode::InvalidArgument,
            "A Source reader is required.",
        });
    }

    try {
        auto reference = reader->reference();
        auto opened = WavReader::open(std::move(reader));
        if (!opened) {
            return core::Result<SourceResource>::failure(*opened.error());
        }

        auto wavInfo = (*opened.value())->info();
        auto closed = (*opened.value())->close();
        if (!closed) {
            return core::Result<SourceResource>::failure(*closed.error());
        }

        return core::Result<SourceResource>::success(SourceResource{
            std::move(reference),
            wavInfo,
        });
    } catch (const std::bad_alloc&) {
        return core::Result<SourceResource>::failure(core::Error{
            core::ErrorCode::IoFailure,
            "Unable to allocate Source metadata state.",
        });
    }
}

SourceResource::SourceResource(
    core::ResourceReference reference,
    WavStreamInfo wavInfo) noexcept
    : reference_(std::move(reference))
    , wavInfo_(wavInfo)
{
}

const core::ResourceReference& SourceResource::reference() const noexcept
{
    return reference_;
}

const WavStreamInfo& SourceResource::wav_info() const noexcept
{
    return wavInfo_;
}

}  // namespace rgsml::audio
