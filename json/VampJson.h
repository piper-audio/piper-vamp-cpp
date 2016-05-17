/* -*- c-basic-offset: 4 indent-tabs-mode: nil -*-  vi:set ts=8 sts=4 sw=4: */

#ifndef VAMP_JSON_H
#define VAMP_JSON_H

#include <vector>
#include <string>
#include <sstream>
#include <stdexcept>

#include <json11/json11.hpp>
#include <base-n/include/basen.hpp>

#include <vamp-hostsdk/Plugin.h>
#include <vamp-hostsdk/PluginLoader.h>

#include "bits/PluginHandleMapper.h"

namespace vampipe {

/**
 * Convert the structures laid out in the Vamp SDK classes into JSON
 * (and back again) following the schema in the vamp-json-schema
 * project repo.
 */
class VampJson
{
public:
    class Failure : virtual public std::runtime_error {
    public:
        Failure(std::string s) : runtime_error(s) { }
    };
    
    template <typename T>
    static json11::Json
    fromBasicDescriptor(const T &t) {
        return json11::Json::object { 
            { "identifier", t.identifier },
            { "name", t.name },
            { "description", t.description }
        };
    }

    template <typename T>
    static void
    toBasicDescriptor(json11::Json j, T &t) {
        if (!j.is_object()) {
            throw Failure("object expected for basic descriptor content");
        }
        if (!j["identifier"].is_string()) {
            throw Failure("string expected for identifier");
        }
        t.identifier = j["identifier"].string_value();
        t.name = j["name"].string_value();
        t.description = j["description"].string_value();
    }

    template <typename T>
    static json11::Json
    fromValueExtents(const T &t) {
        return json11::Json::object {
            { "min", t.minValue },
            { "max", t.maxValue }
        };
    }

    template <typename T>
    static bool
    toValueExtents(json11::Json j, T &t) {
        if (j["extents"].is_null()) {
            return false;
        } else if (j["extents"].is_object()) {
            if (j["extents"]["min"].is_number() &&
                j["extents"]["max"].is_number()) {
                t.minValue = j["extents"]["min"].number_value();
                t.maxValue = j["extents"]["max"].number_value();
                return true;
            } else {
                throw Failure("numbers expected for min and max");
            }
        } else {
            throw Failure("object expected for extents (if present)");
        }
    }

    static json11::Json
    fromRealTime(const Vamp::RealTime &r) {
        return json11::Json::object {
            { "s", r.sec },
            { "n", r.nsec }
        };
    }

    static Vamp::RealTime
    toRealTime(json11::Json j) {
        json11::Json sec = j["s"];
        json11::Json nsec = j["n"];
        if (!sec.is_number() || !nsec.is_number()) {
            throw Failure("invalid Vamp::RealTime object " + j.dump());
        }
        return Vamp::RealTime(sec.int_value(), nsec.int_value());
    }

    static std::string
    fromSampleType(Vamp::Plugin::OutputDescriptor::SampleType type) {
        switch (type) {
        case Vamp::Plugin::OutputDescriptor::OneSamplePerStep:
            return "OneSamplePerStep";
        case Vamp::Plugin::OutputDescriptor::FixedSampleRate:
            return "FixedSampleRate";
        case Vamp::Plugin::OutputDescriptor::VariableSampleRate:
            return "VariableSampleRate";
        }
        return "";
    }

    static Vamp::Plugin::OutputDescriptor::SampleType
    toSampleType(std::string text) {
        if (text == "OneSamplePerStep") {
            return Vamp::Plugin::OutputDescriptor::OneSamplePerStep;
        } else if (text == "FixedSampleRate") {
            return Vamp::Plugin::OutputDescriptor::FixedSampleRate;
        } else if (text == "VariableSampleRate") {
            return Vamp::Plugin::OutputDescriptor::VariableSampleRate;
        } else {
            throw Failure("invalid sample type string: " + text);
        }
    }

    static json11::Json
    fromOutputDescriptor(const Vamp::Plugin::OutputDescriptor &desc) {
        json11::Json::object jo {
            { "basic", fromBasicDescriptor(desc) },
            { "unit", desc.unit },
            { "sampleType", fromSampleType(desc.sampleType) },
            { "sampleRate", desc.sampleRate },
            { "hasDuration", desc.hasDuration }
        };
        if (desc.hasFixedBinCount) {
            jo["binCount"] = int(desc.binCount);
            jo["binNames"] = json11::Json::array
                (desc.binNames.begin(), desc.binNames.end());
        }
        if (desc.hasKnownExtents) {
            jo["extents"] = fromValueExtents(desc);
        }
        if (desc.isQuantized) {
            jo["quantizeStep"] = desc.quantizeStep;
        }
        return json11::Json(jo);
    }

