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
#include "vamp-capnp/VampnProto.h"
#include "vamp-support/RequestOrResponse.h"
#include "vamp-support/CountingPluginHandleMapper.h"
#include "vamp-support/LoaderRequests.h"

#include <iostream>
#include <sstream>
#include <stdexcept>

#include <capnp/serialize.h>

#include <map>
#include <set>

// pid for logging
#ifdef _WIN32
#include <process.h>
static int pid = _getpid();
#else
#include <unistd.h>
static int pid = getpid();
#endif

// for _setmode stuff
#ifdef _WIN32
#include <io.h>
#include <fcntl.h>
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
        ": Load & run Vamp plugins in response to Piper messages\n\n"
        "    Usage: " << myname << " [-d] <format>\n"
        "           " << myname << " -v\n"
        "           " << myname << " -h\n\n"
        "    where\n"
        "       <format>: the format to read and write messages in (\"json\" or \"capnp\")\n"
        "       -d: also print debug information to stderr\n"
        "       -v: print version number to stdout and exit\n"
        "       -h: print this text to stderr and exit\n\n"
        "Expects Piper request messages in either Cap'n Proto or JSON format on stdin,\n"
        "and writes response messages in the same format to stdout.\n\n"
        "This server is intended for simple process separation. It's only suitable for\n"
        "use with a single trusted client per server invocation.\n\n"
        "The two formats behave differently in case of parser errors. JSON messages are\n"
        "expected one per input line; because the JSON support is really intended for\n"
        "interactive troubleshooting, any unparseable message is reported and discarded\n"
        "and the server waits for another message. In contrast, because of the assumption\n"
        "that the client is trusted and coupled to the server instance, a mangled\n"
        "Cap'n Proto message causes the server to exit.\n\n";
    if (successful) exit(0);
    else exit(2);
}

static CountingPluginHandleMapper mapper;

static RequestOrResponse::RpcId
readId(const piper::RpcRequest::Reader &r)
{
    int number;
    string tag;
    switch (r.getId().which()) {
    case piper::RpcRequest::Id::Which::NUMBER:
        number = r.getId().getNumber();
        return { RequestOrResponse::RpcId::Number, number, "" };
    case piper::RpcRequest::Id::Which::TAG:
        tag = r.getId().getTag();
        return { RequestOrResponse::RpcId::Tag, 0, tag };
    case piper::RpcRequest::Id::Which::NONE:
        return { RequestOrResponse::RpcId::Absent, 0, "" };
    }
    return {};
}

static void
buildId(piper::RpcResponse::Builder &b, const RequestOrResponse::RpcId &id)
{
    switch (id.type) {
    case RequestOrResponse::RpcId::Number:
        b.getId().setNumber(id.number);
        break;
    case RequestOrResponse::RpcId::Tag:
        b.getId().setTag(id.tag);
        break;
    case RequestOrResponse::RpcId::Absent:
        b.getId().setNone();
        break;
    }
}

static RequestOrResponse::RpcId
readJsonId(const Json &j)
{
    RequestOrResponse::RpcId id;

    if (j["id"].is_number()) {
        id.type = RequestOrResponse::RpcId::Number;
        id.number = j["id"].number_value();
    } else if (j["id"].is_string()) {
        id.type = RequestOrResponse::RpcId::Tag;
        id.tag = j["id"].string_value();
    } else {
        id.type = RequestOrResponse::RpcId::Absent;
    }

    return id;
}

static Json
writeJsonId(const RequestOrResponse::RpcId &id)
{
    if (id.type == RequestOrResponse::RpcId::Number) {
        return id.number;
    } else if (id.type == RequestOrResponse::RpcId::Tag) {
        return id.tag;
    } else {
        return Json();
    }
}

static Json
convertRequestJson(string input, string &err)
{
    Json j = Json::parse(input, err);
    if (err != "") {
        err = "invalid json: " + err;
        return {};
    }
    if (!j.is_object()) {
        err = "object expected at top level";
    } else if (!j["method"].is_string()) {
        err = "string expected for method field";
    } else if (!j["params"].is_null() && !j["params"].is_object()) {
        err = "object expected for params field";
    }
    return j;
}

