
#include "VampnProto.h"

#include "bits/RequestOrResponse.h"
#include "bits/CountingPluginHandleMapper.h"

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

static CountingPluginHandleMapper mapper;

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
	VampnProto::readVampRequest_Finish(rr.finishRequest, reader, mapper);
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

    if (!rr.success) {

	VampnProto::buildVampResponse_Error(builder, rr.errorText, rr.type);

    } else {
	
	switch (rr.type) {

	case RRType::List:
	    VampnProto::buildVampResponse_List(builder, rr.listResponse);
	    break;
	case RRType::Load:
	    VampnProto::buildVampResponse_Load(builder, rr.loadResponse, mapper);
	    break;
	case RRType::Configure:
	    VampnProto::buildVampResponse_Configure(builder, rr.configurationResponse, mapper);
	    break;
	case RRType::Process:
	    VampnProto::buildVampResponse_Process(builder, rr.processResponse, mapper);
	    break;
	case RRType::Finish:
	    VampnProto::buildVampResponse_Finish(builder, rr.finishResponse, mapper);
	    break;
	case RRType::NotValid:
	    break;
	}
    }
    
    writeMessageToFd(1, message);
}

void
writeExceptionCapnp(const std::exception &e, RRType type)
{
    ::capnp::MallocMessageBuilder message;
    VampResponse::Builder builder = message.initRoot<VampResponse>();
    VampnProto::buildVampResponse_Exception(builder, e, type);
    
    writeMessageToFd(1, message);
}

RequestOrResponse
handleRequest(const RequestOrResponse &request)
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
	auto &creq = request.configurationRequest;
	auto h = mapper.pluginToHandle(creq.plugin);
	if (mapper.isConfigured(h)) {
	    throw runtime_error("plugin has already been configured");
	}

	response.configurationResponse = loader->configurePlugin(creq);
	
	if (!response.configurationResponse.outputs.empty()) {
	    mapper.markConfigured
		(h, creq.configuration.channelCount, creq.configuration.blockSize);
	    response.success = true;
	}
	break;
    }

    case RRType::Process:
    {
	auto &preq = request.processRequest;
	auto h = mapper.pluginToHandle(preq.plugin);
	if (!mapper.isConfigured(h)) {
	    throw runtime_error("plugin has not been configured");
	}

	int channels = int(preq.inputBuffers.size());
	if (channels != mapper.getChannelCount(h)) {
	    throw runtime_error("wrong number of channels supplied to process");
	}
		
	const float **fbuffers = new const float *[channels];
	for (int i = 0; i < channels; ++i) {
	    if (int(preq.inputBuffers[i].size()) != mapper.getBlockSize(h)) {
		delete[] fbuffers;
		throw runtime_error("wrong block size supplied to process");
	    }
	    fbuffers[i] = preq.inputBuffers[i].data();
	}

	response.processResponse.plugin = preq.plugin;
	response.processResponse.features =
	    preq.plugin->process(fbuffers, preq.timestamp);
	response.success = true;

	delete[] fbuffers;
	break;
    }

    case RRType::Finish:
    {
	response.finishResponse.plugin = request.finishRequest.plugin;
	response.finishResponse.features =
	    request.finishRequest.plugin->getRemainingFeatures();

	// We do not delete the plugin here -- we need it in the
	// mapper when converting the features. It gets deleted by the
	// caller.
	
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

	RequestOrResponse request;
	
	try {

	    request = readRequestCapnp();

	    cerr << "vampipe-server: request received, of type "
		 << int(request.type)
		 << endl;
	    
	    // NotValid without an exception indicates EOF:
	    if (request.type == RRType::NotValid) {
		cerr << "vampipe-server: eof reached" << endl;
		break;
	    }

	    RequestOrResponse response = handleRequest(request);

	    cerr << "vampipe-server: request handled, writing response"
		 << endl;
	    
	    writeResponseCapnp(response);

	    cerr << "vampipe-server: response written" << endl;

	    if (request.type == RRType::Finish) {
		auto h = mapper.pluginToHandle(request.finishRequest.plugin);
		mapper.removePlugin(h);
		delete request.finishRequest.plugin;
	    }
	    
	} catch (std::exception &e) {

	    cerr << "vampipe-server: error: " << e.what() << endl;

	    writeExceptionCapnp(e, request.type);
	    
	    exit(1);
	}
    }

    exit(0);
}
