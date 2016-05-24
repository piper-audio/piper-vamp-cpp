
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

    bool isInitialised(int32_t h) {
	return m_initialisedPlugins.find(h) != m_initialisedPlugins.end();
    }

    void markInitialised(int32_t h) {
	m_initialisedPlugins.insert(h);
    }
    
private:
    int32_t m_nextHandle; // NB plugin handle type must fit in JSON number
    map<uint32_t, Plugin *> m_plugins;
    map<Plugin *, uint32_t> m_rplugins;
    set<uint32_t> m_initialisedPlugins;
};

static Mapper mapper;

RequestOrResponse
readRequestCapnp()
{
    RequestOrResponse rr;
    rr.direction = RequestOrResponse::Request;

    ::capnp::PackedFdMessageReader message(0); // stdin
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

    writePackedMessageToFd(1, message);
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
	
    default:
	//!!!
	;
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

	    // NotValid without an exception indicates EOF:

	    //!!! not yet it doesn't -- have to figure out how to
	    //!!! handle this with capnp
	    if (request.type == RRType::NotValid) break;

	    RequestOrResponse response = processRequest(request);
	    
	    writeResponseCapnp(response);
	    
	} catch (std::exception &e) {
	    cerr << "Error: " << e.what() << endl;
	    exit(1);
	}
    }

    exit(0);
}
