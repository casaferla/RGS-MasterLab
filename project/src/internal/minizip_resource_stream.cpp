#include "minizip_resource_stream.hpp"

extern "C" {
#include <mz.h>
}

#include <limits>

namespace rgsml::project::internal {
namespace {

ResourceStream* self(void* stream) { return static_cast<ResourceStream*>(stream); }
std::int32_t bridge_open(void* stream, const char*, std::int32_t)
{
    self(stream)->open = true;
    return MZ_OK;
}
std::int32_t bridge_is_open(void* stream)
{
    return self(stream)->open ? MZ_OK : MZ_OPEN_ERROR;
}
std::int32_t bridge_read(void* stream, void* buffer, std::int32_t size)
{
    auto* bridge = self(stream);
    if (!bridge->reader || size < 0) return MZ_READ_ERROR;
    auto result = bridge->reader->read(
        {static_cast<std::byte*>(buffer), static_cast<std::size_t>(size)});
    if (!result) return bridge->lastError = MZ_READ_ERROR;
    return static_cast<std::int32_t>(*result.value());
}
std::int32_t bridge_write(void* stream, const void* buffer, std::int32_t size)
{
    auto* bridge = self(stream);
    if (!bridge->writer || size < 0) return MZ_WRITE_ERROR;
    auto result = bridge->writer->write(
        {static_cast<const std::byte*>(buffer), static_cast<std::size_t>(size)});
    if (!result) return bridge->lastError = MZ_WRITE_ERROR;
    return static_cast<std::int32_t>(*result.value());
}
std::int64_t bridge_tell(void* stream)
{
    auto* bridge = self(stream);
    auto result = bridge->reader ? bridge->reader->position_bytes()
                                 : bridge->writer->position_bytes();
    if (!result || *result.value() >
        static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max())) {
        return MZ_TELL_ERROR;
    }
    return static_cast<std::int64_t>(*result.value());
}
std::int32_t bridge_seek(void* stream, std::int64_t offset, std::int32_t origin)
{
    auto* bridge = self(stream);
    std::int64_t base = 0;
    if (origin == MZ_SEEK_CUR) base = bridge_tell(stream);
    else if (origin == MZ_SEEK_END) {
        if (!bridge->reader) return MZ_SEEK_ERROR;
        auto size = bridge->reader->size_bytes();
        if (!size || *size.value() >
            static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max())) {
            return MZ_SEEK_ERROR;
        }
        base = static_cast<std::int64_t>(*size.value());
    } else if (origin != MZ_SEEK_SET) return MZ_SEEK_ERROR;
    if (base < 0 || (offset < 0 && offset < -base) ||
        (offset > 0 && base > std::numeric_limits<std::int64_t>::max() - offset)) {
        return MZ_SEEK_ERROR;
    }
    auto result = bridge->reader ?
        bridge->reader->seek_bytes(static_cast<std::uint64_t>(base + offset)) :
        bridge->writer->seek_bytes(static_cast<std::uint64_t>(base + offset));
    return result ? MZ_OK : (bridge->lastError = MZ_SEEK_ERROR);
}
std::int32_t bridge_close(void* stream) { self(stream)->open = false; return MZ_OK; }
std::int32_t bridge_error(void* stream) { return self(stream)->lastError; }
void* bridge_create() { return nullptr; }
void bridge_destroy(void**) {}
std::int32_t bridge_get_prop(void*, std::int32_t, std::int64_t*) { return MZ_EXIST_ERROR; }
std::int32_t bridge_set_prop(void*, std::int32_t, std::int64_t) { return MZ_EXIST_ERROR; }

mz_stream_vtbl kVtable{bridge_open, bridge_is_open, bridge_read, bridge_write,
    bridge_tell, bridge_seek, bridge_close, bridge_error, bridge_create,
    bridge_destroy, bridge_get_prop, bridge_set_prop};

}  // namespace

ResourceStream ResourceStream::reading(core::IResourceReader& endpoint)
{
    return ResourceStream{{&kVtable, nullptr}, &endpoint, nullptr};
}
ResourceStream ResourceStream::writing(core::IResourceWriter& endpoint)
{
    return ResourceStream{{&kVtable, nullptr}, nullptr, &endpoint};
}

}  // namespace rgsml::project::internal
