#include "m22_request_plan_candidate.hpp"
#include <cstdint>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#include <utility>

using namespace rgsml::m22_private;
#define REQUIRE(c) do { if (!(c)) throw std::runtime_error(std::string("FAIL line ") + std::to_string(__LINE__)); } while (0)

static Intent baseline() {
    Intent in;
    in.source_identity = "source-A"; in.source_revision = "rev-3";
    in.prepared_identity = "prepared-A"; in.prepared_revision = "rev-7";
    in.chain_identity = "manual-chain-A"; in.chain_revision = 17;
    in.source_frames = 480000; in.window = {0, 480000};
    in.processing_rate_hz = 48000; in.target_rate_hz = 48000;
    in.channels = 2; in.terminal_limiter_signature = "M17-real-contract";
    in.terminal_limiter_parameters = "ceiling=-1.0";
    in.terminal_limiter_latency_frames = 96;
    in.execution_policy_signature = "binary64-policy";
    in.format = Format::PCM24; in.dither_seed = 1227;
    in.stored_order = {
        {"eq", InlineKind::PARAMETRIC_EQ, "eq-v1", "eq-params", false, 0},
        {"comp", InlineKind::COMPRESSOR, "comp-v1", "comp-params", false, 240},
        {"gain", InlineKind::GAIN, "gain-v1", "gain-params", true, 0},
        {"stereo", InlineKind::STEREO_MS, "stereo-v1", "mono-bass", false, 48}
    };
    return in;
}
int main() {
    try {
        auto input = baseline();
        Failure err{};
        auto req = ResolvedRenderRequest::freeze(input, err);
        REQUIRE(req && err == Failure::NONE);
        auto plan = RenderExecutionPlan::preflight(*req, err);
        REQUIRE(plan && err == Failure::NONE);
        REQUIRE(plan->stages().size() == 6);
        REQUIRE(plan->stages()[0].instance_or_signature == "eq");
        REQUIRE(plan->stages()[1].instance_or_signature == "comp");
        REQUIRE(plan->stages()[2].instance_or_signature == "gain");
        REQUIRE(plan->stages()[2].bypass_identity);
        REQUIRE(plan->stages()[3].instance_or_signature == "stereo");
        REQUIRE(plan->stages()[4].stage == Stage::TRUE_PEAK_TERMINAL);
        REQUIRE(plan->stages()[5].stage == Stage::OUTPUT_REPRESENTATION);
        REQUIRE(plan->total_active_latency_frames() == 384);
        REQUIRE(plan->logical_output_frames() == 480000);
        REQUIRE(plan->required_terminal_processing_end_frame() == 480000);
        REQUIRE(plan->terminal_requires_true_source_eos());
        REQUIRE(plan->absolute_dither_first_frame() == 0);
        REQUIRE(plan->state_origin_policy() == StateOrigin::REPLAY_FROM_CANONICAL_ZERO);
        // Frozen snapshot must survive mutable editor settings being overwritten.
        input.stored_order[0].parameter_signature = "changed-after-freeze";
        input.stored_order.swap(input.stored_order);
        input.source_revision = "rev-changed";
        REQUIRE(req->intent().stored_order[0].parameter_signature == "eq-params");
        REQUIRE(req->intent().source_revision == "rev-3");

        auto mid = baseline();
        mid.mode = Mode::PREVIEW; mid.window = {105001, 180027};
        auto p_req = ResolvedRenderRequest::freeze(mid, err);
        REQUIRE(p_req);
        auto p = RenderExecutionPlan::preflight(*p_req, err);
        REQUIRE(p);
        REQUIRE(p->logical_output_frames() == 75026);
        REQUIRE(p->logical_output_first_frame() == 105001);
        REQUIRE(p->absolute_dither_first_frame() == 105001);
        REQUIRE(p->replay_input_first_frame() == 0);
        REQUIRE(p->required_terminal_processing_end_frame() == 480000);
        REQUIRE(p->stages() == plan->stages());
        REQUIRE(p->total_active_latency_frames() == plan->total_active_latency_frames());

        // The order is not hard-coded: a second, noncommutative legal order persists.
        auto swapped = baseline();
        std::swap(swapped.stored_order[0], swapped.stored_order[1]);
        auto alt = ResolvedRenderRequest::freeze(swapped, err);
        REQUIRE(alt);
        auto altPlan = RenderExecutionPlan::preflight(*alt, err);
        REQUIRE(altPlan && altPlan->stages()[0].instance_or_signature == "comp");
        REQUIRE(altPlan->stages()[1].instance_or_signature == "eq");

        auto bad = baseline();
        bad.target_rate_hz = 44100;
        REQUIRE(!ResolvedRenderRequest::freeze(bad, err) && err == Failure::INVALID_CLOCK);
        bad = baseline(); bad.processing_rate_hz = 22050;
        REQUIRE(!ResolvedRenderRequest::freeze(bad, err) && err == Failure::INVALID_CLOCK);
        bad = baseline(); bad.channels = 6;
        REQUIRE(!ResolvedRenderRequest::freeze(bad, err) && err == Failure::INVALID_CHANNELS);
        bad = baseline(); bad.window = {11, 480000};
        REQUIRE(!ResolvedRenderRequest::freeze(bad, err) && err == Failure::INVALID_WINDOW);
        bad = baseline(); bad.mode = Mode::PREVIEW; bad.window = {480000, 480001};
        REQUIRE(!ResolvedRenderRequest::freeze(bad, err) && err == Failure::INVALID_WINDOW);
        bad = baseline(); bad.state = static_cast<State>(255);
        REQUIRE(!ResolvedRenderRequest::freeze(bad, err) && err == Failure::INVALID_STATE);
        bad = baseline(); bad.mode = static_cast<Mode>(255);
        REQUIRE(!ResolvedRenderRequest::freeze(bad, err) && err == Failure::INVALID_WINDOW);
        bad = baseline(); bad.stored_order[0].kind = static_cast<InlineKind>(255);
        REQUIRE(!ResolvedRenderRequest::freeze(bad, err) && err == Failure::INVALID_NODE);
        bad = baseline(); bad.format = static_cast<Format>(255);
        REQUIRE(!ResolvedRenderRequest::freeze(bad, err) && err == Failure::INVALID_DITHER_POLICY);
        bad = baseline(); bad.stored_order[1].instance_id = "eq";
        REQUIRE(!ResolvedRenderRequest::freeze(bad, err) && err == Failure::DUPLICATE_INSTANCE);
        bad = baseline(); bad.source_identity.clear();
        REQUIRE(!ResolvedRenderRequest::freeze(bad, err) && err == Failure::MISSING_PROVENANCE);
        bad = baseline(); bad.terminal_limiter_signature.clear();
        REQUIRE(!ResolvedRenderRequest::freeze(bad, err) && err == Failure::INVALID_TERMINAL);
        bad = baseline(); bad.format = Format::FLOAT32;
        REQUIRE(!ResolvedRenderRequest::freeze(bad, err) && err == Failure::INVALID_DITHER_POLICY);
        bad.dither_seed.reset();
        REQUIRE(ResolvedRenderRequest::freeze(bad, err));
        bad = baseline(); bad.dither_seed.reset();
        REQUIRE(!ResolvedRenderRequest::freeze(bad, err) && err == Failure::INVALID_DITHER_POLICY);
        // Checked latency preflight: no authoritative partial result on failure.
        bad = baseline();
        bad.stored_order[0].active_latency_frames = std::numeric_limits<std::uint64_t>::max();
        auto huge = ResolvedRenderRequest::freeze(bad, err);
        REQUIRE(huge);
        REQUIRE(!RenderExecutionPlan::preflight(*huge, err) && err == Failure::LATENCY_OVERFLOW);
        // All supported same-rate clocks retain absolute-frame semantics.
        for (auto hz : {44100u, 48000u, 88200u, 96000u, 176400u, 192000u}) {
            auto one = baseline();
            one.processing_rate_hz = hz; one.target_rate_hz = hz;
            auto r = ResolvedRenderRequest::freeze(one, err);
            REQUIRE(r && RenderExecutionPlan::preflight(*r, err));
        }
        std::cout << "M22_PRIVATE_REQUEST_PLAN_TEST_PASS_NOT_GATE_I_G_L\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
