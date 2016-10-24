/* -*- c-basic-offset: 4 indent-tabs-mode: nil -*-  vi:set ts=8 sts=4 sw=4: */

/*
    Piper C++

    An API for audio analysis and feature extraction plugins.

    Centre for Digital Music, Queen Mary, University of London.
    Copyright 2006-2016 Chris Cannam and QMUL.
  
    Permission is hereby granted, free of charge, to any person
    obtaining a copy of this software and associated documentation
    files (the "Software"), to deal in the Software without
    restriction, including without limitation the rights to use, copy,
    modify, merge, publish, distribute, sublicense, and/or sell copies
    of the Software, and to permit persons to whom the Software is
    furnished to do so, subject to the following conditions:

    The above copyright notice and this permission notice shall be
    included in all copies or substantial portions of the Software.

    THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
    EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
    MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
    NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS BE LIABLE FOR
    ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF
    CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
    WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

    Except as contained in this notice, the names of the Centre for
    Digital Music; Queen Mary, University of London; and Chris Cannam
    shall not be used in advertising or otherwise to promote the sale,
    use or other dealings in this Software without prior written
    authorization.
*/

#ifndef PIPER_LOADER_REQUESTS_H
#define PIPER_LOADER_REQUESTS_H

#include "PluginStaticData.h"
#include "PluginConfiguration.h"

#include <vamp-hostsdk/PluginLoader.h>

#include <map>
#include <string>

namespace piper_vamp {

class LoaderRequests
{
public:
    ListResponse
    listPluginData() {

	auto loader = Vamp::HostExt::PluginLoader::getInstance();
	auto keys = loader->listPlugins();
	ListResponse response;

	for (std::string key: keys) {
	    Vamp::Plugin *p = loader->loadPlugin(key, 44100, 0);
	    if (!p) continue;
	    auto category = loader->getPluginCategory(key);
	    response.available.push_back
		(PluginStaticData::fromPlugin(key, category, p));
	    delete p;
	}

	return response;
    }

    LoadResponse
    loadPlugin(LoadRequest req) {

	auto loader = Vamp::HostExt::PluginLoader::getInstance();
	
	Vamp::Plugin *plugin = loader->loadPlugin(req.pluginKey,
						  req.inputSampleRate,
						  req.adapterFlags);

	LoadResponse response;
	response.plugin = plugin;
	if (!plugin) return response;

	response.plugin = plugin;
	response.staticData = PluginStaticData::fromPlugin
	    (req.pluginKey,
	     loader->getPluginCategory(req.pluginKey),
	     plugin);

	int defaultChannels = 0;
	if (plugin->getMinChannelCount() == plugin->getMaxChannelCount()) {
	    defaultChannels = int(plugin->getMinChannelCount());
	}
	
	response.defaultConfiguration = PluginConfiguration::fromPlugin
	    (plugin,
	     defaultChannels,
	     int(plugin->getPreferredStepSize()),
	     int(plugin->getPreferredBlockSize()));
	
	return response;
    }

    ConfigurationResponse
    configurePlugin(ConfigurationRequest req) {
	
	for (PluginConfiguration::ParameterMap::const_iterator i =
		 req.configuration.parameterValues.begin();
	     i != req.configuration.parameterValues.end(); ++i) {
	    req.plugin->setParameter(i->first, i->second);
	}

	if (req.configuration.currentProgram != "") {
	    req.plugin->selectProgram(req.configuration.currentProgram);
	}

	ConfigurationResponse response;

	response.plugin = req.plugin;
    
	if (req.plugin->initialise(req.configuration.channelCount,
				   req.configuration.stepSize,
				   req.configuration.blockSize)) {
	    response.outputs = req.plugin->getOutputDescriptors();
	}

	return response;
    }
};

}

#endif
