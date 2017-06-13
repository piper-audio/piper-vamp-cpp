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

#include "vamp-json/VampJson.h"
#include "vamp-support/RequestOrResponse.h"
#include "vamp-support/LoaderRequests.h"
#include "vamp-support/RdfTypes.h"

#include <iostream>
#include <sstream>
#include <stdexcept>

#include <map>
#include <set>

// for _setmode stuff and _dup
#ifdef _WIN32
#include <io.h>
#include <fcntl.h>
#endif

// for dup, open etc
#ifndef _WIN32
#include <fcntl.h>
#include <unistd.h>
#endif

using namespace std;
using namespace json11;
using namespace piper_vamp;
using namespace Vamp;

static string myname = "piper-vamp-simple-server";

static void version()
{
    cout << "1.0" << endl;
    exit(0);
}

static void usage(bool successful = false)
{
    cerr << "\n" << myname <<
        ": Emit stub version of main code\nfor a Piper Adapter implementation of a Vamp plugin library\n\n"
        "    Usage: " << myname << " [-d] <soname>\n"
        "           " << myname << " -v\n"
        "           " << myname << " -h\n\n"
        "    where\n"
        "       <soname>: the Vamp plugin library name to emit stub conversion code for\n"
        "       -d: also print debug information to stderr\n"
        "       -v: print version number to stdout and exit\n"
        "       -h: print this text to stderr and exit\n\n";
    if (successful) exit(0);
    else exit(2);
}

// We write our output to stdout, but want to ensure that the plugin
// doesn't write anything itself. To do this we open a null file
// descriptor and dup2() it into place of stdout in the gaps between
// our own output activity.

static int normalFd = -1;
static int suspendedFd = -1;

static void initFds(bool binary)
{
#ifdef _WIN32
    if (binary) {
        int result = _setmode(0, _O_BINARY);
        if (result == -1) {
            throw runtime_error("Failed to set binary mode on stdin");
        }
        result = _setmode(1, _O_BINARY);
        if (result == -1) {
            throw runtime_error("Failed to set binary mode on stdout");
        }
    }
    normalFd = _dup(1);
    suspendedFd = _open("NUL", _O_WRONLY);
#else
    (void)binary;
    normalFd = dup(1);
    suspendedFd = open("/dev/null", O_WRONLY);
#endif
    
    if (normalFd < 0 || suspendedFd < 0) {
        throw runtime_error("Failed to initialise fds for stdio suspend/resume");
    }
}

static void suspendOutput()
{
#ifdef _WIN32
    _dup2(suspendedFd, 1);
#else
    dup2(suspendedFd, 1);
#endif
}

static void resumeOutput()
{
#ifdef _WIN32
    _dup2(normalFd, 1);
#else
    dup2(normalFd, 1);
#endif
}

ListResponse
makeRequest(string soname, bool debug)
{
    ListRequest req;
    req.from.push_back(soname);
    return LoaderRequests().listPluginData(req);
}

struct PlausibleMetadata
{
    string className;
    string adapterName;
};

PlausibleMetadata
inventPlausibleMetadata(string key)
{
    PlausibleMetadata pm;
    pm.className = "MyPlugin";//!!!
    pm.adapterName = "myPluginAdapter";//!!!
    return pm;
}

void
emitFor(string soname, const ListResponse &resp, bool debug)
{
    cout <<
        "\n#include \"PiperExport.h\"\n"
        "\n"
        "// #include your own plugin headers here\n"
        "\n"
        "using piper_vamp_js::PiperAdapter;\n"
        "using piper_vamp_js::PiperPluginLibrary;\n"
        "\n"
        "static std::string soname(\"" << soname << "\");\n"
        "\n";

    bool first = true;
    
    for (const auto &plugin: resp.available) {
        PlausibleMetadata pm = inventPlausibleMetadata(plugin.pluginKey);
        cout << "static PiperAdapter<"
             << pm.className
             << "> // replace with the actual Vamp plugin class name for \""
             << plugin.basic.identifier << "\" plugin\n" << pm.adapterName
             << "(\n    soname,\n    ";
        
        string catString = "{ ";
        first = true;
        for (auto c: plugin.category) {
            if (!first) catString += ", ";
            catString += "\"" + c + "\"";
            first = false;
        }
        catString += " }";
        cout << catString << ",\n    ";

        cout << "{\n    ";
        first = true;
        for (auto o: plugin.staticOutputInfo) {
            if (!first) {
                cout << ",\n    ";
            }
            cout << "    ";
            string outputId = o.first;
            const StaticOutputDescriptor &desc = o.second;
            cout << "{ \"" << outputId << "\",\n            { \""
                 << desc.typeURI << "\" }\n        }";
            first = false;
        }
        cout << "\n    }\n";
        cout << "    );\n\n";
    }

    cout << "static PiperPluginLibrary library({\n    ";
    first = true;
    for (const auto &plugin: resp.available) {
        PlausibleMetadata pm = inventPlausibleMetadata(plugin.pluginKey);
        if (!first) {
            cout << ",\n    ";
        }
        cout << "&" << pm.adapterName;
        first = false;
    }
    cout << "\n});\n\n";
    cout << "PIPER_EXPORT_LIBRARY(library);\n" << endl;
}    

int main(int argc, char **argv)
{
    if (argc != 2 && argc != 3) {
        usage();
    }

    bool debug = false;
    
    string arg = argv[1];
    if (arg == "-h") {
        if (argc == 2) {
            usage(true);
        } else {
            usage();
        }
    } else if (arg == "-v") {
        if (argc == 2) {
            version();
        } else {
            usage();
        }
    } else if (arg == "-d") {
        if (argc == 2) {
            usage();
        } else {
            debug = true;
            arg = argv[2];
        }
    }
    
    string soname = arg;

    try {            
        initFds(false);
    } catch (exception &e) {
        cerr << "ERROR: " << e.what() << endl;
        exit(1);
    }

    suspendOutput();

    ListResponse resp = makeRequest(soname, debug);

    resumeOutput();

    emitFor(soname, resp, debug);
    
    exit(0);
}
