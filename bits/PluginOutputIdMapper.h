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

#ifndef VAMPIPE_PLUGIN_ID_MAPPER_H
#define VAMPIPE_PLUGIN_ID_MAPPER_H

#include <vamp-hostsdk/Plugin.h>

#include <map>
#include <string>

namespace vampipe {

class PluginOutputIdMapper
{
// NB not threadsafe. Does this matter?

//!!! simplify. A single vector is almost certainly faster.
    
public:
    PluginOutputIdMapper(Vamp::Plugin *p) : m_plugin(p) { }

    class NotFound : virtual public std::runtime_error {
    public:
        NotFound() : runtime_error("output id or index not found in mapper") { }
    };

    int idToIndex(std::string outputId) const {
	if (m_idIndexMap.empty()) populate();
	auto i = m_idIndexMap.find(outputId);
	if (i == m_idIndexMap.end()) throw NotFound();
	return i->second;
    }

    std::string indexToId(int index) const {
	if (m_ids.empty()) populate();
	if (index < 0 || size_t(index) >= m_ids.size()) throw NotFound();
	return m_ids[index];
    }

private:
    Vamp::Plugin *m_plugin;
    mutable std::vector<std::string> m_ids;
    mutable std::map<std::string, int> m_idIndexMap;

    void populate() const {
	Vamp::Plugin::OutputList outputs = m_plugin->getOutputDescriptors();
	for (const auto &d: outputs) {
	    m_idIndexMap[d.identifier] = m_ids.size();
	    m_ids.push_back(d.identifier);
	}
    }
};

}

#endif
