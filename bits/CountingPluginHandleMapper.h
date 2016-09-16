/* -*- c-basic-offset: 4 indent-tabs-mode: nil -*-  vi:set ts=8 sts=4 sw=4: */

/*
    Vampipe

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

#ifndef VAMPIPE_COUNTING_PLUGIN_HANDLE_MAPPER_H
#define VAMPIPE_COUNTING_PLUGIN_HANDLE_MAPPER_H

#include "PluginHandleMapper.h"
#include "PluginOutputIdMapper.h"
#include "DefaultPluginOutputIdMapper.h"

#include <set>
#include <map>

namespace vampipe {

//!!! NB not thread-safe at present, should it be?
class CountingPluginHandleMapper : public PluginHandleMapper
{
public:
    CountingPluginHandleMapper() : m_nextHandle(1) { }

    ~CountingPluginHandleMapper() {
        for (auto &o: m_outputMappers) {
            delete o.second;
        }
    }

    void addPlugin(Vamp::Plugin *p) {
	if (m_rplugins.find(p) == m_rplugins.end()) {
	    Handle h = m_nextHandle++;
	    m_plugins[h] = p;
	    m_rplugins[p] = h;
            m_outputMappers[h] = new DefaultPluginOutputIdMapper(p);
	}
    }

    void removePlugin(Handle h) {
	if (m_plugins.find(h) == m_plugins.end()) {
	    throw NotFound();
	}
	Vamp::Plugin *p = m_plugins[h];
        delete m_outputMappers.at(h);
        m_outputMappers.erase(h);
	m_plugins.erase(h);
	if (isConfigured(h)) {
	    m_configuredPlugins.erase(h);
	    m_channelCounts.erase(h);
	}
	m_rplugins.erase(p);
    }
    
    Handle pluginToHandle(Vamp::Plugin *p) const {
	if (m_rplugins.find(p) == m_rplugins.end()) {
	    throw NotFound();
	}
	return m_rplugins.at(p);
    }
    
    Vamp::Plugin *handleToPlugin(Handle h) const {
	if (m_plugins.find(h) == m_plugins.end()) {
	    throw NotFound();
	}
	return m_plugins.at(h);
    }

    //!!! iffy: mapper will move when another plugin is added. return by value?
    const PluginOutputIdMapper &pluginToOutputIdMapper(Vamp::Plugin *p) const {
        // pluginToHandle checks the plugin has been registered with us
        return *m_outputMappers.at(pluginToHandle(p));
    }

    //!!! iffy: mapper will move when another plugin is added. return by value?
    const PluginOutputIdMapper &handleToOutputIdMapper(Handle h) const {
	if (m_plugins.find(h) == m_plugins.end()) {
	    throw NotFound();
	}
        return *m_outputMappers.at(h);
    }

    bool isConfigured(Handle h) const {
	return m_configuredPlugins.find(h) != m_configuredPlugins.end();
    }

    void markConfigured(Handle h, int channelCount, int blockSize) {
	m_configuredPlugins.insert(h);
	m_channelCounts[h] = channelCount;
	m_blockSizes[h] = blockSize;
    }

    int getChannelCount(Handle h) const {
	if (m_channelCounts.find(h) == m_channelCounts.end()) {
	    throw NotFound();
	}
	return m_channelCounts.at(h);
    }

    int getBlockSize(Handle h) const {
	if (m_blockSizes.find(h) == m_blockSizes.end()) {
	    throw NotFound();
	}
	return m_blockSizes.at(h);
    }
    
private:
    Handle m_nextHandle; // NB plugin handle type must fit in JSON number
    std::map<Handle, Vamp::Plugin *> m_plugins;
    std::map<Vamp::Plugin *, Handle> m_rplugins;
    std::set<Handle> m_configuredPlugins;
    std::map<Handle, int> m_channelCounts;
    std::map<Handle, int> m_blockSizes;
    std::map<Handle, DefaultPluginOutputIdMapper *> m_outputMappers;
};

}

#endif
