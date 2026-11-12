#pragma once
// R1d-P2 I1.5 isolated candidate: RGS planar double PCM <-> epoch-frame bridge.
// NOT an rgsml::dsp::IModule, not in ModuleRegistry, and not a production callback.
// Control operations require host callback quiescence; never call concurrently.

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <span>

namespace rgsml::research::r1d_p2 {

enum class HostBridgeStatus : std::uint8_t {
    ok, stale_epoch, not_prepared, invalid_context, invalid_pcm,
    invalid_configuration, wrong_lifecycle, backend_fault
};

// Session must expose begin_epoch, process, finish, latency_frames and epoch,
// with EpochCode::ok. Config must have channels and sample_rate fields.
// This is intentionally a *separate* host boundary: it does not register or
// modify any of RGS's existing frozen DSP and terminal module interfaces.
template<class Session, class Config, class EpochCode>
class PlanarHostBridge final {
public:
    using Frame = std::array<double,2>;
    static constexpr std::size_t kMaxBlock = 4096;
    PlanarHostBridge() = default;
    PlanarHostBridge(const PlanarHostBridge&) = delete;
    PlanarHostBridge& operator=(const PlanarHostBridge&) = delete;

    // Must be called off audio thread, after host callbacks are quiescent.
    // A rejected new configuration leaves the prior valid epoch untouched.
    [[nodiscard]] HostBridgeStatus begin_epoch(
        std::uint64_t token, Config config, bool bypass,
        std::int64_t source_absolute_frame) {
        if (source_absolute_frame < 0 || config.channels < 1 || config.channels > 2)
            return HostBridgeStatus::invalid_configuration;
        const auto ec = session_.begin_epoch(token, config, bypass);
        if (ec != EpochCode::ok)
            return HostBridgeStatus::invalid_configuration;
        active_ = true;
        faulted_ = false;
        eos_ = false;
        started_ = false;
        token_ = token;
        channels_ = config.channels;
        next_absolute_frame_ = source_absolute_frame;
        return HostBridgeStatus::ok;
    }

    [[nodiscard]] std::size_t latency_frames() const noexcept {
        return active_ ? session_.latency_frames() : 0U;
    }
    [[nodiscard]] std::int64_t next_absolute_frame() const noexcept {
        return next_absolute_frame_;
    }
    [[nodiscard]] std::uint64_t epoch() const noexcept { return token_; }

    // RGS planar view contract: input and output planes must be disjoint,
    // each input/output plane has N frames; mono has *empty* right planes.
    // stream_begin is true exactly for the first processed (possibly empty)
    // block of an epoch. stream_end is true on its last processed block.
    [[nodiscard]] HostBridgeStatus process(
        std::uint64_t token, std::int64_t absolute_start,
        std::span<const double> in_l, std::span<const double> in_r,
        std::span<double> out_l, std::span<double> out_r,
        bool stream_begin, bool stream_end) noexcept {
        if (!active_) return silent(out_l,out_r,HostBridgeStatus::not_prepared);
        if (token != token_) return silent(out_l,out_r,HostBridgeStatus::stale_epoch);
        if (faulted_ || eos_) return silent(out_l,out_r,HostBridgeStatus::wrong_lifecycle);
        const auto n=in_l.size();
        if (n > kMaxBlock || out_l.size()!=n ||
            (channels_==1 && (!in_r.empty() || !out_r.empty())) ||
            (channels_==2 && (in_r.size()!=n || out_r.size()!=n)) ||
            absolute_start != next_absolute_frame_ ||
            stream_begin != !started_ ||
            (n > static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max()-next_absolute_frame_)) ||
            overlap(in_l,out_l) || overlap(in_l,out_r) ||
            overlap(in_r,out_l) || overlap(in_r,out_r) ||
            overlap(in_l,in_r) || overlap(out_l,out_r))
            return poison(out_l,out_r,HostBridgeStatus::invalid_context);
        // Reject entire block before advancing either P2 history or host cursor.
        for(std::size_t i=0;i<n;++i) {
            if (!valid(in_l[i]) || (channels_==2 && !valid(in_r[i])))
                return poison(out_l,out_r,HostBridgeStatus::invalid_pcm);
        }
        for(std::size_t i=0;i<n;++i) {
            in_[i][0]=in_l[i];
            in_[i][1]=channels_==2 ? in_r[i] : 0.0;
        }
        const auto ec=session_.process(token,std::span<const Frame>(in_.data(),n),
                                       std::span<Frame>(out_.data(),n));
        if (ec != EpochCode::ok)
            return poison(out_l,out_r,HostBridgeStatus::backend_fault);
        for(std::size_t i=0;i<n;++i) {
            out_l[i]=out_[i][0];
            if (channels_==2) out_r[i]=out_[i][1];
        }
        next_absolute_frame_+=static_cast<std::int64_t>(n);
        started_=true;
        eos_=stream_end;
        return HostBridgeStatus::ok;
    }

