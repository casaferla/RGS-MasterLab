#pragma once
// I2 PREVIEW, nonfrozen. Distinct from frozen M17 TruePeakLimiterParameters.
#include <rgsml/core/result.hpp>
#include <rgsml/core/error.hpp>
#include <cmath>
namespace rgsml::dsp {
class StreamingTruePeakLimiterParameters final {
public:
  [[nodiscard]] static rgsml::core::Result<StreamingTruePeakLimiterParameters>
  create(double ceiling, double drive, int margin) {
    using rgsml::core::Result;
    using rgsml::core::Error;
    using rgsml::core::ErrorCode;
    if (!std::isfinite(ceiling) || !std::isfinite(drive))
      return Result<StreamingTruePeakLimiterParameters>::failure(
        Error{ErrorCode::InvalidArgument, "Streaming P2 parameters must be finite."});
    if (ceiling < -12.0 || ceiling > 0.0 ||
        drive < -24.0 || drive > 24.0 || (margin != 2 && margin != 5))
      return Result<StreamingTruePeakLimiterParameters>::failure(
        Error{ErrorCode::OutOfRange, "Preview Ceiling [-12,0], Drive [-24,24], margin 2/5."});
    return Result<StreamingTruePeakLimiterParameters>::success(
      StreamingTruePeakLimiterParameters{
        ceiling == 0.0 ? 0.0 : ceiling,
        drive == 0.0 ? 0.0 : drive, margin});
  }
  [[nodiscard]] static rgsml::core::Result<StreamingTruePeakLimiterParameters>
  create_default() { return create(-1.0,0.0,2); }
  [[nodiscard]] double ceiling_db_tp() const noexcept { return ceiling_; }
  [[nodiscard]] double drive_db() const noexcept { return drive_; }
  [[nodiscard]] int margin_percent() const noexcept { return margin_; }
  friend bool operator==(const StreamingTruePeakLimiterParameters&,
                         const StreamingTruePeakLimiterParameters&) = default;
private:
  StreamingTruePeakLimiterParameters(double ceiling,double drive,int margin) noexcept
    : ceiling_(ceiling),drive_(drive),margin_(margin) {}
  double ceiling_,drive_;
  int margin_;
};
} // namespace rgsml::dsp
