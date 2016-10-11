
#include <vamp-hostsdk/Plugin.h>
#include <vamp-hostsdk/PluginLoader.h>
#include <vamp-hostsdk/PluginStaticData.h>
#include <vamp-hostsdk/PluginConfiguration.h>

#include <cstdint>

namespace piper { //!!! should be something else

typedef int32_t PiperPluginHandle;

class PiperClientBase
{
public:
    virtual
    Vamp::Plugin::OutputList
    configure(PiperPluginHandle handle,
              Vamp::HostExt::PluginConfiguration config) = 0;
    
    virtual
    Vamp::Plugin::FeatureSet
    process(PiperPluginHandle handle,
            const float *const *inputBuffers,
            Vamp::RealTime timestamp) = 0;

    virtual Vamp::Plugin::FeatureSet
    finish(PiperPluginHandle handle) = 0;
};

class PiperStubPlugin : public Vamp::Plugin
{
    enum State {
        Loaded, Configured, Finished
    };
    
public:
    PiperStubPlugin(PiperClientBase *client,
                    PiperPluginHandle handle,
                    float inputSampleRate,
                    Vamp::HostExt::PluginStaticData psd,
                    Vamp::HostExt::PluginConfiguration defaultConfig) :
        Plugin(inputSampleRate),
        m_client(client),
        m_handle(handle),
        m_state(Loaded),
        m_psd(psd),
        m_defaultConfig(defaultConfig),
        m_config(defaultConfig)
    { }

    virtual ~PiperStubPlugin() {
        if (m_state != Finished) {
            (void)m_client->finish(m_handle);
            m_state = Finished;
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

        m_outputs = m_client->configure(m_handle, m_config);

        if (!m_outputs.empty()) {
            m_state = Configured;
            return true;
        } else {
            return false;
        }
    }

    virtual void reset() {
        //!!! hm, how to deal with this? there is no reset() in Piper!
        throw "Please do not call this function again.";
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

        return m_client->process(m_handle, inputBuffers, timestamp);
    }

    virtual FeatureSet getRemainingFeatures() {

        if (m_state == Loaded) {
            throw std::logic_error("Plugin has not been configured");
        }
        if (m_state == Finished) {
            throw std::logic_error("Plugin has already been disposed of");
        }

        m_state = Finished;

        return m_client->finish(m_handle);
    }
    
private:
    PiperClientBase *m_client;
    PiperPluginHandle m_handle;
    State m_state;
    Vamp::HostExt::PluginStaticData m_psd;
    OutputList m_outputs;
    Vamp::HostExt::PluginConfiguration m_defaultConfig;
    Vamp::HostExt::PluginConfiguration m_config;
};


}

