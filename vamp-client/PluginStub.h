
#ifndef PIPER_PLUGIN_STUB_H
#define PIPER_PLUGIN_STUB_H

#include <vamp-hostsdk/Plugin.h>
#include <vamp-hostsdk/PluginLoader.h>

#include "vamp-support/PluginStaticData.h"
#include "vamp-support/PluginConfiguration.h"

#include <cstdint>

#include "PluginClient.h"

namespace piper_vamp {
namespace client {

class PluginStub : public Vamp::Plugin
{
    enum State {
        Loaded, Configured, Finished
    };
    
public:
    PluginStub(PluginClient *client,
               std::string pluginKey,
               float inputSampleRate,
               int adapterFlags,
               PluginStaticData psd,
               PluginConfiguration defaultConfig) :
        Plugin(inputSampleRate),
        m_client(client),
        m_key(pluginKey),
        m_adapterFlags(adapterFlags),
        m_state(Loaded),
        m_psd(psd),
        m_defaultConfig(defaultConfig),
        m_config(defaultConfig)
    { }

    virtual ~PluginStub() {
        if (m_state != Finished) {
	    (void)m_client->finish(this);
        }
    }
    
    virtual std::string getIdentifier() const {
        return m_psd.basic.identifier;
    }

    virtual std::string getName() const {
        return m_psd.basic.name;
    }

    virtual std::string getDescription() const {
        return m_psd.basic.description;
    }

    virtual std::string getMaker() const {
        return m_psd.maker;
    }

    virtual std::string getCopyright() const {
        return m_psd.copyright;
    }

    virtual int getPluginVersion() const {
        return m_psd.pluginVersion;
    }

    virtual ParameterList getParameterDescriptors() const {
        return m_psd.parameters;
    }

    virtual float getParameter(std::string name) const {
        if (m_config.parameterValues.find(name) != m_config.parameterValues.end()) {
            return m_config.parameterValues.at(name);
        } else {
            return 0.f;
        }
    }

    virtual void setParameter(std::string name, float value) {
        if (m_state != Loaded) {
            throw std::logic_error("Can't set parameter after plugin initialised");
        }
        m_config.parameterValues[name] = value;
    }

    virtual ProgramList getPrograms() const {
        return m_psd.programs;
    }

    virtual std::string getCurrentProgram() const {
        return m_config.currentProgram;
    }
    
    virtual void selectProgram(std::string program) {
        if (m_state != Loaded) {
            throw std::logic_error("Can't select program after plugin initialised");
        }
        m_config.currentProgram = program;
    }

    virtual bool initialise(size_t inputChannels,
                            size_t stepSize,
                            size_t blockSize) {

        if (m_state != Loaded) {
            throw std::logic_error("Plugin has already been initialised");
        }
        
        m_config.channelCount = inputChannels;
        m_config.stepSize = stepSize;
        m_config.blockSize = blockSize;

        m_outputs = m_client->configure(this, m_config);

        if (!m_outputs.empty()) {
            m_state = Configured;
            return true;
        } else {
            return false;
        }
    }

    virtual void reset() {
        
        if (m_state == Loaded) {
            // reset is a no-op if the plugin hasn't been initialised yet
            return;
        }
        
        m_client->reset(this, m_config);

        m_state = Configured;
    }

    virtual InputDomain getInputDomain() const {
        return m_psd.inputDomain;
    }

    virtual size_t getPreferredBlockSize() const {
        return m_defaultConfig.blockSize;
    }

    virtual size_t getPreferredStepSize() const {
        return m_defaultConfig.stepSize;
    }

    virtual size_t getMinChannelCount() const {
        return m_psd.minChannelCount;
    }

    virtual size_t getMaxChannelCount() const {
        return m_psd.maxChannelCount;
    }

    virtual OutputList getOutputDescriptors() const {
        if (m_state == Configured) {
            return m_outputs;
        }

        //!!! todo: figure out for which hosts (and adapters?) it may
        //!!! be a problem that the output descriptors are incomplete
        //!!! here. Any such hosts/adapters are broken, but I bet they
        //!!! exist
        
        OutputList staticOutputs;
        for (const auto &o: m_psd.basicOutputInfo) {
            OutputDescriptor od;
            od.identifier = o.identifier;
            od.name = o.name;
            od.description = o.description;
            staticOutputs.push_back(od);
        }
        return staticOutputs;
    }

    virtual FeatureSet process(const float *const *inputBuffers,
			       Vamp::RealTime timestamp) {

        if (m_state == Loaded) {
            throw std::logic_error("Plugin has not been initialised");
        }
        if (m_state == Finished) {
            throw std::logic_error("Plugin has already been disposed of");
        }

        //!!! ew
        std::vector<std::vector<float> > vecbuf;
        for (int c = 0; c < m_config.channelCount; ++c) {
            vecbuf.push_back(std::vector<float>
                             (inputBuffers[c],
                              inputBuffers[c] + m_config.blockSize));
        }
        
        return m_client->process(this, vecbuf, timestamp);
    }

    virtual FeatureSet getRemainingFeatures() {

        if (m_state == Loaded) {
            throw std::logic_error("Plugin has not been configured");
        }
        if (m_state == Finished) {
            throw std::logic_error("Plugin has already been disposed of");
        }

        m_state = Finished;

        return m_client->finish(this);
    }

    // Not Plugin methods, but needed by the PluginClient to support reloads:
    
    virtual float getInputSampleRate() const {
        return m_inputSampleRate;
    }

    virtual std::string getPluginKey() const {
        return m_key;
    }

    virtual int getAdapterFlags() const {
        return m_adapterFlags;
    }
    
private:
    PluginClient *m_client;
    std::string m_key;
    int m_adapterFlags;
    State m_state;
    PluginStaticData m_psd;
    OutputList m_outputs;
    PluginConfiguration m_defaultConfig;
    PluginConfiguration m_config;
};

}
}

#endif
