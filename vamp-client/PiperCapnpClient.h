
#ifndef PIPER_CAPNP_CLIENT_H
#define PIPER_CAPNP_CLIENT_H

#include "PiperClient.h"
#include "SynchronousTransport.h"

#include "vamp-support/AssignedPluginHandleMapper.h"
#include "vamp-capnp/VampnProto.h"

namespace piper { //!!! change

class PiperCapnpClient : public PiperStubPluginClientInterface
{
    // unsigned to avoid undefined behaviour on possible wrap
    typedef uint32_t ReqId;
    
public:
    PiperCapnpClient(SynchronousTransport *transport) : //!!! ownership? shared ptr?
        m_transport(transport) {
    }

    ~PiperCapnpClient() {
    }

    //!!! obviously, factor out all repetitive guff

    //!!! list and load are supposed to be called by application code,
    //!!! but the rest are only supposed to be called by the plugin --
    //!!! sort out the api here
    
    Vamp::Plugin *
    load(std::string key, float inputSampleRate, int adapterFlags) {

        if (!m_transport->isOK()) {
            throw std::runtime_error("Piper server failed to start");
        }

        Vamp::HostExt::LoadRequest request;
        request.pluginKey = key;
        request.inputSampleRate = inputSampleRate;
        request.adapterFlags = adapterFlags;

        capnp::MallocMessageBuilder message;
        RpcRequest::Builder builder = message.initRoot<RpcRequest>();

        VampnProto::buildRpcRequest_Load(builder, request);
        ReqId id = getId();
        builder.getId().setNumber(id);

        auto arr = messageToFlatArray(message);

        auto responseBuffer = m_transport->call(arr.asChars().begin(),
                                                arr.asChars().size());
        
        //!!! ... --> will also need some way to kill this process
        //!!! (from another thread)

	auto karr = toKJArray(responseBuffer);
        capnp::FlatArrayMessageReader responseMessage(karr);
        RpcResponse::Reader reader = responseMessage.getRoot<RpcResponse>();

        //!!! handle (explicit) error case

        checkResponseType(reader, RpcResponse::Response::Which::LOAD, id);
        
        const LoadResponse::Reader &lr = reader.getResponse().getLoad();

        Vamp::HostExt::PluginStaticData psd;
        Vamp::HostExt::PluginConfiguration defaultConfig;
        VampnProto::readExtractorStaticData(psd, lr.getStaticData());
        VampnProto::readConfiguration(defaultConfig, lr.getDefaultConfiguration());
        
        Vamp::Plugin *plugin = new PiperStubPlugin(this,
                                                   inputSampleRate,
                                                   psd,
                                                   defaultConfig);

        m_mapper.addPlugin(lr.getHandle(), plugin);

        return plugin;
    };     

protected:
    virtual
    Vamp::Plugin::OutputList
    configure(PiperStubPlugin *plugin,
              Vamp::HostExt::PluginConfiguration config) override {

        if (!m_transport->isOK()) {
            throw std::runtime_error("Piper server failed to start");
        }

        Vamp::HostExt::ConfigurationRequest request;
        request.plugin = plugin;
        request.configuration = config;

        capnp::MallocMessageBuilder message;
        RpcRequest::Builder builder = message.initRoot<RpcRequest>();

        VampnProto::buildRpcRequest_Configure(builder, request, m_mapper);
        ReqId id = getId();
        builder.getId().setNumber(id);
        
        auto arr = messageToFlatArray(message);
        auto responseBuffer = m_transport->call(arr.asChars().begin(),
                                                arr.asChars().size());
	auto karr = toKJArray(responseBuffer);
        capnp::FlatArrayMessageReader responseMessage(karr);
        RpcResponse::Reader reader = responseMessage.getRoot<RpcResponse>();

        //!!! handle (explicit) error case

        checkResponseType(reader, RpcResponse::Response::Which::CONFIGURE, id);

        Vamp::HostExt::ConfigurationResponse cr;
        VampnProto::readConfigurationResponse(cr,
                                              reader.getResponse().getConfigure(),
                                              m_mapper);

        return cr.outputs;
    };
    
