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

#include <set>
#include <map>

namespace vampipe {

//!!! NB not thread-safe at present, should it be?
class CountingPluginHandleMapper : public PluginHandleMapper
{
public:
    CountingPluginHandleMapper() : m_nextHandle(1) { }

    void addPlugin(Vamp::Plugin *p) {
	if (m_rplugins.find(p) == m_rplugins.end()) {
	    int32_t h = m_nextHandle++;
	    m_plugins[h] = p;
	    m_rplugins[p] = h;
	}
    }

    void removePlugin(int32_t h) {
	if (m_plugins.find(h) == m_plugins.end()) {
	    throw NotFound();
	}
	Vamp::Plugin *p = m_plugins[h];
	m_plugins.erase(h);
	if (isConfigured(h)) {
	    m_configuredPlugins.erase(h);
	    m_channelCounts.erase(h);
	}
	m_rplugins.erase(p);
    }
    
    int32_t pluginToHandle(Vamp::Plugin *p) const {
	if (m_rplugins.find(p) == m_rplugins.end()) {
	    throw NotFound();
	}
	return m_rplugins.at(p);
    }
    
    Vamp::Plugin *handleToPlugin(int32_t h) const {
	if (m_plugins.find(h) == m_plugins.end()) {
	    throw NotFound();
	}
	return m_plugins.at(h);
    }

    bool isConfigured(int32_t h) const {
	return m_configuredPlugins.find(h) != m_configuredPlugins.end();
    }

    void markConfigured(int32_t h, int channelCount, int blockSize) {
	m_configuredPlugins.insert(h);
	m_channelCounts[h] = channelCount;
	m_blockSizes[h] = blockSize;
    }

    int getChannelCount(int32_t h) const {
	if (m_channelCounts.find(h) == m_channelCounts.end()) {
	    throw NotFound();
	}
	return m_channelCounts.at(h);
    }

    int getBlockSize(int32_t h) const {
	if (m_blockSizes.find(h) == m_blockSizes.end()) {
	    throw NotFound();
	}
	return m_blockSizes.at(h);
    }
    
private:
    int32_t m_nextHandle; // NB plugin handle type must fit in JSON number
    std::map<uint32_t, Vamp::Plugin *> m_plugins;
    std::map<Vamp::Plugin *, uint32_t> m_rplugins;
    std::set<uint32_t> m_configuredPlugins;
    std::map<uint32_t, int> m_channelCounts;
    std::map<uint32_t, int> m_blockSizes;
};

}

#endif
