
#include "VampJson.h"

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

static map<uint32_t, Plugin *> loadedPlugins;
static set<uint32_t> initialisedPlugins;

static uint32_t nextHandle = 1; // plugin handle type must fit in JSON number

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
    
Vamp::Plugin::OutputList
configurePlugin(Vamp::Plugin *plugin, json11::Json j) {
    
    auto config = VampJson::toPluginConfiguration(j);
    auto loader = Vamp::HostExt::PluginLoader::getInstance();
    auto outputs = loader->configurePlugin(plugin, config);

    if (outputs.empty()) {
	throw VampJson::Failure("plugin initialisation failed (invalid channelCount, stepSize, blockSize?)");
    }
    
    return outputs;
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
    
    uint32_t h = nextHandle++;
    loadedPlugins[h] = loadResponse.plugin;

    Json::object response;
    response["pluginHandle"] = double(h);
    response["staticData"] =
	VampJson::fromPluginStaticData(loadResponse.staticData);
    response["defaultConfiguration"] =
	VampJson::fromPluginConfiguration(loadResponse.defaultConfiguration);

    cerr << "Loaded plugin: handle is " << h << endl;
    
    return Json(response);
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

    uint32_t handle = j["pluginHandle"].int_value();

    if (loadedPlugins.find(handle) == loadedPlugins.end()) {
	throw VampJson::Failure("unknown plugin handle");
    }

    if (initialisedPlugins.find(handle) != initialisedPlugins.end()) {
	throw VampJson::Failure("plugin has already been initialised");
    }

    Plugin *plugin = loadedPlugins[handle];
    
    Json config = j["configuration"];

    configurePlugin(plugin, config);

    initialisedPlugins.insert(handle);

    cerr << "Configured and initialised plugin " << handle << endl;

    Json::object jout;
    Json::array outs;
    Vamp::Plugin::OutputList vouts = plugin->getOutputDescriptors();
    for (auto &o: vouts) {
	outs.push_back(VampJson::fromOutputDescriptor(o));
    }
    jout["outputList"] = outs;
    return Json(jout);
}

Json
handle(string input)
{
    string err;
    Json j = Json::parse(input, err);

    if (err != "") {
	throw VampJson::Failure("invalid request: " + err);
    }

    if (!j["verb"].is_string()) {
	throw VampJson::Failure("verb expected in request");
    }

    if (!j["content"].is_null() &&
	!j["content"].is_object()) {
	throw VampJson::Failure("object expected for content");
    }

    string verb = j["verb"].string_value();
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
	}
    }

    return 0;
}


