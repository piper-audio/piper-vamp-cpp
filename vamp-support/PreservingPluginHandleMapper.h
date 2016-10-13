/* -*- c-basic-offset: 4 indent-tabs-mode: nil -*-  vi:set ts=8 sts=4 sw=4: */

/*
    Piper C++

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

#ifndef PIPER_PRESERVING_PLUGIN_HANDLE_MAPPER_H
#define PIPER_PRESERVING_PLUGIN_HANDLE_MAPPER_H

#include "PluginHandleMapper.h"
#include "PreservingPluginOutputIdMapper.h"

#include <iostream>

namespace piper_vamp {

//!!! document -- this is a passthrough thing for a single plugin
//!!! handle only, it does not use actually valid Plugin pointers at
//!!! all

class PreservingPluginHandleMapper : public PluginHandleMapper
{
public:
    PreservingPluginHandleMapper() :
        m_handle(0),
        m_plugin(0),
        m_omapper(std::make_shared<PreservingPluginOutputIdMapper>()) { }

    virtual Handle pluginToHandle(Vamp::Plugin *p) const noexcept {
        if (!p) return INVALID_HANDLE;
	if (p == m_plugin) return m_handle;
	else {
	    std::cerr << "PreservingPluginHandleMapper: p = " << p
		      << " differs from saved m_plugin " << m_plugin
		      << " (not returning saved handle " << m_handle << ")"
		      << std::endl;
            return INVALID_HANDLE;
	}
    }

    virtual Vamp::Plugin *handleToPlugin(Handle h) const noexcept {
        if (h == INVALID_HANDLE) return nullptr;
	m_handle = h;
	m_plugin = reinterpret_cast<Vamp::Plugin *>(h);
	return m_plugin;
    }

    virtual const std::shared_ptr<PluginOutputIdMapper> pluginToOutputIdMapper
    (Vamp::Plugin *p) const noexcept {
        if (!p) return {};
        return m_omapper;
    }
        
    virtual const std::shared_ptr<PluginOutputIdMapper> handleToOutputIdMapper
    (Handle h) const noexcept {
        if (h == INVALID_HANDLE) return {};
        return m_omapper;
    }
    
private:
    mutable Handle m_handle;
    mutable Vamp::Plugin *m_plugin;
    std::shared_ptr<PreservingPluginOutputIdMapper> m_omapper;
};

}

#endif
