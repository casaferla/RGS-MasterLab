#pragma once
#include "internal/true_peak_itu4x_coefficients.hpp"
#include <algorithm>
#include <array>
#include <bit>
#include <cfenv>
#include <cfloat>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <span>
#if defined(__x86_64__) || defined(_M_X64)
#include <xmmintrin.h>
#include <emmintrin.h>
#endif

namespace research {
using Frame = std::array<double, 2>;
using rgsml::analysis::internal::kTruePeakItu4xPhaseMajor;
inline constexpr size_t quantum = 64;
inline constexpr size_t fir_tail = 11;
inline constexpr size_t capacity = quantum + 2202;
inline constexpr double tap_sum = 16571.0 / 8192;
inline constexpr double dominance = 4179.0 / 8192;

inline bool compatible_fp() noexcept {
#if defined(__x86_64__) || defined(_M_X64)
    constexpr unsigned masks = (1u << 15) | (1u << 6) | (3u << 13);
    return std::numeric_limits<double>::is_iec559 && FLT_EVAL_METHOD == 0 &&
        std::fegetround() == FE_TONEAREST && (_mm_getcsr() & masks) == 0;
#else
    return false;
#endif
}
struct Config {
    int fs;
    int channels;
    int margin_percent;
    double ceiling;
    bool baseline = false;
    bool audit_every_plan = false;
    friend bool operator==(const Config&, const Config&) = default;
};
enum class Status { ok, incompatible_backend, invalid_config, invalid_pcm, wrong_state, invariant };
enum class Path { waiting, interior, identity, reserve, scaled, repaired, conservative, backup, drain };
inline const char* path_name(Path p) {
    constexpr const char* names[]{"waiting", "interior", "identity", "reserve", "scaled",
        "repaired", "conservative", "backup", "drain"};
    return names[static_cast<unsigned>(p)];
}
struct Step {
    Status status = Status::ok;
    size_t frames = 0;
    Path path = Path::waiting;
    unsigned full_checks = 0;
    unsigned sweeps = 0;
};
struct Meter {
    // Slot 0 is the current sample. The next call shifts old slots by one.
    std::array<std::array<double, 12>, 2> taps{};
    double peak = 0;

    static void phases(const std::array<double,12>& x, double* result) noexcept {
#ifdef RESEARCH_P2_PHASE_SSE
        // SIMD is across independent phases. Tap order within each lane stays canonical.
        static constexpr auto paired=[] {
            std::array<std::array<double,2>,24> result{};
            for(size_t pair=0;pair<2;++pair)
                for(size_t k=0;k<12;++k)
                    result[12*pair+k]={kTruePeakItu4xPhaseMajor[24*pair+k],
                                       kTruePeakItu4xPhaseMajor[24*pair+12+k]};
            return result;
        }();
        for(size_t pair=0;pair<2;++pair) {
            __m128d sum=_mm_setzero_pd();
            for(size_t k=0;k<12;++k) {
                const auto coeff=_mm_loadu_pd(paired[12*pair+k].data());
                sum=_mm_add_pd(sum,_mm_mul_pd(coeff,_mm_set1_pd(x[k])));
            }
            _mm_storeu_pd(result+2*pair,sum);
        }
#else
        for (size_t p = 0; p < 4; ++p) {
            double sum = 0;
            for (size_t k = 0; k < 12; ++k)
                sum += kTruePeakItu4xPhaseMajor[12*p+k] * x[k];
            result[p] = sum;
        }
#endif
    }

