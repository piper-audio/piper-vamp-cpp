#ifndef TST_PIPER_JSON_CLIENT_H
#define TST_PIPER_JSON_CLIENT_H

#include "catch/catch.hpp"
#include "vamp-support/RequestResponse.h"
#include "vamp-client/JsonClient.h"
#include "vamp-client/SynchronousTransport.h"
#include <functional>
#include <map>
#include <vector>
#include <string>
#include <algorithm>

namespace piper_vamp
{
    // For assertion convenience, operator== is desired for a given type. 
    // One could implement a decorator.. to add it to individual types (wrap)
    // or, could just implement operator==() as members of various types...
    // it will suffice to have these free functions here for now
    
    bool operator==(const PluginStaticData::Basic& lhs,
                    const PluginStaticData::Basic& rhs)
    {
        return lhs.identifier == rhs.identifier &&
               lhs.name == rhs.name &&
               lhs.description == rhs.description;
    }

    bool operator==(const PluginStaticData& lhs,
                    const PluginStaticData& rhs) 
    {        
        // need to compare the list of params,
        // defining an operator== fn for Vamp::PluginBase::ParameterDescriptor
        // would work, as the std::vector == would then compare all elements, 
        // comparing manually here instead
        const bool hasEqualParameters =
            lhs.parameters.size() == rhs.parameters.size() &&
            std::equal(
                lhs.parameters.begin(),
                lhs.parameters.end(),
                rhs.parameters.begin(),
                [](const Vamp::PluginBase::ParameterDescriptor& lhs, 
                   const Vamp::PluginBase::ParameterDescriptor& rhs) -> bool {
                    return  lhs.identifier == rhs.identifier &&
                            lhs.name == rhs.name &&
                            lhs.description == rhs.description &&
                            lhs.unit == rhs.unit &&
                            lhs.minValue == rhs.minValue &&
                            lhs.maxValue == rhs.maxValue &&
                            lhs.defaultValue == rhs.defaultValue &&
                            lhs.isQuantized == rhs.isQuantized &&
                            lhs.quantizeStep == rhs.quantizeStep &&
                            lhs.valueNames == rhs.valueNames;
                }
            );

        return  lhs.pluginKey == rhs.pluginKey &&
                lhs.basic == rhs.basic &&
                lhs.maker == rhs.maker &&
                lhs.copyright == rhs.copyright &&
                lhs.pluginVersion == rhs.pluginVersion &&
                lhs.category == rhs.category &&
                lhs.minChannelCount == rhs.minChannelCount &&
                lhs.maxChannelCount == rhs.maxChannelCount &&
                lhs.programs == rhs.programs &&
                lhs.inputDomain == rhs.inputDomain &&
                lhs.basicOutputInfo == rhs.basicOutputInfo && 
                hasEqualParameters;
    }

    bool operator==(const ListResponse& lhs, const ListResponse& rhs)
    {
        return lhs.available == rhs.available;
    }
    
    bool operator==(const PluginConfiguration& lhs,
                    const PluginConfiguration& rhs)
    {
        return  lhs.channelCount == rhs.channelCount &&
                lhs.framing.blockSize == rhs.framing.blockSize &&
                lhs.framing.stepSize == rhs.framing.stepSize &&
                lhs.parameterValues == rhs.parameterValues &&
                lhs.currentProgram == rhs.currentProgram;
    }
}

using namespace piper_vamp::client;
using namespace piper_vamp;

class StubSynchronousTransport : public SynchronousTransport
{
public:
    using RpcMethod = std::string;
    using WireRpcResponse = std::vector<char>;
    using WireRpcRequest = const char*;
    
    StubSynchronousTransport() : m_okay(true)
    {}
    
    WireRpcResponse call(WireRpcRequest data, 
                         size_t bytes,
                         RpcMethod /*type*/, 
                         bool /*slow*/) override
    {   
        m_lastRequest = std::string {data, data + bytes};        
        return {
            m_nextResponse.begin(), 
            m_nextResponse.end()
        };    
    }
    
    bool isOK() const override
    {
        return m_okay;
    }
    
    void setCompletenessChecker(MessageCompletenessChecker*) override
    {}
    
    void setIsOK(const bool ok)
    {
        m_okay = ok;
    }
    
    void setNextResponse(std::string nextResponse)
    {
        m_nextResponse = std::move(nextResponse);
    }
    
    std::string getLastRequest() const
    {
        return m_lastRequest;
    }
    
private:
    bool m_okay;
    std::string m_nextResponse;
    std::string m_lastRequest;
};

// As a server handles actually interpretting requests, 
// testing the semantics of responses any further than ensuring matching type,
// i.e. filtering list requests produce a response with desired subset, 
// calling process of a non-configured extractor fails etc, 
// are not appropriate here.

