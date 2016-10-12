
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

class PiperClient : public PiperClientStubRequirements
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
		m_process->closeWriteChannel();
                m_process->waitForFinished(200);
                m_process->close();
                m_process->waitForFinished();
		cerr << "server exited" << endl;
            }
            delete m_process;
        }
    }

    //!!! obviously, factor out all repetitive guff

    //!!! list and load are supposed to be called by application code,
    //!!! but the rest are only supposed to be called by the plugin --
    //!!! sort out the api here
    
    Vamp::Plugin *
    load(std::string key, float inputSampleRate, int adapterFlags) {

        if (!m_process) {
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
        m_process->write(arr.asChars().begin(), arr.asChars().size());

        //!!! ... --> will also need some way to kill this process
        //!!! (from another thread)

        QByteArray buffer = readResponseBuffer();
	auto karr = toKJArray(buffer);
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

        capnp::MallocMessageBuilder message;
        RpcRequest::Builder builder = message.initRoot<RpcRequest>();

        VampnProto::buildRpcRequest_Configure(builder, request, m_mapper);
        ReqId id = getId();
        builder.getId().setNumber(id);
        
        auto arr = messageToFlatArray(message);
        m_process->write(arr.asChars().begin(), arr.asChars().size());
        
        QByteArray buffer = readResponseBuffer();
	auto karr = toKJArray(buffer);
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
            Vamp::RealTime timestamp) {

        if (!m_process) {
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
        m_process->write(arr.asChars().begin(), arr.asChars().size());
        
        QByteArray buffer = readResponseBuffer();
	auto karr = toKJArray(buffer);
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
    finish(PiperStubPlugin *plugin) {

        if (!m_process) {
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
        m_process->write(arr.asChars().begin(), arr.asChars().size());
        
        QByteArray buffer = readResponseBuffer();
	auto karr = toKJArray(buffer);
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
    QProcess *m_process;
    AssignedPluginHandleMapper m_mapper;
    ReqId getId() {
        //!!! todo: mutex
        static ReqId m_nextId = 0;
        return m_nextId++;
    }

    kj::Array<capnp::word>
    toKJArray(QByteArray qarr) {
	// We could do this whole thing with fewer copies, but let's
	// see whether it matters first
        size_t wordSize = sizeof(capnp::word);
	size_t words = qarr.size() / wordSize;
	kj::Array<capnp::word> karr(kj::heapArray<capnp::word>(words));
	memcpy(karr.begin(), qarr.data(), words * wordSize);
	return karr;
    }

    QByteArray
    readResponseBuffer() { 
        
        QByteArray buffer;
        size_t wordSize = sizeof(capnp::word);
        bool complete = false;
        
        while (!complete) {

            m_process->waitForReadyRead(1000);
            qint64 byteCount = m_process->bytesAvailable();
            qint64 wordCount = byteCount / wordSize;

            if (!wordCount) {
                if (m_process->state() == QProcess::NotRunning) {
                    cerr << "ERROR: Subprocess exited: Load failed" << endl;
                    throw std::runtime_error("Piper server exited unexpectedly");
                }
            } else {
                buffer.append(m_process->read(wordCount * wordSize));
                size_t haveWords = buffer.size() / wordSize;
                size_t expectedWords =
                    capnp::expectedSizeInWordsFromPrefix(toKJArray(buffer));
                if (haveWords >= expectedWords) {
                    if (haveWords > expectedWords) {
                        cerr << "WARNING: obtained more data than expected ("
                             << haveWords << " words, expected " << expectedWords
                             << ")" << endl;
                    }
                    complete = true;
                }
            }
        }
/*
        cerr << "buffer = ";
        for (int i = 0; i < buffer.size(); ++i) {
            if (i % 16 == 0) cerr << "\n";
            cerr << int(buffer[i]) << " ";
        }
        cerr << "\n";
*/        
        return buffer;
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
};
    
}

int main(int, char **)
{
    piper::PiperClient client;
    Vamp::Plugin *plugin = client.load("vamp-example-plugins:zerocrossing", 16, 0);
    if (!plugin->initialise(1, 4, 4)) {
        cerr << "initialisation failed" << endl;
    } else {
        std::vector<float> buf = { 1.0, -1.0, 1.0, -1.0 };
        float *bd = buf.data();
        Vamp::Plugin::FeatureSet features = plugin->process
            (&bd, Vamp::RealTime::zeroTime);
        cerr << "results for output 0:" << endl;
        auto fl(features[0]);
        for (const auto &f: fl) {
            cerr << f.values[0] << endl;
        }
    }

    (void)plugin->getRemainingFeatures();

    cerr << "calling reset..." << endl;
    plugin->reset();
    cerr << "...round 2!" << endl;

    std::vector<float> buf = { 1.0, -1.0, 1.0, -1.0 };
    float *bd = buf.data();
    Vamp::Plugin::FeatureSet features = plugin->process
	(&bd, Vamp::RealTime::zeroTime);
    cerr << "results for output 0:" << endl;
    auto fl(features[0]);
    for (const auto &f: fl) {
	cerr << f.values[0] << endl;
    }
    
    (void)plugin->getRemainingFeatures();

    delete plugin;
    //!!! -- and also implement reset(), which will need to reconstruct internally
}

