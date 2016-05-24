
#include "VampJson.h"
#include "VampnProto.h"

#include <iostream>
#include <sstream>
#include <stdexcept>

using namespace std;
using namespace json11;
using namespace vampipe;

// Accepting JSON objects with two fields, "type" and "content". The
// "type" string corresponds to the JSON schema filename
// (e.g. "outputdescriptor") and the "content" is the JSON object
// encoded with that schema.

class PreservingPluginHandleMapper : public PluginHandleMapper
{
public:
    PreservingPluginHandleMapper() : m_handle(0), m_plugin(0) { }

    virtual int32_t pluginToHandle(Vamp::Plugin *p) {
	if (p == m_plugin) return m_handle;
	else throw NotFound();
    }

    virtual Vamp::Plugin *handleToPlugin(int32_t h) {
	m_handle = h;
	m_plugin = reinterpret_cast<Vamp::Plugin *>(h);
	return m_plugin;
    }

private:
    int32_t m_handle;
    Vamp::Plugin *m_plugin;
};

void usage()
{
    string myname = "vampipe-convert";
    cerr << "\n" << myname <<
	": Validate and convert Vamp request and response messages\n\n"
	"    Usage: " << myname << " [-i <informat>] [-o <outformat>] request\n"
	"           " << myname << " [-i <informat>] [-o <outformat>] response\n\n"
	"    where\n"
	"       <informat>: the format to read from stdin\n"
	"           (\"json\" or \"capnp\", default is \"json\")\n"
	"       <outformat>: the format to convert to and write to stdout\n"
	"           (\"json\" or \"capnp\", default is \"json\")\n"
	"       request|response: whether to expect Vamp request or response messages\n\n"
	"If <informat> and <outformat> differ, convert from <informat> to <outformat>.\n"
	"If <informat> and <outformat> are the same, just check validity of incoming\n"
	"messages and pass them to output.\n\n";

    exit(2);
}

class RequestOrResponse
{
public:
    enum Direction {
	Request, Response
    };
    
    RequestOrResponse() : // nothing by default
	direction(Request),
	type(RRType::NotValid),
	success(false),
	finishPlugin(0) { }

    Direction direction;
    RRType type;
    bool success;
    string errorText;

    PreservingPluginHandleMapper mapper;

    vector<Vamp::HostExt::PluginStaticData> listResponse;
    Vamp::HostExt::LoadRequest loadRequest;
    Vamp::HostExt::LoadResponse loadResponse;
    Vamp::HostExt::ConfigurationRequest configurationRequest;
    Vamp::HostExt::ConfigurationResponse configurationResponse;
    Vamp::HostExt::ProcessRequest processRequest;
    Vamp::HostExt::ProcessResponse processResponse;
    Vamp::Plugin *finishPlugin;
    Vamp::HostExt::ProcessResponse finishResponse;
};

Json
convertRequestJson(string input)
{
    string err;
    Json j = Json::parse(input, err);
    if (err != "") {
	throw VampJson::Failure("invalid json: " + err);
    }
    if (!j.is_object()) {
	throw VampJson::Failure("object expected at top level");
    }
    if (!j["type"].is_string()) {
	throw VampJson::Failure("string expected for type field");
    }
    if (!j["content"].is_object()) {
	throw VampJson::Failure("object expected for content field");
    }
    return j;
}

Json
convertResponseJson(string input)
{
    string err;
    Json j = Json::parse(input, err);
    if (err != "") {
	throw VampJson::Failure("invalid json: " + err);
    }
    if (!j.is_object()) {
	throw VampJson::Failure("object expected at top level");
    }
    if (!j["success"].is_bool()) {
	throw VampJson::Failure("bool expected for success field");
    }
    if (!j["content"].is_object()) {
	throw VampJson::Failure("object expected for content field");
    }
    return j;
}

//!!! Lots of potential for refactoring the conversion classes based
//!!! on the common matter in the following eight functions...