    virtual
    Vamp::Plugin::FeatureSet
    process(PiperStubPlugin *plugin,
            std::vector<std::vector<float> > inputBuffers,
            Vamp::RealTime timestamp) override {

        if (!m_transport->isOK()) {
            throw std::runtime_error("Piper server failed to start");
        }

        Vamp::HostExt::ProcessRequest request;
        request.plugin = plugin;
        request.inputBuffers = inputBuffers;
        request.timestamp = timestamp;
        
        capnp::MallocMessageBuilder message;
        RpcRequest::Builder builder = message.initRoot<RpcRequest>();

        VampnProto::buildRpcRequest_Process(builder, request, m_mapper);
        ReqId id = getId();
        builder.getId().setNumber(id);
        
        auto arr = messageToFlatArray(message);
        auto responseBuffer = m_transport->call(arr.asChars().begin(),
                                                arr.asChars().size());
	auto karr = toKJArray(responseBuffer);
        capnp::FlatArrayMessageReader responseMessage(karr);
        RpcResponse::Reader reader = responseMessage.getRoot<RpcResponse>();

        //!!! handle (explicit) error case

        checkResponseType(reader, RpcResponse::Response::Which::PROCESS, id);

        Vamp::HostExt::ProcessResponse pr;
        VampnProto::readProcessResponse(pr,
                                        reader.getResponse().getProcess(),
                                        m_mapper);

        return pr.features;
    }

    virtual Vamp::Plugin::FeatureSet
    finish(PiperStubPlugin *plugin) override {

        if (!m_transport->isOK()) {
            throw std::runtime_error("Piper server failed to start");
        }

        Vamp::HostExt::FinishRequest request;
        request.plugin = plugin;
        
        capnp::MallocMessageBuilder message;
        RpcRequest::Builder builder = message.initRoot<RpcRequest>();

        VampnProto::buildRpcRequest_Finish(builder, request, m_mapper);
        ReqId id = getId();
        builder.getId().setNumber(id);
        
        auto arr = messageToFlatArray(message);
        auto responseBuffer = m_transport->call(arr.asChars().begin(),
                                                arr.asChars().size());
	auto karr = toKJArray(responseBuffer);
        capnp::FlatArrayMessageReader responseMessage(karr);
        RpcResponse::Reader reader = responseMessage.getRoot<RpcResponse>();

        //!!! handle (explicit) error case

        checkResponseType(reader, RpcResponse::Response::Which::FINISH, id);

        Vamp::HostExt::ProcessResponse pr;
        VampnProto::readFinishResponse(pr,
                                       reader.getResponse().getFinish(),
                                       m_mapper);

        m_mapper.removePlugin(m_mapper.pluginToHandle(plugin));

	// Don't delete the plugin. It's the plugin that is supposed
	// to be calling us here
        
        return pr.features;
    }

private:
    AssignedPluginHandleMapper m_mapper;
    ReqId getId() {
        //!!! todo: mutex
        static ReqId m_nextId = 0;
        return m_nextId++;
    }

    kj::Array<capnp::word>
    toKJArray(const std::vector<char> &buffer) {
	// We could do this whole thing with fewer copies, but let's
	// see whether it matters first
        size_t wordSize = sizeof(capnp::word);
	size_t words = buffer.size() / wordSize;
	kj::Array<capnp::word> karr(kj::heapArray<capnp::word>(words));
	memcpy(karr.begin(), buffer.data(), words * wordSize);
	return karr;
    }

    void
    checkResponseType(const RpcResponse::Reader &r,
                      RpcResponse::Response::Which type,
                      ReqId id) {
        
        if (r.getResponse().which() != type) {
            throw std::runtime_error("Wrong response type");
        }
        if (ReqId(r.getId().getNumber()) != id) {
            throw std::runtime_error("Wrong response id");
        }
    }

private:
    SynchronousTransport *m_transport; //!!! I don't own this, but should I?
};

}

#endif
