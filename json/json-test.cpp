
#include "VampJson.h"

using std::cerr;
using std::endl;

int main(int, char **)
{
    Vamp::PluginBase::ParameterDescriptor d;
    d.identifier = "threshold";
    d.name = "Energy rise threshold";
    d.description = "Energy rise within a frequency bin necessary to count toward broadband total";
    d.unit = "dB";
    d.minValue = 0;
    d.maxValue = 20.5;
    d.defaultValue = 3;
    d.isQuantized = false;
    cerr << VampJson::fromParameterDescriptor(d).dump() << endl;

    Vamp::Plugin::OutputDescriptor od;
    od.identifier = "powerspectrum";
    od.name = "Power Spectrum";
    od.description = "Power values of the frequency spectrum bins calculated from the input signal";
    od.unit = "";
    od.hasFixedBinCount = true;
    od.binCount = 513;
    od.hasKnownExtents = false;
    od.isQuantized = false;
    od.sampleType = Vamp::Plugin::OutputDescriptor::OneSamplePerStep;
    cerr << VampJson::fromOutputDescriptor(od).dump() << endl;

    cerr << VampJson::fromFeature(Vamp::Plugin::Feature()).dump() << endl;
    
    Vamp::Plugin::Feature f;
    f.hasTimestamp = true;
    f.timestamp = Vamp::RealTime::fromSeconds(3.14159);
    f.hasDuration = false;
    f.values = { 1, 2, 3.000001, 4, 5, 6, 6.5, 7 };
    f.label = "A feature";
    cerr << VampJson::fromFeature(f).dump() << endl;

    Vamp::Plugin::FeatureSet fs;
    fs[0].push_back(f);
    std::string fs_str = VampJson::fromFeatureSet(fs).dump();
    cerr << fs_str << endl;

    std::string err;
    
    try {
	Vamp::Plugin::ParameterDescriptor d1 =
	    VampJson::toParameterDescriptor
	    (json11::Json::parse
	     (VampJson::fromParameterDescriptor(d).dump(), err));
	if (err != "") {
	    cerr << "error returned from parser: " << err << endl;
	}
	cerr << "\nsuccessfully converted parameter descriptor back from json: serialising it again, we get:" << endl;
	cerr << VampJson::fromParameterDescriptor(d1).dump() << endl;
    } catch (std::runtime_error &e) {
	cerr << "caught exception: " << e.what() << endl;
    }
    
    try {
	Vamp::Plugin::OutputDescriptor od1 =
	    VampJson::toOutputDescriptor
	    (json11::Json::parse
	     (VampJson::fromOutputDescriptor(od).dump(), err));
	if (err != "") {
	    cerr << "error returned from parser: " << err << endl;
	}
	cerr << "\nsuccessfully converted output descriptor back from json: serialising it again, we get:" << endl;
	cerr << VampJson::fromOutputDescriptor(od1).dump() << endl;
    } catch (std::runtime_error &e) {
	cerr << "caught exception: " << e.what() << endl;
    }
    
    try {
	Vamp::Plugin::FeatureSet fs1 =
	    VampJson::toFeatureSet
	    (json11::Json::parse(fs_str, err));
	if (err != "") {
	    cerr << "error returned from parser: " << err << endl;
	}
	cerr << "\nsuccessfully converted feature set back from json: serialising it again, we get:" << endl;
	cerr << VampJson::fromFeatureSet(fs1).dump() << endl;
    } catch (std::runtime_error &e) {
	cerr << "caught exception: " << e.what() << endl;
    }

    return 0;
}

    
    
