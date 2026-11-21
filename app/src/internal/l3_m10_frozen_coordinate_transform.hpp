#pragma once
// L3-M10 accepted P01 source-coordinate transforms, isolated pure math.
// Bounds/reference are supplied by the M09 seed/topology/evidence adapter,
// never inferred from public DSP defaults or an incumbent's last value.
// No clamp, no nonlocal topology re-sorting and no singleton division.
#include <cmath>
#include <cstdint>
#include <optional>

namespace rgsml::app::l3_automatch::m10_frozen {
enum class Transform :std::uint8_t {
    additive_or_linear,
    frequency_log2,
    positive_time_or_q_log,
    ratio_log_from_one,
    zero_inclusive_time
};
class CoordinateTransform final {
public:
    [[nodiscard]] static std::optional<CoordinateTransform> create(
        Transform kind,double lower,double upper) noexcept
    {
        if(!std::isfinite(lower)||!std::isfinite(upper)
           ||lower>=upper)return std::nullopt;
        switch(kind) {
        case Transform::additive_or_linear:
            break;
        case Transform::frequency_log2:
        case Transform::positive_time_or_q_log:
            if(lower<=0.0)return std::nullopt;
            break;
        case Transform::ratio_log_from_one:
            if(lower!=1.0)return std::nullopt;
            break;
        case Transform::zero_inclusive_time:
            if(lower!=0.0)return std::nullopt;
            break;
        default:return std::nullopt;
        }
        return CoordinateTransform{kind,lower,upper};
    }
    [[nodiscard]] std::optional<double> to_normalized(
        double value) const noexcept
    {
        if(!std::isfinite(value)||value<lo_||value>hi_)
            return std::nullopt;
        if(value==lo_)return 0.0;
        if(value==hi_)return 1.0;
        double n=0.0;
        switch(kind_) {
        case Transform::additive_or_linear:
            n=(value-lo_)/(hi_-lo_);break;
        case Transform::frequency_log2:
            n=std::log2(value/lo_)/std::log2(hi_/lo_);break;
        case Transform::positive_time_or_q_log:
            n=std::log(value/lo_)/std::log(hi_/lo_);break;
        case Transform::ratio_log_from_one:
            n=std::log(value)/std::log(hi_);break;
        case Transform::zero_inclusive_time: {
            constexpr double t0=0.1;
            n=std::log((value+t0)/t0)/
              std::log((hi_+t0)/t0);
            break;
        }
        default:return std::nullopt;
        }
        if(!std::isfinite(n)||n<0.0||n>1.0)
            return std::nullopt;
        return n;
    }
    [[nodiscard]] std::optional<double> from_normalized(
        double u) const noexcept
    {
        if(!std::isfinite(u)||u<0.0||u>1.0)
            return std::nullopt;
        if(u==0.0)return lo_;
        if(u==1.0)return hi_;
        double value=0.0;
        switch(kind_) {
        case Transform::additive_or_linear:
            value=lo_+u*(hi_-lo_);break;
        case Transform::frequency_log2:
            value=lo_*std::exp2(u*std::log2(hi_/lo_));break;
        case Transform::positive_time_or_q_log:
            value=lo_*std::exp(u*std::log(hi_/lo_));break;
        case Transform::ratio_log_from_one:
            value=std::exp(u*std::log(hi_));break;
        case Transform::zero_inclusive_time: {
            constexpr double t0=0.1;
            value=t0*std::exp(u*std::log((hi_+t0)/t0))-t0;
            break;
        }
        default:return std::nullopt;
        }
        if(!std::isfinite(value)||value<lo_||value>hi_)
            return std::nullopt;
        return value;
    }
    [[nodiscard]] Transform kind() const noexcept{return kind_;}
    [[nodiscard]] double lower() const noexcept{return lo_;}
    [[nodiscard]] double upper() const noexcept{return hi_;}
private:
    CoordinateTransform(Transform k,double lo,double hi) noexcept
        :kind_(k),lo_(lo),hi_(hi) {}
    Transform kind_;
    double lo_;
    double hi_;
};
} // namespace rgsml::app::l3_automatch::m10_frozen
