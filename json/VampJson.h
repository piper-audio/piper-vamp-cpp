/* -*- c-basic-offset: 4 indent-tabs-mode: nil -*-  vi:set ts=8 sts=4 sw=4: */

/*
    VamPipe

    Centre for Digital Music, Queen Mary, University of London.
    Copyright 2015-2016 QMUL.
  
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
#include "bits/PluginOutputIdMapper.h"
#include "bits/RequestResponseType.h"

namespace vampipe {

/**
 * Convert the structures laid out in the Vamp SDK classes into JSON
 * (and back again) following the schema in the vamp-json-schema
 * project repo.
 */
class VampJson
{
public:
    /** Serialisation format for arrays of floats (process input and
     *  feature values). Structures that can be serialised in more
     *  than one way will include either a "values" field (for Text
     *  serialisation) or a "b64values" field (for Base64) but should
     *  not include both. When parsing, if a "b64values" field is
     *  found, it will always take priority over a "values" field.
     */
    enum class BufferSerialisation {

        /** Default JSON serialisation of values in array form. This
         *  is relatively slow to parse and serialise, and can take a
         *  lot of space.
         */
        Text,

        /** Base64-encoded string of the raw data as packed IEEE
         *  32-bit floats. Faster and more compact than the text
         *  encoding but more complicated to provide, especially if
         *  starting from an environment that does not use IEEE 32-bit
         *  floats! Note that Base64 serialisations produced by this
         *  library do not including padding characters and so are not
         *  necessarily multiples of 4 characters long. You will need
         *  to pad them yourself if concatenating them or supplying to
         *  a consumer that expects padding.
         */
        Base64
    };
    
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
    fromFeature(const Vamp::Plugin::Feature &f,
                BufferSerialisation serialisation) {

        json11::Json::object jo;
        if (f.values.size() > 0) {
            if (serialisation == BufferSerialisation::Text) {
                jo["values"] = json11::Json::array(f.values.begin(),
                                                   f.values.end());
            } else {
                jo["b64values"] = fromFloatBuffer(f.values.data(),
                                                  f.values.size());
            }
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
    toFeature(json11::Json j,
              BufferSerialisation &serialisation) {

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
            serialisation = BufferSerialisation::Base64;
        } else if (j["values"].is_array()) {
            for (auto v : j["values"].array_items()) {
                f.values.push_back(v.number_value());
            }
            serialisation = BufferSerialisation::Text;
        }
        f.label = j["label"].string_value();
        return f;
    }

    static json11::Json
    fromFeatureSet(const Vamp::Plugin::FeatureSet &fs,
                   const PluginOutputIdMapper &omapper,
                   BufferSerialisation serialisation) {

        json11::Json::object jo;
        for (const auto &fsi : fs) {
            std::vector<json11::Json> fj;
            for (const Vamp::Plugin::Feature &f: fsi.second) {
                fj.push_back(fromFeature(f, serialisation));
            }
            jo[omapper.indexToId(fsi.first)] = fj;
        }
        return json11::Json(jo);
    }

    static Vamp::Plugin::FeatureList
    toFeatureList(json11::Json j,
                  BufferSerialisation &serialisation) {

        Vamp::Plugin::FeatureList fl;
        if (!j.is_array()) {
            throw Failure("array expected for feature list");
        }
        for (const json11::Json &fj : j.array_items()) {
            fl.push_back(toFeature(fj, serialisation));
        }
        return fl;
    }

    static Vamp::Plugin::FeatureSet
    toFeatureSet(json11::Json j,
                 const PluginOutputIdMapper &omapper,
                 BufferSerialisation &serialisation) {

        Vamp::Plugin::FeatureSet fs;
        if (!j.is_object()) {
            throw Failure("object expected for feature set");
        }
        for (auto &entry : j.object_items()) {
            int n = omapper.idToIndex(entry.first);
            if (fs.find(n) != fs.end()) {
                throw Failure("duplicate numerical index for output");
            }
            fs[n] = toFeatureList(entry.second, serialisation);
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
    fromLoadRequest(const Vamp::HostExt::LoadRequest &req) {

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
                    { "inputSampleRate", json11::Json::NUMBER } }, err)) {
            throw Failure("malformed load request: " + err);
        }
    
        Vamp::HostExt::LoadRequest req;
        req.pluginKey = j["pluginKey"].string_value();
        req.inputSampleRate = j["inputSampleRate"].number_value();
        if (!j["adapterFlags"].is_null()) {
            req.adapterFlags = toAdapterFlags(j["adapterFlags"]);
        }
        return req;
    }

