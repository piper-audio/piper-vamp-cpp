/* -*- c-basic-offset: 4 indent-tabs-mode: nil -*-  vi:set ts=8 sts=4 sw=4: */

/*
    Piper C++

    An API for audio analysis and feature extraction plugins.

    Centre for Digital Music, Queen Mary, University of London.
    Copyright 2006-2019 Chris Cannam and QMUL.
  
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

#ifndef PIPER_PLUGIN_PROGRAM_PARAMETERS_H
#define PIPER_PLUGIN_PROGRAM_PARAMETERS_H

#include <vamp-hostsdk/Plugin.h>

#include <map>
#include <string>

namespace piper_vamp {

/**
 * \class PluginProgramParameters
 * 
 * PluginProgramParameters is a structure mapping from program names
 * to the parameter settings associated with those programs.
 */
struct PluginProgramParameters
{
    std::map<std::string, std::map<std::string, float>> programParameters;

    /**
     * Extract the program parameters from the given plugin (without
     * retaining any persistent reference to the plugin itself).
     */
    static PluginProgramParameters
    fromPlugin(Vamp::Plugin *p, const PluginConfiguration &defaultConfiguration) {

        auto programs = p->getPrograms();
        if (programs.empty()) return {};

        PluginProgramParameters pp;

        for (auto program: programs) {

            p->selectProgram(program);

            for (auto param: defaultConfiguration.parameterValues) {
                auto id = param.first;
                pp.programParameters[program][id] = p->getParameter(id);
            }
        }

        if (defaultConfiguration.currentProgram != "") {
            p->selectProgram(defaultConfiguration.currentProgram);
        }

        for (auto param: defaultConfiguration.parameterValues) {
            p->setParameter(param.first, param.second);
        }

        return pp;
    }
};

}

#endif
