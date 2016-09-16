/* -*- c-basic-offset: 4 indent-tabs-mode: nil -*-  vi:set ts=8 sts=4 sw=4: */

/*
    VamPipe

    Centre for Digital Music, Queen Mary, University of London.
    Copyright 2015-2016 QMUL.
  
    Permission is hereby granted, free of charge, to any person
    obtaining a copy of this software and associated documentation
    files (the "Software"), to deal in the Software without
    restriction, including without limitation the rights to use, copy,
    modify, merge, publish, distribute, sublicense, and/or sell copies
    of the Software, and to permit persons to whom the Software is
    furnished to do so, subject to the following conditions:

    The above copyright notice and this permission notice shall be
    included in all copies or substantial portions of the Software.

    THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
    EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
    MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
    NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS BE LIABLE FOR
    ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF
    CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
    WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

    Except as contained in this notice, the names of the Centre for
    Digital Music; Queen Mary, University of London; and Chris Cannam
    shall not be used in advertising or otherwise to promote the sale,
    use or other dealings in this Software without prior written
    authorization.
*/

#include "vamp.capnp.h"

#include <capnp/message.h>
#include <capnp/serialize-packed.h>

#include <vamp-hostsdk/Plugin.h>
#include <vamp-hostsdk/PluginLoader.h>
#include <vamp-hostsdk/PluginStaticData.h>

#include "bits/PluginHandleMapper.h"
#include "bits/PluginOutputIdMapper.h"
#include "bits/RequestResponseType.h"

namespace vampipe
{

/**
 * Convert the structures laid out in the Vamp SDK classes into Cap'n
 * Proto structures (and back again).
 * 
 * At least some of this will be necessary for any implementation
 * using Cap'n Proto that uses the C++ Vamp SDK to provide its
 * reference structures. An implementation could alternatively use the
 * Cap'n Proto structures directly, and interact with Vamp plugins
 * using the Vamp C API, without using the C++ Vamp SDK classes at
 * all. That would avoid a lot of copying (in Cap'n Proto style).
 */
class VampnProto
{
public:
    typedef ::capnp::MallocMessageBuilder MsgBuilder;

    template <typename T, typename B>
    static void buildBasicDescriptor(B &basic, const T &t) {
        basic.setIdentifier(t.identifier);
        basic.setName(t.name);
        basic.setDescription(t.description);
    }

    template <typename T, typename B>
    static void readBasicDescriptor(T &t, const B &basic) {
        t.identifier = basic.getIdentifier();
        t.name = basic.getName();
        t.description = basic.getDescription();
    }

    template <typename T, typename M>
    static void buildValueExtents(M &m, const T &t) {
        m.setMinValue(t.minValue);
        m.setMaxValue(t.maxValue);
    }

    template <typename T, typename M>
    static void readValueExtents(T &t, const M &m) {
        t.minValue = m.getMinValue();
        t.maxValue = m.getMaxValue();
    }

    static void buildRealTime(RealTime::Builder &b, const Vamp::RealTime &t) {
        b.setSec(t.sec);
        b.setNsec(t.nsec);
    }

    static void readRealTime(Vamp::RealTime &t, const RealTime::Reader &r) {
        t.sec = r.getSec();
        t.nsec = r.getNsec();
    }

    static SampleType
    fromSampleType(Vamp::Plugin::OutputDescriptor::SampleType t) {
        switch (t) {
        case Vamp::Plugin::OutputDescriptor::OneSamplePerStep:
            return SampleType::ONE_SAMPLE_PER_STEP;
        case Vamp::Plugin::OutputDescriptor::FixedSampleRate:
            return SampleType::FIXED_SAMPLE_RATE;
        case Vamp::Plugin::OutputDescriptor::VariableSampleRate:
            return SampleType::VARIABLE_SAMPLE_RATE;
        }
        throw std::logic_error("unexpected Vamp SampleType enum value");
    }

    static Vamp::Plugin::OutputDescriptor::SampleType
    toSampleType(SampleType t) {
        switch (t) {
        case SampleType::ONE_SAMPLE_PER_STEP:
            return Vamp::Plugin::OutputDescriptor::OneSamplePerStep;
        case SampleType::FIXED_SAMPLE_RATE:
            return Vamp::Plugin::OutputDescriptor::FixedSampleRate;
        case SampleType::VARIABLE_SAMPLE_RATE:
            return Vamp::Plugin::OutputDescriptor::VariableSampleRate;
        }
        throw std::logic_error("unexpected Capnp SampleType enum value");
    }