    static json11::Json
    fromLoadResponse(const Vamp::HostExt::LoadResponse &resp,
                     const PluginHandleMapper &pmapper) {

        json11::Json::object jo;
        jo["pluginHandle"] = double(pmapper.pluginToHandle(resp.plugin));
        jo["staticData"] = fromPluginStaticData(resp.staticData);
        jo["defaultConfiguration"] =
            fromPluginConfiguration(resp.defaultConfiguration);
        return json11::Json(jo);
    }

    static Vamp::HostExt::LoadResponse
    toLoadResponse(json11::Json j,
                   const PluginHandleMapper &pmapper) {

        std::string err;

        if (!j.has_shape({
                    { "pluginHandle", json11::Json::NUMBER },
                    { "staticData", json11::Json::OBJECT },
                    { "defaultConfiguration", json11::Json::OBJECT } }, err)) {
            throw Failure("malformed load response: " + err);
        }

        Vamp::HostExt::LoadResponse resp;
        resp.plugin = pmapper.handleToPlugin(j["pluginHandle"].int_value());
        resp.staticData = toPluginStaticData(j["staticData"]);
        resp.defaultConfiguration = toPluginConfiguration(j["defaultConfiguration"]);
        return resp;
    }

    static json11::Json
    fromConfigurationRequest(const Vamp::HostExt::ConfigurationRequest &cr,
                             const PluginHandleMapper &pmapper) {

        json11::Json::object jo;

        jo["pluginHandle"] = pmapper.pluginToHandle(cr.plugin);
        jo["configuration"] = fromPluginConfiguration(cr.configuration);
        
        return json11::Json(jo);
    }

    static Vamp::HostExt::ConfigurationRequest
    toConfigurationRequest(json11::Json j,
                           const PluginHandleMapper &pmapper) {

        std::string err;

        if (!j.has_shape({
                    { "pluginHandle", json11::Json::NUMBER },
                    { "configuration", json11::Json::OBJECT } }, err)) {
            throw Failure("malformed configuration request: " + err);
        }

        Vamp::HostExt::ConfigurationRequest cr;
        cr.plugin = pmapper.handleToPlugin(j["pluginHandle"].int_value());
        cr.configuration = toPluginConfiguration(j["configuration"]);
        return cr;
    }

    static json11::Json
    fromConfigurationResponse(const Vamp::HostExt::ConfigurationResponse &cr,
                              const PluginHandleMapper &pmapper) {

        json11::Json::object jo;

        jo["pluginHandle"] = pmapper.pluginToHandle(cr.plugin);
        
        json11::Json::array outs;
        for (auto &d: cr.outputs) {
            outs.push_back(fromOutputDescriptor(d));
        }
        jo["outputList"] = outs;
        
        return json11::Json(jo);
    }

    static Vamp::HostExt::ConfigurationResponse
    toConfigurationResponse(json11::Json j,
                            const PluginHandleMapper &pmapper) {
        
        Vamp::HostExt::ConfigurationResponse cr;

        cr.plugin = pmapper.handleToPlugin(j["pluginHandle"].int_value());
        
        if (!j["outputList"].is_array()) {
            throw Failure("array expected for output list");
        }

        for (const auto &o: j["outputList"].array_items()) {
            cr.outputs.push_back(toOutputDescriptor(o));
        }

        return cr;
    }

