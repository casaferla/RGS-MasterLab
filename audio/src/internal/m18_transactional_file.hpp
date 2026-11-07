#pragma once
// M18 PREPRODUCTION: real disk-backed staging + no-overwrite publication.
// Staging occurs in a uniquely created sibling directory on the same volume.
// Publication uses an atomic filesystem hard-link CREATE (never replacement)
// then removes the private staging file. Unsupported volume/link policies
// FAIL CLOSED. This prototype does not promise crash/power-loss durability;
// Windows production exporter owns OS flush+directory durability policy.
// A caller must finish M18StreamingWavWriter before publish. The entire
// SHA-256 and file length are independently read back before publication.
#include "m18_streaming_wav.hpp"

#include <array>
#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <limits>
#include <memory>
#include <span>
#include <string>
#include <system_error>

namespace rgsml::audio::internal {

enum class M18FileStatus : std::uint8_t {
    OK, INVALID_DESTINATION, STAGE_FAILED, WRITE_FAILED,
    VERIFY_FAILED, SHA_MISMATCH, TARGET_EXISTS,
    PUBLISH_FAILED, PUBLISHED_CLEANUP_WARNING
};

class M18TransactionalFile final : public M18ByteSink {
public:
    // Returns null on ANY failure; never opens/changes final destination.
    [[nodiscard]] static std::unique_ptr<M18TransactionalFile> open(
        const std::filesystem::path& destination) noexcept
    {
        namespace fs=std::filesystem;
        try {
            if(destination.empty() || destination.filename().empty())
                return {};
            std::error_code ec;
            if(fs::exists(destination,ec)||ec)return {};
            const auto parent=destination.has_parent_path()
                ?destination.parent_path():fs::path{"."};
            if(!fs::is_directory(parent,ec)||ec)return {};
            static std::atomic<std::uint64_t> counter{0U};
            for(unsigned attempt=0;attempt<32U;++attempt){
                const auto stamp=std::chrono::steady_clock::now()
                    .time_since_epoch().count();
                const auto seq=counter.fetch_add(
                    1U,std::memory_order_relaxed);
                const auto name=destination.filename().string()
                    +".rgsml-m18-private-"
                    +std::to_string(stamp)+"-"+std::to_string(seq);
                const auto privateDir=parent/name;
                ec.clear();
                if(!fs::create_directory(privateDir,ec)){
                    if(ec && ec!=std::make_error_code(std::errc::file_exists))
                        return {};
                    continue;
                }
                const auto stage=privateDir/"candidate.wav";
                auto result=std::unique_ptr<M18TransactionalFile>(
                    new M18TransactionalFile(destination,privateDir,stage));
                if(!result->out_.is_open()){
                    result.reset(); // destructor removes owned dir
                    return {};
                }
                return result;
            }
        }catch(...){}
        return {};
    }

    M18TransactionalFile(const M18TransactionalFile&)=delete;
    M18TransactionalFile& operator=(const M18TransactionalFile&)=delete;
    M18TransactionalFile(M18TransactionalFile&&)=delete;
    M18TransactionalFile& operator=(M18TransactionalFile&&)=delete;

    [[nodiscard]] bool write_all(
        std::span<const std::uint8_t> bytes) noexcept override
    {
        if(!out_.is_open()||bad_||published_)return false;
        if(bytes.size()>static_cast<std::uint64_t>(
            std::numeric_limits<std::streamsize>::max())){
            bad_=true;
            return false;
        }
        out_.write(reinterpret_cast<const char*>(bytes.data()),
            static_cast<std::streamsize>(bytes.size()));
        if(!out_){bad_=true;return false;}
        bytes_written_+=static_cast<std::uint64_t>(bytes.size());
        return true;
    }

    [[nodiscard]] M18FileStatus publish(
        const M18StreamReceipt& evidence) noexcept
    {
        namespace fs=std::filesystem;
        if(published_)return M18FileStatus::PUBLISH_FAILED;
        if(bad_ || !out_.is_open())
            return M18FileStatus::WRITE_FAILED;
        if(evidence.frames!=evidence.plan.spec.frame_count
           ||bytes_written_!=evidence.plan.file_bytes)
            return M18FileStatus::VERIFY_FAILED;
        out_.flush();
        if(!out_) {bad_=true;return M18FileStatus::WRITE_FAILED;}
        out_.close();
        if(out_.fail()){bad_=true;return M18FileStatus::WRITE_FAILED;}
        std::error_code ec;
        const auto measured=fs::file_size(privateFile_,ec);
        if(ec || measured!=bytes_written_)
            return M18FileStatus::VERIFY_FAILED;
        // Incremental producer SHA and independently reread SHA must match.
        // File-system errors, truncated reads and changes FAIL CLOSED.
        core::Sha256 independentHash;
        std::ifstream input(privateFile_,std::ios::binary);
        if(!input)return M18FileStatus::VERIFY_FAILED;
        std::array<std::byte,64U*1024U> buffer{};
        std::uint64_t seen=0;
        while(input){
            input.read(reinterpret_cast<char*>(buffer.data()),
                static_cast<std::streamsize>(buffer.size()));
            const auto got=input.gcount();
            if(got<0)return M18FileStatus::VERIFY_FAILED;
            if(got==0)break;
            seen+=static_cast<std::uint64_t>(got);
            if(seen>bytes_written_)return M18FileStatus::VERIFY_FAILED;
            independentHash.update(
                std::span{buffer.data(),static_cast<std::size_t>(got)});
        }
        if(input.bad()||seen!=bytes_written_)
            return M18FileStatus::VERIFY_FAILED;
        input.close();
        if(core::sha256_hex(independentHash.finalize())
            !=evidence.full_file_sha256)
            return M18FileStatus::SHA_MISMATCH;
        ec.clear();
        if(fs::exists(finalFile_,ec)||ec)
            return M18FileStatus::TARGET_EXISTS;
        // Exclusive atomic no-replace: hardlink creation fails if the
        // destination already exists, unlike POSIX rename().
        fs::create_hard_link(privateFile_,finalFile_,ec);
        if(ec)return M18FileStatus::PUBLISH_FAILED;
        published_=true;
        const bool cleaned=cleanup();
        return cleaned ? M18FileStatus::OK
            :M18FileStatus::PUBLISHED_CLEANUP_WARNING;
    }

    [[nodiscard]] std::uint64_t staged_bytes() const noexcept
    {return bytes_written_;}

    // Abort (or destructor) touches only this transaction's PRIVATE files.
    // It never deletes the final destination, even after successful publish.
    ~M18TransactionalFile() noexcept override
    {
        if(out_.is_open())out_.close();
        (void)cleanup();
    }

private:
    explicit M18TransactionalFile(
        std::filesystem::path destination,
        std::filesystem::path privateDir,
        std::filesystem::path privateFile)
        : finalFile_(std::move(destination)),
          privateDir_(std::move(privateDir)),
          privateFile_(std::move(privateFile)),
          out_(privateFile_,std::ios::binary|std::ios::trunc)
    {}

    [[nodiscard]] bool cleanup() noexcept
    {
        namespace fs=std::filesystem;
        std::error_code ec;
        fs::remove(privateFile_,ec);
        if(ec)return false;
        ec.clear();
        fs::remove(privateDir_,ec);
        return !ec;
    }

    std::filesystem::path finalFile_;
    std::filesystem::path privateDir_;
    std::filesystem::path privateFile_;
    std::ofstream out_;
    std::uint64_t bytes_written_{0};
    bool bad_{false};
    bool published_{false};
};

} // namespace rgsml::audio::internal