    static void
    buildOutputDescriptor(OutputDescriptor::Builder &b,
                          const Vamp::Plugin::OutputDescriptor &od) {

        auto basic = b.initBasic();
        buildBasicDescriptor(basic, od);

        b.setUnit(od.unit);

        b.setSampleType(fromSampleType(od.sampleType));
        b.setSampleRate(od.sampleRate);
        b.setHasDuration(od.hasDuration);

        b.setHasFixedBinCount(od.hasFixedBinCount);
        if (od.hasFixedBinCount) {
            b.setBinCount(od.binCount);
            if (od.binNames.size() > 0) {
                auto binNames = b.initBinNames(od.binNames.size());
                for (size_t i = 0; i < od.binNames.size(); ++i) {
                    binNames.set(i, od.binNames[i]);
                }
            }
        }

        b.setHasKnownExtents(od.hasKnownExtents);
        if (od.hasKnownExtents) {
            buildValueExtents(b, od);
        }

        b.setIsQuantized(od.isQuantized);
        if (od.isQuantized) {
            b.setQuantizeStep(od.quantizeStep);
        }
    }

    static void
    readOutputDescriptor(Vamp::Plugin::OutputDescriptor &od,
                         const OutputDescriptor::Reader &r) {

        readBasicDescriptor(od, r.getBasic());

        od.unit = r.getUnit();

        od.sampleType = toSampleType(r.getSampleType());
        od.sampleRate = r.getSampleRate();
        od.hasDuration = r.getHasDuration();

        od.hasFixedBinCount = r.getHasFixedBinCount();
        if (od.hasFixedBinCount) {
            od.binCount = r.getBinCount();
            od.binNames.clear();
            auto nn = r.getBinNames();
            for (const auto &n: nn) {
                od.binNames.push_back(n);
            }
        }

        od.hasKnownExtents = r.getHasKnownExtents();
        if (od.hasKnownExtents) {
            readValueExtents(od, r);
        }

        od.isQuantized = r.getIsQuantized();
        if (od.isQuantized) {
            od.quantizeStep = r.getQuantizeStep();
        }
    }

    static void
    buildParameterDescriptor(ParameterDescriptor::Builder &b,
                             const Vamp::Plugin::ParameterDescriptor &pd) {

        auto basic = b.initBasic();
        buildBasicDescriptor(basic, pd);

        b.setUnit(pd.unit);

        buildValueExtents(b, pd);

        b.setDefaultValue(pd.defaultValue);

        b.setIsQuantized(pd.isQuantized);
        if (pd.isQuantized) {
            b.setQuantizeStep(pd.quantizeStep);
        }
        
        if (pd.valueNames.size() > 0) {
            auto valueNames = b.initValueNames(pd.valueNames.size());
            for (size_t i = 0; i < pd.valueNames.size(); ++i) {
                valueNames.set(i, pd.valueNames[i]);
            }
        }
    }

    static void
    readParameterDescriptor(Vamp::Plugin::ParameterDescriptor &pd,
                            const ParameterDescriptor::Reader &r) {

        readBasicDescriptor(pd, r.getBasic());

        pd.unit = r.getUnit();

        readValueExtents(pd, r);

        pd.defaultValue = r.getDefaultValue();

        pd.isQuantized = r.getIsQuantized();
        if (pd.isQuantized) {
            pd.quantizeStep = r.getQuantizeStep();
        }

        pd.valueNames.clear();
        auto nn = r.getValueNames();
        for (const auto &n: nn) {
            pd.valueNames.push_back(n);
        }
    }
    
    static void
    buildFeature(Feature::Builder &b,
                 const Vamp::Plugin::Feature &f) {

        b.setHasTimestamp(f.hasTimestamp);
        if (f.hasTimestamp) {
            auto timestamp = b.initTimestamp();
            buildRealTime(timestamp, f.timestamp);
        }

        b.setHasDuration(f.hasDuration);
        if (f.hasDuration) {
            auto duration = b.initDuration();
            buildRealTime(duration, f.duration);
        }

        b.setLabel(f.label);

        if (f.values.size() > 0) {
            auto values = b.initValues(f.values.size());
            for (size_t i = 0; i < f.values.size(); ++i) {
                values.set(i, f.values[i]);
            }
        }
    }

