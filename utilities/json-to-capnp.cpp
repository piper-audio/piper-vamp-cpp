
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

    if (type == "basic") {
	throw VampJson::Failure("can't convert Basic block on its own");

    } else if (type == "configurationrequest") {
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
    
int main(int, char **)
{
    string input;

    while (getline(cin, input)) {
	try {
	    ::capnp::MallocMessageBuilder message;
	    handle_input(message, input);
	    writePackedMessageToFd(1, message); // stdout
	    return 0;
	} catch (const VampJson::Failure &e) {
	    cerr << "Failed to convert JSON to Cap'n Proto message: "
		 << e.what() << endl;
	    return 1;
	}
    }
}


