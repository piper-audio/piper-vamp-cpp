/* -*- c-basic-offset: 4 indent-tabs-mode: nil -*-  vi:set ts=8 sts=4 sw=4: */
/*
  Piper C++

  An API for audio analysis and feature extraction plugins.

  Centre for Digital Music, Queen Mary, University of London.
  Copyright 2006-2017 Chris Cannam, Lucas Thompson, and QMUL.
  
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
  Digital Music; Queen Mary, University of London; Chris Cannam; and
  Lucas Thompson; shall not be used in advertising or otherwise to 
  promote the sale, use or other dealings in this Software without 
  prior written authorization.
*/

#ifndef PIPER_JSON_CLIENT_H
#define PIPER_JSON_CLIENT_H

#include "PluginClient.h"
#include "Loader.h"
#include "SynchronousTransport.h"
#include "vamp-json/VampJson.h"
#include "vamp-support/AssignedPluginHandleMapper.h"
#include "vamp-support/PreservingPluginHandleMapper.h"
#include "PiperVampPlugin.h"
#include "Exceptions.h"
#include <memory>
#include <functional>

namespace piper_vamp {
namespace client {

class JsonClient : public PluginClient, public Loader
{
public:
    using AudioBuffer = std::vector<std::vector<float>>;
    using RequestId = uint32_t;// std::atomic<uint32_t>? string? RequestOrResponce::RpcId?
    using RequestIdProvider = std::function<RequestId()>; // crazy? // consider interface, perhaps RequestIdProvider<T>::getNextId() -> T
    
    JsonClient(std::shared_ptr<SynchronousTransport> transport,
               RequestIdProvider getNextId,
               VampJson::BufferSerialisation format 
                   = VampJson::BufferSerialisation::Base64)
        : m_transport(transport), getNextId(getNextId), m_bufferFormat(format)
    {}
    
    ListResponse list(const ListRequest& request) override
    {
        return handle<int, ListRequest, ListResponse>( 
            "list",
            request,
            VampJson::fromRpcRequest_List,
            VampJson::toRpcResponse_List
        );
    }
    
    LoadResponse load(const LoadRequest& request) override
    {        
        return handle<int, LoadRequest, LoadResponse>(
            "load",
            request,
            VampJson::fromRpcRequest_Load,
            [&](json11::Json j, std::string& err) -> LoadResponse {
                PreservingPluginHandleMapper passthrough;
                auto response = 
                    VampJson::toRpcResponse_Load(j, passthrough, err);
            
                const auto handle = passthrough.pluginToHandle(
                    response.plugin
                );
            
                response.plugin = new PiperVampPlugin(
                    this, // needs a client, we are one!
                    request.pluginKey,
                    request.inputSampleRate,
                    request.adapterFlags,
                    response.staticData,
                    response.defaultConfiguration
                ); // feel uneasy about allocating here.. who owns this?
                  //  I believe it is ultimately the plugin host.. 
                 //   or... AutoPlugin (owned by host)?
                m_mapper.addPlugin(
                    handle,
                    response.plugin
                ); // TODO this could throw, causing a memory leak?
                return response;
            }
        );
    }

    ConfigurationResponse
    configure(PiperVampPlugin* plugin,
              PluginConfiguration config) override
    {
        // seems unnecessary to populate a ConfigurationRequest here,
        // the method could just take one as a parameter
        ConfigurationRequest request;
        request.plugin = plugin;
        request.configuration = config;
        using ReqIdType = int;
        return handle<ReqIdType, ConfigurationRequest, ConfigurationResponse>(
            "configure",
            request,
            [&](const ConfigurationRequest& req, ReqIdType id) -> json11::Json {
                return VampJson::fromRpcRequest_Configure(req, m_mapper, id);
            },
            [&](json11::Json j, std::string& err) -> ConfigurationResponse {
                return VampJson::toRpcResponse_Configure(j, m_mapper, err);
            }
        );
    }
    
    Vamp::Plugin::FeatureSet
    process(PiperVampPlugin* plugin,
            AudioBuffer inputBuffers,
            Vamp::RealTime timestamp) override
    {
        // seems unnecessary to populate a ProcessRequest here,
        // the method could just take one as a parameter
        ProcessRequest request;
        request.plugin = plugin;
        request.inputBuffers = inputBuffers;
        request.timestamp = timestamp;
        using RequestIdType = int;
        return handle<RequestIdType, ProcessRequest, Vamp::Plugin::FeatureSet>(
            "process",
            request,
            [&](const ProcessRequest& req, RequestIdType id) -> json11::Json {
                return VampJson::fromRpcRequest_Process(
                    req, 
                    m_mapper,
                    m_bufferFormat,
                    id
                );
            },
            [&](json11::Json j, std::string& err) -> Vamp::Plugin::FeatureSet {
                const auto response = VampJson::toRpcResponse_Process(
                    j, 
                    m_mapper, 
                    m_bufferFormat,
                    err
                );
                return response.features; 
            }
        );
    }