RequestOrResponse
readRequestJson(string &err)
{
    RequestOrResponse rr;
    rr.direction = RequestOrResponse::Request;

    string input;
    if (!getline(cin, input)) {
        // the EOF case, not actually an error
        rr.type = RRType::NotValid;
        return rr;
    }
    
    Json j = convertRequestJson(input, err);
    if (err != "") return {};

    rr.type = VampJson::getRequestResponseType(j, err);
    if (err != "") return {};

    rr.id = readJsonId(j);

    VampJson::BufferSerialisation serialisation =
        VampJson::BufferSerialisation::Array;

    switch (rr.type) {

    case RRType::List:
        rr.listRequest = VampJson::toRpcRequest_List(j, err);
        break;
    case RRType::Load:
        rr.loadRequest = VampJson::toRpcRequest_Load(j, err);
        break;
    case RRType::Configure:
        rr.configurationRequest = VampJson::toRpcRequest_Configure(j, mapper, err);
        break;
    case RRType::Process:
        rr.processRequest = VampJson::toRpcRequest_Process(j, mapper, serialisation, err);
        break;
    case RRType::Finish:
        rr.finishRequest = VampJson::toRpcRequest_Finish(j, mapper, err);
        break;
    case RRType::NotValid:
        break;
    }

    return rr;
}

void
writeResponseJson(RequestOrResponse &rr, bool useBase64)
{
    Json j;

    VampJson::BufferSerialisation serialisation =
        (useBase64 ?
         VampJson::BufferSerialisation::Base64 :
         VampJson::BufferSerialisation::Array);

    Json id = writeJsonId(rr.id);

    if (!rr.success) {

        j = VampJson::fromError(rr.errorText, rr.type, id);

    } else {
    
        switch (rr.type) {

        case RRType::List:
            j = VampJson::fromRpcResponse_List(rr.listResponse, id);
            break;
        case RRType::Load:
            j = VampJson::fromRpcResponse_Load(rr.loadResponse, mapper, id);
            break;
        case RRType::Configure:
            j = VampJson::fromRpcResponse_Configure(rr.configurationResponse,
                                                    mapper, id);
            break;
        case RRType::Process:
            j = VampJson::fromRpcResponse_Process
                (rr.processResponse, mapper, serialisation, id);
            break;
        case RRType::Finish:
            j = VampJson::fromRpcResponse_Finish
                (rr.finishResponse, mapper, serialisation, id);
            break;
        case RRType::NotValid:
            break;
        }
    }
    
    cout << j.dump() << endl;
}

void
writeExceptionJson(const std::exception &e, RRType type)
{
    Json j = VampJson::fromError(e.what(), type, Json());
    cout << j.dump() << endl;
}

RequestOrResponse
readRequestCapnp()
{
    RequestOrResponse rr;
    rr.direction = RequestOrResponse::Request;

    static kj::FdInputStream stream(0); // stdin
    static kj::BufferedInputStreamWrapper buffered(stream);

    if (buffered.tryGetReadBuffer() == nullptr) {
        rr.type = RRType::NotValid;
        return rr;
    }

    capnp::InputStreamMessageReader message(buffered);
    piper::RpcRequest::Reader reader = message.getRoot<piper::RpcRequest>();
    
    rr.type = VampnProto::getRequestResponseType(reader);
    rr.id = readId(reader);

    switch (rr.type) {

    case RRType::List:
        VampnProto::readRpcRequest_List(rr.listRequest, reader);
        break;
    case RRType::Load:
        VampnProto::readRpcRequest_Load(rr.loadRequest, reader);
        break;
    case RRType::Configure:
        VampnProto::readRpcRequest_Configure(rr.configurationRequest,
                                             reader, mapper);
        break;
    case RRType::Process:
        VampnProto::readRpcRequest_Process(rr.processRequest, reader, mapper);
        break;
    case RRType::Finish:
        VampnProto::readRpcRequest_Finish(rr.finishRequest, reader, mapper);
        break;
    case RRType::NotValid:
        break;
    }

    return rr;
}