    static Vamp::Plugin::OutputDescriptor
    toOutputDescriptor(json11::Json j) {

        Vamp::Plugin::OutputDescriptor od;
        if (!j.is_object()) {
            throw Failure("object expected for output descriptor");
        }
    
        toBasicDescriptor(j["basic"], od);

        od.unit = j["unit"].string_value();

        od.sampleType = toSampleType(j["sampleType"].string_value());

        if (!j["sampleRate"].is_number()) {
            throw Failure("number expected for sample rate");
        }
        od.sampleRate = j["sampleRate"].number_value();
        od.hasDuration = j["hasDuration"].bool_value();

        if (j["binCount"].is_number() && j["binCount"].int_value() > 0) {
            od.hasFixedBinCount = true;
            od.binCount = j["binCount"].int_value();
            for (auto &n: j["binNames"].array_items()) {
                if (!n.is_string()) {
                    throw Failure("string expected for bin name");
                }
                od.binNames.push_back(n.string_value());
            }
        } else {
            od.hasFixedBinCount = false;
        }

        bool extentsPresent = toValueExtents(j, od);
        od.hasKnownExtents = extentsPresent;

        if (j["quantizeStep"].is_number()) {
            od.isQuantized = true;
            od.quantizeStep = j["quantizeStep"].number_value();
        } else {
            od.isQuantized = false;
        }

        return od;
    }

    static json11::Json
    fromParameterDescriptor(const Vamp::PluginBase::ParameterDescriptor &desc) {

        json11::Json::object jo {
            { "basic", fromBasicDescriptor(desc) },
            { "unit", desc.unit },
            { "extents", fromValueExtents(desc) },
            { "defaultValue", desc.defaultValue },
            { "valueNames", json11::Json::array
                    (desc.valueNames.begin(), desc.valueNames.end()) }
        };
        if (desc.isQuantized) {
            jo["quantizeStep"] = desc.quantizeStep;
        }
        return json11::Json(jo);
    }

    static Vamp::PluginBase::ParameterDescriptor
    toParameterDescriptor(json11::Json j) {

        Vamp::PluginBase::ParameterDescriptor pd;
        if (!j.is_object()) {
            throw Failure("object expected for parameter descriptor");
        }
    
        toBasicDescriptor(j["basic"], pd);

        pd.unit = j["unit"].string_value();

        bool extentsPresent = toValueExtents(j, pd);
        if (!extentsPresent) {
            throw Failure("extents must be present in parameter descriptor");
        }
    
        if (!j["defaultValue"].is_number()) {
            throw Failure("number expected for default value");
        }
    
        pd.defaultValue = j["defaultValue"].number_value();

        pd.valueNames.clear();
        for (auto &n: j["valueNames"].array_items()) {
            if (!n.is_string()) {
                throw Failure("string expected for value name");
            }
            pd.valueNames.push_back(n.string_value());
        }

        if (j["quantizeStep"].is_number()) {
            pd.isQuantized = true;
            pd.quantizeStep = j["quantizeStep"].number_value();
        } else {
            pd.isQuantized = false;
        }

        return pd;
    }

    static std::string
    fromFloatBuffer(const float *buffer, size_t nfloats) {
        // must use char pointers, otherwise the converter will only
        // encode every 4th byte (as it will count up in float* steps)
        const char *start = reinterpret_cast<const char *>(buffer);
        const char *end = reinterpret_cast<const char *>(buffer + nfloats);
        std::string encoded;
        bn::encode_b64(start, end, back_inserter(encoded));
        return encoded;
    }

    static std::vector<float>
    toFloatBuffer(std::string encoded) {
        std::string decoded;
        bn::decode_b64(encoded.begin(), encoded.end(), back_inserter(decoded));
        const float *buffer = reinterpret_cast<const float *>(decoded.c_str());
        size_t n = decoded.size() / sizeof(float);
        return std::vector<float>(buffer, buffer + n);
    }

    static json11::Json
    fromFeature(const Vamp::Plugin::Feature &f) {

        json11::Json::object jo;
        if (f.values.size() > 0) {
            jo["b64values"] = fromFloatBuffer(f.values.data(), f.values.size());
        }
        if (f.label != "") {
            jo["label"] = f.label;
        }
        if (f.hasTimestamp) {
            jo["timestamp"] = fromRealTime(f.timestamp);
        }
        if (f.hasDuration) {
            jo["duration"] = fromRealTime(f.duration);
        }
        return json11::Json(jo);
    }

