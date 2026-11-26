#pragma once
// L3-M13 P01 diagnostic report sidecar. This is explicitly NOT the
// final ReferenceMatchVerificationReport and cannot grant VERIFIED.
// Uses the existing Project canonical JSON adapter and core SHA-256:
// /VerificationContentChecksum is omitted structurally before hashing.
// Every other dependency/checksum, including nested SHA values, stays.
// No alternative JSON/canonicalization/checksum implementation.
#include "l3_m13_provisional_verification_assessment.hpp"
#include <rgsml/core/sha256.hpp>
#include <rgsml/project/opaque_json_value.hpp>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QString>
#include <array>
#include <charconv>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace rgsml::app::l3_automatch::m13_frozen {
inline constexpr std::string_view kProvisionalDiagnosticSchema=
    "rgsml.reference-match.verification-diagnostic/1.0.0";
inline constexpr std::string_view kM13OwnChecksumPolicy=
    "rgsml.qualification.content-checksum/1.0.0";
struct M13ProvisionalDiagnosticEnvelope final {
    project::OpaqueJsonValue canonical_report;
    std::string exact_report_checksum;
    [[nodiscard]] constexpr bool is_m13_verified() const noexcept
    {return false;}
    [[nodiscard]] constexpr bool authorizes_wav_export() const noexcept
    {return false;}
    [[nodiscard]] constexpr bool is_final_p01_report() const noexcept
    {return false;}
};
[[nodiscard]] inline QString m13_ascii(std::string_view value)
{
    return QString::fromLatin1(
        value.data(),static_cast<qsizetype>(value.size()));
}
[[nodiscard]] inline QString m13_decimal_from_binary64(double value)
{
    if(!std::isfinite(value))return {};
    std::array<char,64> chars{};
    const auto [end,err]=std::to_chars(
        chars.data(),chars.data()+chars.size(),value,
        std::chars_format::general,
        std::numeric_limits<double>::max_digits10);
    if(err!=std::errc{})return {};
    return QString::fromLatin1(chars.data(),
        static_cast<qsizetype>(end-chars.data()));
}
[[nodiscard]] inline std::string m13_digest_canonical_json(
    const project::OpaqueJsonValue& canonical)
{
    const auto& bytes=canonical.canonical_utf8();
    return core::sha256_hex(std::as_bytes(
        std::span<const char>{bytes.data(),bytes.size()}));
}
[[nodiscard]] inline const char* m13_state_token(
    VerificationState state) noexcept
{
    switch(state) {
    case VerificationState::not_verified:return "NOT_VERIFIED";
    case VerificationState::verified:return "VERIFIED";
    case VerificationState::verification_failed:return "VERIFICATION_FAILED";
    case VerificationState::verification_unavailable:
        return "VERIFICATION_UNAVAILABLE";
    case VerificationState::stale:return "STALE";
    }
    return "";
}
[[nodiscard]] inline std::optional<QJsonObject>
m13_provisional_payload_without_own_checksum(
    const M13ImmutableProductFreshProbeRequest& admitted,
    const M13FreshProductMasterProbe& fresh,
    const M13ProvisionalAssessment& assessment,
    core::Uuid exact_report_id)
{
    if(exact_report_id.is_nil()||
       !fresh.matches_executed_request(admitted)||
       assessment.state==VerificationState::verified||
       assessment.verified_profile_approval_exists||
       !admitted.exact_materialized_preview_owner().replay||
       !admitted.exact_materialized_preview_owner().exact_prepared_owner)
        return std::nullopt;
    const auto& plan=*admitted.exact_materialized_preview_owner().replay;
    if(plan.project_id.is_nil()||plan.controller_id.is_nil()||
       plan.plan_id.is_nil()||plan.linked_chain_id.is_nil()||
       plan.exact_plan_checksum.empty()||
       !std::isfinite(fresh.fresh_m16.global_linear_peak())||
       !std::isfinite(admitted.frozen_independent_m16_ceiling_db_tp()))
        return std::nullopt;
    const auto reasons=normalize_verification_reasons(
        assessment.canonical_reasons);
    if(!reasons||*reasons!=assessment.canonical_reasons)
        return std::nullopt;
    QJsonArray orderedReasons;
    for(const auto& reason:*reasons) {
        QJsonObject item{
            {"stage",static_cast<int>(reason.stage)},
            {"code",QString::fromStdString(reason.code)},
            {"fieldPath",QString::fromStdString(reason.field_path)},
            {"bindingKey",QString::fromStdString(reason.binding_key)},
            {"componentKey",QString::fromStdString(reason.component_key)},
            {"dependencyRef",QString::fromStdString(reason.dependency_ref)},
            {"expected",QString::fromStdString(reason.expected_ref_or_value)},
            {"actual",QString::fromStdString(reason.actual_ref_or_value)},
            {"numericCategory",QString::fromStdString(reason.numeric_category)},
            {"predicate",static_cast<int>(reason.predicate)},
            {"measuredDelta",reason.measured_delta
                ?QJsonValue(m13_decimal_from_binary64(*reason.measured_delta))
                :QJsonValue(QJsonValue::Null)},
            {"allowedBound",reason.allowed_bound
                ?QJsonValue(m13_decimal_from_binary64(*reason.allowed_bound))
                :QJsonValue(QJsonValue::Null)}
        };
        if((reason.measured_delta&&!std::isfinite(*reason.measured_delta))||
           (reason.allowed_bound&&!std::isfinite(*reason.allowed_bound)))
            return std::nullopt;
        orderedReasons.append(item);
    }
    const auto peak=m13_decimal_from_binary64(
        fresh.fresh_m16.global_linear_peak());
    const auto ceiling=m13_decimal_from_binary64(
        admitted.frozen_independent_m16_ceiling_db_tp());
    if(peak.isEmpty()||ceiling.isEmpty())return std::nullopt;
    // Exact integers are decimal strings: never pass a u64 revision
    // through QJsonValue's IEEE754 double.
    QJsonObject evidence{
        {"freshNativeRender",assessment.fresh_master_execution_attested},
        {"freshBindingPopulationComplete",
            assessment.actual_27_binding_population_attested},
        {"freshProximityMatchesPrediction",
            assessment.same_run_proximity_attested},
        {"requiredNativeComponents",QString::number(
            static_cast<qulonglong>(assessment.required_native_components))},
        {"configuredNativeComponents",QString::number(
            static_cast<qulonglong>(assessment.configured_native_components))},
        {"verificationEvaluations",QString::number(
            static_cast<qulonglong>(fresh.logical_verification_evaluations))},
        {"sampleRateHz",QString::number(
            static_cast<qlonglong>(fresh.fresh_m19.sample_rate_hz))},
        {"frameCount",QString::number(static_cast<qlonglong>(
            fresh.fresh_full_master_pcm.view().frame_count().value()))},
        {"fullResourceM16LinearPeak",peak},
        {"independentM16CeilingDbTp",ceiling}
    };
    return QJsonObject{
        {"schemaId",m13_ascii(kProvisionalDiagnosticSchema)},
        {"verifierId",m13_ascii(kVerifierId)},
        {"verificationPolicyId",m13_ascii(kVerificationPolicyId)},
        {"reportId",QString::fromStdString(exact_report_id.to_string())},
        {"reportRevision",QStringLiteral("1")},
        {"qualificationToleranceProfileId",m13_ascii(kQtpId)},
        {"qualificationToleranceProfileState",QStringLiteral("PROVISIONAL")},
        {"qualifiedProfileApprovalRef",QJsonValue::Null},
        {"reportKind",QStringLiteral("PROVISIONAL_DIAGNOSTIC_NOT_VERIFIED")},
        {"reportRunOutcome",QString::fromLatin1(
            m13_state_token(assessment.state))},
        {"checksumPolicyId",m13_ascii(kM13OwnChecksumPolicy)},
        {"checksumExclusion",QStringLiteral(
            "/VerificationContentChecksum")},
        {"projectId",QString::fromStdString(plan.project_id.to_string())},
        {"acceptedPlanId",QString::fromStdString(plan.plan_id.to_string())},
        {"controllerId",QString::fromStdString(plan.controller_id.to_string())},
        {"linkedChainId",QString::fromStdString(plan.linked_chain_id.to_string())},
        {"acceptedPlanContentChecksum",
            QString::fromStdString(plan.exact_plan_checksum)},
        {"freshEvidence",evidence},
        {"canonicalReasons",orderedReasons},
        {"primaryReason",orderedReasons.isEmpty()
            ?QJsonValue(QJsonValue::Null):QJsonValue(orderedReasons[0].toObject())}
    };
}
[[nodiscard]] inline core::Result<M13ProvisionalDiagnosticEnvelope>
encode_m13_provisional_diagnostic(
    const M13ImmutableProductFreshProbeRequest& admitted,
    const M13FreshProductMasterProbe& fresh,
    const M13ProvisionalAssessment& assessment,
    core::Uuid exact_report_id)
{
    using R=core::Result<M13ProvisionalDiagnosticEnvelope>;
    const auto fail=[](const char* msg){
        return R::failure(core::Error{core::ErrorCode::InvalidState,msg});
    };
    auto payload=m13_provisional_payload_without_own_checksum(
        admitted,fresh,assessment,exact_report_id);
    if(!payload)return fail("L3_M13_PROVISIONAL_DIAGNOSTIC_INPUT_INVALID");
    auto canonical=project::OpaqueJsonValue::parse(
        QJsonDocument(*payload).toJson(QJsonDocument::Compact).toStdString());
    if(!canonical)return R::failure(*canonical.error());
    const auto checksum=m13_digest_canonical_json(*canonical.value());
    payload->insert("VerificationContentChecksum",
                    QString::fromStdString(checksum));
    auto sealed=project::OpaqueJsonValue::parse(
        QJsonDocument(*payload).toJson(QJsonDocument::Compact).toStdString());
    if(!sealed)return R::failure(*sealed.error());
    return R::success(M13ProvisionalDiagnosticEnvelope{
        std::move(*sealed.value()),checksum});
}
// This is a strict same-artifact integrity check, NOT a digital
// signature. A caller must separately authenticate Project ownership.
[[nodiscard]] inline bool verify_m13_provisional_diagnostic_integrity(
    const project::OpaqueJsonValue& serialized)
{
    const auto& bytes=serialized.canonical_utf8();
    const auto doc=QJsonDocument::fromJson(
        QByteArray::fromStdString(bytes));
    if(!doc.isObject())return false;
    auto root=doc.object();
    if(root.size()!=21U||
       root.value("schemaId").toString()!=
           m13_ascii(kProvisionalDiagnosticSchema)||
       root.value("verifierId").toString()!=
           m13_ascii(kVerifierId)||
       root.value("verificationPolicyId").toString()!=
           m13_ascii(kVerificationPolicyId)||
       root.value("qualificationToleranceProfileId").toString()!=
           m13_ascii(kQtpId)||
       root.value("qualificationToleranceProfileState").toString()!="PROVISIONAL"||
       !root.value("qualifiedProfileApprovalRef").isNull()||
       root.value("reportKind").toString()!="PROVISIONAL_DIAGNOSTIC_NOT_VERIFIED"||
       root.value("reportRunOutcome").toString()=="VERIFIED"||
       root.value("checksumPolicyId").toString()!=
           m13_ascii(kM13OwnChecksumPolicy)||
       root.value("checksumExclusion").toString()!=
           "/VerificationContentChecksum"||
       !root.value("canonicalReasons").isArray()||
       !root.value("freshEvidence").isObject())
        return false;
    if(root.value("reportRevision").toString()!=QStringLiteral("1")||
       root.value("reportId").toString().isEmpty()||
       root.value("acceptedPlanContentChecksum").toString().isEmpty()||
       root.value("reportRunOutcome").toString().isEmpty())
        return false;
    const auto check_id=[&](const char* field)->bool {
        const auto id=core::Uuid::parse(
            root.value(QString::fromLatin1(field)).toString().toStdString());
        return id&&!id.value()->is_nil();
    };
    if(!check_id("reportId")||!check_id("projectId")||
       !check_id("acceptedPlanId")||!check_id("controllerId")||
       !check_id("linkedChainId"))return false;
    const auto outcome=root.value("reportRunOutcome").toString();
    if(outcome!="NOT_VERIFIED"&&outcome!="VERIFICATION_FAILED"&&
       outcome!="VERIFICATION_UNAVAILABLE"&&outcome!="STALE")
        return false;
    // The diagnostic is a frozen numerical testimony, not arbitrary
    // JSON. Only the six native rates, one genuine fresh full render,
    // valid component cardinalities and canonical numeric strings pass.
    const auto observed=root.value("freshEvidence").toObject();
    if(observed.size()!=10U||
       !observed.value("freshNativeRender").isBool()||
       !observed.value("freshBindingPopulationComplete").isBool()||
       !observed.value("freshProximityMatchesPrediction").isBool())
        return false;
    const auto exact_uint=[&](const char* field)->
        std::optional<std::uint64_t> {
        const auto token=observed.value(field);
        if(!token.isString())return std::nullopt;
        const auto str=token.toString().toStdString();
        if(str.empty())return std::nullopt;
        std::uint64_t value{};
        const auto [end,err]=std::from_chars(
            str.data(),str.data()+str.size(),value);
        if(err!=std::errc{}||end!=str.data()+str.size()||
           std::to_string(value)!=str)return std::nullopt;
        return value;
    };
    const auto exact_real=[&](const char* field)->
        std::optional<double> {
        const auto token=observed.value(field);
        if(!token.isString())return std::nullopt;
        const auto str=token.toString().toStdString();
        if(str.empty())return std::nullopt;
        double value{};
        const auto [end,err]=std::from_chars(
            str.data(),str.data()+str.size(),value,
            std::chars_format::general);
        if(err!=std::errc{}||end!=str.data()+str.size()||
           !std::isfinite(value)||m13_decimal_from_binary64(value)!=token.toString())return std::nullopt;
        return value;
    };
    const auto configured=exact_uint("configuredNativeComponents");
    const auto required=exact_uint("requiredNativeComponents");
    const auto evaluations=exact_uint("verificationEvaluations");
    const auto fs=exact_uint("sampleRateHz");
    const auto frames=exact_uint("frameCount");
    const auto measured=exact_real("fullResourceM16LinearPeak");
    const auto ceiling=exact_real("independentM16CeilingDbTp");
    if(!configured||!required||!evaluations||!fs||!frames||
       !measured||!ceiling||*required>*configured||
       *evaluations!=1U||*frames==0U||
       (*fs!=44100U&&*fs!=48000U&&*fs!=88200U&&*fs!=96000U&&
        *fs!=176400U&&*fs!=192000U)||
       *measured<0.0)return false;
    const auto reasons=root.value("canonicalReasons").toArray();
    std::vector<VerificationReason> decodedReasons;
    decodedReasons.reserve(static_cast<std::size_t>(reasons.size()));
    const auto decode_number=[](const QJsonValue& item)->
        std::optional<std::optional<double>> {
        if(item.isNull())return
            std::optional<std::optional<double>>{
                std::optional<double>{}};
        if(!item.isString())return std::nullopt;
        const auto str=item.toString().toStdString();
        double decoded{};
        const auto [end,err]=std::from_chars(
            str.data(),str.data()+str.size(),decoded,
            std::chars_format::general);
        if(err!=std::errc{}||end!=str.data()+str.size()||
           !std::isfinite(decoded)||decoded<0.0)return std::nullopt;
        return std::optional<std::optional<double>>{
            std::optional<double>{decoded}};
    };
    for(const auto& value:reasons) {
        if(!value.isObject())return false;
        const auto item=value.toObject();
        if(item.size()!=12U||!item.value("stage").isDouble()||
           !item.value("predicate").isDouble())return false;
        const auto st=item.value("stage").toInt(-1);
        const auto pr=item.value("predicate").toInt(-1);
        if(st<0||st>static_cast<int>(VerificationStage::proximity)||
           pr<0||pr>static_cast<int>(PredicateState::stale))
            return false;
        for(const auto field:{"code","fieldPath","bindingKey",
            "componentKey","dependencyRef","expected","actual",
            "numericCategory"})
            if(!item.value(field).isString())return false;
        const auto delta=decode_number(item.value("measuredDelta"));
        const auto bound=decode_number(item.value("allowedBound"));
        if(!delta||!bound)return false;
        VerificationReason record;
        record.stage=static_cast<VerificationStage>(st);
        record.code=item.value("code").toString().toStdString();
        record.field_path=item.value("fieldPath").toString().toStdString();
        record.binding_key=item.value("bindingKey").toString().toStdString();
        record.component_key=item.value("componentKey").toString().toStdString();
        record.dependency_ref=item.value("dependencyRef").toString().toStdString();
        record.expected_ref_or_value=item.value("expected").toString().toStdString();
        record.actual_ref_or_value=item.value("actual").toString().toStdString();
        record.numeric_category=item.value("numericCategory").toString().toStdString();
        record.predicate=static_cast<PredicateState>(pr);
        record.measured_delta=*delta;
        record.allowed_bound=*bound;
        decodedReasons.push_back(std::move(record));
    }
    const auto normalized=normalize_verification_reasons(decodedReasons);
    if(!normalized||*normalized!=decodedReasons)return false;
    const auto first=root.value("primaryReason");
    if(reasons.isEmpty()) {
        if(!first.isNull())return false;
    } else if(!first.isObject()||first.toObject()!=
              reasons.first().toObject()) {
        return false;
    }
    const auto digest=root.take("VerificationContentChecksum").toString();
    if(digest.size()!=64)return false;
    for(const auto ch:digest)
        if(!((ch>='0'&&ch<='9')||(ch>='a'&&ch<='f')))
            return false;
    auto withoutSelf=project::OpaqueJsonValue::parse(
        QJsonDocument(root).toJson(QJsonDocument::Compact).toStdString());
    if(!withoutSelf)return false;
    return m13_digest_canonical_json(*withoutSelf.value())==
        digest.toStdString();
}
} // namespace rgsml::app::l3_automatch::m13_frozen
