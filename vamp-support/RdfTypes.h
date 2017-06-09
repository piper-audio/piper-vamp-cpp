/* -*- c-basic-offset: 4 indent-tabs-mode: nil -*-  vi:set ts=8 sts=4 sw=4: */

/*
    Piper C++

    An API for audio analysis and feature extraction plugins.

    Centre for Digital Music, Queen Mary, University of London.
    Copyright 2006-2017 Chris Cannam and QMUL.
  
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

#ifndef PIPER_RDF_TYPES_H
#define PIPER_RDF_TYPES_H

#include "StaticOutputDescriptor.h"

#include <vamp-hostsdk/PluginLoader.h>

#include <sord/sord.h>

#include <mutex>

namespace piper_vamp {

class RdfTypes
{
public:
    RdfTypes() :
        m_world(sord_world_new())
    {}

    ~RdfTypes() {
        sord_world_free(m_world);
    }
    
    StaticOutputInfo loadStaticOutputInfo(Vamp::HostExt::PluginLoader::PluginKey
                                          pluginKey) {

        StaticOutputInfo info;
        SordModel *model = sord_new(m_world, SORD_SPO|SORD_OPS|SORD_POS, false);
        if (loadRdf(model, candidateRdfFilesFor(pluginKey))) {
            // we want to find a graph like
            // :plugin vamp:output :output1
            // :plugin vamp:output :output2
            // :plugin vamp:output :output3
            // :output1 vamp:computes_event_type :event
            // :output2 vamp:computes_feature :feature
            // :output3 vamp:computes_signal_type :signal
        }
        sord_free(model);
        return info;
    }

private:
    SordWorld *m_world;

    bool loadRdf(SordModel *targetModel, std::vector<std::string> filenames) {
        for (auto f: filenames) {
            if (loadRdfFile(targetModel, f)) {
                return true;
            }
        }
        return false;
    }

    bool loadRdfFile(SordModel *targetModel, std::string filename) {
        std::string base = "file://" + filename;
        SerdURI bu;
        if (serd_uri_parse((const uint8_t *)base.c_str(), &bu) !=
            SERD_SUCCESS) {
            std::cerr << "Failed to parse base URI " << base << std::endl;
            return false;
        }
        SerdNode bn = serd_node_from_string(SERD_URI,
                                            (const uint8_t *)base.c_str());
        SerdEnv *env = serd_env_new(&bn);
        SerdReader *reader = sord_new_reader(targetModel, env, SERD_TURTLE, 0);
        SerdStatus rv = serd_reader_read_file
            (reader, (const uint8_t *)filename.c_str());
        bool success = (rv == SERD_SUCCESS);
        if (!success) {
            std::cerr << "Failed to import RDF from " << filename << std::endl;
        } else {
            std::cerr << "Imported RDF from " << filename << std::endl;
        }
        serd_reader_free(reader);
        serd_env_free(env);
        return success;
    }
    
    std::vector<std::string> candidateRdfFilesFor(Vamp::HostExt::
                                                  PluginLoader::PluginKey key) {

        std::string library = Vamp::HostExt::PluginLoader::getInstance()->
            getLibraryPathForPlugin(key);
	
        auto li = library.rfind('.');
        if (li == std::string::npos) return {};
        auto withoutSuffix = library.substr(0, li);

        std::vector<std::string> suffixes { "ttl", "TTL", "n3", "N3" };
        std::vector<std::string> candidates;

        for (auto suffix : suffixes) {
            candidates.push_back(withoutSuffix + "." + suffix);
        }

        return candidates;
    }
};

}

#endif
