
#include "VampJson.h"
#include "vampipe-convert.h"

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
	throw VampJson::Failure("not implemented yet"); ///!!!

    } else if (type == "configurationresponse") {
	throw VampJson::Failure("not implemented yet"); ///!!!

    } else if (type == "feature") {
	auto f = message.initRoot<Feature>();
	VampSDKConverter::buildFeature
	    (f, VampJson::toFeature(payload));

    } else if (type == "featureset") {
	auto fs = message.initRoot<FeatureSet>();
	VampSDKConverter::buildFeatureSet
	    (fs, VampJson::toFeatureSet(payload));

    } else if (type == "loadrequest") {
	auto req = message.initRoot<LoadRequest>();
	VampSDKConverter::buildLoadRequest
	    (req, VampJson::toLoadRequest(payload));
	
    } else if (type == "loadresponse") {
	//!!! response types & configure call for plugin handles, but
	//!!! we don't have any context in which a plugin handle can
	//!!! be persistent here
	throw VampJson::Failure("not implemented yet"); ///!!!

    } else if (type == "outputdescriptor") {
	auto od = message.initRoot<OutputDescriptor>();
	VampSDKConverter::buildOutputDescriptor
	    (od, VampJson::toOutputDescriptor(payload));

    } else if (type == "parameterdescriptor") {
	auto pd = message.initRoot<ParameterDescriptor>();
	VampSDKConverter::buildParameterDescriptor
	    (pd, VampJson::toParameterDescriptor(payload));

    } else if (type == "pluginconfiguration") {
	auto pc = message.initRoot<PluginConfiguration>();
	auto config = VampJson::toPluginConfiguration(payload);
	VampSDKConverter::buildPluginConfiguration(pc, config);

    } else if (type == "pluginstaticdata") {
	auto pc = message.initRoot<PluginStaticData>();
	auto sd = VampJson::toPluginStaticData(payload);
 	VampSDKConverter::buildPluginStaticData(pc, sd);

    } else if (type == "processblock") {
	throw VampJson::Failure("not implemented yet"); ///!!!

    } else if (type == "realtime") {
	auto b = message.initRoot<RealTime>();
	VampSDKConverter::buildRealTime
	    (b, VampJson::toRealTime(payload));
	
    } else if (type == "valueextents") {
	throw VampJson::Failure("no ValueExtents struct in Cap'n Proto mapping");
	
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