    static Vamp::Plugin::Feature
    toFeature(json11::Json j) {

        Vamp::Plugin::Feature f;
        if (!j.is_object()) {
            throw Failure("object expected for feature");
        }
        if (j["timestamp"].is_object()) {
            f.timestamp = toRealTime(j["timestamp"]);
            f.hasTimestamp = true;
        }
        if (j["duration"].is_object()) {
            f.duration = toRealTime(j["duration"]);
            f.hasDuration = true;
        }
        if (j["b64values"].is_string()) {
            f.values = toFloatBuffer(j["b64values"].string_value());
        } else if (j["values"].is_array()) {
            for (auto v : j["values"].array_items()) {
                f.values.push_back(v.number_value());
            }
        }
        f.label = j["label"].string_value();
        return f;
    }

    static json11::Json
    fromFeatureSet(const Vamp::Plugin::FeatureSet &fs) {

        json11::Json::object jo;
        for (const auto &fsi : fs) {
            std::vector<json11::Json> fj;
            for (const Vamp::Plugin::Feature &f: fsi.second) {
                fj.push_back(fromFeature(f));
            }
            std::stringstream sstr;
            sstr << fsi.first;
            std::string n = sstr.str();
            jo[n] = fj;
        }
        return json11::Json(jo);
    }

    static Vamp::Plugin::FeatureList
    toFeatureList(json11::Json j) {

        Vamp::Plugin::FeatureList fl;
        if (!j.is_array()) {
            throw Failure("array expected for feature list");
        }
        for (const json11::Json &fj : j.array_items()) {
            fl.push_back(toFeature(fj));
        }
        return fl;
    }

    static Vamp::Plugin::FeatureSet
    toFeatureSet(json11::Json j) {

        Vamp::Plugin::FeatureSet fs;
        if (!j.is_object()) {
            throw Failure("object expected for feature set");
        }
        for (auto &entry : j.object_items()) {
            std::string nstr = entry.first;
            size_t count = 0;
            int n = stoi(nstr, &count);
            if (n < 0 || fs.find(n) != fs.end() || count < nstr.size()) {
                throw Failure("invalid or duplicate numerical index for output");
            }
            fs[n] = toFeatureList(entry.second);
        }
        return fs;
    }

    static std::string
    fromInputDomain(Vamp::Plugin::InputDomain domain) {

        switch (domain) {
        case Vamp::Plugin::TimeDomain:
            return "TimeDomain";
        case Vamp::Plugin::FrequencyDomain:
            return "FrequencyDomain";
        }
        return "";
    }

    static Vamp::Plugin::InputDomain
    toInputDomain(std::string text) {

        if (text == "TimeDomain") {
            return Vamp::Plugin::TimeDomain;
        } else if (text == "FrequencyDomain") {
            return Vamp::Plugin::FrequencyDomain;
        } else {
            throw Failure("invalid input domain string: " + text);
        }
    }

    static json11::Json
    fromPluginStaticData(const Vamp::HostExt::PluginStaticData &d) {

        json11::Json::object jo;
        jo["pluginKey"] = d.pluginKey;
        jo["basic"] = fromBasicDescriptor(d.basic);
        jo["maker"] = d.maker;
        jo["copyright"] = d.copyright;
        jo["pluginVersion"] = d.pluginVersion;

        json11::Json::array cat;
        for (const std::string &c: d.category) cat.push_back(c);
        jo["category"] = cat;

        jo["minChannelCount"] = d.minChannelCount;
        jo["maxChannelCount"] = d.maxChannelCount;

        json11::Json::array params;
        Vamp::PluginBase::ParameterList vparams = d.parameters;
        for (auto &p: vparams) params.push_back(fromParameterDescriptor(p));
        jo["parameters"] = params;

        json11::Json::array progs;
        Vamp::PluginBase::ProgramList vprogs = d.programs;
        for (auto &p: vprogs) progs.push_back(p);
        jo["programs"] = progs;

        jo["inputDomain"] = fromInputDomain(d.inputDomain);

        json11::Json::array outinfo;
        auto vouts = d.basicOutputInfo;
        for (auto &o: vouts) outinfo.push_back(fromBasicDescriptor(o));
        jo["basicOutputInfo"] = outinfo;
    
        return json11::Json(jo);
    }