    std::array<double, 8> push(Frame f, int channels) noexcept {
        std::array<double, 8> result{};
        for (int c = 0; c < channels; ++c) {
            auto& x = taps[c];
            std::memmove(x.data()+1, x.data(), 11*sizeof(double));
            x[0] = f[c];
            phases(x, result.data()+4*c);
            for(size_t p=0;p<4;++p)peak=std::max(peak,std::abs(result[4*c+p]));
        }
        return result;
    }
};
inline double floor_threshold(double ceiling, unsigned percent) noexcept {
    const auto raw = std::bit_cast<uint64_t>(ceiling);
    int exponent = int((raw >> 52) & 2047) - 1023;
    auto significand = (raw & ((uint64_t{1}<<52)-1)) | (uint64_t{1}<<52);
    auto scaled = significand * (100-percent);
    if (scaled / 100 < (uint64_t{1}<<52)) {
        scaled *= 2;
        --exponent;
    }
    return std::ldexp(static_cast<double>(scaled / 100), exponent-52);
}

class Stream {
public:
    Status prepare(Config cfg) noexcept {
        if (prepared_) return Status::wrong_state;
        if (!compatible_fp()) return Status::incompatible_backend;
        const int rates[]{44100,48000,88200,96000,176400,192000};
        if (std::find(std::begin(rates),std::end(rates),cfg.fs)==std::end(rates) ||
            cfg.channels < 1 || cfg.channels > 2 || !(cfg.ceiling >= .25 && cfg.ceiling <= 1))
            return Status::invalid_config;
        switch (cfg.margin_percent) {
            case 1: horizon_ = cfg.baseline ? 2202 : 500; break;
            case 2: horizon_ = cfg.baseline ? 1090 : 247; break;
            case 5: horizon_ = cfg.baseline ? 422 : 96; break;
            case 10: horizon_ = cfg.baseline ? 200 : 45; break;
            default: return Status::invalid_config;
        }
        config_ = cfg;
        threshold_ = floor_threshold(cfg.ceiling, cfg.margin_percent);
        // P0 exact gate enclosed by a proved cold-preparation bound.
        amplitude_ = (cfg.ceiling * (100-cfg.margin_percent) / 100) / dominance
                     + std::ldexp(cfg.ceiling, -42);
        for (size_t i=0; i<horizon_; ++i)
            fade_[i] = double(horizon_-i) / double(horizon_+1);
        observed_meter_ = {};
        emitted_meter_ = {};
        observed_ = committed_ = pending_ = 0;
        last_gain_ = 1;
        eligible_ = true;
        prepared_ = true;
        closed_ = stop_ = bypass_ = false;
        bridge_zeros_ = 0;
        ++epoch_;
        return Status::ok;
    }
    [[nodiscard]] size_t horizon() const {return horizon_;}
    [[nodiscard]] size_t pending() const {return pending_;}
    [[nodiscard]] uint64_t observed() const {return observed_;}
    [[nodiscard]] uint64_t committed() const {return committed_;}
    [[nodiscard]] double threshold() const {return threshold_;}
    [[nodiscard]] double amplitude_gate() const {return amplitude_;}
    [[nodiscard]] double emitted_peak() const {return emitted_meter_.peak;}
    [[nodiscard]] bool eligible() const {return eligible_;}