    static void
    readFeature(Vamp::Plugin::Feature &f,
                const Feature::Reader &r) {

        f.hasTimestamp = r.getHasTimestamp();
        if (f.hasTimestamp) {
            readRealTime(f.timestamp, r.getTimestamp());
        }

        f.hasDuration = r.getHasDuration();
        if (f.hasDuration) {
            readRealTime(f.duration, r.getDuration());
        }

        f.label = r.getLabel();

        f.values.clear();
        auto vv = r.getValues();
        for (auto v: vv) {
            f.values.push_back(v);
        }
    }
    
    static void
    buildFeatureSet(FeatureSet::Builder &b,
                    const Vamp::Plugin::FeatureSet &fs,
                    const PluginOutputIdMapper &omapper) {

        auto featureset = b.initFeaturePairs(fs.size());
        int ix = 0;
        for (const auto &fsi : fs) {
            auto fspair = featureset[ix];
            fspair.setOutput(omapper.indexToId(fsi.first));
            auto featurelist = fspair.initFeatures(fsi.second.size());
            for (size_t j = 0; j < fsi.second.size(); ++j) {
                auto feature = featurelist[j];
                buildFeature(feature, fsi.second[j]);
            }
            ++ix;
        }
    }

    static void
    readFeatureSet(Vamp::Plugin::FeatureSet &fs,
                   const FeatureSet::Reader &r,
                   const PluginOutputIdMapper &omapper) {

        fs.clear();
        auto pp = r.getFeaturePairs();
        for (const auto &p: pp) {
            Vamp::Plugin::FeatureList vfl;
            auto ff = p.getFeatures();
            for (const auto &f: ff) {
                Vamp::Plugin::Feature vf;
                readFeature(vf, f);
                vfl.push_back(vf);
            }
            fs[omapper.idToIndex(p.getOutput())] = vfl;
        }
    }
    
    static InputDomain
    fromInputDomain(Vamp::Plugin::InputDomain d) {
        switch(d) {
        case Vamp::Plugin::TimeDomain:
            return InputDomain::TIME_DOMAIN;
        case Vamp::Plugin::FrequencyDomain:
            return InputDomain::FREQUENCY_DOMAIN;
        default:
            throw std::logic_error("unexpected Vamp InputDomain enum value");
        }
    }

    static Vamp::Plugin::InputDomain
    toInputDomain(InputDomain d) {
        switch(d) {
        case InputDomain::TIME_DOMAIN:
            return Vamp::Plugin::TimeDomain;
        case InputDomain::FREQUENCY_DOMAIN:
            return Vamp::Plugin::FrequencyDomain;
        default:
            throw std::logic_error("unexpected Capnp InputDomain enum value");
        }
    }
    
    static void
    buildPluginStaticData(PluginStaticData::Builder &b,
                          const Vamp::HostExt::PluginStaticData &d) {

        b.setPluginKey(d.pluginKey);

        auto basic = b.initBasic();
        buildBasicDescriptor(basic, d.basic);

        b.setMaker(d.maker);
        b.setCopyright(d.copyright);
        b.setPluginVersion(d.pluginVersion);

        auto clist = b.initCategory(d.category.size());
        for (size_t i = 0; i < d.category.size(); ++i) {
            clist.set(i, d.category[i]);
        }

        b.setMinChannelCount(d.minChannelCount);
        b.setMaxChannelCount(d.maxChannelCount);

        const auto &vparams = d.parameters;
        auto plist = b.initParameters(vparams.size());
        for (size_t i = 0; i < vparams.size(); ++i) {
            auto pd = plist[i];
            buildParameterDescriptor(pd, vparams[i]);
        }
        
        const auto &vprogs = d.programs;
        auto pglist = b.initPrograms(vprogs.size());
        for (size_t i = 0; i < vprogs.size(); ++i) {
            pglist.set(i, vprogs[i]);
        }

        b.setInputDomain(fromInputDomain(d.inputDomain));

        const auto &vouts = d.basicOutputInfo;
        auto olist = b.initBasicOutputInfo(vouts.size());
        for (size_t i = 0; i < vouts.size(); ++i) {
            auto od = olist[i];
            buildBasicDescriptor(od, vouts[i]);
        }
    }

