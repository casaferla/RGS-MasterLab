#include <rgsml/project/project_repository.hpp>
#include "raw_zip_validator.hpp"

#include <QtTest/QtTest>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <span>
#include <string>
#include <vector>
#include <string_view>

namespace {

namespace core = rgsml::core;
namespace project = rgsml::project;

struct Storage final { std::vector<std::byte> data; };

class Endpoint final : public core::IResourceReader, public core::IResourceWriter {
public:
    explicit Endpoint(std::shared_ptr<Storage> storage, std::uint64_t reportedSize = 0)
        : storage_(std::move(storage)), reportedSize_(reportedSize),
          reference_(*core::ResourceReference::create(
            "test", "project", true, true).value()) {}
    const core::ResourceReference& reference() const noexcept override { return reference_; }
    core::ResourceCapabilities capabilities() const noexcept override
    { return core::ResourceCapabilities::create(true, true, true, true); }
    core::Result<std::uint64_t> size_bytes() const override
    { return core::Result<std::uint64_t>::success(
        reportedSize_ == 0 ? storage_->data.size() : reportedSize_); }
    core::Result<std::uint64_t> position_bytes() const override
    { return core::Result<std::uint64_t>::success(position_); }
    core::Result<std::size_t> read(std::span<std::byte> dst) override
    {
        if (closed_) return core::Result<std::size_t>::failure(core::Error{core::ErrorCode::InvalidState});
        const auto count = std::min(dst.size(), storage_->data.size() - position_);
        std::copy_n(storage_->data.data() + position_, count, dst.data());
        position_ += count;
        return core::Result<std::size_t>::success(count);
    }
    core::Result<std::size_t> write(std::span<const std::byte> src) override
    {
        if (closed_) return core::Result<std::size_t>::failure(core::Error{core::ErrorCode::InvalidState});
        if (position_ + src.size() > storage_->data.size())
            storage_->data.resize(position_ + src.size());
        std::copy(src.begin(), src.end(), storage_->data.begin() + position_);
        position_ += src.size();
        return core::Result<std::size_t>::success(src.size());
    }
    core::Status seek_bytes(std::uint64_t offset) override
    {
        if (closed_ || offset > storage_->data.size())
            return core::Status::failure(core::Error{core::ErrorCode::OutOfRange});
        position_ = static_cast<std::size_t>(offset);
        return core::Status::success();
    }
    core::Status resize_bytes(std::uint64_t size) override
    {
        if (closed_) return core::Status::failure(core::Error{core::ErrorCode::InvalidState});
        storage_->data.resize(static_cast<std::size_t>(size));
        position_ = std::min(position_, storage_->data.size());
        return core::Status::success();
    }
    core::Status flush() override { return core::Status::success(); }
    core::Status close() override { closed_ = true; return core::Status::success(); }

private:
    std::shared_ptr<Storage> storage_;
    std::uint64_t reportedSize_{0};
    core::ResourceReference reference_;
    std::size_t position_{0};
    bool closed_{false};
};

[[nodiscard]] core::Uuid id(const char* text)
{
    auto parsed = core::Uuid::parse(text);
    Q_ASSERT(parsed);
    return *parsed.value();
}

[[nodiscard]] project::ProjectSnapshot snapshot()
{
    project::ProjectDocument doc;
    doc.projectId = id("a1111111-1111-4111-8111-111111111111");
    doc.displayName = "Roundtrip";
    doc.sourceResourceId = id("a2222222-2222-4222-8222-222222222222");
    doc.resources.push_back({doc.sourceResourceId, "AUDIO",
        {{"rgsml.windows.local-file", "C:/test/source.wav", "source.wav"}}, std::nullopt});
    return *project::ProjectSnapshot::create(std::move(doc)).value();
}

[[nodiscard]] std::shared_ptr<Storage> archive()
{
    auto storage = std::make_shared<Storage>();
    auto saved = project::ProjectRepository::save_create_new(snapshot(),
        std::make_unique<Endpoint>(storage), [storage]() {
            return core::Result<std::unique_ptr<core::IResourceReader>>::success(
                std::make_unique<Endpoint>(storage));
        });
    Q_ASSERT(saved);
    return storage;
}

[[nodiscard]] std::size_t locate(const std::vector<std::byte>& data,
                                 std::initializer_list<std::byte> needle)
{
    const auto found = std::search(data.begin(), data.end(), needle.begin(), needle.end());
    return found == data.end() ? data.size() : static_cast<std::size_t>(found - data.begin());
}

void write16(std::vector<std::byte>& data, std::size_t offset, std::uint16_t value)
{
    data[offset] = std::byte(value & 0xffU);
    data[offset + 1] = std::byte(value >> 8U);
}

void write32(std::vector<std::byte>& data, std::size_t offset, std::uint32_t value)
{
    for (std::size_t i = 0; i < 4U; ++i)
        data[offset + i] = std::byte((value >> (8U * i)) & 0xffU);
}

void write64(std::vector<std::byte>& data, std::size_t offset, std::uint64_t value)
{
    for (std::size_t i = 0; i < 8U; ++i)
        data[offset + i] = std::byte((value >> (8U * i)) & 0xffU);
}

[[nodiscard]] std::uint16_t read16(const std::vector<std::byte>& data, std::size_t offset)
{
    return static_cast<std::uint16_t>(std::to_integer<unsigned>(data[offset]) |
        (std::to_integer<unsigned>(data[offset + 1]) << 8U));
}

[[nodiscard]] std::uint32_t read32(const std::vector<std::byte>& data, std::size_t offset)
{
    return static_cast<std::uint32_t>(read16(data, offset) |
        (std::uint32_t{read16(data, offset + 2)} << 16U));
}

[[nodiscard]] std::size_t central_for_name(const std::vector<std::byte>& data,
                                           std::string_view name)
{
    auto cursor = locate(data,
        {std::byte{0x50}, std::byte{0x4b}, std::byte{0x01}, std::byte{0x02}});
    while (cursor + 46U <= data.size() && read32(data, cursor) == 0x02014b50U) {
        const auto nameLen = read16(data, cursor + 28U);
        const auto extraLen = read16(data, cursor + 30U);
        const auto commentLen = read16(data, cursor + 32U);
        if (cursor + 46U + nameLen + extraLen + commentLen > data.size()) break;
        if (nameLen == name.size() && std::equal(name.begin(), name.end(),
            data.begin() + static_cast<std::ptrdiff_t>(cursor + 46U),
            [](char a, std::byte b) {
                return static_cast<unsigned char>(a) == std::to_integer<unsigned char>(b);
            })) return cursor;
        cursor += 46U + nameLen + extraLen + commentLen;
    }
    return data.size();
}

[[nodiscard]] std::uint32_t crc32(std::span<const std::byte> data)
{
    std::uint32_t crc = 0xffffffffU;
    for (const auto byte : data) {
        crc ^= std::to_integer<unsigned char>(byte);
        for (int bit = 0; bit < 8; ++bit)
            crc = (crc >> 1U) ^ ((crc & 1U) ? 0xedb88320U : 0U);
    }
    return ~crc;
}

[[nodiscard]] bool mutate_manifest(std::vector<std::byte>& data,
                                   std::string_view before, std::string_view after)
{
    if (before.size() != after.size()) return false;
    const auto central = central_for_name(data, "manifest.json");
    if (central == data.size()) return false;
    const auto local = read32(data, central + 42U);
    const auto payload = local + 30U + read16(data, local + 26U) +
        read16(data, local + 28U);
    const auto size = read32(data, central + 24U);
    if (payload > data.size() || size > data.size() - payload) return false;
    auto begin = data.begin() + static_cast<std::ptrdiff_t>(payload);
    auto end = begin + static_cast<std::ptrdiff_t>(size);
    const auto found = std::search(begin, end, before.begin(), before.end(),
        [](std::byte a, char b) {
            return std::to_integer<unsigned char>(a) == static_cast<unsigned char>(b);
        });
    if (found == end) return false;
    for (std::size_t i = 0; i < after.size(); ++i)
        found[static_cast<std::ptrdiff_t>(i)] = std::byte{static_cast<unsigned char>(after[i])};
    const auto checksum = crc32(std::span<const std::byte>(data).subspan(payload, size));
    write32(data, central + 16U, checksum);
    write32(data, local + 14U, checksum);
    return true;
}

class SparseEndpoint final : public core::IResourceReader {
public:
    struct Segment { std::uint64_t offset; std::vector<std::byte> data; };
    SparseEndpoint(std::uint64_t size, std::vector<Segment> segments)
        : size_(size), segments_(std::move(segments)),
          reference_(*core::ResourceReference::create(
              "test", "sparse", true, false).value()) {}
    const core::ResourceReference& reference() const noexcept override { return reference_; }
    core::ResourceCapabilities capabilities() const noexcept override
    { return core::ResourceCapabilities::create(true, true, false, false); }
    core::Result<std::uint64_t> size_bytes() const override
    { return core::Result<std::uint64_t>::success(size_); }
    core::Result<std::uint64_t> position_bytes() const override
    { return core::Result<std::uint64_t>::success(position_); }
    core::Result<std::size_t> read(std::span<std::byte> dst) override
    {
        const auto count = static_cast<std::size_t>(
            std::min<std::uint64_t>(dst.size(), size_ - position_));
        std::fill_n(dst.begin(), count, std::byte{0});
        for (const auto& segment : segments_) {
            const auto from = std::max(position_, segment.offset);
            const auto to = std::min(position_ + count,
                                     segment.offset + segment.data.size());
            if (from < to) {
                std::copy_n(segment.data.begin() +
                        static_cast<std::ptrdiff_t>(from - segment.offset),
                    static_cast<std::size_t>(to - from),
                    dst.begin() + static_cast<std::ptrdiff_t>(from - position_));
            }
        }
        position_ += count;
        return core::Result<std::size_t>::success(count);
    }
    core::Status seek_bytes(std::uint64_t offset) override
    {
        if (offset > size_) return core::Status::failure(core::Error{core::ErrorCode::OutOfRange});
        position_ = offset;
        return core::Status::success();
    }
    core::Status close() override { return core::Status::success(); }
private:
    std::uint64_t size_;
    std::uint64_t position_{0};
    std::vector<Segment> segments_;
    core::ResourceReference reference_;
};

[[nodiscard]] SparseEndpoint sparse_declared_sizes(
    const std::array<std::uint32_t, 5>& optionalSizes)
{
    constexpr std::array<std::string_view, 8> names{
        "mimetype", "manifest.json", "project.json", "artifacts/0",
        "artifacts/1", "artifacts/2", "artifacts/3", "artifacts/4"};
    std::vector<SparseEndpoint::Segment> segments;
    std::vector<std::byte> central;
    std::uint64_t cursor = 0;
    for (std::size_t i = 0; i < names.size(); ++i) {
        const auto size = i < 3U ? 0U : optionalSizes[i - 3U];
        std::vector<std::byte> local(30U + names[i].size(), std::byte{0});
        write32(local, 0, 0x04034b50U);
        write16(local, 4, 20U);
        write16(local, 6, 0x0800U);
        write32(local, 18, size);
        write32(local, 22, size);
        write16(local, 26, static_cast<std::uint16_t>(names[i].size()));
        for (std::size_t k = 0; k < names[i].size(); ++k)
            local[30U + k] = std::byte{static_cast<unsigned char>(names[i][k])};
        segments.push_back({cursor, std::move(local)});
        const auto start = central.size();
        central.resize(start + 46U + names[i].size(), std::byte{0});
        write32(central, start, 0x02014b50U);
        write16(central, start + 4U, 20U);
        write16(central, start + 6U, 20U);
        write16(central, start + 8U, 0x0800U);
        write32(central, start + 20U, size);
        write32(central, start + 24U, size);
        write16(central, start + 28U,
                static_cast<std::uint16_t>(names[i].size()));
        write32(central, start + 42U, static_cast<std::uint32_t>(cursor));
        for (std::size_t k = 0; k < names[i].size(); ++k)
            central[start + 46U + k] =
                std::byte{static_cast<unsigned char>(names[i][k])};
        cursor += 30U + names[i].size() + size;
    }
    const auto cdOffset = cursor;
    const auto cdSize = central.size();
    segments.push_back({cursor, std::move(central)});
    cursor += cdSize;
    std::vector<std::byte> eocd(22U, std::byte{0});
    write32(eocd, 0, 0x06054b50U);
    write16(eocd, 8, static_cast<std::uint16_t>(names.size()));
    write16(eocd, 10, static_cast<std::uint16_t>(names.size()));
    write32(eocd, 12, static_cast<std::uint32_t>(cdSize));
    write32(eocd, 16, static_cast<std::uint32_t>(cdOffset));
    segments.push_back({cursor, std::move(eocd)});
    return SparseEndpoint{cursor + 22U, std::move(segments)};
}

class ProjectPackageTest final : public QObject {
    Q_OBJECT
private slots:
    void create_reopen_validate();
    void optional_payload_roundtrip();
    void raw_size_limit();
    void raw_header_rejections();
    void manifest_hash_rejection();
    void zip64_record_roundtrip();
    void unsafe_path_and_comment_rejections();
    void declared_size_bounds_before_payload_read();
    void valid_crc_manifest_integrity_rejections();
    void sparse_optional_and_aggregate_limits();
};

void ProjectPackageTest::create_reopen_validate()
{
    auto storage = std::make_shared<Storage>();
    auto original = snapshot();
    auto saved = project::ProjectRepository::save_create_new(original,
        std::make_unique<Endpoint>(storage), [storage]() {
            return core::Result<std::unique_ptr<core::IResourceReader>>::success(
                std::make_unique<Endpoint>(storage));
        });
    if (!saved) qWarning() << QString::fromStdString(saved.error()->message());
    QVERIFY(saved);
    auto opened = project::ProjectRepository::open(std::make_unique<Endpoint>(storage));
    QVERIFY(opened);
    QCOMPARE(opened.value()->document().projectId.to_string(),
             original.document().projectId.to_string());
    QVERIFY(storage->data.size() > 100U);
}

void ProjectPackageTest::optional_payload_roundtrip()
{
    auto storage = std::make_shared<Storage>();
    auto doc = snapshot().document();
    auto original = project::ProjectSnapshot::create(doc,
        {{"artifacts/cafe.txt", "application/octet-stream", {std::byte{0}, std::byte{0xff}}},
         {"definitions/future.bin", "application/octet-stream", {std::byte{0x42}}}});
    QVERIFY(original);
    QVERIFY(project::ProjectRepository::save_create_new(*original.value(),
        std::make_unique<Endpoint>(storage), [storage]() {
            return core::Result<std::unique_ptr<core::IResourceReader>>::success(
                std::make_unique<Endpoint>(storage));
        }));
    auto opened = project::ProjectRepository::open(std::make_unique<Endpoint>(storage));
    QVERIFY(opened);
    QCOMPARE(opened.value()->optional_entries().size(), 2U);
    QCOMPARE(opened.value()->optional_entries().front().bytes.size(), 2U);
}

void ProjectPackageTest::raw_size_limit()
{
    // Synthetic reported length proves the pre-read bound without allocating 320 MiB.
    auto storage = std::make_shared<Storage>();
    auto opened = project::ProjectRepository::open(std::make_unique<Endpoint>(
        storage, 320ULL * 1024ULL * 1024ULL + 1U));
    QVERIFY(!opened);
    QCOMPARE(opened.error()->code(), core::ErrorCode::OutOfRange);
}

void ProjectPackageTest::raw_header_rejections()
{
    const auto original = archive();
    const auto central = locate(original->data,
        {std::byte{0x50}, std::byte{0x4b}, std::byte{0x01}, std::byte{0x02}});
    const auto local = locate(original->data,
        {std::byte{0x50}, std::byte{0x4b}, std::byte{0x03}, std::byte{0x04}});
    const auto eocd = original->data.size() - 22U;
    QVERIFY(central < original->data.size());
    QVERIFY(local < original->data.size());
    auto reject_mutation = [&](auto mutate) {
        auto changed = std::make_shared<Storage>(*original);
        mutate(changed->data);
        QVERIFY(!project::ProjectRepository::open(std::make_unique<Endpoint>(changed)));
    };
    reject_mutation([&](auto& data) { write16(data, central + 10U, 8); }); // non-STORE
    reject_mutation([&](auto& data) { write16(data, central + 8U, 1); }); // encryption
    reject_mutation([&](auto& data) { write16(data, central + 30U, 4097); }); // extra bound
    reject_mutation([&](auto& data) { write32(data, central + 38U, 0x10U); }); // directory
    reject_mutation([&](auto& data) { write32(data, central + 38U, 0xa0000000U); }); // symlink
    reject_mutation([&](auto& data) { write16(data, eocd + 4U, 1); }); // split disk
    reject_mutation([&](auto& data) { write16(data, eocd + 10U, 1025); }); // record limit
    reject_mutation([&](auto& data) { write16(data, eocd + 10U, 0xffffU); }); // malformed ZIP64
    reject_mutation([&](auto& data) { write16(data, local + 8U, 8); }); // local/CD mismatch
    reject_mutation([&](auto& data) { data[central] = std::byte{0}; }); // zipped/malformed CD
}

void ProjectPackageTest::manifest_hash_rejection()
{
    auto changed = archive();
    const std::string_view needle = "Roundtrip";
    const auto found = std::search(changed->data.begin(), changed->data.end(),
        needle.begin(), needle.end(), [](std::byte a, char b) {
            return std::to_integer<unsigned char>(a) == static_cast<unsigned char>(b);
        });
    QVERIFY(found != changed->data.end());
    *found = std::byte{'r'};
    QVERIFY(!project::ProjectRepository::open(std::make_unique<Endpoint>(changed)));
}

void ProjectPackageTest::zip64_record_roundtrip()
{
    auto storage = archive();
    auto& data = storage->data;
    const auto eocd = data.size() - 22U;
    const auto count = read16(data, eocd + 10U);
    const auto cdSize = read32(data, eocd + 12U);
    const auto cdOffset = read32(data, eocd + 16U);
    std::vector<std::byte> trailer(76U, std::byte{0});
    write32(trailer, 0, 0x06064b50U);
    write64(trailer, 4, 44U);
    write16(trailer, 12, 45U);
    write16(trailer, 14, 45U);
    write64(trailer, 24, count);
    write64(trailer, 32, count);
    write64(trailer, 40, cdSize);
    write64(trailer, 48, cdOffset);
    write32(trailer, 56, 0x07064b50U);
    write64(trailer, 64, eocd);
    write32(trailer, 72, 1U);
    data.insert(data.begin() + static_cast<std::ptrdiff_t>(eocd),
                trailer.begin(), trailer.end());
    const auto newEocd = data.size() - 22U;
    write16(data, newEocd + 8U, 0xffffU);
    write16(data, newEocd + 10U, 0xffffU);
    write32(data, newEocd + 12U, 0xffffffffU);
    write32(data, newEocd + 16U, 0xffffffffU);
    auto opened = project::ProjectRepository::open(std::make_unique<Endpoint>(storage));
    if (!opened) qWarning() << QString::fromStdString(opened.error()->message());
    QVERIFY(opened);
}

void ProjectPackageTest::unsafe_path_and_comment_rejections()
{
    const auto original = archive();
    const auto central = locate(original->data,
        {std::byte{0x50}, std::byte{0x4b}, std::byte{0x01}, std::byte{0x02}});
    const auto local = locate(original->data,
        {std::byte{0x50}, std::byte{0x4b}, std::byte{0x03}, std::byte{0x04}});
    QVERIFY(central < original->data.size());
    QVERIFY(local < original->data.size());
    const auto eocd = original->data.size() - 22U;
    auto reject_mutation = [&](auto mutate) {
        auto changed = std::make_shared<Storage>(*original);
        mutate(changed->data);
        QVERIFY(!project::ProjectRepository::open(std::make_unique<Endpoint>(changed)));
    };
    reject_mutation([&](auto& data) { write16(data, central + 32U, 1U); });
    reject_mutation([&](auto& data) { write16(data, eocd + 20U, 1U); });
    reject_mutation([&](auto& data) { data[central + 46U] = std::byte{'/'}; });
    reject_mutation([&](auto& data) { data[central + 46U] = std::byte{'\\'}; });
    reject_mutation([&](auto& data) { data[local + 30U] = std::byte{'/'}; });
    reject_mutation([&](auto& data) { write32(data, central + 42U, 1U); });
}

void ProjectPackageTest::declared_size_bounds_before_payload_read()
{
    const auto original = archive();
    const auto manifest = central_for_name(original->data, "manifest.json");
    const auto project = central_for_name(original->data, "project.json");
    QVERIFY(manifest < original->data.size());
    QVERIFY(project < original->data.size());
    auto reject_mutation = [&](auto mutate) {
        auto changed = std::make_shared<Storage>(*original);
        mutate(changed->data);
        QVERIFY(!project::ProjectRepository::open(std::make_unique<Endpoint>(changed)));
    };
    reject_mutation([&](auto& data) {
        write32(data, manifest + 20U, 1024U * 1024U + 1U);
        write32(data, manifest + 24U, 1024U * 1024U + 1U);
    });
    reject_mutation([&](auto& data) {
        write32(data, project + 20U, 16U * 1024U * 1024U + 1U);
        write32(data, project + 24U, 16U * 1024U * 1024U + 1U);
    });
    reject_mutation([&](auto& data) { write16(data, manifest + 28U, 513U); });
}

void ProjectPackageTest::valid_crc_manifest_integrity_rejections()
{
    const auto original = archive();
    auto reject_manifest = [&](std::string_view before, std::string_view after) {
        auto changed = std::make_shared<Storage>(*original);
        QVERIFY(mutate_manifest(changed->data, before, after));
        auto opened = project::ProjectRepository::open(std::make_unique<Endpoint>(changed));
        QVERIFY(!opened);
        QCOMPARE(opened.error()->code(), core::ErrorCode::ParseFailure);
        const auto& details = opened.error()->details();
        const auto phase = std::find_if(details.begin(), details.end(),
            [](const core::ErrorDetail& detail) { return detail.key == "phase"; });
        QVERIFY(phase != details.end());
        QCOMPARE(phase->value, std::string("manifest"));
    };
    reject_manifest("\"path\":\"project.json\"", "\"path\":\"project.jsom\"");
    reject_manifest("\"required\":true", "\"required\":null");
    reject_manifest("\"formatId\":\"rgsml-project\"",
                    "\"formatId\":\"rgsml-projecu\"");
}

void ProjectPackageTest::sparse_optional_and_aggregate_limits()
{
    constexpr std::uint32_t mib = 1024U * 1024U;
    auto oversizedEntry = sparse_declared_sizes({64U * mib + 1U, 0, 0, 0, 0});
    auto rejected = project::internal::validate_raw_zip(oversizedEntry);
    QVERIFY(!rejected);
    QCOMPARE(rejected.error()->code(), core::ErrorCode::OutOfRange);
    // Logical holes model payload bytes without allocating or writing 270 MiB.
    auto oversizedAggregate = sparse_declared_sizes(
        {54U * mib, 54U * mib, 54U * mib, 54U * mib, 54U * mib});
    rejected = project::internal::validate_raw_zip(oversizedAggregate);
    QVERIFY(!rejected);
    QCOMPARE(rejected.error()->code(), core::ErrorCode::OutOfRange);
    const auto& details = rejected.error()->details();
    const auto reason = std::find_if(details.begin(), details.end(),
        [](const core::ErrorDetail& detail) { return detail.key == "reason"; });
    QVERIFY(reason != details.end());
    QCOMPARE(reason->value, std::string("entry_or_aggregate_size"));
}

}  // namespace

QTEST_GUILESS_MAIN(ProjectPackageTest)
#include "test_project_package.moc"