void
writeResponseCapnp(RequestOrResponse &rr)
{
    capnp::MallocMessageBuilder message;
    piper::RpcResponse::Builder builder = message.initRoot<piper::RpcResponse>();

    buildId(builder, rr.id);
    
    if (!rr.success) {

        VampnProto::buildRpcResponse_Error(builder, rr.errorText, rr.type);

    } else {
        
        switch (rr.type) {

        case RRType::List:
            VampnProto::buildRpcResponse_List(builder, rr.listResponse);
            break;
        case RRType::Load:
            VampnProto::buildRpcResponse_Load(builder, rr.loadResponse, mapper);
            break;
        case RRType::Configure:
            VampnProto::buildRpcResponse_Configure(builder, rr.configurationResponse, mapper);
            break;
        case RRType::Process:
            VampnProto::buildRpcResponse_Process(builder, rr.processResponse, mapper);
            break;
        case RRType::Finish:
            VampnProto::buildRpcResponse_Finish(builder, rr.finishResponse, mapper);
            break;
        case RRType::NotValid:
            break;
        }
    }
    
    writeMessageToFd(1, message);
}

void
writeExceptionCapnp(const std::exception &e, RRType type)
{
    capnp::MallocMessageBuilder message;
    piper::RpcResponse::Builder builder = message.initRoot<piper::RpcResponse>();
    VampnProto::buildRpcResponse_Exception(builder, e, type);
    
    writeMessageToFd(1, message);
}

RequestOrResponse
handleRequest(const RequestOrResponse &request, bool debug)
{
    RequestOrResponse response;
    response.direction = RequestOrResponse::Response;
    response.type = request.type;

    switch (request.type) {

    case RRType::List:
        response.listResponse =
            LoaderRequests().listPluginData(request.listRequest);
        response.success = true;
        break;

    case RRType::Load:
        response.loadResponse =
            LoaderRequests().loadPlugin(request.loadRequest);
        if (response.loadResponse.plugin != nullptr) {
            mapper.addPlugin(response.loadResponse.plugin);
            if (debug) {
                cerr << "piper-vamp-server " << pid
                     << ": loaded plugin, handle = "
                     << mapper.pluginToHandle(response.loadResponse.plugin)
                     << endl;
            }
            response.success = true;
        }
        break;
        
    case RRType::Configure:
    {
        auto &creq = request.configurationRequest;
        auto h = mapper.pluginToHandle(creq.plugin);
        if (mapper.isConfigured(h)) {
            throw runtime_error("plugin has already been configured");
        }

        response.configurationResponse = LoaderRequests().configurePlugin(creq);
        
        if (!response.configurationResponse.outputs.empty()) {
            mapper.markConfigured
                (h, creq.configuration.channelCount, creq.configuration.blockSize);
            response.success = true;
        }
        break;
    }

    case RRType::Process:
    {
        auto &preq = request.processRequest;
        auto h = mapper.pluginToHandle(preq.plugin);
        if (!mapper.isConfigured(h)) {
            throw runtime_error("plugin has not been configured");
        }

        int channels = int(preq.inputBuffers.size());
        if (channels != mapper.getChannelCount(h)) {
            throw runtime_error("wrong number of channels supplied to process");
        }
                
        const float **fbuffers = new const float *[channels];
        for (int i = 0; i < channels; ++i) {
            if (int(preq.inputBuffers[i].size()) != mapper.getBlockSize(h)) {
                delete[] fbuffers;
                throw runtime_error("wrong block size supplied to process");
            }
            fbuffers[i] = preq.inputBuffers[i].data();
        }

        response.processResponse.plugin = preq.plugin;
        response.processResponse.features =
            preq.plugin->process(fbuffers, preq.timestamp);
        response.success = true;

        delete[] fbuffers;
        break;
    }

    case RRType::Finish:
    {
        auto &freq = request.finishRequest;
        response.finishResponse.plugin = freq.plugin;

        auto h = mapper.pluginToHandle(freq.plugin);
        // Finish can be called (to unload the plugin) even if the
        // plugin has never been configured or used. But we want to
        // make sure we call getRemainingFeatures only if we have
        // actually configured the plugin.
        if (mapper.isConfigured(h)) {
            response.finishResponse.features = freq.plugin->getRemainingFeatures();
        }

        // We do not delete the plugin here -- we need it in the
        // mapper when converting the features. It gets deleted in the
        // calling function.
        response.success = true;
        break;
    }

    case RRType::NotValid:
        break;
    }
    
    return response;
}

