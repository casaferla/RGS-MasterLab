#pragma once

// Phase-11 Final SRC: test-facing, unqualified strict-scalar C++ reference.
// NOT a product converter, runtime kernel loader, release admission path or
// replacement for frozen playback SRC. Assets supplied here are SYNTHETIC;
// release must separately verify the RGSMSR1 bytes, independent frozen SHA,
// MPFR/Ziv interval rounding and continuous spectral Q0-Q6.
#include <rgsml/core/result.hpp>

#include <bit>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <functional>
#include <numeric>
#include <span>

namespace rgsml::audio::internal {
namespace phase11_detail {

[[nodiscard]] inline core::Error bad(
    core::ErrorCode code, const char* msg)
{
    return core::Error{code,msg,{{"category","PHASE11_TEST_REFERENCE_ONLY"}}};
}

// Positive L; C++ integer division truncates towards zero. This identity
// computes mathematical ceil(a/L) even when a is a negative dependency.
[[nodiscard]] inline std::int64_t signed_ceil(
    std::int64_t a, std::int64_t positive)
{
    return a/positive + ((a%positive)>0 ? 1 : 0);
}

[[nodiscard]] inline std::int64_t reflect_even_endpoint(
    std::int64_t n, std::int64_t sourceFrames)
{
    if(sourceFrames==1)return 0;
    const auto period=sourceFrames*2;
    auto phase=n%period;
    if(phase<0)phase+=period;
    return phase<sourceFrames ? phase : period-1-phase;
}
} // namespace phase11_detail

// Immutable full SOURCE samples, zero-origin absolute OUTPUT index and
// preloaded binary64 TAP BITS. This routine intentionally has NO production
// loading/discovery or automatic product capability registration.
[[nodiscard]] inline core::Result<void> render_phase11_strict_scalar_reference(
    std::span<const double> fullSource,
    std::span<double> output,
    std::int64_t absoluteOutputStart,
    std::int64_t L,
    std::int64_t M,
    std::span<const std::uint64_t> kernelBits,
    const std::function<bool()>& cancelled = {})
{
    using core::ErrorCode;
    const auto reject=[](ErrorCode code,const char* msg) {
        return core::Result<void>::failure(phase11_detail::bad(code,msg));
    };
    constexpr auto maximum=std::numeric_limits<std::int64_t>::max();
    if(fullSource.empty()||L<=0||M<=0||std::gcd(L,M)!=1
       ||fullSource.size()>static_cast<std::uint64_t>(maximum/2)
       ||L>maximum/static_cast<std::int64_t>(fullSource.size()))
        return reject(ErrorCode::InvalidArgument,"Invalid full source or rational L/M");
    const auto nFrames=static_cast<std::int64_t>(fullSource.size());
    const auto lengthProduct=nFrames*L;
    const auto totalOutput=lengthProduct/M+
        ((lengthProduct%M)!=0 ? 1 : 0);
    if(absoluteOutputStart<0||absoluteOutputStart>totalOutput
       ||output.size()>static_cast<std::uint64_t>(
           totalOutput-absoluteOutputStart))
        return reject(ErrorCode::OutOfRange,"Crop outside absolute output frame map");
    for(const auto sample:fullSource)
        if(!std::isfinite(sample))
            return reject(ErrorCode::InvalidAudioSample,"Nonfinite source PCM");
    const bool identity=L==1&&M==1;
    if(identity) {
        if(!kernelBits.empty())
            return reject(ErrorCode::InvalidArgument,
                          "Identity must not evaluate a filter");
        for(std::size_t i=0;i<output.size();++i) {
            if((i%1024)==0 &&cancelled &&cancelled())
                return reject(ErrorCode::InvalidState,
                              "Phase11 identity retired during PCM copy");
            output[i]=fullSource[static_cast<std::size_t>(absoluteOutputStart)+i];
        }
        return core::Result<void>::success();
    }
    if(kernelBits.size()<3 || kernelBits.size()%2!=1
       ||kernelBits.size()>static_cast<std::uint64_t>(maximum/2))
        return reject(ErrorCode::InvalidArgument,"Invalid candidate prototype geometry");
    const auto tapCount=static_cast<std::int64_t>(kernelBits.size());
    const auto delay=(tapCount-1)/2;
    for(std::size_t i=0;i<kernelBits.size();++i) {
        const auto v=std::bit_cast<double>(kernelBits[i]);
        if(!std::isfinite(v) || kernelBits[i]!=kernelBits[kernelBits.size()-i-1])
            return reject(ErrorCode::InvalidArgument,
                          "Candidate prototype not finite and bit-symmetric");
    }
    if(totalOutput>0 && (totalOutput-1)>(maximum-delay)/M)
        return reject(ErrorCode::IntegerOverflow,
                      "Absolute high-rate output coordinate overflows");
    for(std::size_t i=0;i<output.size();++i) {
        // A canceled full-resource job must stop before returning any
        // candidate. The output buffer remains privately owned by the
        // caller until the entire conversion succeeds.
        if((i%1024)==0 &&cancelled &&cancelled())
            return reject(ErrorCode::InvalidState,
                          "Phase11 candidate retired during SRC conversion");
        const auto m=absoluteOutputStart+static_cast<std::int64_t>(i);
        const auto q=delay+m*M;
        const auto first=phase11_detail::signed_ceil(q-(tapCount-1),L);
        const auto last=q/L;
        double sum=0.0;
        for(auto n=first;n<=last;) {
            const auto j=q-n*L;
            const auto sourceIndex=phase11_detail::reflect_even_endpoint(
                n,nFrames);
            const auto sample=fullSource[static_cast<std::size_t>(sourceIndex)];
            const auto coefficient=std::bit_cast<double>(
                kernelBits[static_cast<std::size_t>(j)]);
            const double product=sample*coefficient;
            sum=sum+product; // /fp:strict or -ffp-contract=off in qualification
            if(n==last)break;
            ++n;
        }
        if(!std::isfinite(sum))
            return reject(ErrorCode::InvalidAudioSample,
                          "Nonfinite strict scalar output");
        output[i]=sum;
    }
    return core::Result<void>::success();
}
} // namespace rgsml::audio::internal
