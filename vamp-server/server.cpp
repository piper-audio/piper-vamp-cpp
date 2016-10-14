
#include "vamp-capnp/VampnProto.h"
#include "vamp-support/RequestOrResponse.h"
#include "vamp-support/CountingPluginHandleMapper.h"
#include "vamp-support/LoaderRequests.h"

#include <iostream>
#include <sstream>
#include <stdexcept>

#include <capnp/serialize.h>

#include <map>
#include <set>

#include <unistd.h> // getpid for logging

using namespace std;
using namespace piper_vamp;
using namespace Vamp;

static int pid = getpid();
    
void usage()
{
    string myname = "piper-vamp-server";
    cerr << "\n" << myname <<
	": Load and run Vamp plugins in response to messages from stdin\n\n"
	"    Usage: " << myname << "\n\n"
	"Expects Piper request messages in Cap'n Proto packed format on stdin,\n"
	"and writes Piper response messages in the same format to stdout.\n\n";

    exit(2);
}

static CountingPluginHandleMapper mapper;

static RequestOrResponse::RpcId readId(const piper::RpcRequest::Reader &r)
{
    int number;
    string tag;
    switch (r.getId().which()) {
    case piper::RpcRequest::Id::Which::NUMBER:
        number = r.getId().getNumber();
        return { RequestOrResponse::RpcId::Number, number, "" };
    case piper::RpcRequest::Id::Which::TAG:
        tag = r.getId().getTag();
        return { RequestOrResponse::RpcId::Tag, 0, tag };
    case piper::RpcRequest::Id::Which::NONE:
        return { RequestOrResponse::RpcId::Absent, 0, "" };
    }
    return {};
}

static void buildId(piper::RpcResponse::Builder &b, const RequestOrResponse::RpcId &id)
{
    switch (id.type) {
    case RequestOrResponse::RpcId::Number:
        b.getId().setNumber(id.number);
        break;
    case RequestOrResponse::RpcId::Tag:
        b.getId().setTag(id.tag);
        break;
    case RequestOrResponse::RpcId::Absent:
        b.getId().setNone();
        break;
    }
}

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

    capnp::InputStreamMessageReader message(buffered);
    piper::RpcRequest::Reader reader = message.getRoot<piper::RpcRequest>();
    
    rr.type = VampnProto::getRequestResponseType(reader);
    rr.id = readId(reader);

    switch (rr.type) {

    case RRType::List:
	VampnProto::readRpcRequest_List(reader); // type check only
	break;
    case RRType::Load:
	VampnProto::readRpcRequest_Load(rr.loadRequest, reader);
	break;
    case RRType::Configure:
	VampnProto::readRpcRequest_Configure(rr.configurationRequest,
					      reader, mapper);
	break;
    case RRType::Process:
	VampnProto::readRpcRequest_Process(rr.processRequest, reader, mapper);
	break;
    case RRType::Finish:
	VampnProto::readRpcRequest_Finish(rr.finishRequest, reader, mapper);
	break;
    case RRType::NotValid:
	break;
    }

    return rr;
}

void
writeResponseCapnp(RequestOrResponse &rr)
{
    capnp::MallocMessageBuilder message;
    piper::RpcResponse::Builder builder = message.initRoot<piper::RpcResponse>();

    buildId(builder, rr.id);
    
    if (!rr.success) {

	VampnProto::buildRpcResponse_Error(builder, rr.errorText, rr.type);

    } else {
	
	switch (rr.type) {

	case RRType::List:
	    VampnProto::buildRpcResponse_List(builder, rr.listResponse);
	    break;
	case RRType::Load:
	    VampnProto::buildRpcResponse_Load(builder, rr.loadResponse, mapper);
	    break;
	case RRType::Configure:
	    VampnProto::buildRpcResponse_Configure(builder, rr.configurationResponse, mapper);
	    break;
	case RRType::Process:
	    VampnProto::buildRpcResponse_Process(builder, rr.processResponse, mapper);
	    break;
	case RRType::Finish:
	    VampnProto::buildRpcResponse_Finish(builder, rr.finishResponse, mapper);
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
    capnp::MallocMessageBuilder message;
    piper::RpcResponse::Builder builder = message.initRoot<piper::RpcResponse>();
    VampnProto::buildRpcResponse_Exception(builder, e, type);
    
    writeMessageToFd(1, message);
}

RequestOrResponse
handleRequest(const RequestOrResponse &request)
{
    RequestOrResponse response;
    response.direction = RequestOrResponse::Response;
    response.type = request.type;

    switch (request.type) {

    case RRType::List:
	response.listResponse = LoaderRequests().listPluginData();
	response.success = true;
	break;

    case RRType::Load:
	response.loadResponse = LoaderRequests().loadPlugin(request.loadRequest);
	if (response.loadResponse.plugin != nullptr) {
	    mapper.addPlugin(response.loadResponse.plugin);
            cerr << "piper-vamp-server " << pid << ": loaded plugin, handle = " << mapper.pluginToHandle(response.loadResponse.plugin) << endl;
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

	response.configurationResponse = LoaderRequests().configurePlugin(creq);
	
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
	auto &freq = request.finishRequest;
	response.finishResponse.plugin = freq.plugin;

	auto h = mapper.pluginToHandle(freq.plugin);
        // Finish can be called (to unload the plugin) even if the
        // plugin has never been configured or used. But we want to
        // make sure we call getRemainingFeatures only if we have
        // actually configured the plugin.
	if (mapper.isConfigured(h)) {
            response.finishResponse.features = freq.plugin->getRemainingFeatures();
	}

	// We do not delete the plugin here -- we need it in the
	// mapper when converting the features. It gets deleted in the
	// calling function.
	response.success = true;
	break;
    }

    case RRType::NotValid:
	break;
    }
    
    return response;
}

int main(int argc, char **)
{
    if (argc != 1) {
	usage();
    }

    while (true) {

	RequestOrResponse request;
	
	try {

	    request = readRequestCapnp();

	    cerr << "piper-vamp-server " << pid << ": request received, of type "
		 << int(request.type)
		 << endl;
	    
	    // NotValid without an exception indicates EOF:
	    if (request.type == RRType::NotValid) {
		cerr << "piper-vamp-server " << pid << ": eof reached, exiting" << endl;
		break;
	    }

	    RequestOrResponse response = handleRequest(request);
            response.id = request.id;

	    cerr << "piper-vamp-server " << pid << ": request handled, writing response"
		 << endl;
	    
	    writeResponseCapnp(response);

	    cerr << "piper-vamp-server " << pid << ": response written" << endl;

	    if (request.type == RRType::Finish) {
		auto h = mapper.pluginToHandle(request.finishRequest.plugin);
                cerr << "piper-vamp-server " << pid << ": deleting the plugin with handle " << h << endl;
		mapper.removePlugin(h);
		delete request.finishRequest.plugin;
	    }
	    
	} catch (std::exception &e) {

	    cerr << "piper-vamp-server " << pid << ": error: " << e.what() << endl;

	    writeExceptionCapnp(e, request.type);
	    
	    exit(1);
	}
    }

    exit(0);
}