    static json11::Json
    fromProcessRequest(const Vamp::HostExt::ProcessRequest &r,
                       const PluginHandleMapper &pmapper,
                       BufferSerialisation serialisation) {

        json11::Json::object jo;
        jo["pluginHandle"] = pmapper.pluginToHandle(r.plugin);

        json11::Json::object io;
        io["timestamp"] = fromRealTime(r.timestamp);

        json11::Json::array chans;
        for (size_t i = 0; i < r.inputBuffers.size(); ++i) {
            json11::Json::object c;
            if (serialisation == BufferSerialisation::Text) {
                c["values"] = json11::Json::array(r.inputBuffers[i].begin(),
                                                  r.inputBuffers[i].end());
            } else {
                c["b64values"] = fromFloatBuffer(r.inputBuffers[i].data(),
                                                 r.inputBuffers[i].size());
            }
            chans.push_back(c);
        }
        io["inputBuffers"] = chans;
        
        jo["processInput"] = io;
        return json11::Json(jo);
    }

    static Vamp::HostExt::ProcessRequest
    toProcessRequest(json11::Json j,
                     const PluginHandleMapper &pmapper,
                     BufferSerialisation &serialisation) {

        std::string err;

        if (!j.has_shape({
                    { "pluginHandle", json11::Json::NUMBER },
                    { "processInput", json11::Json::OBJECT } }, err)) {
            throw Failure("malformed process request: " + err);
        }

        auto input = j["processInput"];

        if (!input.has_shape({
                    { "timestamp", json11::Json::OBJECT },
                    { "inputBuffers", json11::Json::ARRAY } }, err)) {
            throw Failure("malformed process request: " + err);
        }

        Vamp::HostExt::ProcessRequest r;
        r.plugin = pmapper.handleToPlugin(j["pluginHandle"].int_value());

        r.timestamp = toRealTime(input["timestamp"]);

        for (auto a: input["inputBuffers"].array_items()) {

            if (a["b64values"].is_string()) {
                std::vector<float> buf = toFloatBuffer(a["b64values"].string_value());
                r.inputBuffers.push_back(buf);
                serialisation = BufferSerialisation::Base64;

            } else if (a["values"].is_array()) {
                std::vector<float> buf;
                for (auto v : a["values"].array_items()) {
                    buf.push_back(v.number_value());
                }
                r.inputBuffers.push_back(buf);
                serialisation = BufferSerialisation::Text;

            } else {
                throw Failure("expected values or b64values in inputBuffers object");
            }
        }

        return r;
    }

    static json11::Json
    fromVampRequest_List() {

        json11::Json::object jo;
        jo["type"] = "list";
        return json11::Json(jo);
    }

    static json11::Json
    fromVampResponse_List(std::string errorText,
                          const Vamp::HostExt::ListResponse &resp) {

        json11::Json::object jo;
        jo["type"] = "list";
        jo["success"] = (errorText == "");
        jo["errorText"] = errorText;

        json11::Json::array arr;
        for (const auto &a: resp.pluginData) {
            arr.push_back(fromPluginStaticData(a));
        }
        json11::Json::object po;
        po["plugins"] = arr;

        jo["content"] = po;
        return json11::Json(jo);
    }
    
    static json11::Json
    fromVampRequest_Load(const Vamp::HostExt::LoadRequest &req) {

        json11::Json::object jo;
        jo["type"] = "load";
        jo["content"] = fromLoadRequest(req);
        return json11::Json(jo);
    }    

    static json11::Json
    fromVampResponse_Load(const Vamp::HostExt::LoadResponse &resp,
                          const PluginHandleMapper &pmapper) {

        json11::Json::object jo;
        jo["type"] = "load";
        jo["success"] = (resp.plugin != 0);
        jo["errorText"] = "";
        jo["content"] = fromLoadResponse(resp, pmapper);
        return json11::Json(jo);
    }

