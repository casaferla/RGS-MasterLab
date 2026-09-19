#include <rgsml/core/resource_io.hpp>

extern "C" {
#include <mz.h>
#include <mz_strm.h>
#include <mz_zip.h>
}

#include <QTest>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <span>
#include <string>
#include <vector>

namespace {

namespace core = rgsml::core;

class MemoryEndpoint final : public core::IResourceReader, public core::IResourceWriter {
public:
    MemoryEndpoint()
        : reference_(*core::ResourceReference::create(
            "l1m10-spike", "memory", true, true).value())
    {
    }

    const core::ResourceReference& reference() const noexcept override { return reference_; }
    core::ResourceCapabilities capabilities() const noexcept override
    {
        return core::ResourceCapabilities::create(true, true, true, true);
    }
    core::Result<std::uint64_t> size_bytes() const override
    {
        return core::Result<std::uint64_t>::success(data_.size());
    }
    core::Result<std::uint64_t> position_bytes() const override
    {
        return core::Result<std::uint64_t>::success(position_);
    }
    core::Result<std::size_t> read(std::span<std::byte> destination) override
    {
        const auto available = data_.size() - position_;
        const auto count = std::min(available, destination.size());
        std::copy_n(data_.data() + position_, count, destination.data());
        position_ += count;
        return core::Result<std::size_t>::success(count);
    }
    core::Result<std::size_t> write(std::span<const std::byte> source) override
    {
        if (source.size() > std::numeric_limits<std::size_t>::max() - position_) {
            return core::Result<std::size_t>::failure(
                core::Error{core::ErrorCode::IntegerOverflow, "spike memory size overflow"});
        }
        const auto end = position_ + source.size();
        if (end > data_.size()) {
            data_.resize(end);
        }
        std::copy(source.begin(), source.end(), data_.begin() + position_);
        position_ = end;
        return core::Result<std::size_t>::success(source.size());
    }
    core::Status seek_bytes(std::uint64_t absolute) override
    {
        if (absolute > data_.size()) {
            return core::Status::failure(
                core::Error{core::ErrorCode::OutOfRange, "spike seek past end"});
        }
        position_ = static_cast<std::size_t>(absolute);
        return core::Status::success();
    }
    core::Status resize_bytes(std::uint64_t size) override
    {
        if (size > std::numeric_limits<std::size_t>::max()) {
            return core::Status::failure(
                core::Error{core::ErrorCode::IntegerOverflow, "spike resize overflow"});
        }
        data_.resize(static_cast<std::size_t>(size));
        position_ = std::min(position_, data_.size());
        return core::Status::success();
    }
    core::Status flush() override { return core::Status::success(); }
    core::Status close() override { return core::Status::success(); }
    const std::vector<std::byte>& data() const noexcept { return data_; }

private:
    core::ResourceReference reference_;
    std::vector<std::byte> data_;
    std::size_t position_{0};
};

struct ResourceStream {
    mz_stream stream;
    core::IResourceReader* reader;
    core::IResourceWriter* writer;
    bool open{true};
    int32_t last_error{MZ_OK};
};

ResourceStream* self(void* stream) { return static_cast<ResourceStream*>(stream); }
int32_t bridge_open(void* stream, const char*, int32_t)
{
    self(stream)->open = true;
    return MZ_OK;
}
int32_t bridge_is_open(void* stream) { return self(stream)->open ? MZ_OK : MZ_OPEN_ERROR; }
int32_t bridge_read(void* stream, void* buffer, int32_t size)
{
    auto* bridge = self(stream);
    if (!bridge->reader || size < 0) return MZ_READ_ERROR;
    auto result = bridge->reader->read({static_cast<std::byte*>(buffer), static_cast<std::size_t>(size)});
    if (!result) return bridge->last_error = MZ_READ_ERROR;
    return static_cast<int32_t>(*result.value());
}
int32_t bridge_write(void* stream, const void* buffer, int32_t size)
{
    auto* bridge = self(stream);
    if (!bridge->writer || size < 0) return MZ_WRITE_ERROR;
    auto result = bridge->writer->write({static_cast<const std::byte*>(buffer), static_cast<std::size_t>(size)});
    if (!result) return bridge->last_error = MZ_WRITE_ERROR;
    return static_cast<int32_t>(*result.value());
}
int64_t bridge_tell(void* stream)
{
    auto* bridge = self(stream);
    auto result = bridge->reader ? bridge->reader->position_bytes()
                                 : bridge->writer->position_bytes();
    return result ? static_cast<int64_t>(*result.value()) : MZ_TELL_ERROR;
}
int32_t bridge_seek(void* stream, int64_t offset, int32_t origin)
{
    auto* bridge = self(stream);
    int64_t base = 0;
    if (origin == MZ_SEEK_CUR) base = bridge_tell(stream);
    else if (origin == MZ_SEEK_END) {
        if (!bridge->reader) return MZ_SEEK_ERROR;
        auto size = bridge->reader->size_bytes();
        if (!size) return MZ_SEEK_ERROR;
        base = static_cast<int64_t>(*size.value());
    } else if (origin != MZ_SEEK_SET) return MZ_SEEK_ERROR;
    if (base < 0 || (offset < 0 && offset < -base)
        || (offset > 0 && base > std::numeric_limits<int64_t>::max() - offset)) {
        return MZ_SEEK_ERROR;
    }
    auto result = bridge->reader ? bridge->reader->seek_bytes(static_cast<std::uint64_t>(base + offset))
                                 : bridge->writer->seek_bytes(static_cast<std::uint64_t>(base + offset));
    return result ? MZ_OK : (bridge->last_error = MZ_SEEK_ERROR);
}
int32_t bridge_close(void* stream) { self(stream)->open = false; return MZ_OK; }
int32_t bridge_error(void* stream) { return self(stream)->last_error; }
void* bridge_create() { return nullptr; }
void bridge_destroy(void**) {}
int32_t bridge_get_prop(void*, int32_t, int64_t*) { return MZ_EXIST_ERROR; }
int32_t bridge_set_prop(void*, int32_t, int64_t) { return MZ_EXIST_ERROR; }

mz_stream_vtbl kBridgeVtable{
    bridge_open, bridge_is_open, bridge_read, bridge_write, bridge_tell,
    bridge_seek, bridge_close, bridge_error, bridge_create, bridge_destroy,
    bridge_get_prop, bridge_set_prop};

ResourceStream writing_stream(core::IResourceWriter& writer)
{
    return ResourceStream{{&kBridgeVtable, nullptr}, nullptr, &writer};
}
ResourceStream reading_stream(core::IResourceReader& reader)
{
    return ResourceStream{{&kBridgeVtable, nullptr}, &reader, nullptr};
}

class StoreStreamSpikeTest final : public QObject {
    Q_OBJECT
private slots:
    void autoStoreMultipleUtf8Entries();
    void forcedZip64SmallEntry();
};

void verify_archive(bool forced)
{
    MemoryEndpoint endpoint;
    auto output = writing_stream(endpoint);
    void* writer = mz_zip_create();
    QVERIFY(writer != nullptr);
    QCOMPARE(mz_zip_open(writer, &output.stream, MZ_OPEN_MODE_WRITE | MZ_OPEN_MODE_CREATE), MZ_OK);
    const std::string utf8Name = "caf\xC3\xA9.txt";
    const std::array<std::pair<std::string, std::string>, 2> entries{{
        {"one.txt", "first payload"}, {utf8Name, "second payload"}}};
    for (const auto& [name, payload] : entries) {
        mz_zip_file info{};
        info.filename = name.c_str();
        info.flag = MZ_ZIP_FLAG_UTF8;
        info.compression_method = MZ_COMPRESS_METHOD_STORE;
        info.uncompressed_size = static_cast<int64_t>(payload.size());
        info.compressed_size = static_cast<int64_t>(payload.size());
        info.zip64 = forced ? MZ_ZIP64_FORCE : MZ_ZIP64_AUTO;
        QCOMPARE(mz_zip_entry_write_open(writer, &info, 0, 0, nullptr), MZ_OK);
        QCOMPARE(mz_zip_entry_write(writer, payload.data(), static_cast<int32_t>(payload.size())),
                 static_cast<int32_t>(payload.size()));
        QCOMPARE(mz_zip_entry_write_close(writer, 0, -1, -1), MZ_OK);
    }
    QCOMPARE(mz_zip_close(writer), MZ_OK);
    mz_zip_delete(&writer);
    QVERIFY(endpoint.data().size() > 100U);
    // The upstream force flag must materialize a ZIP64 extra field even when
    // the payload is tiny; AUTO normal archives need no ZIP64 local header.
    const std::array<std::byte, 4> zip64Extra{
        std::byte{0x01}, std::byte{0x00}, std::byte{0x10}, std::byte{0x00}};
    const auto found = std::search(endpoint.data().begin(), endpoint.data().end(),
                                   zip64Extra.begin(), zip64Extra.end());
    QCOMPARE(found != endpoint.data().end(), forced);

    QVERIFY(endpoint.seek_bytes(0));
    auto input = reading_stream(endpoint);
    void* reader = mz_zip_create();
    QVERIFY(reader != nullptr);
    QCOMPARE(mz_zip_open(reader, &input.stream, MZ_OPEN_MODE_READ), MZ_OK);
    QCOMPARE(mz_zip_get_number_entry(reader, nullptr), MZ_PARAM_ERROR);
    std::uint64_t entryCount = 0;
    QCOMPARE(mz_zip_get_number_entry(reader, &entryCount), MZ_OK);
    QCOMPARE(entryCount, std::uint64_t{2});
    std::uint32_t centralDisk = std::numeric_limits<std::uint32_t>::max();
    QCOMPARE(mz_zip_get_disk_number_with_cd(reader, &centralDisk), MZ_OK);
    QCOMPARE(centralDisk, std::uint32_t{0});
    QCOMPARE(mz_zip_goto_first_entry(reader), MZ_OK);
    for (const auto& [name, payload] : entries) {
        mz_zip_file* info = nullptr;
        QCOMPARE(mz_zip_entry_get_info(reader, &info), MZ_OK);
        QVERIFY(info != nullptr);
        QCOMPARE(std::string(info->filename), name);
        QCOMPARE(info->compression_method, std::uint16_t{MZ_COMPRESS_METHOD_STORE});
        QCOMPARE(info->flag & MZ_ZIP_FLAG_ENCRYPTED, 0);
        QCOMPARE(info->aes_version, std::uint16_t{0});
        if (forced) QVERIFY(info->extrafield_size > 0);
        QCOMPARE(info->uncompressed_size, static_cast<int64_t>(payload.size()));
        QCOMPARE(info->disk_number, std::uint32_t{0});
        QCOMPARE(mz_zip_entry_is_dir(reader), MZ_EXIST_ERROR);
        QCOMPARE(mz_zip_entry_is_symlink(reader), MZ_EXIST_ERROR);
        QCOMPARE(mz_zip_entry_read_open(reader, 0, nullptr), MZ_OK);
        std::string decoded(payload.size(), '\0');
        QCOMPARE(mz_zip_entry_read(reader, decoded.data(), static_cast<int32_t>(decoded.size())),
                 static_cast<int32_t>(decoded.size()));
        QCOMPARE(decoded, payload);
        QCOMPARE(mz_zip_entry_read_close(reader, nullptr, nullptr, nullptr), MZ_OK);
        if (name != entries.back().first) QCOMPARE(mz_zip_goto_next_entry(reader), MZ_OK);
    }
    QCOMPARE(mz_zip_close(reader), MZ_OK);
    mz_zip_delete(&reader);
}

void StoreStreamSpikeTest::autoStoreMultipleUtf8Entries() { verify_archive(false); }
void StoreStreamSpikeTest::forcedZip64SmallEntry() { verify_archive(true); }

}  // namespace

QTEST_APPLESS_MAIN(StoreStreamSpikeTest)
#include "test_store_stream.moc"
