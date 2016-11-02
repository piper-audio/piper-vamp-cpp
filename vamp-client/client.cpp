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

#include "ProcessQtTransport.h"
#include "CapnpRRClient.h"
#include "AutoPlugin.h"

#include <stdexcept>

using std::cerr;
using std::endl;

int main(int, char **)
{
    piper_vamp::client::ProcessQtTransport transport("../bin/piper-vamp-simple-server",
                                                     "capnp");
    piper_vamp::client::CapnpRRClient client(&transport);

    piper_vamp::ListResponse lr = client.listPluginData({});
    cerr << "Plugins available:" << endl;
    int i = 1;
    for (const auto &p: lr.available) {
        cerr << i++ << ". [" << p.pluginKey << "] " << p.basic.name << endl;
    }
    
    piper_vamp::LoadRequest req;
    req.pluginKey = "vamp-example-plugins:zerocrossing";
    req.inputSampleRate = 16;
    piper_vamp::LoadResponse resp = client.loadPlugin(req);
    Vamp::Plugin *plugin = resp.plugin;
    
    if (!plugin->initialise(1, 4, 4)) {
        cerr << "initialisation failed" << endl;
    } else {
        std::vector<float> buf = { 1.0, -1.0, 1.0, -1.0 };
        float *bd = buf.data();
        Vamp::Plugin::FeatureSet features = plugin->process
            (&bd, Vamp::RealTime::zeroTime);
        cerr << "results for output 0:" << endl;
        auto fl(features[0]);
        for (const auto &f: fl) {
            cerr << f.values[0] << endl;
        }
    }

    (void)plugin->getRemainingFeatures();

    cerr << "calling reset..." << endl;
    plugin->reset();
    cerr << "...round 2!" << endl;

    std::vector<float> buf = { 1.0, -1.0, 1.0, -1.0 };
    float *bd = buf.data();
    Vamp::Plugin::FeatureSet features = plugin->process
        (&bd, Vamp::RealTime::zeroTime);
    cerr << "results for output 0:" << endl;
    auto fl(features[0]);
    for (const auto &f: fl) {
        cerr << f.values[0] << endl;
    }
    
    (void)plugin->getRemainingFeatures();

    delete plugin;

    // Let's try a crazy AutoPlugin

    piper_vamp::client::AutoPlugin ap("../bin/piper-vamp-simple-server",
                                      "vamp-example-plugins:zerocrossing", 16, 0);
    if (!ap.isOK()) {
        cerr << "AutoPlugin creation failed" << endl;
    } else {
        if (!ap.initialise(1, 4, 4)) {
            cerr << "initialisation failed" << endl;
        } else {
            std::vector<float> buf = { 1.0, -1.0, 1.0, -1.0 };
            float *bd = buf.data();
            Vamp::Plugin::FeatureSet features = ap.process
                (&bd, Vamp::RealTime::zeroTime);
            cerr << "results for output 0:" << endl;
            auto fl(features[0]);
            for (const auto &f: fl) {
                cerr << f.values[0] << endl;
            }
        }
    }
}

