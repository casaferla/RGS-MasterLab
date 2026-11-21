#include "dynamic_eq_c01_engine.hpp"
#include <rgsml/core/error.hpp>
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <numbers>
#include <string>
#include <utility>
#include <vector>

namespace rgsml::dsp::internal {
namespace {
[[nodiscard]] core::Error fail(std::string reason) {
    return core::Error{core::ErrorCode::InvalidState,
        "L3_M01_C01_RUNTIME_FAILURE",{{"category",std::move(reason)}}};
}
template<class A,class B>
[[nodiscard]] bool overlaps(std::span<A> a,std::span<B> b) noexcept {
    if(a.empty()||b.empty())return false;
    const auto ab=reinterpret_cast<std::uintptr_t>(a.data());
    const auto bb=reinterpret_cast<std::uintptr_t>(b.data());
    return ab<bb+b.size_bytes()&&bb<ab+a.size_bytes();
}
[[nodiscard]] double alpha(double fs,double milliseconds) {
    return std::exp(-1.0/(fs*(milliseconds/1000.0)));
}
} // namespace

C01FrameEngine::C01FrameEngine(std::uint32_t channels,std::vector<Band> bands)
    :channels_(channels),bands_(std::move(bands)),
     states_(bands_.size()) {}

core::Result<C01FrameEngine> C01FrameEngine::create(
    const DynamicEqParameters& params,std::uint32_t fs,std::uint32_t channels)
{
    using R=core::Result<C01FrameEngine>;
    const auto valid=params.validate_for_audio(fs,channels);
    if(!valid)return R::failure(*valid.error());
    try {
        std::vector<Band> bands;
        bands.reserve(params.bands().size());
        for(const auto& parameter:params.bands()) {
            const auto& spec=parameter.spec();
            if(!spec.enabled)continue;
            auto gen=C01CoefficientGenerator::create(parameter,fs,channels);
            if(!gen)return R::failure(*gen.error());
            auto detector=gen.value()->detector_coefficients();
            if(!detector)return R::failure(*detector.error());
            const double rate=static_cast<double>(fs);
            const auto& c=spec.control;
            const double ar=alpha(rate,c.rms_time_constant_ms);
            const double aa=alpha(rate,c.attack_ms);
            const double al=alpha(rate,c.release_ms);
            if(!std::isfinite(ar)||!std::isfinite(aa)||!std::isfinite(al)
               ||ar<0.0||ar>=1.0||aa<0.0||aa>=1.0||al<0.0||al>=1.0)
                return R::failure(fail("INVALID_CONTROL_ALPHA"));
            bands.push_back(Band{spec.routing,c,std::move(*gen.value()),
                                 *detector.value(),ar,aa,al});
        }
        return R::success(C01FrameEngine(channels,std::move(bands)));
    }catch(...) {
        return R::failure(fail("ENGINE_CONSTRUCTION_FAILURE"));
    }
}
void C01FrameEngine::reset() noexcept {
    for(auto& b:states_)b=C01BandState{};
}
const std::vector<C01BandState>& C01FrameEngine::states() const noexcept {
    return states_;
}
std::uint32_t C01FrameEngine::channels() const noexcept {return channels_;}
core::Status C01FrameEngine::restore_states(
    const std::vector<C01BandState>& candidate)
{
    if(candidate.size()!=states_.size())
        return core::Status::failure(fail("CHECKPOINT_BAND_COUNT"));
    for(std::size_t i=0;i<candidate.size();++i) {
        const auto& b=candidate[i];
        if(!std::isfinite(b.unsigned_magnitude_db)||
           b.unsigned_magnitude_db<0.0||
           b.unsigned_magnitude_db>std::abs(bands_[i].control.range_db))
            return core::Status::failure(fail("CHECKPOINT_CONTROL"));
        for(const auto& lane:b.lanes) {
            if(!std::isfinite(lane.audio.s1)||
               !std::isfinite(lane.audio.s2)||
               !std::isfinite(lane.detector.s1)||
               !std::isfinite(lane.detector.s2)||
               !std::isfinite(lane.power)||lane.power<0.0)
                return core::Status::failure(fail("CHECKPOINT_LANE_STATE"));
        }
    }
    try {
        auto new_states=candidate;
        states_.swap(new_states);
        return core::Status::success();
    }catch(...) {
        return core::Status::failure(fail("CHECKPOINT_ALLOCATION"));
    }
}

bool C01FrameEngine::process_section(
    const BiquadCoefficients& c,C01BiquadState& s,double x,double& y)
{
    // C01 frozen operation order. New coefficients NEVER reset or remap s1/s2.
    const double value=c.b0*x+s.s1;
    const double s1=c.b1*x-c.a1*value+s.s2;
    const double s2=c.b2*x-c.a2*value;
    if(!std::isfinite(value)||!std::isfinite(s1)||!std::isfinite(s2))
        return false;
    s.s1=s1;s.s2=s2;y=value;
    return true;
}
bool C01FrameEngine::update_lane(
    const Band& band,C01LaneState& lane,double x)
{
    double d=0.0;
    if(!process_section(band.detector,lane.detector,x,d))
        return false;
    // P03: smooth power BEFORE stereo max-link, never smooth the max.
    const double square=d*d;
    const double power=band.rms_a*lane.power+
        (1.0-band.rms_a)*square;
    if(!std::isfinite(square)||!std::isfinite(power)||power<0.0)
        return false;
    lane.power=power;
    return true;
}
bool C01FrameEngine::apply_audio(
    const Band& band,C01BandState& state,double gain,
    double& l,double& r,std::uint32_t channels)
{
    auto c=band.generator.audio_coefficients(gain);
    if(!c)return false;
    const auto& coeff=*c.value();
    if(channels==1)
        return process_section(coeff,state.lanes[0].audio,l,l);
    switch(band.routing) {
    case EqRouting::STEREO:
        return process_section(coeff,state.lanes[0].audio,l,l)&&
               process_section(coeff,state.lanes[1].audio,r,r);
    case EqRouting::LEFT:
        return process_section(coeff,state.lanes[0].audio,l,l);
    case EqRouting::RIGHT:
        return process_section(coeff,state.lanes[1].audio,r,r);
    case EqRouting::MID: {
        const double m=(l+r)/std::numbers::sqrt2;
        const double s=(l-r)/std::numbers::sqrt2;
        double filtered=0.0;
        if(!std::isfinite(m)||!std::isfinite(s)||
           !process_section(coeff,state.lanes[0].audio,m,filtered))
            return false;
        l=(filtered+s)/std::numbers::sqrt2;
        r=(filtered-s)/std::numbers::sqrt2;
        return std::isfinite(l)&&std::isfinite(r);
    }
    case EqRouting::SIDE: {
        const double m=(l+r)/std::numbers::sqrt2;
        const double s=(l-r)/std::numbers::sqrt2;
        double filtered=0.0;
        if(!std::isfinite(m)||!std::isfinite(s)||
           !process_section(coeff,state.lanes[1].audio,s,filtered))
            return false;
        l=(m+filtered)/std::numbers::sqrt2;
        r=(m-filtered)/std::numbers::sqrt2;
        return std::isfinite(l)&&std::isfinite(r);
    }
    }
    return false;
}
core::Status C01FrameEngine::run(
    std::span<const double> input_left,std::span<const double> input_right,
    std::vector<double>& out_left,std::vector<double>& out_right,
    std::vector<C01BandState>& staged) const
{
    std::array<double,6> gains{};
    for(std::size_t i=0;i<input_left.size();++i) {
        const double original_l=input_left[i];
        const double original_r=channels_==2?input_right[i]:0.0;
        if(!std::isfinite(original_l)||
           (channels_==2&&!std::isfinite(original_r)))
            return core::Status::failure(fail("NONFINITE_INPUT"));

        // P03/P06: ALL sidechains observe the identical pre-module input
        // for this frame, regardless of persisted audio band order.
        for(std::size_t j=0;j<bands_.size();++j) {
            const auto& band=bands_[j];
            auto& state=staged[j];
            double power=0.0;
            const auto route=band.routing;
            if(route==EqRouting::STEREO) {
                if(!update_lane(band,state.lanes[0],original_l))
                    return core::Status::failure(fail("DETECTOR_FAILURE"));
                power=state.lanes[0].power;
                if(channels_==2) {
                    if(!update_lane(band,state.lanes[1],original_r))
                        return core::Status::failure(fail("DETECTOR_FAILURE"));
                    power=std::max(power,state.lanes[1].power);
                }
            }else if(route==EqRouting::LEFT||route==EqRouting::RIGHT) {
                const auto lane=route==EqRouting::LEFT?std::size_t{0}:std::size_t{1};
                const double x=lane==0?original_l:original_r;
                if(!update_lane(band,state.lanes[lane],x))
                    return core::Status::failure(fail("DETECTOR_FAILURE"));
                power=state.lanes[lane].power;
            }else if(route==EqRouting::MID||route==EqRouting::SIDE) {
                const double x=route==EqRouting::MID
                    ?(original_l+original_r)/std::numbers::sqrt2
                    :(original_l-original_r)/std::numbers::sqrt2;
                const auto lane=route==EqRouting::MID?std::size_t{0}:std::size_t{1};
                if(!std::isfinite(x)||!update_lane(band,state.lanes[lane],x))
                    return core::Status::failure(fail("DETECTOR_FAILURE"));
                power=state.lanes[lane].power;
            }else {
                return core::Status::failure(fail("INVALID_ROUTE"));
            }
            // Exact P=0 bypasses log10; level is a tagged nonfinite
            // mathematical state and never enters numeric computation.
            double excursion=0.0;
            if(power>0.0) {
                const double level=10.0*std::log10(power);
                excursion=std::max(0.0,level-band.control.threshold_dbfs);
                if(!std::isfinite(level)||!std::isfinite(excursion))
                    return core::Status::failure(fail("NONFINITE_DETECTOR_LEVEL"));
            }
            const double slope=1.0-1.0/band.control.ratio;
            const double target=std::min(std::abs(band.control.range_db),
                                         slope*excursion);
            const double previous=state.unsigned_magnitude_db;
            const double a=target>previous?band.attack_a:band.release_a;
            const double magnitude=a*previous+(1.0-a)*target;
            if(!std::isfinite(target)||!std::isfinite(magnitude)
               ||magnitude<0.0)
                return core::Status::failure(fail("NONFINITE_CONTROL"));
            state.unsigned_magnitude_db=magnitude;
            const double sign=band.control.range_db>0.0?1.0:
                (band.control.range_db<0.0?-1.0:0.0);
            gains[j]=band.control.base_gain_db+sign*magnitude;
            if(!std::isfinite(gains[j]))
                return core::Status::failure(fail("NONFINITE_GAIN"));
        }

        // C01: audio is serial in persisted order; per-frame coefficients
        // are recomputed using the just-updated control and TDF-II state.
        double l=original_l,r=original_r;
        for(std::size_t j=0;j<bands_.size();++j)
            if(!apply_audio(bands_[j],staged[j],gains[j],l,r,channels_))
                return core::Status::failure(fail("NONFINITE_AUDIO_OR_COEFFICIENT"));
        if(!std::isfinite(l)||(channels_==2&&!std::isfinite(r)))
            return core::Status::failure(fail("NONFINITE_OUTPUT"));
        out_left[i]=l;
        if(channels_==2)out_right[i]=r;
    }
    return core::Status::success();
}
core::Status C01FrameEngine::process_block(
    std::span<const double> in_l,std::span<const double> in_r,
    std::span<double> out_l,std::span<double> out_r)
{
    if(out_l.size()!=in_l.size()
       ||(channels_==1&&(!in_r.empty()||!out_r.empty()))
       ||(channels_==2&&(in_r.size()!=in_l.size()||
                         out_r.size()!=in_l.size()))
       ||overlaps(in_l,out_l)||overlaps(in_l,out_r)
       ||overlaps(in_r,out_l)||overlaps(in_r,out_r)
       ||overlaps(out_l,out_r))
        return core::Status::failure(fail("INVALID_OR_OVERLAPPING_PLANES"));
    try {
        // Transactional staging: neither public PCM nor live recursive
        // states advance unless EVERY frame passes finite checks.
        auto staged=states_;
        std::vector<double> left(in_l.size(),0.0);
        std::vector<double> right(channels_==2?in_l.size():0,0.0);
        auto status=run(in_l,in_r,left,right,staged);
        if(!status)return status;
        std::copy(left.begin(),left.end(),out_l.begin());
        if(channels_==2)std::copy(right.begin(),right.end(),out_r.begin());
        states_=std::move(staged);
        return core::Status::success();
    }catch(...) {
        return core::Status::failure(fail("TRANSACTIONAL_PROCESS_FAILURE"));
    }
}
} // namespace rgsml::dsp::internal