    Vamp::Plugin::FeatureSet
    finish(PiperVampPlugin* plugin) override
    {
        FinishRequest request;
        request.plugin = plugin;
        using RequestIdType = int;
        return handle<RequestIdType, FinishRequest, Vamp::Plugin::FeatureSet>(
            "finish",
            request,
            [&](const FinishRequest& req, RequestIdType id) -> json11::Json {
                return VampJson::fromRpcRequest_Finish(req, m_mapper, id);
            },
            [&](json11::Json j, std::string& err) -> Vamp::Plugin::FeatureSet {
                const auto response = VampJson::toRpcResponse_Finish(
                    j,
                    m_mapper,
                    m_bufferFormat,
                    err
                );
                m_mapper.removePlugin(m_mapper.pluginToHandle(plugin));
                return response.features;
            }
        );
    }

    void
    reset(PiperVampPlugin* /*plugin*/,
          PluginConfiguration /*config*/) override
    {
  
    }
private:
    AssignedPluginHandleMapper m_mapper;
    std::shared_ptr<SynchronousTransport> m_transport;
    RequestIdProvider getNextId;
    VampJson::BufferSerialisation m_bufferFormat;
    
    template<typename RequestIdType, class Request>
    using RequestSerialiser =
        std::function<json11::Json(const Request&, const RequestIdType&)>;

    template<class Response>
    using ResponseParser = 
        std::function<Response(json11::Json, std::string&)>; 
        
    template<typename RequestIdType, class Request, class Response>
    Response handle(const std::string& method,
                    const Request& request,
                    const RequestSerialiser<RequestIdType, Request>& serialise,
                    const ResponseParser<Response>& parse) const
    {
        if (!m_transport->isOK()) throw ServerCrashed();
        
        const auto requestId = getNextId();
        const auto wireData = serialise(
            request, 
            static_cast<RequestIdType>(requestId) // making uint32_t pointless?
        ).dump();
        
        const auto wireResponse = m_transport->call(
            wireData.c_str(), 
            wireData.size(),
            method, 
            false // TODO when is slow appropriate?
        );

        std::string err;
        const auto parsedJson = json11::Json::parse(
            { wireResponse.begin(), wireResponse.end() },
            err
        );  
        
        // a few different errors could've occured by now, 
        // let's see if we can give up yet...
        const bool isParseError = err != "";
        if (isParseError) throw ProtocolError(err.c_str());
        
        // extract this into a private member?
        const bool hasMatchingId = parsedJson["id"].is_number() &&
            parsedJson["id"].number_value() == requestId;
        const bool isSuccess = parsedJson["result"].is_object();
        const bool isError = parsedJson["error"].is_object();
        const bool isNotSchemaComplient = !isError && !isSuccess;
        
        // TODO whilst VampJson methods check for method mismatches when
        // parsing request types, they do not for response types.... 
        // do here for now. 
        // though calling VampJson::getRequestResponseType would populate
        // the err string...
        const bool hasMismatchedMethod = parsedJson["method"].is_string() &&
            parsedJson["method"] != method;
        
        if (isNotSchemaComplient) { // happens that this would fall out later...
            throw ProtocolError("Non-compliant response");
        } 
        if (hasMatchingId && isError) {
            throw ServiceError(parsedJson["error"]["message"].string_value());
        } 
        if (!hasMatchingId) {
            throw ProtocolError("Wrong response id");
        }
        if (hasMismatchedMethod) {
            throw ProtocolError("Wrong response type");
        }
    
        // ah, well... moving on!
        
        // TODO duplication.. above, and in general... 
        // VampJson::toRpcResponse_* uses private fn VampJson::successful
        // internally, mostly populating err if there is an error..
        // there is one case where that doesn't happen, which was written
        // that way to allow logic to fall out as desired in convert.cpp
        // regarding allowing to write out error responses
        // but here it results in not knowing an error happened...
        // well, unless we duplicate similar logic as used in convert.cpp
        // which is what happened above, i.e. inspecting the parsed json
        // Actually... readResponseJson in convert.cpp does 
        // most of the logic needed in this entire class...
        // should probably do something about re-using that :))
        
        // should arguably just return parsedJson here and do the
        // rest at the call site, avoiding passing std::functions
        // and code bloat from, probably unnecessarily, using templates
        auto response = parse(parsedJson, err);
        
        // some more possible errors, will all be Protocol related by now?
        if (err != "") throw ProtocolError(err.c_str());
        return response;
    }
};

} // ::client
} // piper_vamp::

#endif