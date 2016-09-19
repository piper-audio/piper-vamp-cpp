
//!!! This program was an early test -- it should still compile but
//!!! it's incomplete. Remove it and use the server program instead.

#include "VampJson.h"
#include "bits/CountingPluginHandleMapper.h"

#include <iostream>
#include <sstream>
#include <stdexcept>

#include <map>
#include <set>

using namespace std;
using namespace Vamp;
using namespace Vamp::HostExt;
using namespace json11;
using namespace vampipe;

static CountingPluginHandleMapper mapper;

Vamp::HostExt::LoadResponse
loadPlugin(json11::Json j) {

    auto req = VampJson::toLoadRequest(j);
    auto loader = Vamp::HostExt::PluginLoader::getInstance();
    auto response = loader->loadPlugin(req);
    
    if (!response.plugin) {
	throw VampJson::Failure("plugin load failed");
    }
    
    return response;
}
    
Vamp::HostExt::ConfigurationResponse
configurePlugin(Vamp::Plugin *plugin, json11::Json j) {
    
    auto config = VampJson::toPluginConfiguration(j);
    Vamp::HostExt::ConfigurationRequest req;
    req.plugin = plugin;
    req.configuration = config;
    auto loader = Vamp::HostExt::PluginLoader::getInstance();

    auto response = loader->configurePlugin(req);
    if (response.outputs.empty()) {
	throw VampJson::Failure("plugin initialisation failed (invalid channelCount, stepSize, blockSize?)");
    }
    return response;
}

Json
handle_list(Json content)
{
    if (content != Json()) {
	throw VampJson::Failure("no content expected for list request");
    }
    
    auto loader = PluginLoader::getInstance();
    auto pluginData = loader->listPluginData();

    Json::array j;
    for (const auto &pd: pluginData) {
	j.push_back(VampJson::fromPluginStaticData(pd));
    }
    return Json(j);
}

Json
handle_load(Json j)
{
    auto loadResponse = loadPlugin(j);

    if (!loadResponse.plugin) {
	throw VampJson::Failure("plugin load failed");
    }

    mapper.addPlugin(loadResponse.plugin);
    
    return VampJson::fromLoadResponse(loadResponse, mapper);
}

Json
handle_configure(Json j)
{
    string err;

    if (!j.has_shape({
		{ "pluginHandle", Json::NUMBER },
		{ "configuration", Json::OBJECT }}, err)) {
	throw VampJson::Failure("malformed configuration request: " + err);
    }

    int32_t handle = j["pluginHandle"].int_value();

    if (mapper.isConfigured(handle)) {
	throw VampJson::Failure("plugin has already been configured");
    }

    Plugin *plugin = mapper.handleToPlugin(handle);
    
    Json config = j["configuration"];

    auto response = configurePlugin(plugin, config);

    mapper.markConfigured(handle, 0, 0); //!!!

    cerr << "Configured and initialised plugin " << handle << endl;

    return VampJson::fromConfigurationResponse(response, mapper);
}

Json
handle(string input)
{
    string err;
    Json j = Json::parse(input, err);

    if (err != "") {
	throw VampJson::Failure("invalid request: " + err);
    }

    if (!j["type"].is_string()) {
	throw VampJson::Failure("type expected in request");
    }

    if (!j["content"].is_null() &&
	!j["content"].is_object()) {
	throw VampJson::Failure("object expected for content");
    }

    string verb = j["type"].string_value();
    Json content = j["content"];
    Json result;

    if (verb == "list") {
	result = handle_list(content);
    } else if (verb == "load") {
	result = handle_load(content);
    } else if (verb == "configure") {
	result = handle_configure(content);
    } else {
	throw VampJson::Failure("unknown verb: " + verb +
				" (known verbs are: list load configure)");
    }

    return result;
}

Json
success_response(Json payload)
{
    Json::object obj;
    obj["success"] = true;
    obj["response"] = payload;
    return Json(obj);
}

Json
error_response(string text)
{
    Json::object obj;
    obj["success"] = false;
    obj["errorText"] = text;
    return Json(obj);
}

template<typename T>
T &getline(T &in, string prompt, string &out)
{
    cerr << prompt;
    return getline(in, out);
}

int main(int, char **)
{
    string line;

    while (getline(cin, "> ", line)) {
	try {
	    Json result = handle(line);
	    cout << success_response(result).dump() << endl;
	} catch (const VampJson::Failure &e) {
	    cout << error_response(e.what()).dump() << endl;
	} catch (const PluginHandleMapper::NotFound &e) {
	    cout << error_response(e.what()).dump() << endl;
	}
    }

    return 0;
}