RequestOrResponse
readRequest(string format)
{
    if (format == "capnp") {
        return readRequestCapnp();
    } else if (format == "json") {
        string err;
        auto result = readRequestJson(err);
        if (err != "") throw runtime_error(err);
        else return result;
    } else {
        throw runtime_error("unknown input format \"" + format + "\"");
    }
}

void
writeResponse(string format, RequestOrResponse &rr)
{
    if (format == "capnp") {
        writeResponseCapnp(rr);
    } else if (format == "json") {
        writeResponseJson(rr, false);
    } else {
        throw runtime_error("unknown output format \"" + format + "\"");
    }
}

void
writeException(string format, const std::exception &e, RRType type)
{
    if (format == "capnp") {
        writeExceptionCapnp(e, type);
    } else if (format == "json") {
        writeExceptionJson(e, type);
    } else {
        throw runtime_error("unknown output format \"" + format + "\"");
    }
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
    
    string format = arg;

    if (format != "capnp" && format != "json") {
        usage();
    }

#ifdef _WIN32
    if (format == "capnp") {
        int result = _setmode(_fileno(stdin), _O_BINARY);
        if (result == -1) {
            cerr << "Failed to set binary mode on stdin, necessary for capnp format" << endl;
            exit(1);
        }
        result = _setmode(_fileno(stdout), _O_BINARY);
        if (result == -1) {
            cerr << "Failed to set binary mode on stdout, necessary for capnp format" << endl;
            exit(1);
        }
    }
#endif

    if (debug) {
        cerr << myname << " " << pid << ": waiting for format: " << format << endl;
    }

    while (true) {

        RequestOrResponse request;
        
        try {

            request = readRequest(format);
            
            // NotValid without an exception indicates EOF:
            if (request.type == RRType::NotValid) {
                if (debug) {
                    cerr << myname << " " << pid << ": eof reached, exiting" << endl;
                }
                break;
            }

            if (debug) {
                cerr << myname << " " << pid << ": request received, of type "
                     << int(request.type)
                     << endl;
            }

            RequestOrResponse response = handleRequest(request, debug);
            response.id = request.id;

            if (debug) {
                cerr << myname << " " << pid << ": request handled, writing response"
                     << endl;
            }
            
            writeResponse(format, response);

            if (debug) {
                cerr << myname << " " << pid << ": response written" << endl;
            }

            if (request.type == RRType::Finish) {
                auto h = mapper.pluginToHandle(request.finishRequest.plugin);
                if (debug) {
                    cerr << myname << " " << pid << ": deleting the plugin with handle " << h << endl;
                }
                mapper.removePlugin(h);
                delete request.finishRequest.plugin;
            }
            
        } catch (std::exception &e) {

            if (debug) {
                cerr << myname << " " << pid << ": error: " << e.what() << endl;
            }

            writeException(format, e, request.type);

            if (format == "capnp") {
                // Don't try to continue; we can't recover from a
                // mangled input stream. However, we can return a
                // successful error code because we are reporting the
                // status in our Capnp output stream instead
                if (debug) {
                    cerr << myname << " " << pid << ": not attempting to recover from capnp parse problems, exiting" << endl;
                }
                exit(0);
            }
        }
    }

    exit(0);
}