    static void
    readPluginStaticData(Vamp::HostExt::PluginStaticData &d,
                         const PluginStaticData::Reader &r) {
        
        d.pluginKey = r.getPluginKey();

        readBasicDescriptor(d.basic, r.getBasic());

        d.maker = r.getMaker();
        d.copyright = r.getCopyright();
        d.pluginVersion = r.getPluginVersion();

        d.category.clear();
        auto cc = r.getCategory();
        for (auto c: cc) {
            d.category.push_back(c);
        }

        d.minChannelCount = r.getMinChannelCount();
        d.maxChannelCount = r.getMaxChannelCount();

        d.parameters.clear();
        auto pp = r.getParameters();
        for (auto p: pp) {
            Vamp::Plugin::ParameterDescriptor pd;
            readParameterDescriptor(pd, p);
            d.parameters.push_back(pd);
        }

        d.programs.clear();
        auto prp = r.getPrograms();
        for (auto p: prp) {
            d.programs.push_back(p);
        }

        d.inputDomain = toInputDomain(r.getInputDomain());

        d.basicOutputInfo.clear();
        auto oo = r.getBasicOutputInfo();
        for (auto o: oo) {
            Vamp::HostExt::PluginStaticData::Basic b;
            readBasicDescriptor(b, o);
            d.basicOutputInfo.push_back(b);
        }
    }

    static void
    buildPluginConfiguration(PluginConfiguration::Builder &b,
                             const Vamp::HostExt::PluginConfiguration &c) {

        const auto &vparams = c.parameterValues;
        auto params = b.initParameterValues(vparams.size());
        int i = 0;
        for (const auto &pp : vparams) {
            auto param = params[i++];
            param.setParameter(pp.first);
            param.setValue(pp.second);
        }

        b.setCurrentProgram(c.currentProgram);
        b.setChannelCount(c.channelCount);
        b.setStepSize(c.stepSize);
        b.setBlockSize(c.blockSize);
    }

    static void
    readPluginConfiguration(Vamp::HostExt::PluginConfiguration &c,
                            const PluginConfiguration::Reader &r) {

        auto pp = r.getParameterValues();
        for (const auto &p: pp) {
            c.parameterValues[p.getParameter()] = p.getValue();
        }

        c.currentProgram = r.getCurrentProgram();
        c.channelCount = r.getChannelCount();
        c.stepSize = r.getStepSize();
        c.blockSize = r.getBlockSize();
    }

    static void
    buildLoadRequest(LoadRequest::Builder &r,
                     const Vamp::HostExt::LoadRequest &req) {

        r.setPluginKey(req.pluginKey);
        r.setInputSampleRate(req.inputSampleRate);

        std::vector<AdapterFlag> flags;
        if (req.adapterFlags & Vamp::HostExt::PluginLoader::ADAPT_INPUT_DOMAIN) {
            flags.push_back(AdapterFlag::ADAPT_INPUT_DOMAIN);
        }
        if (req.adapterFlags & Vamp::HostExt::PluginLoader::ADAPT_CHANNEL_COUNT) {
            flags.push_back(AdapterFlag::ADAPT_CHANNEL_COUNT);
        }
        if (req.adapterFlags & Vamp::HostExt::PluginLoader::ADAPT_BUFFER_SIZE) {
            flags.push_back(AdapterFlag::ADAPT_BUFFER_SIZE);
        }

        auto f = r.initAdapterFlags(flags.size());
        for (size_t i = 0; i < flags.size(); ++i) {
            f.set(i, flags[i]);
        }
    }

    static void
    readLoadRequest(Vamp::HostExt::LoadRequest &req,
                    const LoadRequest::Reader &r) {

        req.pluginKey = r.getPluginKey();
        req.inputSampleRate = r.getInputSampleRate();

        int flags = 0;
        auto aa = r.getAdapterFlags();
        for (auto a: aa) {
            if (a == AdapterFlag::ADAPT_INPUT_DOMAIN) {
                flags |= Vamp::HostExt::PluginLoader::ADAPT_INPUT_DOMAIN;
            }
            if (a == AdapterFlag::ADAPT_CHANNEL_COUNT) {
                flags |= Vamp::HostExt::PluginLoader::ADAPT_CHANNEL_COUNT;
            }
            if (a == AdapterFlag::ADAPT_BUFFER_SIZE) {
                flags |= Vamp::HostExt::PluginLoader::ADAPT_BUFFER_SIZE;
            }
        }
        req.adapterFlags = flags;
    }

