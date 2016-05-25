
#include "VampnProto.h"

#include "bits/RequestOrResponse.h"

#include <iostream>
#include <sstream>
#include <stdexcept>

#include <map>
#include <set>

using namespace std;
using namespace vampipe;
using namespace Vamp;
using namespace Vamp::HostExt;

void usage()
{
    string myname = "vampipe-server";
    cerr << "\n" << myname <<
	": Load and run Vamp plugins in response to messages from stdin\n\n"
	"    Usage: " << myname << "\n\n"
	"Expects Vamp request messages in Cap'n Proto packed format on stdin,\n"
	"and writes Vamp response messages in the same format to stdout.\n\n";

    exit(2);
}

class Mapper : public PluginHandleMapper
{
public:
    Mapper() : m_nextHandle(1) { }

    void addPlugin(Plugin *p) {
	if (m_rplugins.find(p) == m_rplugins.end()) {
	    int32_t h = m_nextHandle++;
	    m_plugins[h] = p;
	    m_rplugins[p] = h;
	}
    }

    void removePlugin(int32_t h) {
	if (m_plugins.find(h) == m_plugins.end()) {
	    throw NotFound();
	}
	Plugin *p = m_plugins[h];
	m_plugins.erase(h);
	m_rplugins.erase(p);
    }
    
    int32_t pluginToHandle(Plugin *p) {
	if (m_rplugins.find(p) == m_rplugins.end()) {
	    throw NotFound();
	}
	return m_rplugins[p];
    }
    
    Plugin *handleToPlugin(int32_t h) {
	if (m_plugins.find(h) == m_plugins.end()) {
	    throw NotFound();
	}
	return m_plugins[h];
    }

    bool isConfigured(int32_t h) {
	return m_configuredPlugins.find(h) != m_configuredPlugins.end();
    }

    void markConfigured(int32_t h) {
	m_configuredPlugins.insert(h);
    }
    
private:
    int32_t m_nextHandle; // NB plugin handle type must fit in JSON number
    map<uint32_t, Plugin *> m_plugins;
    map<Plugin *, uint32_t> m_rplugins;
    set<uint32_t> m_configuredPlugins;
};

static Mapper mapper;

RequestOrResponse
readRequestCapnp()
{
    RequestOrResponse rr;
    rr.direction = RequestOrResponse::Request;

    static kj::FdInputStream stream(0); // stdin
    static kj::BufferedInputStreamWrapper buffered(stream);

    if (buffered.tryGetReadBuffer() == nullptr) {
	rr.type = RRType::NotValid;
	return rr;
    }

    ::capnp::InputStreamMessageReader message(buffered);
    VampRequest::Reader reader = message.getRoot<VampRequest>();
    
    rr.type = VampnProto::getRequestResponseType(reader);

    switch (rr.type) {

    case RRType::List:
	VampnProto::readVampRequest_List(reader); // type check only
	break;
    case RRType::Load:
	VampnProto::readVampRequest_Load(rr.loadRequest, reader);
	break;
    case RRType::Configure:
	VampnProto::readVampRequest_Configure(rr.configurationRequest,
					      reader, mapper);
	break;
    case RRType::Process:
	VampnProto::readVampRequest_Process(rr.processRequest, reader, mapper);
	break;
    case RRType::Finish:
	VampnProto::readVampRequest_Finish(rr.finishPlugin, reader, mapper);
	break;
    case RRType::NotValid:
	break;
    }

    return rr;
}

void
writeResponseCapnp(RequestOrResponse &rr)
{
    ::capnp::MallocMessageBuilder message;
    VampResponse::Builder builder = message.initRoot<VampResponse>();

    switch (rr.type) {

    case RRType::List:
	VampnProto::buildVampResponse_List(builder, "", rr.listResponse);
	break;
    case RRType::Load:
	VampnProto::buildVampResponse_Load(builder, rr.loadResponse, mapper);
	break;
    case RRType::Configure:
	VampnProto::buildVampResponse_Configure(builder, rr.configurationResponse);
	break;
    case RRType::Process:
	VampnProto::buildVampResponse_Process(builder, rr.processResponse);
	break;
    case RRType::Finish:
	VampnProto::buildVampResponse_Finish(builder, rr.finishResponse);
	break;
    case RRType::NotValid:
	break;
    }

    writeMessageToFd(1, message);
}

RequestOrResponse
processRequest(const RequestOrResponse &request)
{
    RequestOrResponse response;
    response.direction = RequestOrResponse::Response;
    response.type = request.type;

    auto loader = PluginLoader::getInstance();

    switch (request.type) {

    case RRType::List:
	response.listResponse = loader->listPluginData();
	response.success = true;
	break;

    case RRType::Load:
	response.loadResponse = loader->loadPlugin(request.loadRequest);
	if (response.loadResponse.plugin != nullptr) {
	    mapper.addPlugin(response.loadResponse.plugin);
	    response.success = true;
	}
	break;
	
    case RRType::Configure:
    {
	auto h = mapper.pluginToHandle(request.configurationRequest.plugin);
	if (mapper.isConfigured(h)) {
	    throw runtime_error("plugin has already been configured");
	}

	response.configurationResponse =
	    loader->configurePlugin(request.configurationRequest);
	
	if (!response.configurationResponse.outputs.empty()) {
	    mapper.markConfigured(h);
	    response.success = true;
	}
	break;
    }

    case RRType::Process:
    {
	auto &preq = request.processRequest;
	int channels = int(preq.inputBuffers.size());
	const float **fbuffers = new const float *[channels];
	for (int i = 0; i < channels; ++i) {
	    fbuffers[i] = preq.inputBuffers[i].data();
	}

	response.processResponse.features =
	    preq.plugin->process(fbuffers, preq.timestamp);
	response.success = true;

	delete[] fbuffers;
	break;
    }

    case RRType::Finish:
    {
	auto h = mapper.pluginToHandle(request.finishPlugin);

	response.finishResponse.features =
	    request.finishPlugin->getRemainingFeatures();
	    
	mapper.removePlugin(h);
	delete request.finishPlugin;
	response.success = true;
	break;
    }

    case RRType::NotValid:
	break;
    }
    
    return response;
}

int main(int argc, char **argv)
{
    if (argc != 1) {
	usage();
    }

    while (true) {

	try {

	    RequestOrResponse request = readRequestCapnp();

	    cerr << "vampipe-server: request received, of type "
		 << int(request.type)
		 << endl;
	    
	    // NotValid without an exception indicates EOF:
	    if (request.type == RRType::NotValid) {
		cerr << "vampipe-server: eof reached" << endl;
		break;
	    }

	    RequestOrResponse response = processRequest(request);

	    cerr << "vampipe-server: request processed, writing response"
		 << endl;
	    
	    writeResponseCapnp(response);

	    cerr << "vampipe-server: response written" << endl;
	    
	} catch (std::exception &e) {
	    cerr << "vampipe-server: error: " << e.what() << endl;
	    exit(1);
	}
    }

    exit(0);
}
