#pragma once
// M20 authentic PCM spectral evidence carried across the M21 Product bridge.
// This is an internal read-only proof, NOT a Qt/QML data publisher.
// Numerical points cannot be admitted to user-visible UI by this class.
#include "m21_product_qt_readiness_bridge.hpp"
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

namespace rgsml::app::internal {

class M21ProductSpectrumEvidence final {
public:
    using Readiness=M21ProductQtReadinessBridge;
    using Column=m21_private::Column;
    using Routing=Readiness::Routing;
    using Controllers=Readiness::Controllers;
    using Fanout=Readiness::Fanout;
    using Inbox=Readiness::Inbox;
    struct Bin final {
        double hz{};
        double linear_power{};
        [[nodiscard]] bool operator==(const Bin&) const = default;
    };
    struct Proof final {
        bool current_owner{false};
        bool actual_spectrum{false};
        bool product_ui_authorized{false}; // NOT release qualified
        std::uint64_t owner_epoch{};
        std::uint64_t routing_revision{};
        Column selected{Column::prepared};
        Readiness::Identity identity{};
        std::vector<Bin> bins{};
    };

    [[nodiscard]] static Proof capture(
        const Controllers& product,const Fanout& fanout,
        const AuditionSourceSelector& selector,
        const GoldSelectionViewModel& gold,const Inbox& inbox,
        const Routing& routing)
    {
        Proof proof;
        const auto frame=Readiness::capture(
            product,fanout,selector,gold,inbox,routing);
        if(!frame.current_product_owner||frame.product_ui_authorized)
            return proof;
        constexpr std::size_t m20=1;
        proof.current_owner=true;
        proof.owner_epoch=frame.owner_epoch;
        proof.routing_revision=frame.routing_revision;
        proof.selected=frame.selected[m20];
        proof.identity=frame.identities[m20];
        // An absent independently selected REFERENCE is not a license
        // to fall back to SOURCE or to display a synthetic flat spectrum.
        if(proof.identity.lifecycle!=m21_private::Lifecycle::current
           ||proof.identity.resource.empty()
           ||proof.identity.generation==0)
            return proof;
        const auto compare=inbox.view_current(
            product,fanout,selector,gold);
        const auto i=static_cast<std::size_t>(proof.selected);
        if(i>=compare.columns.size()||!compare.columns[i])return proof;
        const auto& record=*compare.columns[i];
        if(record.lifecycle!=m21_private::Lifecycle::current
           ||record.resource_identity!=proof.identity.resource
           ||record.generation_token!=proof.identity.generation
           ||record.absolute_scope_begin!=proof.identity.start
           ||record.scope_frames!=proof.identity.frames
           ||record.sample_rate_hz!=proof.identity.sample_rate
           ||record.analysis_scope_label!=proof.identity.scope
           ||record.average_spectrum.empty()
           ||record.average_spectrum.size()>65537U)
            return proof;
        double last=-1.0;
        std::vector<Bin> collected;
        collected.reserve(record.average_spectrum.size());
        for(const auto& point:record.average_spectrum) {
            if(!std::isfinite(point.hz)||point.hz<=last
               ||point.hz<0.0
               ||point.hz>static_cast<double>(proof.identity.sample_rate)/2.0
               ||point.power_density.quality!=m21_private::Quality::present
               ||!point.power_density.unrounded
               ||!std::isfinite(*point.power_density.unrounded)
               ||*point.power_density.unrounded<0.0)
                return proof;
            collected.push_back({point.hz,*point.power_density.unrounded});
            last=point.hz;
        }
        // A second owner check prevents accepting a late Product
        // generation or an M20 per-panel routing change.
        if(!Readiness::still_current(frame,product,fanout,selector,
                                     gold,inbox,routing))
            return proof;
        proof.actual_spectrum=true;
        proof.bins=std::move(collected);
        return proof;
    }
    [[nodiscard]] static bool still_current(
        const Proof& proof,const Controllers& product,
        const Fanout& fanout,const AuditionSourceSelector& selector,
        const GoldSelectionViewModel& gold,const Inbox& inbox,
        const Routing& routing)
    {
        if(!proof.current_owner||!proof.actual_spectrum
           ||proof.product_ui_authorized
           ||proof.owner_epoch==0
           ||!routing.is_current_route(proof.routing_revision))
            return false;
        const auto frame=Readiness::capture(
            product,fanout,selector,gold,inbox,routing);
        constexpr std::size_t m20=1;
        return frame.current_product_owner
            &&!frame.product_ui_authorized
            &&frame.owner_epoch==proof.owner_epoch
            &&frame.routing_revision==proof.routing_revision
            &&frame.selected[m20]==proof.selected
            &&frame.identities[m20]==proof.identity;
    }
};
} // namespace rgsml::app::internal