    static void
    buildLoadResponse(LoadResponse::Builder &b,
                      const Vamp::HostExt::LoadResponse &resp,
                      const PluginHandleMapper &pmapper) {

        b.setPluginHandle(pmapper.pluginToHandle(resp.plugin));
        auto sd = b.initStaticData();
        buildPluginStaticData(sd, resp.staticData);
        auto conf = b.initDefaultConfiguration();
        buildPluginConfiguration(conf, resp.defaultConfiguration);
    }

    static void
    readLoadResponse(Vamp::HostExt::LoadResponse &resp,
                     const LoadResponse::Reader &r,
                     const PluginHandleMapper &pmapper) {

        resp.plugin = pmapper.handleToPlugin(r.getPluginHandle());
        readPluginStaticData(resp.staticData, r.getStaticData());
        readPluginConfiguration(resp.defaultConfiguration,
                                r.getDefaultConfiguration());
    }

    static void
    buildConfigurationRequest(ConfigurationRequest::Builder &b,
                              const Vamp::HostExt::ConfigurationRequest &cr,
                              const PluginHandleMapper &pmapper) {

        b.setPluginHandle(pmapper.pluginToHandle(cr.plugin));
        auto c = b.initConfiguration();
        buildPluginConfiguration(c, cr.configuration);
    }

    static void
    readConfigurationRequest(Vamp::HostExt::ConfigurationRequest &cr,
                             const ConfigurationRequest::Reader &r,
                             const PluginHandleMapper &pmapper) {

        auto h = r.getPluginHandle();
        cr.plugin = pmapper.handleToPlugin(h);
        auto c = r.getConfiguration();
        readPluginConfiguration(cr.configuration, c);
    }

    static void
    buildConfigurationResponse(ConfigurationResponse::Builder &b,
                               const Vamp::HostExt::ConfigurationResponse &cr) {

        auto olist = b.initOutputs(cr.outputs.size());
        for (size_t i = 0; i < cr.outputs.size(); ++i) {
            auto od = olist[i];
            buildOutputDescriptor(od, cr.outputs[i]);
        }
    }

    static void
    readConfigurationResponse(Vamp::HostExt::ConfigurationResponse &cr,
                              const ConfigurationResponse::Reader &r) {

        cr.outputs.clear();
        auto oo = r.getOutputs();
        for (const auto &o: oo) {
            Vamp::Plugin::OutputDescriptor desc;
            readOutputDescriptor(desc, o);
            cr.outputs.push_back(desc);
        }
    }

    static void
    buildProcessInput(ProcessInput::Builder &b,
                      Vamp::RealTime timestamp,
                      const std::vector<std::vector<float> > &buffers) {

        auto t = b.initTimestamp();
        buildRealTime(t, timestamp);
        auto vv = b.initInputBuffers(buffers.size());
        for (size_t ch = 0; ch < buffers.size(); ++ch) {
            const int n = int(buffers[ch].size());
            vv.init(ch, n);
            auto v = vv[ch];
            for (int i = 0; i < n; ++i) {
                v.set(i, buffers[ch][i]);
            }
        }
    }
    
    static void
    readProcessInput(Vamp::RealTime &timestamp,
                     std::vector<std::vector<float> > &buffers,
                     const ProcessInput::Reader &b) {

        readRealTime(timestamp, b.getTimestamp());
        buffers.clear();
        auto vv = b.getInputBuffers();
        for (const auto &v: vv) {
            std::vector<float> buf;
            for (auto x: v) {
                buf.push_back(x);
            }
            buffers.push_back(buf);
        }
    }
    
    static void
    buildProcessRequest(ProcessRequest::Builder &b,
                        const Vamp::HostExt::ProcessRequest &pr,
                        const PluginHandleMapper &pmapper) {

        b.setPluginHandle(pmapper.pluginToHandle(pr.plugin));
        auto input = b.initInput();
        buildProcessInput(input, pr.timestamp, pr.inputBuffers);
    }

    static void
    readProcessRequest(Vamp::HostExt::ProcessRequest &pr,
                       const ProcessRequest::Reader &r,
                       const PluginHandleMapper &pmapper) {

        auto h = r.getPluginHandle();
        pr.plugin = pmapper.handleToPlugin(h);
        readProcessInput(pr.timestamp, pr.inputBuffers, r.getInput());
    }