    static Vamp::HostExt::PluginStaticData
    toPluginStaticData(json11::Json j) {

        std::string err;
        if (!j.has_shape({
                    { "pluginKey", json11::Json::STRING },
                    { "pluginVersion", json11::Json::NUMBER },
                    { "minChannelCount", json11::Json::NUMBER },
                    { "maxChannelCount", json11::Json::NUMBER },
                    { "inputDomain", json11::Json::STRING }}, err)) {
            throw Failure("malformed plugin static data: " + err);
        }

        if (!j["basicOutputInfo"].is_array()) {
            throw Failure("array expected for basic output info");
        }

        if (!j["maker"].is_null() &&
            !j["maker"].is_string()) {
            throw Failure("string expected for maker");
        }
        
        if (!j["copyright"].is_null() &&
            !j["copyright"].is_string()) {
            throw Failure("string expected for copyright");
        }

        if (!j["category"].is_null() &&
            !j["category"].is_array()) {
            throw Failure("array expected for category");
        }

        if (!j["parameters"].is_null() &&
            !j["parameters"].is_array()) {
            throw Failure("array expected for parameters");
        }

        if (!j["programs"].is_null() &&
            !j["programs"].is_array()) {
            throw Failure("array expected for programs");
        }

        if (!j["inputDomain"].is_null() &&
            !j["inputDomain"].is_string()) {
            throw Failure("string expected for inputDomain");
        }

        if (!j["basicOutputInfo"].is_null() &&
            !j["basicOutputInfo"].is_array()) {
            throw Failure("array expected for basicOutputInfo");
        }

        Vamp::HostExt::PluginStaticData psd;

        psd.pluginKey = j["pluginKey"].string_value();

        toBasicDescriptor(j["basic"], psd.basic);

        psd.maker = j["maker"].string_value();
        psd.copyright = j["copyright"].string_value();
        psd.pluginVersion = j["pluginVersion"].int_value();

        for (const auto &c : j["category"].array_items()) {
            if (!c.is_string()) {
                throw Failure("strings expected in category array");
            }
            psd.category.push_back(c.string_value());
        }

        psd.minChannelCount = j["minChannelCount"].int_value();
        psd.maxChannelCount = j["maxChannelCount"].int_value();

        for (const auto &p : j["parameters"].array_items()) {
            auto pd = toParameterDescriptor(p);
            psd.parameters.push_back(pd);
        }

        for (const auto &p : j["programs"].array_items()) {
            if (!p.is_string()) {
                throw Failure("strings expected in programs array");
            }
            psd.programs.push_back(p.string_value());
        }

        psd.inputDomain = toInputDomain(j["inputDomain"].string_value());

        for (const auto &bo : j["basicOutputInfo"].array_items()) {
            Vamp::HostExt::PluginStaticData::Basic b;
            toBasicDescriptor(bo, b);
            psd.basicOutputInfo.push_back(b);
        }

        return psd;
    }

    static json11::Json
    fromPluginConfiguration(const Vamp::HostExt::PluginConfiguration &c) {

        json11::Json::object jo;

        json11::Json::object paramValues;
        for (auto &vp: c.parameterValues) {
            paramValues[vp.first] = vp.second;
        }
        jo["parameterValues"] = paramValues;

        if (c.currentProgram != "") {
            jo["currentProgram"] = c.currentProgram;
        }

        jo["channelCount"] = c.channelCount;
        jo["stepSize"] = c.stepSize;
        jo["blockSize"] = c.blockSize;
    
        return json11::Json(jo);
    }

    static Vamp::HostExt::PluginConfiguration
    toPluginConfiguration(json11::Json j) {
        
        std::string err;
        if (!j.has_shape({
                    { "channelCount", json11::Json::NUMBER },
                    { "stepSize", json11::Json::NUMBER },
                    { "blockSize", json11::Json::NUMBER } }, err)) {
            throw Failure("malformed plugin configuration: " + err);
        }

        if (!j["parameterValues"].is_null() &&
            !j["parameterValues"].is_object()) {
            throw Failure("object expected for parameter values");
        }

        for (auto &pv : j["parameterValues"].object_items()) {
            if (!pv.second.is_number()) {
                throw Failure("number expected for parameter value");
            }
        }
    
        if (!j["currentProgram"].is_null() &&
            !j["currentProgram"].is_string()) {
            throw Failure("string expected for program name");
        }

        Vamp::HostExt::PluginConfiguration config;

        config.channelCount = j["channelCount"].number_value();
        config.stepSize = j["stepSize"].number_value();
        config.blockSize = j["blockSize"].number_value();
        
        for (auto &pv : j["parameterValues"].object_items()) {
            config.parameterValues[pv.first] = pv.second.number_value();
        }

        if (j["currentProgram"].is_string()) {
            config.currentProgram = j["currentProgram"].string_value();
        }

        return config;
    }

