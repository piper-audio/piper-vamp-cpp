
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

    if (rr.type == RRType::Load) {
	rr.loadRequest = VampJson::toVampRequest_Load(j);

    } else if (rr.type == RRType::Configure) {
	rr.configurationRequest = VampJson::toVampRequest_Configure(j, rr.mapper);

    } else if (rr.type == RRType::Process) {
	rr.processRequest = VampJson::toVampRequest_Process(j, rr.mapper);

    } else if (rr.type == RRType::Finish) {
	rr.finishPlugin = VampJson::toVampRequest_Finish(j, rr.mapper);
    }

    return rr;
}

void
writeRequestJson(RequestOrResponse &rr)
{
    Json j;

    if (rr.type == RRType::List) {
	j = VampJson::fromVampRequest_List();
	
    } else if (rr.type == RRType::Load) {
	j = VampJson::fromVampRequest_Load(rr.loadRequest);
	
    } else if (rr.type == RRType::Configure) {
	j = VampJson::fromVampRequest_Configure(rr.configurationRequest, rr.mapper);
	
    } else if (rr.type == RRType::Process) {
	j = VampJson::fromVampRequest_Process(rr.processRequest, rr.mapper);
	
    } else if (rr.type == RRType::Finish) {
	j = VampJson::fromVampRequest_Finish(rr.finishPlugin, rr.mapper);
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

    if (rr.type == RRType::List) {
	rr.listResponse = VampJson::toVampResponse_List(j);
	
    } else if (rr.type == RRType::Load) {
	rr.loadResponse = VampJson::toVampResponse_Load(j, rr.mapper);

    } else if (rr.type == RRType::Configure) {
	rr.configurationResponse = VampJson::toVampResponse_Configure(j);

    } else if (rr.type == RRType::Process) {
	rr.processResponse = VampJson::toVampResponse_Process(j);

    } else if (rr.type == RRType::Finish) {
	rr.finishResponse = VampJson::toVampResponse_Finish(j);
    }

    return rr;
}

void
writeResponseJson(RequestOrResponse &rr)
{
    Json j;

    if (rr.type == RRType::List) {
	j = VampJson::fromVampResponse_List("", rr.listResponse);
	
    } else if (rr.type == RRType::Load) {
	j = VampJson::fromVampResponse_Load(rr.loadResponse, rr.mapper);
    }

    //!!!

    cout << j.dump() << endl;
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
    } else {
	throw runtime_error("unknown or unimplemented format \"" + format + "\"");
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
    } else {
	throw runtime_error("unknown or unimplemented format \"" + format + "\"");
    }
}

int main(int argc, char **argv)
{
    if (argc < 2) {
	usage();
    }

    string informat = "json", outformat = "json";
    RequestOrResponse::Direction direction;
    bool haveDirection = false;
    
    for (int i = 1; i < argc; ++i) {

	string arg = argv[i];
	bool final = (i + 1 == argc);
	
	if (arg == "-i") {
	    if (informat != "" || final) usage();
	    else informat = argv[++i];

	} else if (arg == "-o") {
	    if (outformat != "" || final) usage();
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
	    if (rr.type == RRType::NotValid) break;
	    writeOutput(outformat, rr);
	    
	} catch (std::exception &e) {
	    cerr << "Error: " << e.what() << endl;
	    exit(1);
	}
    }

    exit(0);
}


