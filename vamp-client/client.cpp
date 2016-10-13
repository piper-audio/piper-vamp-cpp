
#include "ProcessQtTransport.h"
#include "CapnpRRClient.h"

#include <stdexcept>

using std::cerr;
using std::endl;

int main(int, char **)
{
    piper::vampclient::ProcessQtTransport transport("../bin/piper-vamp-server");
    piper::vampclient::CapnpRRClient client(&transport);

    Vamp::HostExt::ListResponse lr = client.listPluginData();
    cerr << "Plugins available:" << endl;
    int i = 1;
    for (const auto &p: lr.available) {
        cerr << i++ << ". [" << p.pluginKey << "] " << p.basic.name << endl;
    }
    
    Vamp::HostExt::LoadRequest req;
    req.pluginKey = "vamp-example-plugins:zerocrossing";
    req.inputSampleRate = 16;
    Vamp::HostExt::LoadResponse resp = client.loadPlugin(req);
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
}