RequestOrResponse
readRequestJson()
{
    RequestOrResponse rr;
    rr.direction = RequestOrResponse::Request;

    string input;
    if (!getline(cin, input)) {
	rr.type = RRType::NotValid;
	return rr;
    }
    
    Json j = convertRequestJson(input);

    rr.type = VampJson::getRequestResponseType(j);

    switch (rr.type) {

    case RRType::List:
	VampJson::toVampRequest_List(j); // type check only
	break;
    case RRType::Load:
	rr.loadRequest = VampJson::toVampRequest_Load(j);
	break;
    case RRType::Configure:
	rr.configurationRequest =
	    VampJson::toVampRequest_Configure(j, rr.mapper);
	break;
    case RRType::Process:
	rr.processRequest = VampJson::toVampRequest_Process(j, rr.mapper);
	break;
    case RRType::Finish:
	rr.finishPlugin = VampJson::toVampRequest_Finish(j, rr.mapper);
	break;
    case RRType::NotValid:
	break;
    }

    return rr;
}

void
writeRequestJson(RequestOrResponse &rr)
{
    Json j;

    switch (rr.type) {

    case RRType::List:
	j = VampJson::fromVampRequest_List();
	break;
    case RRType::Load:
	j = VampJson::fromVampRequest_Load(rr.loadRequest);
	break;
    case RRType::Configure:
	j = VampJson::fromVampRequest_Configure(rr.configurationRequest,
						rr.mapper);
	break;
    case RRType::Process:
	j = VampJson::fromVampRequest_Process(rr.processRequest, rr.mapper);
	break;
    case RRType::Finish:
	j = VampJson::fromVampRequest_Finish(rr.finishPlugin, rr.mapper);
	break;
    case RRType::NotValid:
	break;
    }

    cout << j.dump() << endl;
}

RequestOrResponse
readResponseJson()
{
    RequestOrResponse rr;
    rr.direction = RequestOrResponse::Response;

    string input;
    if (!getline(cin, input)) {
	rr.type = RRType::NotValid;
	return rr;
    }

    Json j = convertResponseJson(input);

    rr.type = VampJson::getRequestResponseType(j);

    switch (rr.type) {

    case RRType::List:
	rr.listResponse = VampJson::toVampResponse_List(j);
	break;
    case RRType::Load:
	rr.loadResponse = VampJson::toVampResponse_Load(j, rr.mapper);
	break;
    case RRType::Configure:
	rr.configurationResponse = VampJson::toVampResponse_Configure(j);
	break;
    case RRType::Process: 
	rr.processResponse = VampJson::toVampResponse_Process(j);
	break;
    case RRType::Finish:
	rr.finishResponse = VampJson::toVampResponse_Finish(j);
	break;
    case RRType::NotValid:
	break;
    }

    return rr;
}

void
writeResponseJson(RequestOrResponse &rr)
{
    Json j;

    switch (rr.type) {

    case RRType::List:
	j = VampJson::fromVampResponse_List("", rr.listResponse);
	break;
    case RRType::Load:
	j = VampJson::fromVampResponse_Load(rr.loadResponse, rr.mapper);
	break;
    case RRType::Configure:
	j = VampJson::fromVampResponse_Configure(rr.configurationResponse);
	break;
    case RRType::Process:
	j = VampJson::fromVampResponse_Process(rr.processResponse);
	break;
    case RRType::Finish:
	j = VampJson::fromVampResponse_Finish(rr.finishResponse);
	break;
    case RRType::NotValid:
	break;
    }

    cout << j.dump() << endl;
}

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
	VampnProto::readVampRequest_Configure(rr.configurationRequest, reader,
					      rr.mapper);
	break;
    case RRType::Process:
	VampnProto::readVampRequest_Process(rr.processRequest, reader,
					    rr.mapper);
	break;
    case RRType::Finish:
	VampnProto::readVampRequest_Finish(rr.finishPlugin, reader,
					   rr.mapper);
	break;
    case RRType::NotValid:
	break;
    }

    return rr;
}