    static json11::Json
    fromVampRequest_Configure(const Vamp::HostExt::ConfigurationRequest &req,
                              const PluginHandleMapper &pmapper) {

        json11::Json::object jo;
        jo["type"] = "configure";
        jo["content"] = fromConfigurationRequest(req, pmapper);
        return json11::Json(jo);
    }    

    static json11::Json
    fromVampResponse_Configure(const Vamp::HostExt::ConfigurationResponse &resp,
                               const PluginHandleMapper &pmapper) {
        
        json11::Json::object jo;
        jo["type"] = "configure";
        jo["success"] = (!resp.outputs.empty());
        jo["errorText"] = "";
        jo["content"] = fromConfigurationResponse(resp, pmapper);
        return json11::Json(jo);
    }
    
    static json11::Json
    fromVampRequest_Process(const Vamp::HostExt::ProcessRequest &req,
                            const PluginHandleMapper &pmapper,
                            BufferSerialisation serialisation) {

        json11::Json::object jo;
        jo["type"] = "process";
        jo["content"] = fromProcessRequest(req, pmapper, serialisation);
        return json11::Json(jo);
    }    

    static json11::Json
    fromVampResponse_Process(const Vamp::HostExt::ProcessResponse &resp,
                             const PluginHandleMapper &pmapper,
                             BufferSerialisation serialisation) {
        
        json11::Json::object jo;
        jo["type"] = "process";
        jo["success"] = true;
        jo["errorText"] = "";
        json11::Json::object po;
        po["pluginHandle"] = pmapper.pluginToHandle(resp.plugin);
        po["features"] = fromFeatureSet(resp.features,
                                        *pmapper.pluginToOutputIdMapper(resp.plugin),
                                        serialisation);
        jo["content"] = po;
        return json11::Json(jo);
    }
    
    static json11::Json
    fromVampRequest_Finish(const Vamp::HostExt::FinishRequest &req,
                           const PluginHandleMapper &pmapper) {

        json11::Json::object jo;
        jo["type"] = "finish";
        json11::Json::object fo;
        fo["pluginHandle"] = pmapper.pluginToHandle(req.plugin);
        jo["content"] = fo;
        return json11::Json(jo);
    }    
    
    static json11::Json
    fromVampResponse_Finish(const Vamp::HostExt::ProcessResponse &resp,
                            const PluginHandleMapper &pmapper,
                            BufferSerialisation serialisation) {

        json11::Json::object jo;
        jo["type"] = "finish";
        jo["success"] = true;
        jo["errorText"] = "";
        json11::Json::object po;
        po["pluginHandle"] = pmapper.pluginToHandle(resp.plugin);
        po["features"] = fromFeatureSet(resp.features,
                                        *pmapper.pluginToOutputIdMapper(resp.plugin),
                                        serialisation);
        jo["content"] = po;
        return json11::Json(jo);
    }

    static json11::Json
    fromError(std::string errorText, RRType responseType) {

        json11::Json::object jo;
        std::string type;

        if (responseType == RRType::List) type = "list";
        else if (responseType == RRType::Load) type = "load";
        else if (responseType == RRType::Configure) type = "configure";
        else if (responseType == RRType::Process) type = "process";
        else if (responseType == RRType::Finish) type = "finish";
        else type = "invalid";

        jo["type"] = type;
        jo["success"] = false;
        jo["errorText"] = std::string("error in ") + type + " request: " + errorText;
        return json11::Json(jo);
    }

    static json11::Json
    fromException(const std::exception &e, RRType responseType) {

        return fromError(e.what(), responseType);
    }
    
private: // go private briefly for a couple of helper functions
    
    static void
    checkTypeField(json11::Json j, std::string expected) {
        if (!j["type"].is_string()) {
            throw Failure("string expected for type");
        }
        if (j["type"].string_value() != expected) {
            throw Failure("expected value \"" + expected + "\" for type");
        }
    }