    static void
    buildProcessResponse(ProcessResponse::Builder &b,
                         const Vamp::HostExt::ProcessResponse &pr,
                         const PluginHandleMapper &pmapper) {

        auto f = b.initFeatures();
        buildFeatureSet(f, pr.features,
                        pmapper.pluginToOutputIdMapper(pr.plugin));
    }
    
    static void
    readProcessResponse(Vamp::HostExt::ProcessResponse &pr,
                        const ProcessResponse::Reader &r,
                        const PluginHandleMapper &pmapper) {

        readFeatureSet(pr.features, r.getFeatures(),
                       pmapper.handleToOutputIdMapper(r.getPluginHandle()));
    }

    static void
    buildVampRequest_List(VampRequest::Builder &b) {
        b.getRequest().setList();
    }

    static void
    buildVampResponse_List(VampResponse::Builder &b,
                           std::string errorText,
                           const std::vector<Vamp::HostExt::PluginStaticData> &d) {
        b.setSuccess(errorText == "");
        b.setErrorText(errorText);
        auto r = b.getResponse().initList();
        auto p = r.initPlugins(d.size());
        for (size_t i = 0; i < d.size(); ++i) {
            auto pd = p[i];
            buildPluginStaticData(pd, d[i]);
        }
    }
    
    static void
    buildVampRequest_Load(VampRequest::Builder &b,
                          const Vamp::HostExt::LoadRequest &req) {
        auto u = b.getRequest().initLoad();
        buildLoadRequest(u, req);
    }

    static void
    buildVampResponse_Load(VampResponse::Builder &b,
                           const Vamp::HostExt::LoadResponse &resp,
                           const PluginHandleMapper &pmapper) {
        b.setSuccess(resp.plugin != 0);
        b.setErrorText("");
        auto u = b.getResponse().initLoad();
        buildLoadResponse(u, resp, pmapper);
    }

    static void
    buildVampRequest_Configure(VampRequest::Builder &b,
                               const Vamp::HostExt::ConfigurationRequest &cr,
                               const PluginHandleMapper &pmapper) {
        auto u = b.getRequest().initConfigure();
        buildConfigurationRequest(u, cr, pmapper);
    }

    static void
    buildVampResponse_Configure(VampResponse::Builder &b,
                                const Vamp::HostExt::ConfigurationResponse &cr) {
        b.setSuccess(!cr.outputs.empty());
        b.setErrorText("");
        auto u = b.getResponse().initConfigure();
        buildConfigurationResponse(u, cr);
    }
    
    static void
    buildVampRequest_Process(VampRequest::Builder &b,
                             const Vamp::HostExt::ProcessRequest &pr,
                             const PluginHandleMapper &pmapper) {
        auto u = b.getRequest().initProcess();
        buildProcessRequest(u, pr, pmapper);
    }
    
    static void
    buildVampResponse_Process(VampResponse::Builder &b,
                              const Vamp::HostExt::ProcessResponse &pr,
                              const PluginHandleMapper &pmapper) {
        b.setSuccess(true);
        b.setErrorText("");
        auto u = b.getResponse().initProcess();
        buildProcessResponse(u, pr, pmapper);
    }
    
    static void
    buildVampRequest_Finish(VampRequest::Builder &b,
                            Vamp::Plugin *p,
                            const PluginHandleMapper &pmapper) {

        auto u = b.getRequest().initFinish();
        u.setPluginHandle(pmapper.pluginToHandle(p));
    }
    
    static void
    buildVampResponse_Finish(VampResponse::Builder &b,
                             const Vamp::HostExt::ProcessResponse &pr,
                             const PluginHandleMapper &pmapper) {

        buildVampResponse_Process(b, pr, pmapper);
    }

    static RRType
    getRequestResponseType(const VampRequest::Reader &r) {
        switch (r.getRequest().which()) {
        case VampRequest::Request::Which::LIST:
            return RRType::List;
        case VampRequest::Request::Which::LOAD:
            return RRType::Load;
        case VampRequest::Request::Which::CONFIGURE:
            return RRType::Configure;
        case VampRequest::Request::Which::PROCESS:
            return RRType::Process;
        case VampRequest::Request::Which::FINISH:
            return RRType::Finish;
        }
        return RRType::NotValid;
    }