void
writeRequestCapnp(RequestOrResponse &rr)
{
    ::capnp::MallocMessageBuilder message;
    VampRequest::Builder builder = message.initRoot<VampRequest>();

    switch (rr.type) {

    case RRType::List:
	VampnProto::buildVampRequest_List(builder);
	break;
    case RRType::Load:
	VampnProto::buildVampRequest_Load(builder, rr.loadRequest);
	break;
    case RRType::Configure:
	VampnProto::buildVampRequest_Configure(builder,
					       rr.configurationRequest,
					       rr.mapper);
	break;
    case RRType::Process:
	VampnProto::buildVampRequest_Process(builder,
					     rr.processRequest,
					     rr.mapper);
	break;
    case RRType::Finish:
	VampnProto::buildVampRequest_Finish(builder,
					    rr.finishPlugin,
					    rr.mapper);
	break;
    case RRType::NotValid:
	break;
    }

    writePackedMessageToFd(1, message);
}

RequestOrResponse
readResponseCapnp()
{
    RequestOrResponse rr;
    rr.direction = RequestOrResponse::Response;

    ::capnp::PackedFdMessageReader message(0); // stdin
    VampResponse::Reader reader = message.getRoot<VampResponse>();
    
    rr.type = VampnProto::getRequestResponseType(reader);

    switch (rr.type) {

    case RRType::List:
	VampnProto::readVampResponse_List(rr.listResponse, reader);
	break;
    case RRType::Load:
	VampnProto::readVampResponse_Load(rr.loadResponse, reader, rr.mapper);
	break;
    case RRType::Configure:
	VampnProto::readVampResponse_Configure(rr.configurationResponse,
					       reader);
	break;
    case RRType::Process:
	VampnProto::readVampResponse_Process(rr.processResponse, reader);
	break;
    case RRType::Finish:
	VampnProto::readVampResponse_Finish(rr.finishResponse, reader);
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
	VampnProto::buildVampResponse_Load(builder, rr.loadResponse, rr.mapper);
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
readInput(string format, RequestOrResponse::Direction direction)
{
    if (format == "json") {
	if (direction == RequestOrResponse::Request) {
	    return readRequestJson();
	} else {
	    return readResponseJson();
	}
    } else if (format == "capnp") {
	if (direction == RequestOrResponse::Request) {
	    return readRequestCapnp();
	} else {
	    return readResponseCapnp();
	}
    } else {
	throw runtime_error("unknown input format \"" + format + "\"");
    }
}

void
writeOutput(string format, RequestOrResponse &rr)
{
    if (format == "json") {
	if (rr.direction == RequestOrResponse::Request) {
	    writeRequestJson(rr);
	} else {
	    writeResponseJson(rr);
	}
    } else if (format == "capnp") {
	if (rr.direction == RequestOrResponse::Request) {
	    writeRequestCapnp(rr);
	} else {
	    writeResponseCapnp(rr);
	}
    } else {
	throw runtime_error("unknown output format \"" + format + "\"");
    }
}

int main(int argc, char **argv)
{
    if (argc < 2) {
	usage();
    }

    string informat = "json", outformat = "json";
    RequestOrResponse::Direction direction = RequestOrResponse::Request;
    bool haveDirection = false;
    
    for (int i = 1; i < argc; ++i) {

	string arg = argv[i];
	bool final = (i + 1 == argc);
	
	if (arg == "-i") {
	    if (final) usage();
	    else informat = argv[++i];

	} else if (arg == "-o") {
	    if (final) usage();
	    else outformat = argv[++i];

	} else if (arg == "request") {
	    direction = RequestOrResponse::Request;
	    haveDirection = true;

	} else if (arg == "response") {
	    direction = RequestOrResponse::Response;
	    haveDirection = true;
	    
	} else {
	    usage();
	}
    }

    if (informat == "" || outformat == "" || !haveDirection) {
	usage();
    }

    while (true) {

	try {

	    RequestOrResponse rr = readInput(informat, direction);

	    // NotValid without an exception indicates EOF:
	    if (rr.type == RRType::NotValid) break;

	    writeOutput(outformat, rr);
	    
	} catch (std::exception &e) {
	    cerr << "Error: " << e.what() << endl;
	    exit(1);
	}
    }

    exit(0);
}