    static bool
    successful(json11::Json j) {
        if (!j["success"].is_bool()) {
            throw Failure("bool expected for success");
        }
        return j["success"].bool_value();
    }

public:
    static RRType
    getRequestResponseType(json11::Json j) {

        if (!j["type"].is_string()) {
            throw Failure("string expected for type");
        }
        
        std::string type = j["type"].string_value();

	if (type == "list") return RRType::List;
	else if (type == "load") return RRType::Load;
	else if (type == "configure") return RRType::Configure;
	else if (type == "process") return RRType::Process;
	else if (type == "finish") return RRType::Finish;
	else {
	    throw Failure("unknown or unexpected request/response type \"" +
                          type + "\"");
	}
    }

    static void
    toVampRequest_List(json11::Json j) {
        
        checkTypeField(j, "list");
    }

    static Vamp::HostExt::ListResponse
    toVampResponse_List(json11::Json j) {

        Vamp::HostExt::ListResponse resp;
        if (successful(j)) {
            for (const auto &a: j["content"]["plugins"].array_items()) {
                resp.pluginData.push_back(toPluginStaticData(a));
            }
        }
        return resp;
    }

    static Vamp::HostExt::LoadRequest
    toVampRequest_Load(json11::Json j) {
        
        checkTypeField(j, "load");
        return toLoadRequest(j["content"]);
    }
    
    static Vamp::HostExt::LoadResponse
    toVampResponse_Load(json11::Json j, const PluginHandleMapper &pmapper) {
        
        Vamp::HostExt::LoadResponse resp;
        if (successful(j)) {
            resp = toLoadResponse(j["content"], pmapper);
        }
        return resp;
    }
    
    static Vamp::HostExt::ConfigurationRequest
    toVampRequest_Configure(json11::Json j, const PluginHandleMapper &pmapper) {
        
        checkTypeField(j, "configure");
        return toConfigurationRequest(j["content"], pmapper);
    }
    
    static Vamp::HostExt::ConfigurationResponse
    toVampResponse_Configure(json11::Json j, const PluginHandleMapper &pmapper) {
        
        Vamp::HostExt::ConfigurationResponse resp;
        if (successful(j)) {
            resp = toConfigurationResponse(j["content"], pmapper);
        }
        return resp;
    }
    
    static Vamp::HostExt::ProcessRequest
    toVampRequest_Process(json11::Json j, const PluginHandleMapper &pmapper,
                          BufferSerialisation &serialisation) {
        
        checkTypeField(j, "process");
        return toProcessRequest(j["content"], pmapper, serialisation);
    }
    
    static Vamp::HostExt::ProcessResponse
    toVampResponse_Process(json11::Json j,
                           const PluginHandleMapper &pmapper,
                           BufferSerialisation &serialisation) {
        
        Vamp::HostExt::ProcessResponse resp;
        if (successful(j)) {
            auto jc = j["content"];
            auto h = jc["pluginHandle"].int_value();
            resp.plugin = pmapper.handleToPlugin(h);
            resp.features = toFeatureSet(jc["features"],
                                         *pmapper.handleToOutputIdMapper(h),
                                         serialisation);
        }
        return resp;
    }
    
    static Vamp::HostExt::FinishRequest
    toVampRequest_Finish(json11::Json j, const PluginHandleMapper &pmapper) {
        
        checkTypeField(j, "finish");
        Vamp::HostExt::FinishRequest req;
        req.plugin = pmapper.handleToPlugin
            (j["content"]["pluginHandle"].int_value());
        return req;
    }
    
    static Vamp::HostExt::ProcessResponse
    toVampResponse_Finish(json11::Json j,
                          const PluginHandleMapper &pmapper,
                          BufferSerialisation &serialisation) {
        
        Vamp::HostExt::ProcessResponse resp;
        if (successful(j)) {
            auto jc = j["content"];
            auto h = jc["pluginHandle"].int_value();
            resp.plugin = pmapper.handleToPlugin(h);
            resp.features = toFeatureSet(jc["features"],
                                         *pmapper.handleToOutputIdMapper(h),
                                         serialisation);
        }
        return resp;
    }
};

}

#endif