    Step submit(std::span<const Frame> input, std::span<Frame> output,
                std::span<double> gains) noexcept {
        if (!compatible_fp()) return {Status::incompatible_backend};
        if (!prepared_ || closed_ || input.size()>quantum ||
            output.size()<quantum || gains.size()<quantum) return {Status::wrong_state};
        for (auto f:input) {
            for (int c=0; c<config_.channels; ++c)
                if (!std::isfinite(f[c]) || std::abs(f[c])>1048576.)
                    return {Status::invalid_pcm};
        }
        std::array<Frame,quantum> staged{};
        std::copy(input.begin(),input.end(),staged.begin());
        Step result;
        for (Frame f: std::span(staged).first(input.size())) {
            if (config_.channels==1) f[1]=0;
            observed_meter_.peak=0;
            observed_meter_.push(f,config_.channels);
            row_peak_[pending_]=observed_meter_.peak;
            eligible_ = eligible_ && observed_meter_.peak<=threshold_ &&
                std::max(std::abs(f[0]),std::abs(f[1]))<=amplitude_;
            input_[pending_]=f;
#ifdef RESEARCH_P2_RECOVERY
            // Signed zeros implement RN64(0*v); the M16 absolute peak of the
            // continuation is unchanged by the sign of zero.
            plan_[pending_]={f[0]*0.,f[1]*0.};
#else
            plan_[pending_]={};
#endif
            plan_gain_[pending_]=0;
            ++observed_;
            ++pending_;
            if (pending_==quantum+horizon_) {
                result = process(quantum,output,gains,false);
                if (result.status!=Status::ok) return result;
            }
        }
        return result;
    }
    Step finish(std::span<Frame> output, std::span<double> gains, bool cancel=false) noexcept {
        if (!compatible_fp()) return {Status::incompatible_backend};
        if (!prepared_ || output.size()<quantum || gains.size()<quantum) return {Status::wrong_state};
        closed_=true;
        stop_=stop_||cancel;
        if (!pending_) return {};
        return process(std::min(quantum,pending_),output,gains,true);
    }
    // A successful call returns actual bridge PCM. Only call after completing the drain.
    Step bridge(std::span<Frame> output) noexcept {
        if (!compatible_fp()) return {Status::incompatible_backend};
        if (!closed_ || pending_ || output.size()<fir_tail || bridge_zeros_) return {Status::wrong_state};
        auto check=emitted_meter_;
        for(size_t i=0;i<fir_tail;++i) check.push({},config_.channels);
        if(check.peak>config_.ceiling)return {Status::invariant};
        std::fill_n(output.data(),fir_tail,Frame{});
        emitted_meter_=check;
        bridge_zeros_=fir_tail;
        return {Status::ok,fir_tail,Path::drain};
    }
    Status next_epoch(Config cfg) noexcept {
        if (!closed_ || pending_ || bridge_zeros_<fir_tail) return Status::wrong_state;
        prepared_=false;
        const auto status=prepare(cfg);
        if(status!=Status::ok)prepared_=true;
        return status;
    }
    Status restore(const Stream& checkpoint) noexcept {
        if (!compatible_fp()) return Status::incompatible_backend;
        if (config_!=checkpoint.config_ || epoch_!=checkpoint.epoch_ ||
            committed_!=checkpoint.committed_ || bridge_zeros_!=checkpoint.bridge_zeros_ ||
            std::memcmp(emitted_meter_.taps.data(),checkpoint.emitted_meter_.taps.data(),
                        sizeof(emitted_meter_.taps))!=0 || emitted_meter_.peak!=checkpoint.emitted_meter_.peak)
            return Status::wrong_state;
        *this=checkpoint;
        return Status::ok;
    }
    // Enabling bypass is a guarded request. Future chunks that cannot be
    // committed unchanged go through the normal safety path.
    void request_guarded_bypass(bool enabled) noexcept {bypass_=enabled;}
    [[nodiscard]] bool bypass_requested() const noexcept {return bypass_;}

private:
    Config config_{};
    Meter observed_meter_{},emitted_meter_{};
    std::array<Frame,capacity> input_{},plan_{},candidate_{};
    std::array<double,capacity> row_peak_{},plan_gain_{},gain_{},fade_{};
    size_t horizon_=0,pending_=0,bridge_zeros_=0;
    uint64_t observed_=0,committed_=0,epoch_=0;
    double threshold_=0,amplitude_=0,last_gain_=1;
    bool eligible_=true,prepared_=false,closed_=false,stop_=false,bypass_=false;

    void materialize() noexcept {
        for(size_t j=0;j<pending_;++j) {
            if(gain_[j]==1) candidate_[j]=input_[j];
            else candidate_[j]={input_[j][0]*gain_[j],input_[j][1]*gain_[j]};
        }
    }
#ifdef RESEARCH_P2_CACHE
    std::array<Frame,capacity+fir_tail> checked_pcm_{};
    std::array<double,capacity+fir_tail> checked_peak_{};
    size_t checked_count_=0;
#endif

