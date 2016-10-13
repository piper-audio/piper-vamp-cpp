
#include "vamp-json/VampJson.h"
#include "vamp-capnp/VampnProto.h"
#include "vamp-support/RequestOrResponse.h"
#include "vamp-support/PreservingPluginHandleMapper.h"

#include <iostream>
#include <sstream>
#include <stdexcept>

#include <capnp/serialize.h>

using namespace std;
using namespace json11;
using namespace piper;

void usage()
{
    string myname = "piper-convert";
    cerr << "\n" << myname <<
	": Validate and convert Piper request and response messages\n\n"
	"    Usage: " << myname << " [-i <informat>] [-o <outformat>] request\n"
	"           " << myname << " [-i <informat>] [-o <outformat>] response\n\n"
	"    where\n"
	"       <informat>: the format to read from stdin\n"
	"           (\"json\" or \"capnp\", default is \"json\")\n"
	"       <outformat>: the format to convert to and write to stdout\n"
	"           (\"json\", \"json-b64\" or \"capnp\", default is \"json\")\n"
	"       request|response: whether messages are Vamp request or response type\n\n"
	"If <informat> and <outformat> differ, convert from <informat> to <outformat>.\n"
	"If <informat> and <outformat> are the same, just check validity of incoming\n"
	"messages and pass them to output.\n\n"
	"Specifying \"json-b64\" as output format forces base64 encoding for process and\n"
	"feature blocks, unlike the \"json\" output format which uses text encoding.\n"
	"The \"json\" input format accepts either.\n\n";

    exit(2);
}

Json
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

Json
convertResponseJson(string input, string &err)
{
    Json j = Json::parse(input, err);
    if (err != "") {
	err = "invalid json: " + err;
	return {};
    }
    if (!j.is_object()) {
	err = "object expected at top level";
    } else {
        if (!j["result"].is_object()) {
            if (!j["error"].is_object()) {
                err = "expected either result or error object";
            }
        }
    }
    return j;
}

//!!! Lots of potential for refactoring the conversion classes based
//!!! on the common matter in the following eight functions...

PreservingPluginHandleMapper mapper;

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

template <typename Reader>
static RequestOrResponse::RpcId
readCapnpId(const Reader &r)
{
    int number;
    string tag;
    switch (r.getId().which()) {
    case RpcRequest::Id::Which::NUMBER:
        number = r.getId().getNumber();
        return { RequestOrResponse::RpcId::Number, number, "" };
    case RpcRequest::Id::Which::TAG:
        tag = r.getId().getTag();
        return { RequestOrResponse::RpcId::Tag, 0, tag };
    case RpcRequest::Id::Which::NONE:
        return { RequestOrResponse::RpcId::Absent, 0, "" };
    }
    return {};
}

template <typename Builder>
static void
buildCapnpId(Builder &b, const RequestOrResponse::RpcId &id)
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
	VampJson::toRpcRequest_List(j, err); // type check only
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
writeRequestJson(RequestOrResponse &rr, bool useBase64)
{
    Json j;

    VampJson::BufferSerialisation serialisation =
        (useBase64 ?
         VampJson::BufferSerialisation::Base64 :
         VampJson::BufferSerialisation::Array);

    Json id = writeJsonId(rr.id);
    
    switch (rr.type) {

    case RRType::List:
	j = VampJson::fromRpcRequest_List(id);
	break;
    case RRType::Load:
	j = VampJson::fromRpcRequest_Load(rr.loadRequest, id);
	break;
    case RRType::Configure:
	j = VampJson::fromRpcRequest_Configure(rr.configurationRequest, mapper, id);
	break;
    case RRType::Process:
	j = VampJson::fromRpcRequest_Process
	    (rr.processRequest, mapper, serialisation, id);
	break;
    case RRType::Finish:
	j = VampJson::fromRpcRequest_Finish(rr.finishRequest, mapper, id);
	break;
    case RRType::NotValid:
	break;
    }

    cout << j.dump() << endl;
}

