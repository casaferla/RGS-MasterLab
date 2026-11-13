#pragma once
// Included only by bench_chain_automatch.cpp after its original fixture
// helpers. Research-only REAL Product renderer prefix-cache proof.
// Product renderer, M16/M17 and frozen AutoMatch L3-M10 never touched.
namespace cache = rgsml::research::r1d_p2::perf;

core::Sha256Digest source_sha256(audio::AudioBufferView src)
{
    core::Sha256 hash;
    for(int c=0;c<2;++c) {
        const auto lane=src.channel(c);
        require(bool(lane),"missing stereo source for cache SHA");
        const auto bytes=std::as_bytes(std::span<const double>{
            lane.value()->data(),lane.value()->size()});
        hash.update(bytes);
    }
    return hash.finalize();
}

// Stage-specific complete dependency key; EVERY key member participates in
// structural equality. The fixture contains four known Product DSP types,
// never unregistered arbitrary modules. Unknown versions fail CLOSED.
cache::PrefixKey build_prefix_key(
    const audio::AudioBuffer& src,const dsp::ModuleRegistry& registry,
    double gainDb,std::uint64_t sourceRevision,
    std::uint64_t prefixRevision)
{
    const auto view=src.view();
    cache::PrefixKey key{};
    key.sourcePcmSha256=source_sha256(view);
    key.sourceRevision=sourceRevision;
    key.originFrame=view.absolute_start_frame().value();
    key.frameCount=view.frame_count().value();
    key.sampleRateHz=view.format().sample_rate().value();
    key.channelLayout=view.format().channel_layout();
    key.domain=view.timebase().frame_domain_id();
    key.prefixRevision=prefixRevision;
    key.exactTailContractVersion=1;
    key.programmeAtEos=true;
    for(int i=0;i<4;++i) {
        const auto idx=static_cast<std::size_t>(i);
        key.orderedTypes[idx]=kTypes[idx];
        const auto descriptor=registry.find_descriptor(kTypes[idx]);
        require(bool(descriptor),"cache key Product descriptor missing");
        const auto& d=descriptor.value()->get();
        const auto algorithm=d.algorithm_version();
        const auto codec=d.parameter_schema_id();
        require(algorithm && codec,"cache key Product version unknown");
        key.algorithms[idx]=std::string{*algorithm};
        key.parameterSchemas[idx]=std::string{*codec};
    }
    const auto gain=dsp::GainParameters::create(gainDb);
    require(bool(gain),"cache Gain key invalid");
    core::Uuid::Bytes bytes{};bytes[15]=77;
    auto band=dsp::EqBandParameters::create(
        core::Uuid{bytes},true,dsp::EqFilterType::BELL,
        dsp::EqRouting::STEREO,dsp::BellPayload{1000.0,2.0,0.9});
    require(bool(band),"cache EQ key invalid");
    std::vector<dsp::EqBandParameters> eqBands;
    eqBands.push_back(std::move(*band.value()));
    const auto eq=dsp::ParametricEqParameters::create(std::move(eqBands));
    const auto comp=dsp::CompressorParameters::create_default();
    const auto ms=dsp::StereoMsParameters::create(
        0.0,1.8,false,dsp::MonoBassMode::OFF,120.0,100.0);
    require(bool(eq)&&bool(comp)&&bool(ms),"cache DSP key invalid");
    key.bindings.push_back({id(1),*gain.value()});
    key.bindings.push_back({id(2),*eq.value()});
    key.bindings.push_back({id(3),*comp.value()});
    key.bindings.push_back({id(4),*ms.value()});
    return key;
}
audio::AudioBuffer stage_buffer_from_pcm(
    const audio::AudioBuffer& input,const std::vector<double>& pcm)
{
    const auto view=input.view();
    const auto n=static_cast<std::size_t>(view.frame_count().value());
    require(pcm.size()==2*n,"cached stage PCM not stereo planar");
    auto out=audio::AudioBuffer::create(
        view.format(),view.timebase().frame_domain_id(),
        view.absolute_start_frame(),fc(static_cast<std::int64_t>(n)));
    require(bool(out),"cache stage buffer allocation failed");
    for(int c=0;c<2;++c) {
        const auto lane=out.value()->mutable_view().channel(c);
        require(bool(lane)&&lane.value()->size()==n,
            "cache stage channel mismatch");
        std::copy(pcm.begin()+static_cast<std::size_t>(c)*n,
            pcm.begin()+static_cast<std::size_t>(c+1)*n,
            lane.value()->begin());
    }
    return std::move(*out.value());
}
void demonstrate_bounded_cache(
    const dsp::ModuleRegistry& registry,int hz,int seconds,
    std::ofstream& report)
{
    auto original=synthetic(hz,seconds);
    auto modified=synthetic(hz,seconds);
    const auto lane=modified.mutable_view().channel(0);
    require(bool(lane),"cache changed-source channel missing");
    const auto start=static_cast<std::size_t>(hz/2);
    for(std::size_t i=start;
        i<std::min(lane.value()->size(),start+1024U);++i)
        (*lane.value())[i]+=0.5;
    const auto n=static_cast<std::size_t>(hz)*
                 static_cast<std::size_t>(seconds);
    const auto maxBytes=2*n*sizeof(double)+4096U;
    cache::BoundedPrefixLru store{maxBytes};
    cache::BoundedPrefixLru impossible{0};
    const auto controlKey=build_prefix_key(original,registry,1.0,1,4);
    require(!impossible.store(controlKey,std::vector<double>(2*n,0.0))
        && impossible.bytesUsed()==0 && impossible.refusals()==1,
        "cache budget refusal is not fail-closed");
    struct Request {
        const char* name;
        int source;    // 0 baseline, 1 edited PCM with same object revision
        double gain,drive,ceiling;
        bool bypass;
        std::uint64_t sourceRevision,prefixRevision;
        bool expectedHit;
    };
    const std::array<Request,11> scenarios{{
        {"COLD",0,1.0,0.0,-1.0,false,1,4,false},
        {"P2_DRIVE3",0,1.0,3.0,-1.0,false,1,4,true},
        {"P2_DRIVE6",0,1.0,6.0,-1.0,false,1,4,true},
        {"P2_CEILING_MINUS3",0,1.0,3.0,-3.0,false,1,4,true},
        {"P2_BYPASS",0,1.0,0.0,-1.0,true,1,4,true},
        {"CHANGED_GAIN",0,2.0,3.0,-1.0,false,1,4,false},
        {"CHANGED_GAIN_HIT",0,2.0,6.0,-1.0,false,1,4,true},
        {"CHANGED_CONTENT_SAME_REV",1,2.0,3.0,-1.0,false,1,4,false},
        {"CHANGED_SOURCE_REVISION",1,2.0,6.0,-1.0,false,2,4,false},
        {"REVERTED_PREFIX_AFTER_LRU",0,1.0,0.0,-1.0,false,1,4,false},
        {"PREFIX_REVISION_CHANGED",0,1.0,3.0,-1.0,false,1,5,false}
    }};
    for(const auto& test:scenarios) {
        const auto& input=test.source==0?original:modified;
        const Trial whole{"INDEPENDENT_ORACLE",5,test.bypass,
            test.drive,test.ceiling,test.gain};
        const Trial suffix{"CACHE_SUFFIX_P2",-1,test.bypass,
            test.drive,test.ceiling};
        std::vector<double> fresh,cached;
        // Oracle full RenderPreview is excluded from cached-path timing.
        // This checks ALL double bits, not only a non-cryptographic hash.
        const auto oracle=evaluate(input,registry,whole,&fresh);
        const auto stamp0=Clock::now();
        auto key=build_prefix_key(input,registry,test.gain,
            test.sourceRevision,test.prefixRevision);
        const auto stamp1=Clock::now();
        const auto* entry=store.lookup(key);
        const bool hit=entry!=nullptr;
        require(hit==test.expectedHit,
            "unsafe cache hit or unexpected miss vs dependency changes");
        double prefixMs=0;
        std::vector<double> pcm;
        if(hit) pcm=entry->planarPcm;
        else {
            const Trial four{"NEW_PREFIX_4",4,true,0.0,-1.0,test.gain};
            const auto prefix=evaluate(input,registry,four,&pcm);
            prefixMs=prefix.ms;
            require(store.store(std::move(key),pcm),
                "stage unexpectedly does not fit bounded LRU");
        }
        const auto copy0=Clock::now();
        auto snapshot=stage_buffer_from_pcm(input,pcm);
        const auto copy1=Clock::now();
        const auto result=evaluate(snapshot,registry,suffix,&cached);
        require(fresh.size()==cached.size(),"cache output length changed");
        std::size_t mismatch=0;
        for(std::size_t i=0;i<fresh.size();++i)
            if(std::bit_cast<std::uint64_t>(fresh[i])!=
                std::bit_cast<std::uint64_t>(cached[i]))
                ++mismatch;
        require(mismatch==0,
            "cached Product RenderPreview result diverged bit-for-bit");
        require(store.entries()==1 && store.bytesUsed()<=store.capacity(),
            "LRU cache exceeds bounded PCM budget");
        report<<"I2-REAL-CACHE-1\t"<<hz<<'\t'<<seconds<<'\t'
              <<test.name<<'\t'<<(test.expectedHit?1:0)<<'\t'
              <<(hit?1:0)<<'\t'<<mismatch<<'\t'
              <<millis(stamp0,stamp1)<<'\t'<<prefixMs<<'\t'
              <<millis(copy0,copy1)<<'\t'<<result.ms<<'\t'
              <<oracle.ms<<'\t'<<store.bytesUsed()<<'\t'
              <<store.evictions()<<'\t'<<store.hits()<<'\t'
              <<store.misses()<<'\t'<<store.refusals()<<'\t'
              <<std::hex<<oracle.hash<<'\t'<<result.hash<<std::dec<<'\n';
        report.flush();
        require(bool(report),"cache report could not flush");
        std::cout<<"BOUNDED_CACHE rate="<<hz<<" scenario="<<test.name
                 <<" hit="<<hit<<" exact=YES"
                 <<" key_ms="<<millis(stamp0,stamp1)
                 <<" prefix_ms="<<prefixMs<<" suffix_ms="<<result.ms
                 <<" memory_bytes="<<store.bytesUsed()
                 <<" evictions="<<store.evictions()<<'\n';
    }
}