    bool check(size_t count, bool zeros, Meter& at_commit, size_t n) noexcept {
#ifdef RESEARCH_P2_CACHE
        const size_t rows=count+(zeros?fir_tail:0);
        const size_t valid=checked_count_;
        size_t unchanged=0;
        double peak=emitted_meter_.peak;
        std::array<double,4> phase{};
        const auto frame_at=[&](size_t row) noexcept {
            return row<count?candidate_[row]:Frame{};
        };
        for(size_t row=0;row<rows;++row) {
            const auto current=frame_at(row);
            if(row<valid && std::memcmp(&current,&checked_pcm_[row],sizeof(Frame))==0)++unchanged;
            else {unchanged=0;checked_pcm_[row]=current;}
            const bool cache_hit=unchanged>=std::min(row+1,fir_tail+1);
            const bool snapshot=row+1==n;
            if(!cache_hit || snapshot) {
                double row_peak=0.;
                for(int c=0;c<config_.channels;++c) {
                    std::array<double,12> taps{};
                    for(size_t k=0;k<12;++k)
                        taps[k]=k<=row?frame_at(row-k)[c]:emitted_meter_.taps[c][k-row-1];
                    if(snapshot)at_commit.taps[c]=taps;
                    if(!cache_hit) {
                        Meter::phases(taps,phase.data());
                        for(double v:phase)row_peak=std::max(row_peak,std::abs(v));
                    }
                }
                if(!cache_hit)checked_peak_[row]=row_peak;
            }
            peak=std::max(peak,checked_peak_[row]);
            if(snapshot)at_commit.peak=peak;
            if(peak>config_.ceiling) {
                checked_count_=row+1;
                return false;
            }
        }
        checked_count_=rows;
        return true;
#else
        auto meter=emitted_meter_;
        for(size_t j=0;j<count;++j) {
            meter.push(candidate_[j],config_.channels);
            if (j+1==n) at_commit=meter;
            if (meter.peak>config_.ceiling) return false;
        }
        if (zeros) {
            for(size_t k=0;k<fir_tail;++k) meter.push({},config_.channels);
        }
        return meter.peak<=config_.ceiling;
#endif
    }
    void reserve(size_t n, double desired=1., bool smooth=false) noexcept {
        const size_t length=pending_-n;
        const double slope=1.0/(.040*config_.fs);
        for(size_t j=0;j<pending_;++j) {
            double g=smooth?std::min(desired,last_gain_+(j+1)*slope):desired;
            if (j>=n) {
                const size_t i=j-n;
                g*=length==horizon_?fade_[i]:double(length-i)/double(length+1);
            }
            gain_[j]=g;
        }
    }
    bool project() noexcept {
        for(int sweep=0;sweep<2;++sweep) {
            for(size_t n=0;n<pending_+fir_tail;++n) {
                for(int c=0;c<config_.channels;++c) for(size_t p=0;p<4;++p) {
                    std::array<double,12> a{};
                    double fixed=0,sum=0,norm=0;
                    for(size_t k=0;k<12;++k) {
                        const auto index=static_cast<ptrdiff_t>(n)-static_cast<ptrdiff_t>(k);
                        const double tap=kTruePeakItu4xPhaseMajor[12*p+k];
                        if(index<0) fixed+=tap*emitted_meter_.taps[c][-index-1];
                        else if(size_t(index)<pending_) {
                            a[k]=tap*input_[index][c];
                            sum+=a[k]*gain_[index];
                            norm+=a[k]*a[k];
                        }
                    }
                    for(int sign:{1,-1}) {
                        const double violation=sign*(fixed+sum)-config_.ceiling;
                        if(violation<=0)continue;
                        if(norm==0)return false;
                        const double distance=sign*violation/norm;
                        if(!std::isfinite(distance))return false;
                        sum=0;
                        for(size_t k=0;k<12;++k) {
                            if(k>n || n-k>=pending_)continue;
                            const double g=gain_[n-k]-distance*a[k];
                            if(!std::isfinite(g))return false;
                            gain_[n-k]=std::clamp(g,0.,1.);
                            sum+=a[k]*gain_[n-k];
                        }
                    }
                }
            }
        }
        return true;
    }
    Step process(size_t n,std::span<Frame> output,std::span<double> gains,bool eos) noexcept {
        Step step{Status::ok,n,Path::backup};
        Meter next{};
#ifdef RESEARCH_P2_CACHE
        checked_count_=0;
#endif
        auto attempt=[&](Path path) {
            materialize();
            ++step.full_checks;
            if(!check(pending_,true,next,n))return false;
            step.path=path;
            return true;
        };
        bool accepted=false;
        if(!stop_) {
            if(bypass_) {
                std::fill_n(gain_.data(),pending_,1.);
                accepted=attempt(Path::identity);
                if(!accepted)bypass_=false;
            }
            if(!accepted && eos && eligible_) {
                std::fill_n(gain_.data(),pending_,1.);
                accepted=attempt(Path::identity);
                if(!accepted)eligible_=false;
            }
            if(!accepted && !eos && eligible_) {
                reserve(n);
                materialize();
                const bool full=config_.audit_every_plan;
                if(!check(full?pending_:n+fir_tail,full,next,n)) {
                    closed_=true; stop_=true;
                    return {Status::invariant};
                }
                step.path=Path::interior;
                accepted=true;
            } else if(!accepted && !config_.baseline) {
                std::fill_n(gain_.data(),pending_,1.);
                accepted=attempt(Path::identity);
                if(!accepted && bypass_) {
                    // Bypass request never overrides the verified history.
                    bypass_=false;
                }
                if(!accepted) {
                    reserve(n);
                    accepted=attempt(Path::reserve);
                }
#ifdef RESEARCH_EASED_TAIL
                if(!accepted) {
                    // An optional proposal. Its quadratic onset reduces the
                    // first change in gain near a peak. Verify every row.
                    std::fill_n(gain_.data(),pending_,1.);
                    const double denominator=double(pending_-n+1);
                    for(size_t j=n;j<pending_;++j) {
                        const double ratio=double(j-n+1)/denominator;
                        gain_[j]=1-ratio*ratio;
                    }
                    accepted=attempt(Path::reserve);
                }
#endif
                if(!accepted) {
                    const double pk=*std::max_element(row_peak_.begin(),row_peak_.begin()+pending_);
                    const double desired=pk>config_.ceiling?config_.ceiling/pk:1.;
                    reserve(n,desired*(1-0x1p-36),true);
                    accepted=attempt(Path::scaled);
                }
                if(!accepted) {
                    step.sweeps=2;
                    if(project())accepted=attempt(Path::repaired);
                }
                if(!accepted) {
                    double amplitude=0;
                    for(size_t j=0;j<pending_;++j)
                        amplitude=std::max({amplitude,std::abs(input_[j][0]),std::abs(input_[j][1])});
                    const double limit=amplitude>0?
                        std::min(1.,config_.ceiling*.98/(tap_sum*amplitude)):1.;
                    reserve(n,limit,true);
                    accepted=attempt(Path::conservative);
                }
#ifdef RESEARCH_P2_RECOVERY
                if(!accepted) {
                    // Preserve the incumbent exactly on its nonzero gains.
                    // Only propose a positive continuation for its zero slots.
                    // The complete post-gain PCM must pass the verifier again.
                    double amplitude=0.;
                    for(size_t j=0;j<pending_;++j)
                        if(plan_gain_[j]==0.)
                            amplitude=std::max({amplitude,std::abs(input_[j][0]),std::abs(input_[j][1])});
                    const double cap=amplitude>0.?std::min(1.,config_.ceiling/(2*tap_sum*amplitude)):1.;
                    double previous=last_gain_;
                    for(size_t j=0;j<pending_;++j) {
                        gain_[j]=plan_gain_[j]>0.?plan_gain_[j]:
                            std::min(cap,previous+1./(.040*config_.fs));
                        previous=gain_[j];
                    }
                    accepted=attempt(Path::conservative);
                }
#endif
            }
        }
        if(accepted) {
            std::copy_n(candidate_.data(),pending_,plan_.data());
            std::copy_n(gain_.data(),pending_,plan_gain_.data());
        } else {
            next=emitted_meter_;
            for(size_t j=0;j<n;++j)next.push(plan_[j],config_.channels);
            if(next.peak>config_.ceiling)return {Status::invariant};
            if(stop_)step.path=Path::drain;
        }
        std::copy_n(plan_.data(),n,output.data());
        std::copy_n(plan_gain_.data(),n,gains.data());
        last_gain_=plan_gain_[n-1];
        emitted_meter_=next;
        committed_+=n;
        pending_-=n;
        std::memmove(input_.data(),input_.data()+n,pending_*sizeof(Frame));
        std::memmove(plan_.data(),plan_.data()+n,pending_*sizeof(Frame));
        std::memmove(plan_gain_.data(),plan_gain_.data()+n,pending_*sizeof(double));
        std::memmove(row_peak_.data(),row_peak_.data()+n,pending_*sizeof(double));
        return step;
    }
};

class MusicController {
public:
    MusicController(int fs,double drive_db,double release_ms,double target)
      :drive_(std::pow(10.,drive_db/20.)),target_(target),
       release_(std::exp(-1.0/(fs*release_ms*.001))),hold_frames_(fs/20){}
    Frame process(Frame input,bool active=true) noexcept {
        if(!active)return input;
        Frame driven{input[0]*drive_,input[1]*drive_};
        const double peak=std::max(std::abs(driven[0]),std::abs(driven[1]));
        const double need=peak>target_?target_/peak:1.;
        if(need<=gain_) {gain_=need;hold_=hold_frames_;}
        else if(hold_>0) --hold_;
        else gain_=std::min(need,1-(1-gain_)*release_);
        return {driven[0]*gain_,driven[1]*gain_};
    }
private:
    double drive_,target_,release_,gain_=1;
    int hold_frames_,hold_=0;
};
} // namespace research