    static json11::Json
    fromAdapterFlags(int flags) {

        json11::Json::array arr;

        if (flags & Vamp::HostExt::PluginLoader::ADAPT_INPUT_DOMAIN) {
            arr.push_back("AdaptInputDomain");
        }
        if (flags & Vamp::HostExt::PluginLoader::ADAPT_CHANNEL_COUNT) {
            arr.push_back("AdaptChannelCount");
        }
        if (flags & Vamp::HostExt::PluginLoader::ADAPT_BUFFER_SIZE) {
            arr.push_back("AdaptBufferSize");
        }

        return json11::Json(arr);
    }

    static Vamp::HostExt::PluginLoader::AdapterFlags
    toAdapterFlags(json11::Json j) {

        if (!j.is_array()) {
            throw Failure("array expected for adapter flags");
        }
        int flags = 0x0;

        for (auto &jj: j.array_items()) {
            if (!jj.is_string()) {
                throw Failure("string expected for adapter flag");
            }
            std::string text = jj.string_value();
            if (text == "AdaptInputDomain") {
                flags |= Vamp::HostExt::PluginLoader::ADAPT_INPUT_DOMAIN;
            } else if (text == "AdaptChannelCount") {
                flags |= Vamp::HostExt::PluginLoader::ADAPT_CHANNEL_COUNT;
            } else if (text == "AdaptBufferSize") {
                flags |= Vamp::HostExt::PluginLoader::ADAPT_BUFFER_SIZE;
            } else if (text == "AdaptAllSafe") {
                flags |= Vamp::HostExt::PluginLoader::ADAPT_ALL_SAFE;
            } else if (text == "AdaptAll") {
                flags |= Vamp::HostExt::PluginLoader::ADAPT_ALL;
            } else {
                throw Failure("invalid adapter flag string: " + text);
            }
        }

        return Vamp::HostExt::PluginLoader::AdapterFlags(flags);
    }

    static json11::Json
    fromLoadRequest(Vamp::HostExt::LoadRequest req) {

        json11::Json::object jo;
        jo["pluginKey"] = req.pluginKey;
        jo["inputSampleRate"] = req.inputSampleRate;
        jo["adapterFlags"] = fromAdapterFlags(req.adapterFlags);
        return json11::Json(jo);
    }

    static Vamp::HostExt::LoadRequest
    toLoadRequest(json11::Json j) {
        
        std::string err;

        if (!j.has_shape({
                    { "pluginKey", json11::Json::STRING },
                    { "inputSampleRate", json11::Json::NUMBER },
                    { "adapterFlags", json11::Json::ARRAY } }, err)) {
            throw VampJson::Failure("malformed load request: " + err);
        }
    
        Vamp::HostExt::LoadRequest req;
        req.pluginKey = j["pluginKey"].string_value();
        req.inputSampleRate = j["inputSampleRate"].number_value();
        req.adapterFlags = toAdapterFlags(j["adapterFlags"]);
        return req;
    }

    static json11::Json
    fromLoadResponse(Vamp::HostExt::LoadResponse resp,
                     PluginHandleMapper &mapper) {

        json11::Json::object jo;
        jo["pluginHandle"] = double(mapper.pluginToHandle(resp.plugin));
        jo["staticData"] = fromPluginStaticData(resp.staticData);
        jo["defaultConfiguration"] =
            fromPluginConfiguration(resp.defaultConfiguration);
        return json11::Json(jo);
    }

    static Vamp::HostExt::LoadResponse
    toLoadResponse(json11::Json j,
                   PluginHandleMapper &mapper) {

        std::string err;

        if (!j.has_shape({
                    { "pluginHandle", json11::Json::NUMBER },
                    { "staticData", json11::Json::OBJECT },
                    { "defaultConfiguration", json11::Json::OBJECT } }, err)) {
            throw VampJson::Failure("malformed load response: " + err);
        }

        Vamp::HostExt::LoadResponse resp;
        resp.plugin = mapper.handleToPlugin(j["pluginHandle"].int_value());
        resp.staticData = toPluginStaticData(j["staticData"]);
        resp.defaultConfiguration = toPluginConfiguration(j["defaultConfiguration"]);
        return resp;
    }
};

}

#endif