RequestOrResponse
readResponseJson(string &err)
{
    RequestOrResponse rr;
    rr.direction = RequestOrResponse::Response;

    string input;
    if (!getline(cin, input)) {
	// the EOF case, not actually an error
	rr.type = RRType::NotValid;
	return rr;
    }

    Json j = convertResponseJson(input, err);
    if (err != "") return {};

    rr.type = VampJson::getRequestResponseType(j, err);
    if (err != "") return {};

    rr.id = readJsonId(j);
    
    VampJson::BufferSerialisation serialisation =
        VampJson::BufferSerialisation::Array;

    rr.success = j["success"].bool_value();
    rr.errorText = j["errorText"].string_value();

    switch (rr.type) {

    case RRType::List:
	rr.listResponse = VampJson::toRpcResponse_List(j, err);
	break;
    case RRType::Load:
	rr.loadResponse = VampJson::toRpcResponse_Load(j, mapper, err);
	break;
    case RRType::Configure:
	rr.configurationResponse = VampJson::toRpcResponse_Configure(j, mapper, err);
	break;
    case RRType::Process: 
	rr.processResponse = VampJson::toRpcResponse_Process(j, mapper, serialisation, err);
	break;
    case RRType::Finish:
	rr.finishResponse = VampJson::toRpcResponse_Finish(j, mapper, serialisation, err);
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

RequestOrResponse
readRequestCapnp(kj::BufferedInputStreamWrapper &buffered)
{
    RequestOrResponse rr;
    rr.direction = RequestOrResponse::Request;

    ::capnp::InputStreamMessageReader message(buffered);
    RpcRequest::Reader reader = message.getRoot<RpcRequest>();
    
    rr.type = VampnProto::getRequestResponseType(reader);
    rr.id = readCapnpId(reader);

    switch (rr.type) {

    case RRType::List:
	VampnProto::readRpcRequest_List(reader); // type check only
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
writeRequestCapnp(RequestOrResponse &rr)
{
    ::capnp::MallocMessageBuilder message;
    RpcRequest::Builder builder = message.initRoot<RpcRequest>();

    buildCapnpId(builder, rr.id);
    
    switch (rr.type) {

    case RRType::List:
	VampnProto::buildRpcRequest_List(builder);
	break;
    case RRType::Load:
	VampnProto::buildRpcRequest_Load(builder, rr.loadRequest);
	break;
    case RRType::Configure:
	VampnProto::buildRpcRequest_Configure(builder,
                                              rr.configurationRequest, mapper);
	break;
    case RRType::Process:
	VampnProto::buildRpcRequest_Process(builder, rr.processRequest, mapper);
	break;
    case RRType::Finish:
	VampnProto::buildRpcRequest_Finish(builder, rr.finishRequest, mapper);
	break;
    case RRType::NotValid:
	break;
    }

    writeMessageToFd(1, message);
}

RequestOrResponse
readResponseCapnp(kj::BufferedInputStreamWrapper &buffered)
{
    RequestOrResponse rr;
    rr.direction = RequestOrResponse::Response;

    ::capnp::InputStreamMessageReader message(buffered);
    RpcResponse::Reader reader = message.getRoot<RpcResponse>();
    
    rr.type = VampnProto::getRequestResponseType(reader);
    rr.success = true;
    rr.errorText = "";
    rr.id = readCapnpId(reader);
    int errorCode = 0;

    switch (rr.type) {

    case RRType::List:
	VampnProto::readRpcResponse_List(rr.listResponse, reader);
	break;
    case RRType::Load:
	VampnProto::readRpcResponse_Load(rr.loadResponse, reader, mapper);
	break;
    case RRType::Configure:
	VampnProto::readRpcResponse_Configure(rr.configurationResponse,
					       reader, mapper);
	break;
    case RRType::Process:
	VampnProto::readRpcResponse_Process(rr.processResponse, reader, mapper);
	break;
    case RRType::Finish:
	VampnProto::readRpcResponse_Finish(rr.finishResponse, reader, mapper);
	break;
    case RRType::NotValid:
        // error
        rr.success = false;
        VampnProto::readRpcResponse_Error(errorCode, rr.errorText, reader);
	break;
    }

    return rr;
}

void
writeResponseCapnp(RequestOrResponse &rr)
{
    ::capnp::MallocMessageBuilder message;
    RpcResponse::Builder builder = message.initRoot<RpcResponse>();

    buildCapnpId(builder, rr.id);

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

RequestOrResponse
readInputJson(RequestOrResponse::Direction direction, string &err)
{
    if (direction == RequestOrResponse::Request) {
	return readRequestJson(err);
    } else {
	return readResponseJson(err);
    }
}

RequestOrResponse
readInputCapnp(RequestOrResponse::Direction direction)
{
    static kj::FdInputStream stream(0); // stdin
    static kj::BufferedInputStreamWrapper buffered(stream);

    if (buffered.tryGetReadBuffer() == nullptr) {
	return {};
    }
    
    if (direction == RequestOrResponse::Request) {
	return readRequestCapnp(buffered);
    } else {
	return readResponseCapnp(buffered);
    }
}

RequestOrResponse
readInput(string format, RequestOrResponse::Direction direction)
{
    if (format == "json") {
	string err;
	auto result = readInputJson(direction, err);
	if (err != "") throw runtime_error(err);
	else return result;
    } else if (format == "capnp") {
	return readInputCapnp(direction);
    } else {
	throw runtime_error("unknown input format \"" + format + "\"");
    }
}

void
writeOutput(string format, RequestOrResponse &rr)
{
    if (format == "json") {
	if (rr.direction == RequestOrResponse::Request) {
	    writeRequestJson(rr, false);
	} else {
	    writeResponseJson(rr, false);
	}
    } else if (format == "json-b64") {
	if (rr.direction == RequestOrResponse::Request) {
	    writeRequestJson(rr, true);
	} else {
	    writeResponseJson(rr, true);
	}
    } else if (format == "capnp") {
	if (rr.direction == RequestOrResponse::Request) {
	    writeRequestCapnp(rr);
	} else {
	    writeResponseCapnp(rr);
	}
    } else {
	throw runtime_error("unknown output format \"" + format + "\"");
    }
}

int main(int argc, char **argv)
{
    if (argc < 2) {
	usage();
    }

    string informat = "json", outformat = "json";
    RequestOrResponse::Direction direction = RequestOrResponse::Request;
    bool haveDirection = false;
    
    for (int i = 1; i < argc; ++i) {

	string arg = argv[i];
	bool final = (i + 1 == argc);
	
	if (arg == "-i") {
	    if (final) usage();
	    else informat = argv[++i];

	} else if (arg == "-o") {
	    if (final) usage();
	    else outformat = argv[++i];

	} else if (arg == "request") {
	    direction = RequestOrResponse::Request;
	    haveDirection = true;

	} else if (arg == "response") {
	    direction = RequestOrResponse::Response;
	    haveDirection = true;
	    
	} else {
	    usage();
	}
    }

    if (informat == "" || outformat == "" || !haveDirection) {
	usage();
    }

    while (true) {

	try {

	    RequestOrResponse rr = readInput(informat, direction);

	    // NotValid without an exception indicates EOF:
	    if (rr.type == RRType::NotValid) break;

	    writeOutput(outformat, rr);
	    
	} catch (std::exception &e) {

	    cerr << "Error: " << e.what() << endl;
	    exit(1);
	}
    }

    exit(0);
}


