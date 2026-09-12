#include <rgsml/render/render_result.hpp>

#include <utility>

namespace rgsml::render {

RenderResult::RenderResult(
    rgsml::audio::AudioBuffer buffer,
    rgsml::core::FrameRange render_window,
    rgsml::audio::FrameDomainId frame_domain_id,
    std::uint64_t chain_revision,
    std::vector<GainExecutionSignature> gain_signatures) noexcept
    : buffer_(std::move(buffer))
    , render_window_(render_window)
    , frame_domain_id_(frame_domain_id)
    , chain_revision_(chain_revision)
    , gain_signatures_(std::move(gain_signatures))
{
}

const rgsml::audio::AudioBuffer& RenderResult::buffer() const noexcept { return buffer_; }
rgsml::audio::AudioBufferView RenderResult::view() const noexcept { return buffer_.view(); }
rgsml::core::FrameRange RenderResult::render_window() const noexcept { return render_window_; }
rgsml::audio::FrameDomainId RenderResult::frame_domain_id() const noexcept
{
    return frame_domain_id_;
}
std::uint64_t RenderResult::chain_revision() const noexcept { return chain_revision_; }
const std::vector<GainExecutionSignature>& RenderResult::gain_signatures() const noexcept
{
    return gain_signatures_;
}

}  // namespace rgsml::render
