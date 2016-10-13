
#ifndef PIPER_CAPNP_CLIENT_H
#define PIPER_CAPNP_CLIENT_H

#include "Loader.h"
#include "PluginClient.h"
#include "PluginStub.h"
#include "SynchronousTransport.h"

#include "vamp-support/AssignedPluginHandleMapper.h"
#include "vamp-capnp/VampnProto.h"

#include <capnp/serialize.h>

namespace piper {
namespace vampclient {

class CapnpRRClient : public PluginClient,
                    public Loader
{
    // unsigned to avoid undefined behaviour on possible wrap
    typedef uint32_t ReqId;

    class CompletenessChecker : public MessageCompletenessChecker {
    public:
        bool isComplete(const std::vector<char> &message) const override {
            auto karr = toKJArray(message);
            size_t words = karr.size();
            size_t expected = capnp::expectedSizeInWordsFromPrefix(karr);
            if (words > expected) {
                std::cerr << "WARNING: obtained more data than expected ("
                          << words << " " << sizeof(capnp::word)
                          << "-byte words, expected "
                          << expected << ")" << std::endl;
            }
            return words >= expected;
        }
    };
    
public:
    CapnpRRClient(SynchronousTransport *transport) : //!!! ownership? shared ptr?
        m_transport(transport),
        m_completenessChecker(new CompletenessChecker) {
        transport->setCompletenessChecker(m_completenessChecker);
    }

    ~CapnpRRClient() {
        delete m_completenessChecker;
    }

    //!!! obviously, factor out all repetitive guff

    //!!! list and load are supposed to be called by application code,
    //!!! but the rest are only supposed to be called by the plugin --
    //!!! sort out the api here

    // Loader methods:

    Vamp::HostExt::ListResponse
    listPluginData() override {

        if (!m_transport->isOK()) {
            throw std::runtime_error("Piper server failed to start");
        }

        capnp::MallocMessageBuilder message;
        RpcRequest::Builder builder = message.initRoot<RpcRequest>();
        VampnProto::buildRpcRequest_List(builder);
        ReqId id = getId();
        builder.getId().setNumber(id);

	auto karr = call(message);

        capnp::FlatArrayMessageReader responseMessage(karr);
        RpcResponse::Reader reader = responseMessage.getRoot<RpcResponse>();

        checkResponseType(reader, RpcResponse::Response::Which::LIST, id);

        Vamp::HostExt::ListResponse lr;
        VampnProto::readListResponse(lr, reader.getResponse().getList());
        return lr;
    }
    
    Vamp::HostExt::LoadResponse
    loadPlugin(const Vamp::HostExt::LoadRequest &req) override {

        if (!m_transport->isOK()) {
            throw std::runtime_error("Piper server failed to start");
        }

        Vamp::HostExt::LoadResponse resp;
        PluginHandleMapper::Handle handle = serverLoad(req.pluginKey,
                                                       req.inputSampleRate,
                                                       req.adapterFlags,
                                                       resp.staticData,
                                                       resp.defaultConfiguration);

        Vamp::Plugin *plugin = new PluginStub(this,
                                              req.pluginKey,
                                              req.inputSampleRate,
                                              req.adapterFlags,
                                              resp.staticData,
                                              resp.defaultConfiguration);

        m_mapper.addPlugin(handle, plugin);

        resp.plugin = plugin;
        return resp;
    }

    // PluginClient methods:
    
    virtual
    Vamp::Plugin::OutputList
    configure(PluginStub *plugin,
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

	auto karr = call(message);

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
    process(PluginStub *plugin,
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

	auto karr = call(message);

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
    finish(PluginStub *plugin) override {

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
        
	auto karr = call(message);

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

    virtual void
    reset(PluginStub *plugin,
          Vamp::HostExt::PluginConfiguration config) override {

        // Reload the plugin on the server side, and configure it as requested
        
        if (!m_transport->isOK()) {
            throw std::runtime_error("Piper server failed to start");
        }

        if (m_mapper.havePlugin(plugin)) {
            (void)finish(plugin); // server-side unload
        }

        Vamp::HostExt::PluginStaticData psd;
        Vamp::HostExt::PluginConfiguration defaultConfig;
        PluginHandleMapper::Handle handle =
            serverLoad(plugin->getPluginKey(),
                       plugin->getInputSampleRate(),
                       plugin->getAdapterFlags(),
                       psd, defaultConfig);

        m_mapper.addPlugin(handle, plugin);

        (void)configure(plugin, config);
    }
    
private:
    AssignedPluginHandleMapper m_mapper;
    ReqId getId() {
        //!!! todo: mutex
        static ReqId m_nextId = 0;
        return m_nextId++;
    }

    static
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

    kj::Array<capnp::word>
    call(capnp::MallocMessageBuilder &message) {
        auto arr = capnp::messageToFlatArray(message);
        auto responseBuffer = m_transport->call(arr.asChars().begin(),
                                                arr.asChars().size());
	return toKJArray(responseBuffer);
    }
    
    PluginHandleMapper::Handle
    serverLoad(std::string key, float inputSampleRate, int adapterFlags,
               Vamp::HostExt::PluginStaticData &psd,
               Vamp::HostExt::PluginConfiguration &defaultConfig) {

        Vamp::HostExt::LoadRequest request;
        request.pluginKey = key;
        request.inputSampleRate = inputSampleRate;
        request.adapterFlags = adapterFlags;

        capnp::MallocMessageBuilder message;
        RpcRequest::Builder builder = message.initRoot<RpcRequest>();

        VampnProto::buildRpcRequest_Load(builder, request);
        ReqId id = getId();
        builder.getId().setNumber(id);

	auto karr = call(message);

        //!!! ... --> will also need some way to kill this process
        //!!! (from another thread)

        capnp::FlatArrayMessageReader responseMessage(karr);
        RpcResponse::Reader reader = responseMessage.getRoot<RpcResponse>();

        //!!! handle (explicit) error case

        checkResponseType(reader, RpcResponse::Response::Which::LOAD, id);
        
        const LoadResponse::Reader &lr = reader.getResponse().getLoad();
        VampnProto::readExtractorStaticData(psd, lr.getStaticData());
        VampnProto::readConfiguration(defaultConfig, lr.getDefaultConfiguration());
        return lr.getHandle();
    };     

private:
    SynchronousTransport *m_transport; //!!! I don't own this, but should I?
    CompletenessChecker *m_completenessChecker; // I own this
};

}
}

#endif
