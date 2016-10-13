
#include "PiperStubPlugin.h"
#include "CapnpMessageCompletenessChecker.h"
#include "PipedQProcessTransport.h"
#include "PiperCapnpClient.h"

#include <stdexcept>

using std::cerr;
using std::endl;

int main(int, char **)
{
    piper::CapnpMessageCompletenessChecker checker;
    piper::PipedQProcessTransport transport("../bin/piper-vamp-server", &checker);
    piper::PiperCapnpClient client(&transport);
    
    Vamp::Plugin *plugin = client.load("vamp-example-plugins:zerocrossing", 16, 0);
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
    //!!! -- and also implement reset(), which will need to reconstruct internally
}

