
#ifndef PIPER_AUTO_PLUGIN_H
#define PIPER_AUTO_PLUGIN_H

#include "ProcessQtTransport.h"
#include "CapnpRRClient.h"

#include <cstdint>

namespace piper_vamp {
namespace client {

class AutoPlugin : public Vamp::Plugin
{
public:
    AutoPlugin(std::string pluginKey,
	       float inputSampleRate,
	       int adapterFlags) :
	Vamp::Plugin(inputSampleRate),
	m_transport("../bin/piper-vamp-server"), //!!!*£*$&"$*"
	m_client(&m_transport)
    {
	LoadRequest req;
	req.pluginKey = pluginKey;
	req.inputSampleRate = inputSampleRate;
	req.adapterFlags = adapterFlags;
	LoadResponse resp = m_client.loadPlugin(req);
	m_plugin = resp.plugin;
    }

    virtual ~AutoPlugin() {
	delete m_plugin;
    }

    bool isOK() const {
	return (m_plugin != nullptr);
    }
    
    virtual std::string getIdentifier() const {
	return getPlugin()->getIdentifier();
    }

    virtual std::string getName() const {
	return getPlugin()->getName();
    }

    virtual std::string getDescription() const {
	return getPlugin()->getDescription();
    }

    virtual std::string getMaker() const {
	return getPlugin()->getMaker();
    }

    virtual std::string getCopyright() const {
	return getPlugin()->getCopyright();
    }

    virtual int getPluginVersion() const {
        return getPlugin()->getPluginVersion();
    }

    virtual ParameterList getParameterDescriptors() const {
	return getPlugin()->getParameterDescriptors();
    }

    virtual float getParameter(std::string name) const {
	return getPlugin()->getParameter(name);
    }

    virtual void setParameter(std::string name, float value) {
	getPlugin()->setParameter(name, value);
    }

    virtual ProgramList getPrograms() const {
        return getPlugin()->getPrograms();
    }

    virtual std::string getCurrentProgram() const {
        return getPlugin()->getCurrentProgram();
    }
    
    virtual void selectProgram(std::string program) {
	getPlugin()->selectProgram(program);
    }

    virtual bool initialise(size_t inputChannels,
                            size_t stepSize,
                            size_t blockSize) {
	return getPlugin()->initialise(inputChannels, stepSize, blockSize);
    }

    virtual void reset() {
	getPlugin()->reset();
    }

    virtual InputDomain getInputDomain() const {
	return getPlugin()->getInputDomain();
    }

    virtual size_t getPreferredBlockSize() const {
	return getPlugin()->getPreferredBlockSize();
    }

    virtual size_t getPreferredStepSize() const {
	return getPlugin()->getPreferredStepSize();
    }

    virtual size_t getMinChannelCount() const {
	return getPlugin()->getMinChannelCount();
    }

    virtual size_t getMaxChannelCount() const {
	return getPlugin()->getMaxChannelCount();
    }

    virtual OutputList getOutputDescriptors() const {
	return getPlugin()->getOutputDescriptors();
    }

    virtual FeatureSet process(const float *const *inputBuffers,
			       Vamp::RealTime timestamp) {
	return getPlugin()->process(inputBuffers, timestamp);
    }

    virtual FeatureSet getRemainingFeatures() {
	return getPlugin()->getRemainingFeatures();
    }

private:
    ProcessQtTransport m_transport;
    CapnpRRClient m_client;
    Vamp::Plugin *m_plugin;
    Vamp::Plugin *getPlugin() const {
	if (!m_plugin) {
	    throw std::logic_error
		("Plugin load failed (should have called AutoPlugin::isOK)");
	}
	return m_plugin;
    }
};

}
}

#endif

    