// Also it isn't really appropriate to be testing anything relating to parsing
// or serialising here, as that should be tested elsewhere...

// What is important here is that the client can handle each response type
// and reports errors in the form of different exceptions for well-formed errors
// and unexpected cases

// it is also possible to do some white-box testing..
// i.e inspecting the raw request the server receives, 
// and make some assertions about that, even though that seems a bit redundant
// in that it basically just tests the serialisation logic... and it also 
// assumes some implementation details

PluginStaticData createStubStaticData()
{
    // TODO copied from tst_PluginStub - start building up some fixtures?
    Vamp::PluginBase::ParameterDescriptor stubParam;
    stubParam.identifier = "framing-scale";
    stubParam.name = "Framing Scale Factor";
    stubParam.description = "Scales the preferred framing sizes";
    stubParam.maxValue = 2.0;
    stubParam.defaultValue = 1.0;

    PluginStaticData staticData;
    staticData.pluginKey = "stub";
    staticData.basic = {"param-init", "Stub", "Testing init"};
    staticData.maker = "Lucas Thompson";
    staticData.copyright = "GPL";
    staticData.pluginVersion = 1;
    staticData.category = {"Test"};
    staticData.minChannelCount = 1;
    staticData.maxChannelCount = 1;
    staticData.parameters = {stubParam};
    staticData.inputDomain = Vamp::Plugin::InputDomain::TimeDomain;
    staticData.basicOutputInfo = {{"output", "NA", "Not real"}};
    return staticData;
}

TEST_CASE("JsonClient::list")
{
    // Start the request id from 0 for each test section
    JsonClient::RequestId nextId = 0;
    const auto localScopeCountingId = [&]() -> JsonClient::RequestId {
        return nextId++;   
    };
    
    const auto transport = std::make_shared<StubSynchronousTransport>();
    
    JsonClient client {
        transport,
        localScopeCountingId
    };

    SECTION("Handles valid ListResponses")
    {        
        // manually writing out the JSON here just because... well,  
        // JSON is easily readable / writable.. it might make sense to
        // use the functions in VampJson to do this.. that would almost
        // certainly be done in tests for other, non human readable, binary 
        // serialisation formats..
        transport->setNextResponse(R"({
            "id": 0,
            "method": "list", 
            "result": {
              "available": [
                {
                  "key": "stub",
                  "basic": {
                    "identifier": "param-init",
                    "name": "Stub",
                    "description": "Testing init"
                  },
                  "version": 1,
                  "minChannelCount": 1,
                  "maxChannelCount": 1,
                  "inputDomain": "TimeDomain",
                  "basicOutputInfo": [
                    {
                      "identifier": "output",
                      "name": "NA",
                      "description": "Not real"
                    }
                  ],
                  "parameters": [
                    {
                      "basic": {
                        "identifier": "framing-scale",
                        "name": "Framing Scale Factor",
                        "description": "Scales the preferred framing sizes"
                      },
                      "extents": {
                        "min": 0.0,
                        "max": 2.0
                      },
                      "defaultValue": 1.0
                    }
                  ],
                  "rights": "GPL",
                  "category": ["Test"],
                  "maker": "Lucas Thompson"
                }
              ]
            }
        })");

        const auto staticData = createStubStaticData(); 

        ListResponse expected;
        expected.available.push_back(staticData);
        
        REQUIRE( expected == client.list({}) );
        // try a populated ListRequest, just incase that causes an error
        ListRequest filtered;
        filtered.from.push_back("NA");
        nextId = 0; // reset so we can check again, without altering the JSON 
        REQUIRE( expected == client.list(filtered) );
    }
    
    SECTION("Throws ProtocolError on valid ListResponse with mistmatched id")
    {
        transport->setNextResponse(R"({
            "id": 666,
            "jsonrpc": "2.0", 
            "method": "list",
            "result": {
                "available": []
            }
        })");
        REQUIRE_THROWS_AS( client.list({}), ProtocolError );
    }
    
    // this is to be consistent with behaviour outlined in comments (and code)
    // in CapnpRRClient. TODO confirm
    SECTION("Throws ProtocolError for mistmatched id, generally (even Errors)")
    {
        transport->setNextResponse(R"({
            "id": "1979",
            "jsonrpc": "2.0", 
            "method": "process", 
            "result": {
                "features": {}, 
                "handle": 1
            }
        })");
        REQUIRE_THROWS_AS( client.list({}), ProtocolError );
        transport->setNextResponse(R"({
            "id": "1979",
            "jsonrpc": "2.0", 
            "method": "process", 
            "error": {
                "code": 0, 
                "message": "Testing valid error!"
            }
        })");
        REQUIRE_THROWS_AS( client.list({}), ProtocolError );
    }
    
    // this is to be consistent with behaviour outlined in comments (and code)
    // in CapnpRRClient. TODO confirm
    SECTION("Throws ServiceError on error, despite mismatched response type")
    {
        transport->setNextResponse(R"({
            "id": 0,
            "jsonrpc": "2.0", 
            "method": "process", 
            "error": {
                "code": 0, 
                "message": "Testing valid error!"
            }
        })");
        REQUIRE_THROWS_AS( client.list({}), ServiceError );
    }
    
    SECTION("Throws ProtocolError on mismatched response type")
    {
        transport->setNextResponse(R"({
            "id": 0,
            "jsonrpc": "2.0", 
            "method": "process", 
            "result": {
                "features": {}, 
                "handle": 1
            }
        })");
        REQUIRE_THROWS_AS( client.list({}), ProtocolError );
    }
    
    SECTION("Throws ProtocolError on invalid JSON")
    {
        transport->setNextResponse(R"({
            <NONSENSE>
            #!/not/a/valid/json/string
            </NONSENSE>
        })");
        REQUIRE_THROWS_AS( client.list({}), ProtocolError );
    }
    
    SECTION("Throws ProtocolError for schema non-complient JSON")
    {
        transport->setNextResponse(R"({
            "id": 0,
            "name": "Lucas Thompson", 
            "occupation": "Software Engineer in Test"
        })");
        REQUIRE_THROWS_AS( client.list({}), ProtocolError );
    }
    
    SECTION("Throws ServiceError on server error response with matching id")
    {
        transport->setNextResponse(R"({
            "id": 0,
            "jsonrpc": "2.0", 
            "method": "list",
            "error": {
                "code": 0, 
                "message": "Testing valid error!"
            }
        })");
        REQUIRE_THROWS_AS( client.list({}), ServiceError );
    }
}

