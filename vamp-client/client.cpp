
#include "stub.h"

#include "vamp-capnp/VampnProto.h"

#include "vamp-support/AssignedPluginHandleMapper.h"

// First cut plan: this is to be client-qt.cpp, using a QProcess, so
// we're using pipes and the server is completely synchronous,
// handling only one call at once. Our PiperClient will fire off a
// QProcess and refer to its io device. Each request message is
// serialised into memory using capnp::MallocMessageBuilder and
// shunted into the process pipe; we then wait for some bytes to come
// back and use capnp::expectedSizeInWordsFromPrefix to work out when
// a whole message is available, reading only that amount from the
// device and using FlatArrayMessageReader to convert to a response
// message. If the response message's id does not match the request
// message's, then the server has gone wrong (it should never be
// servicing more than one request at a time).

// Next level: Capnp RPC, but I want to get the first level to work
// first.

namespace piper { //!!! probably something different

class PiperClient : public PiperClientBase
{
    // unsigned to avoid undefined behaviour on possible wrap
    typedef uint32_t ReqId;
    
public:

    PiperClient() { }

    Vamp::Plugin *
    load(std::string key, float inputSampleRate, int adapterFlags) {

        Vamp::HostExt::LoadRequest request;
        request.pluginKey = key;
        request.inputSampleRate = inputSampleRate;
        request.adapterFlags = adapterFlags;

        ::capnp::MallocMessageBuilder message;
        RpcRequest::Builder builder = message.initRoot<RpcRequest>();

        VampnProto::buildRpcRequest_Load(builder, request);
        ReqId id = getId();
        builder.getId().setNumber(id);
    };     
    
    virtual
    Vamp::Plugin::OutputList
    configure(PiperStubPlugin *plugin,
              Vamp::HostExt::PluginConfiguration config) {

        Vamp::HostExt::ConfigurationRequest request;
        request.plugin = plugin;
        request.configuration = config;

        ::capnp::MallocMessageBuilder message;
        RpcRequest::Builder builder = message.initRoot<RpcRequest>();

        VampnProto::buildRpcRequest_Configure(builder, request, m_mapper);
        ReqId id = getId();
        builder.getId().setNumber(id);

        //!!! now what?
    };
    
    
    virtual
    Vamp::Plugin::FeatureSet
    process(PiperStubPlugin *plugin,
            const float *const *inputBuffers,
            Vamp::RealTime timestamp) = 0;

    virtual Vamp::Plugin::FeatureSet
    finish(PiperStubPlugin *plugin) = 0;

private:
    AssignedPluginHandleMapper m_mapper;
    int getId() {
        //!!! todo: mutex
        static ReqId m_nextId = 0;
        return m_nextId++;
    }
};
    
}

