
#include "VampJson.h"
#include "VampnProto.h"

#include <iostream>
#include <sstream>
#include <stdexcept>

using namespace std;
using namespace json11;
using namespace vampipe;

// Accepting JSON objects with two fields, "type" and "payload". The
// "type" string corresponds to the JSON schema filename
// (e.g. "outputdescriptor") and the "payload" is the JSON object
// encoded with that schema.

Json
json_input(string input)
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
    if (!j["payload"].is_object()) {
	throw VampJson::Failure("object expected for payload field");
    }
    return j;
}

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

void 
handle_input(::capnp::MallocMessageBuilder &message, string input)
{
    string err;

    Json j = json_input(input);
    string type = j["type"].string_value();
    Json payload = j["payload"];

    if (type == "configurationrequest") {
	auto req = message.initRoot<ConfigurationRequest>();
	PreservingPluginHandleMapper mapper;
	VampnProto::buildConfigurationRequest
	    (req, VampJson::toConfigurationRequest(payload, mapper), mapper);

    } else if (type == "configurationresponse") {
	auto resp = message.initRoot<ConfigurationResponse>();
	VampnProto::buildConfigurationResponse
	    (resp, VampJson::toConfigurationResponse(payload));

    } else if (type == "feature") {
	auto f = message.initRoot<Feature>();
	VampnProto::buildFeature
	    (f, VampJson::toFeature(payload));

    } else if (type == "featureset") {
	auto fs = message.initRoot<FeatureSet>();
	VampnProto::buildFeatureSet
	    (fs, VampJson::toFeatureSet(payload));

    } else if (type == "loadrequest") {
	auto req = message.initRoot<LoadRequest>();
	VampnProto::buildLoadRequest
	    (req, VampJson::toLoadRequest(payload));
	
    } else if (type == "loadresponse") {
	auto resp = message.initRoot<LoadResponse>();
	PreservingPluginHandleMapper mapper;
	VampnProto::buildLoadResponse
	    (resp, VampJson::toLoadResponse(payload, mapper), mapper);

    } else if (type == "outputdescriptor") {
	auto od = message.initRoot<OutputDescriptor>();
	VampnProto::buildOutputDescriptor
	    (od, VampJson::toOutputDescriptor(payload));

    } else if (type == "parameterdescriptor") {
	auto pd = message.initRoot<ParameterDescriptor>();
	VampnProto::buildParameterDescriptor
	    (pd, VampJson::toParameterDescriptor(payload));

    } else if (type == "pluginconfiguration") {
	auto pc = message.initRoot<PluginConfiguration>();
	auto config = VampJson::toPluginConfiguration(payload);
	VampnProto::buildPluginConfiguration(pc, config);

    } else if (type == "pluginstaticdata") {
	auto pc = message.initRoot<PluginStaticData>();
	auto sd = VampJson::toPluginStaticData(payload);
 	VampnProto::buildPluginStaticData(pc, sd);

    } else if (type == "processrequest") {
	auto p = message.initRoot<ProcessRequest>();
	PreservingPluginHandleMapper mapper;
	VampnProto::buildProcessRequest
	    (p, VampJson::toProcessRequest(payload, mapper), mapper);

    } else if (type == "realtime") {
	auto b = message.initRoot<RealTime>();
	VampnProto::buildRealTime
	    (b, VampJson::toRealTime(payload));
	
    } else {
	throw VampJson::Failure("unknown or unsupported JSON schema type " +
				type);
    }
}

void usage()
{
    string myname = "vampipe-convert";
    cerr << "\n" << myname <<
	": Convert Vamp request and response messages between formats\n\n"
	"    Usage:  " << myname << " -i <informat> -o <outformat>\n\n"
	"Where <informat> and <outformat> may be \"json\" or \"capnp\".\n"
	"Messages are read from stdin and written to stdout.\n" << endl;
    exit(2);
}

class RequestOrResponse
{
public:
    enum Type {
	List, Load, Configure, Process, Finish, Eof
    };
    RequestOrResponse() : // nothing by default
	type(Eof),
	success(false),
	finishPlugin(0) { }

    Type type;
    bool success;
    string errorText;

    PreservingPluginHandleMapper mapper;
    
    Vamp::HostExt::LoadRequest loadRequest;
    Vamp::HostExt::LoadResponse loadResponse;
    Vamp::HostExt::ConfigurationRequest configurationRequest;
    Vamp::HostExt::ConfigurationResponse configurationResponse;
    Vamp::HostExt::ProcessRequest processRequest;
    Vamp::HostExt::ProcessResponse processResponse;
    Vamp::Plugin *finishPlugin;
    Vamp::HostExt::ProcessResponse finishResponse;
    
};

RequestOrResponse
readInputJson()
{
    RequestOrResponse rr;
    string input;
    if (!getline(cin, input)) {
	rr.type = RequestOrResponse::Eof;
	return rr;
    }

    Json j = json_input(input);
    string type = j["type"].string_value();

    if (type == "list") {
	rr.type = RequestOrResponse::List;

    } else if (type == "load") {
	//!!! ah, we need a way to know whether we're dealing with a request or response here
	rr.type = RequestOrResponse::Load;
	rr.loadRequest = VampJson::toLoadRequest(j["content"]);

    } else if (type == "configure") {
	rr.type = RequestOrResponse::Configure;
	rr.configurationRequest =
	    VampJson::toConfigurationRequest(j["content"], rr.mapper);

    } else if (type == "process") {
	rr.type = RequestOrResponse::Process;
	rr.processRequest =
	    VampJson::toProcessRequest(j["content"], rr.mapper);

    } else if (type == "finish") {
	rr.type = RequestOrResponse::Finish;
	//!!! VampJsonify
	rr.finishPlugin = rr.mapper.handleToPlugin(j["content"]["pluginHandle"].int_value());

    } else {
	throw runtime_error("unknown or unexpected request/response type \"" +
			    type + "\"");
    }

    return rr;
}

RequestOrResponse
readInput(string format)
{
    if (format == "json") {
	return readInputJson();
    } else {
	throw runtime_error("unknown or unimplemented format \"" + format + "\"");
    }
}

void
writeOutput(string format, const RequestOrResponse &rr)
{
    throw runtime_error("writeOutput not implemented yet");
    
}

int main(int argc, char **argv)
{
    if (argc != 5) {
	usage();
    }

    string informat, outformat;
    
    for (int i = 1; i + 1 < argc; ++i) {

	string arg = argv[i];
	
	if (arg == "-i") {
	    if (informat != "") usage();
	    else informat = argv[++i];

	} else if (arg == "-o") {
	    if (outformat != "") usage();
	    else outformat = argv[++i];

	} else {
	    usage();
	}
    }

    if (informat == "" || outformat == "") {
	usage();
    }

    while (true) {

	try {

	    RequestOrResponse rr = readInput(informat);
	    if (rr.type == RequestOrResponse::Eof) break;
	    writeOutput(outformat, rr);
	    
	} catch (std::exception &e) {
	    cerr << "Error: " << e.what() << endl;
	    exit(1);
	}
    }

    exit(0);
}


