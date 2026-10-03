#include <rgsml/dsp/imodule.hpp>

#include <rgsml/core/error.hpp>

#include <string>
#include <utility>

namespace rgsml::dsp {
namespace {

[[nodiscard]] rgsml::core::Error module_error(
    rgsml::core::ErrorCode code,
    std::string category,
    std::string message)
{
    return rgsml::core::Error{
        code, std::move(message), {{"category", std::move(category)}}};
}

}  // namespace

rgsml::core::Result<DspRuntimeCheckpoint> IModule::runtime_checkpoint() const
{
    return rgsml::core::Result<DspRuntimeCheckpoint>::failure(module_error(
        rgsml::core::ErrorCode::UnsupportedOperation,
        "RUNTIME_CHECKPOINT_UNSUPPORTED",
        "Module does not support runtime checkpoint."));
}

rgsml::core::Status IModule::restore_runtime_checkpoint(
    const DspRuntimeCheckpoint& /*checkpoint*/)
{
    return rgsml::core::Status::failure(module_error(
        rgsml::core::ErrorCode::UnsupportedOperation,
        "RUNTIME_CHECKPOINT_UNSUPPORTED",
        "Module does not support restoring runtime checkpoints."));
}

rgsml::core::Status IModule::finalize(
    rgsml::audio::MutableAudioBufferView /*output*/,
    const DspProcessContext& /*context*/)
{
    return rgsml::core::Status::failure(module_error(
        rgsml::core::ErrorCode::UnsupportedOperation,
        "DSP_FINALIZE_UNSUPPORTED",
        "Module does not support finalize operations."));
}

}  // namespace rgsml::dsp