    static RRType
    getRequestResponseType(const VampResponse::Reader &r) {
        switch (r.getResponse().which()) {
        case VampResponse::Response::Which::LIST:
            return RRType::List;
        case VampResponse::Response::Which::LOAD:
            return RRType::Load;
        case VampResponse::Response::Which::CONFIGURE:
            return RRType::Configure;
        case VampResponse::Response::Which::PROCESS:
            return RRType::Process;
        case VampResponse::Response::Which::FINISH:
            return RRType::Finish;
        }
        return RRType::NotValid;
    }
    
    static void
    readVampRequest_List(const VampRequest::Reader &r) {
        if (getRequestResponseType(r) != RRType::List) {
            throw std::logic_error("not a list request");
        }
    }

    static void
    readVampResponse_List(std::vector<Vamp::HostExt::PluginStaticData> &v,
                          const VampResponse::Reader &r) {
        if (getRequestResponseType(r) != RRType::List) {
            throw std::logic_error("not a list response");
        }
        v.clear();
        if (r.getSuccess()) {
            auto pp = r.getResponse().getList().getPlugins();
            for (const auto &p: pp) {
                Vamp::HostExt::PluginStaticData psd;
                readPluginStaticData(psd, p);
                v.push_back(psd);
            }
        }
    }
    
    static void
    readVampRequest_Load(Vamp::HostExt::LoadRequest &req,
                         const VampRequest::Reader &r) {
        if (getRequestResponseType(r) != RRType::Load) {
            throw std::logic_error("not a load request");
        }
        readLoadRequest(req, r.getRequest().getLoad());
    }

    static void
    readVampResponse_Load(Vamp::HostExt::LoadResponse &resp,
                          const VampResponse::Reader &r,
                          const PluginHandleMapper &pmapper) {
        if (getRequestResponseType(r) != RRType::Load) {
            throw std::logic_error("not a load response");
        }
        resp = {};
        if (r.getSuccess()) {
            readLoadResponse(resp, r.getResponse().getLoad(), pmapper);
        }
    }
    
    static void
    readVampRequest_Configure(Vamp::HostExt::ConfigurationRequest &req,
                              const VampRequest::Reader &r,
                              const PluginHandleMapper &pmapper) {
        if (getRequestResponseType(r) != RRType::Configure) {
            throw std::logic_error("not a configuration request");
        }
        readConfigurationRequest(req, r.getRequest().getConfigure(), pmapper);
    }

    static void
    readVampResponse_Configure(Vamp::HostExt::ConfigurationResponse &resp,
                               const VampResponse::Reader &r) {
        if (getRequestResponseType(r) != RRType::Configure) {
            throw std::logic_error("not a configuration response");
        }
        resp = {};
        if (r.getSuccess()) {
            readConfigurationResponse(resp, r.getResponse().getConfigure());
        }
    }
    
    static void
    readVampRequest_Process(Vamp::HostExt::ProcessRequest &req,
                            const VampRequest::Reader &r,
                            const PluginHandleMapper &pmapper) {
        if (getRequestResponseType(r) != RRType::Process) {
            throw std::logic_error("not a process request");
        }
        readProcessRequest(req, r.getRequest().getProcess(), pmapper);
    }

    static void
    readVampResponse_Process(Vamp::HostExt::ProcessResponse &resp,
                             const VampResponse::Reader &r,
                             const PluginHandleMapper &pmapper) {
        if (getRequestResponseType(r) != RRType::Process) {
            throw std::logic_error("not a process response");
        }
        resp = {};
        if (r.getSuccess()) {
            readProcessResponse(resp, r.getResponse().getProcess(), pmapper);
        }
    }
    
    static void
    readVampRequest_Finish(Vamp::Plugin *&finishPlugin,
                           const VampRequest::Reader &r,
                           const PluginHandleMapper &pmapper) {
        if (getRequestResponseType(r) != RRType::Finish) {
            throw std::logic_error("not a finish request");
        }
        finishPlugin = pmapper.handleToPlugin
            (r.getRequest().getFinish().getPluginHandle());
    }

    static void
    readVampResponse_Finish(Vamp::HostExt::ProcessResponse &resp,
                            const VampResponse::Reader &r,
                            const PluginHandleMapper &pmapper) {
        if (getRequestResponseType(r) != RRType::Finish) {
            throw std::logic_error("not a finish response");
        }
        resp = {};
        if (r.getSuccess()) {
            readProcessResponse(resp, r.getResponse().getFinish(), pmapper);
        }
    }
};

}