TEST_CASE("JsonClient::load")
{
    // Start the request id from 0 for each test section
    JsonClient::RequestId nextId = 0;
    const auto capturedIdCounter = [&]() -> JsonClient::RequestId {
        return nextId++;   
    };
    
    const auto transport = std::make_shared<StubSynchronousTransport>();
    
    JsonClient client {
        transport,
        capturedIdCounter
    };
    
    LoadRequest req;
    req.inputSampleRate = 16.f; 
    req.pluginKey = "stub"; 
    
    SECTION("Handles valid LoadResponses")
    {
        LoadResponse expected;
        expected.staticData = createStubStaticData();
        PluginConfiguration defaultConfig;
        defaultConfig.framing.blockSize = 16;
        defaultConfig.framing.stepSize = 8;
        defaultConfig.channelCount = 1;
        defaultConfig.parameterValues = {{"framing-scale", 1.0}};
        expected.defaultConfiguration = defaultConfig;
        transport->setNextResponse(R"({
            "id": 0,
            "jsonrpc": "2.0",
            "method": "load",
            "result": {
                "handle": 123,
                "key": "stub",
                "staticData": {
                  "key": "stub",
                  "basic": {
                    "identifier": "param-init",
                    "name": "Stub",
                    "description": "Testing init"
                  },
                  "version": 1,
                  "minChannelCount": 1,
                  "maxChannelCount": 1,
                  "inputDomain": "TimeDomain",
                  "basicOutputInfo": [
                    {
                      "identifier": "output",
                      "name": "NA",
                      "description": "Not real"
                    }
                  ],
                  "parameters": [
                    {
                      "basic": {
                        "identifier": "framing-scale",
                        "name": "Framing Scale Factor",
                        "description": "Scales the preferred framing sizes"
                      },
                      "extents": {
                        "min": 0.0,
                        "max": 2.0
                      },
                      "defaultValue": 1.0
                    }
                  ],
                  "rights": "GPL",
                  "category": ["Test"],
                  "maker": "Lucas Thompson"
                },
                "defaultConfiguration": {
                    "framing": {
                        "blockSize": 16,
                        "stepSize": 8
                    },
                    "channelCount": 1,
                    "parameterValues": {
                        "framing-scale": 1.0
                    }
                }
            }
        })");
        
        const auto response = client.load(req);
        // compare staticData and defaultConfig... don't care about plugin yet
        REQUIRE( response.staticData == expected.staticData );
        REQUIRE(response.defaultConfiguration == expected.defaultConfiguration);
    }
}

TEST_CASE("JsonClient::configure")
{
    // Start the request id from 0 for each test section
    // JsonClient::RequestId nextId = 0;
    // const auto capturedIdCounter = [&]() -> JsonClient::RequestId {
    //     return nextId++;
    // };
    //
    // const auto transport = std::make_shared<StubSynchronousTransport>();
    //
    // JsonClient client {
    //     transport,
    //     capturedIdCounter
    // };
    //
    // ConfigurationRequest req; // don't have an actual plugin
    // req.
    //
    // SECTION("Handles valid configuration responses")
    // {
    //
    // }
}

#endif