/* -*- c-basic-offset: 4 indent-tabs-mode: nil -*-  vi:set ts=8 sts=4 sw=4: */

#ifndef PIPER_CAPNP_CLIENT_H
#define PIPER_CAPNP_CLIENT_H

#include "Loader.h"
#include "PluginClient.h"
#include "PluginStub.h"
#include "SynchronousTransport.h"

#include "vamp-support/AssignedPluginHandleMapper.h"
#include "vamp-capnp/VampnProto.h"

#include <capnp/serialize.h>

namespace piper_vamp {
namespace client {

/**
 * Client for a request-response Piper server, i.e. using the
 * RpcRequest/RpcResponse structures with a single process call rather
 * than having individual RPC methods, with a synchronous transport
 * such as a subprocess pipe arrangement. Only one request can be
 * handled at a time. This class is thread-safe if and only if it is
 * constructed with a thread-safe SynchronousTransport implementation.
 */
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

    ListResponse
    listPluginData() override {

        if (!m_transport->isOK()) {
            throw std::runtime_error("Piper server failed to start");
        }

        capnp::MallocMessageBuilder message;
	piper::RpcRequest::Builder builder = message.initRoot<piper::RpcRequest>();
        VampnProto::buildRpcRequest_List(builder);
        ReqId id = getId();
        builder.getId().setNumber(id);

	auto karr = call(message);

        capnp::FlatArrayMessageReader responseMessage(karr);
        piper::RpcResponse::Reader reader = responseMessage.getRoot<piper::RpcResponse>();

        checkResponseType(reader, piper::RpcResponse::Response::Which::LIST, id);

        ListResponse lr;
        VampnProto::readListResponse(lr, reader.getResponse().getList());
        return lr;
    }
    
    LoadResponse
    loadPlugin(const LoadRequest &req) override {

        if (!m_transport->isOK()) {
            throw std::runtime_error("Piper server failed to start");
        }

        LoadResponse resp;
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
              PluginConfiguration config) override {

        if (!m_transport->isOK()) {
            throw std::runtime_error("Piper server failed to start");
        }

        ConfigurationRequest request;
        request.plugin = plugin;
        request.configuration = config;

        capnp::MallocMessageBuilder message;
        piper::RpcRequest::Builder builder = message.initRoot<piper::RpcRequest>();

        VampnProto::buildRpcRequest_Configure(builder, request, m_mapper);
        ReqId id = getId();
        builder.getId().setNumber(id);

	auto karr = call(message);

        capnp::FlatArrayMessageReader responseMessage(karr);
        piper::RpcResponse::Reader reader = responseMessage.getRoot<piper::RpcResponse>();

        //!!! handle (explicit) error case

        checkResponseType(reader, piper::RpcResponse::Response::Which::CONFIGURE, id);

        ConfigurationResponse cr;
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

        ProcessRequest request;
        request.plugin = plugin;
        request.inputBuffers = inputBuffers;
        request.timestamp = timestamp;
        
        capnp::MallocMessageBuilder message;
        piper::RpcRequest::Builder builder = message.initRoot<piper::RpcRequest>();
        VampnProto::buildRpcRequest_Process(builder, request, m_mapper);
	ReqId id = getId();
        builder.getId().setNumber(id);

	auto karr = call(message);

        capnp::FlatArrayMessageReader responseMessage(karr);
        piper::RpcResponse::Reader reader = responseMessage.getRoot<piper::RpcResponse>();

        //!!! handle (explicit) error case

        checkResponseType(reader, piper::RpcResponse::Response::Which::PROCESS, id);

        ProcessResponse pr;
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

        FinishRequest request;
        request.plugin = plugin;
        
        capnp::MallocMessageBuilder message;
        piper::RpcRequest::Builder builder = message.initRoot<piper::RpcRequest>();

        VampnProto::buildRpcRequest_Finish(builder, request, m_mapper);
        ReqId id = getId();
        builder.getId().setNumber(id);
        
	auto karr = call(message);

        capnp::FlatArrayMessageReader responseMessage(karr);
        piper::RpcResponse::Reader reader = responseMessage.getRoot<piper::RpcResponse>();

        //!!! handle (explicit) error case

        checkResponseType(reader, piper::RpcResponse::Response::Which::FINISH, id);

        FinishResponse pr;
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
          PluginConfiguration config) override {

        // Reload the plugin on the server side, and configure it as requested
        
        if (!m_transport->isOK()) {
            throw std::runtime_error("Piper server failed to start");
        }

        if (m_mapper.havePlugin(plugin)) {
            (void)finish(plugin); // server-side unload
        }

        PluginStaticData psd;
        PluginConfiguration defaultConfig;
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
    checkResponseType(const piper::RpcResponse::Reader &r,
                      piper::RpcResponse::Response::Which type,
                      ReqId id) {
        
        if (r.getResponse().which() != type) {
	    std::cerr << "checkResponseType: wrong response type (received "
		      << int(r.getResponse().which()) << ", expected "
		      << int(type) << ")"
		      << std::endl;
            throw std::runtime_error("Wrong response type");
        }
        if (ReqId(r.getId().getNumber()) != id) {
	    std::cerr << "checkResponseType: wrong response id (received "
		      << r.getId().getNumber() << ", expected " << id << ")"
		      << std::endl;
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
               PluginStaticData &psd,
               PluginConfiguration &defaultConfig) {

        LoadRequest request;
        request.pluginKey = key;
        request.inputSampleRate = inputSampleRate;
        request.adapterFlags = adapterFlags;

        capnp::MallocMessageBuilder message;
        piper::RpcRequest::Builder builder = message.initRoot<piper::RpcRequest>();

        VampnProto::buildRpcRequest_Load(builder, request);
        ReqId id = getId();
        builder.getId().setNumber(id);

	auto karr = call(message);

        //!!! ... --> will also need some way to kill this process
        //!!! (from another thread)

        capnp::FlatArrayMessageReader responseMessage(karr);
        piper::RpcResponse::Reader reader = responseMessage.getRoot<piper::RpcResponse>();

        //!!! handle (explicit) error case

        checkResponseType(reader, piper::RpcResponse::Response::Which::LOAD, id);
        
        const piper::LoadResponse::Reader &lr = reader.getResponse().getLoad();
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
