#include "catch/catch.hpp"
#include "vamp-client/Loader.h"
#include "vamp-client/PluginClient.h"
#include "vamp-client/PluginStub.h"
#include "vamp-support/RequestResponse.h"
#include <vector>

using namespace piper_vamp;
using namespace piper_vamp::client;
using AudioBuffer = std::vector<std::vector<float>>;

// This stub mimicks the interaction with a Piper server
// Here we only need to implement the configure method 
// due to testing only the initialise implemention of PluginStub
class StubClient : public PluginClient
{
public:
    StubClient(PluginStaticData staticData) : m_staticData(staticData) {}
    
    ConfigurationResponse
    configure(PluginStub* plugin,
              PluginConfiguration config) override
    {
        const float scale = plugin->getParameter("framing-scale");
        ConfigurationResponse cr;
        cr.plugin = plugin;
        
        // we want to return different framing sizes
        // than config provides
        // there isn't really any need to be doing this with a plugin param
        cr.framing.blockSize = config.framing.stepSize * scale;
        cr.framing.stepSize = config.framing.blockSize * scale;
        
        
        // just return some outputs anyway 
        // to avoid a failure case we are not testing here.
        Vamp::Plugin::OutputDescriptor output;
        const auto basic = m_staticData.basicOutputInfo[0];
        output.identifier = basic.identifier;
        output.name = basic.name;
        output.description = basic.description;
        cr.outputs = {output};
        return cr;                   
    }
    
    Vamp::Plugin::FeatureSet
    process(PluginStub* /*plugin*/,
            AudioBuffer /*channels*/,
            Vamp::RealTime /*timestamp*/) override
    {
        return {};
    }
    
    Vamp::Plugin::FeatureSet
    finish(PluginStub* /*plugin*/) override
    {
        return {};
    }
    
    void
    reset(PluginStub* /*plugin*/, PluginConfiguration /*config*/) override
    {}
private:
    PluginStaticData m_staticData;
};


TEST_CASE("Init plugin with parameter dependent preferred framing sizes") {
    
    PluginConfiguration defaultConfig;
    defaultConfig.channelCount = 1;
    defaultConfig.parameterValues = {};
    defaultConfig.framing.blockSize = 1024;
    defaultConfig.framing.stepSize = 512;
    defaultConfig.parameterValues = {{"framing-scale", 1.0}};
    
    Vamp::PluginBase::ParameterDescriptor stubParam;
    stubParam.identifier = "framing-scale";
    stubParam.name = "Framing Scale Factor";
    stubParam.description = "Scales the preferred framing sizes";
    stubParam.maxValue = 2.0; 
    
    PluginStaticData staticData;
    staticData.pluginKey = "stub";
    staticData.basic = {"stub:param-init", "Stub", "Testing init"};
    staticData.maker = "Lucas Thompson";
    staticData.copyright = "GPL";
    staticData.pluginVersion = 1;
    staticData.category = {"Test"};
    staticData.minChannelCount = 1;
    staticData.maxChannelCount = 1;
    staticData.parameters = {stubParam};
    staticData.inputDomain = Vamp::Plugin::InputDomain::TimeDomain;
    staticData.basicOutputInfo = {{"output", "NA", "Not real"}};

    StubClient stub {staticData};
    
    PluginStub vampPiperAdapter {
        &stub, 
        "stub", // plugin key
        44100.0, // sample rate
        0, // adapter flags, don't care here
        staticData, 
        defaultConfig 
    };
    
    vampPiperAdapter.setParameter("framing-scale", 2.0);
    // setup
    REQUIRE( 
        vampPiperAdapter.initialise(
            1, 
            vampPiperAdapter.getPreferredStepSize(), 
            vampPiperAdapter.getPreferredBlockSize()
        ) == false 
    );
    REQUIRE( 
        vampPiperAdapter.initialise(
            1, 
            vampPiperAdapter.getPreferredStepSize(), 
            vampPiperAdapter.getPreferredBlockSize()
        )
    );   
}
