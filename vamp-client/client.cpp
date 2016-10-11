
#include "stub.h"

#include "vamp-capnp/VampnProto.h"

#include "vamp-support/AssignedPluginHandleMapper.h"

#include <QProcess>

#include <stdexcept>

using std::cerr;
using std::endl;

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
// first, not least because the server already exists.

namespace piper { //!!! probably something different

class PiperClient : public PiperClientBase
{
    // unsigned to avoid undefined behaviour on possible wrap
    typedef uint32_t ReqId;
    
public:
    PiperClient() {
        m_process = new QProcess();
        m_process->setReadChannel(QProcess::StandardOutput);
        m_process->setProcessChannelMode(QProcess::ForwardedErrorChannel);
        m_process->start("../bin/piper-vamp-server"); //!!!
        if (!m_process->waitForStarted()) {
            cerr << "server failed to start" << endl;
            delete m_process;
            m_process = 0;
        }
    }

    ~PiperClient() {
        if (m_process) {
            if (m_process->state() != QProcess::NotRunning) {
                m_process->close();
                m_process->waitForFinished();
            }
            delete m_process;
        }
    }

    //!!! obviously, factor out all repetitive guff
    
    Vamp::Plugin *
    load(std::string key, float inputSampleRate, int adapterFlags) {

        if (!m_process) {
            throw std::runtime_error("Piper server failed to start");
        }

        Vamp::HostExt::LoadRequest request;
        request.pluginKey = key;
        request.inputSampleRate = inputSampleRate;
        request.adapterFlags = adapterFlags;

        ::capnp::MallocMessageBuilder message;
        RpcRequest::Builder builder = message.initRoot<RpcRequest>();

        VampnProto::buildRpcRequest_Load(builder, request);
        ReqId id = getId();
        builder.getId().setNumber(id);

        auto arr = messageToFlatArray(message);
        m_process->write(arr.asChars().begin(), arr.asChars().size());

        ///.... read...
    };     
    
    virtual
    Vamp::Plugin::OutputList
    configure(PiperStubPlugin *plugin,
              Vamp::HostExt::PluginConfiguration config) {

        if (!m_process) {
            throw std::runtime_error("Piper server failed to start");
        }

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
    QProcess *m_process;
    AssignedPluginHandleMapper m_mapper;
    int getId() {
        //!!! todo: mutex
        static ReqId m_nextId = 0;
        return m_nextId++;
    }
};
    
}