    // Product IModule::finalize must eventually supply these D slots in the
    // same frame domain; this bridge is *not* allowed to invent hidden PCM.
    [[nodiscard]] HostBridgeStatus finish(
        std::uint64_t token, std::int64_t absolute_start,
        std::span<double> tail_l, std::span<double> tail_r) noexcept {
        if (!active_) return silent(tail_l,tail_r,HostBridgeStatus::not_prepared);
        if (token != token_) return silent(tail_l,tail_r,HostBridgeStatus::stale_epoch);
        if (faulted_ || !eos_) return silent(tail_l,tail_r,HostBridgeStatus::wrong_lifecycle);
        const auto n=session_.latency_frames();
        if (n>kMaxBlock || absolute_start != next_absolute_frame_ ||
            tail_l.size()!=n ||
            (channels_==1 && !tail_r.empty()) ||
            (channels_==2 && tail_r.size()!=n) || overlap(tail_l,tail_r))
            return poison(tail_l,tail_r,HostBridgeStatus::invalid_context);
        const auto ec=session_.finish(token,std::span<Frame>(out_.data(),n));
        if (ec!=EpochCode::ok)
            return poison(tail_l,tail_r,HostBridgeStatus::backend_fault);
        for(std::size_t i=0;i<n;++i) {
            tail_l[i]=out_[i][0];
            if (channels_==2) tail_r[i]=out_[i][1];
        }
        // Repeated finish gets wrong_lifecycle and silent output.
        eos_=false;
        faulted_=true;
        return HostBridgeStatus::ok;
    }

private:
    [[nodiscard]] static bool valid(double x) noexcept {
        return std::isfinite(x) && std::abs(x)<=1048576.0;
    }
    template<class A,class B> [[nodiscard]] static bool overlap(
        std::span<A> a,std::span<B> b) noexcept {
        if(a.empty() || b.empty())return false;
        const auto ab=reinterpret_cast<std::uintptr_t>(a.data());
        const auto bb=reinterpret_cast<std::uintptr_t>(b.data());
        const auto ae=ab+a.size_bytes(),be=bb+b.size_bytes();
        return ab<be && bb<ae;
    }
    [[nodiscard]] static HostBridgeStatus silent(
        std::span<double> l,std::span<double> r,HostBridgeStatus code) noexcept {
        std::fill(l.begin(),l.end(),0.0);std::fill(r.begin(),r.end(),0.0);
        return code;
    }
    [[nodiscard]] HostBridgeStatus poison(
        std::span<double> l,std::span<double> r,HostBridgeStatus code) noexcept {
        faulted_=true;return silent(l,r,code);
    }
    Session session_{};
    std::array<Frame,kMaxBlock> in_{};
    std::array<Frame,kMaxBlock> out_{};
    std::uint64_t token_=0;
    std::int64_t next_absolute_frame_=0;
    int channels_=0;
    bool active_=false,started_=false,eos_=false,faulted_=false;
};
} // namespace rgsml::research::r1d_p2
