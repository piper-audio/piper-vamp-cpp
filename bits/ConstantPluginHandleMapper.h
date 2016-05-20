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

#ifndef VAMPIPE_CONSTANT_PLUGIN_HANDLE_MAPPER_H
#define VAMPIPE_CONSTANT_PLUGIN_HANDLE_MAPPER_H

#include "PluginHandleMapper.h"

namespace vampipe {

/**
 * A trivial PluginHandleMapper, to be used for tests and for
 * translating between protocols that contain handles without actually
 * loading any plugins.
 *
 * This mapper only knows about one handle, and if it is asked for
 * that one, it returns a fixed plugin pointer (which might never be
 * dereferenced, depending on the context in which this class is
 * used). The idea would be to create one of these on the fly each
 * time a new handle needs to be translated.
 */
class ConstantPluginHandleMapper : public PluginHandleMapper
{
public:
    ConstantPluginHandleMapper(Vamp::Plugin *plugin, int32_t handle) :
	m_plugin(plugin), m_handle(handle) { }
    
    virtual int32_t pluginToHandle(Vamp::Plugin *p) {
	if (p == m_plugin) return m_handle;
	else throw NotFound();
    }
	
    virtual Vamp::Plugin *handleToPlugin(int32_t h) {
	if (h == m_handle) return m_plugin;
	else throw NotFound();
    }

private:
    Vamp::Plugin *m_plugin;
    int32_t m_handle;
};

}

#endif
