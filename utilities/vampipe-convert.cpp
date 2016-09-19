
#include "VampJson.h"
#include "VampnProto.h"

#include "bits/RequestOrResponse.h"
#include "bits/PreservingPluginHandleMapper.h"

#include <iostream>
#include <sstream>
#include <stdexcept>

using namespace std;
using namespace json11;
using namespace vampipe;

void usage()
{
    string myname = "vampipe-convert";
    cerr << "\n" << myname <<
	": Validate and convert Vamp request and response messages\n\n"
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
convertRequestJson(string input)
{
    string err;
    Json j = Json::parse(input, err);
    if (err != "") {
	throw VampJson::Failure("invalid json: " + err);
    }
    if (!j.is_object()) {
	throw VampJson::Failure("object expected at top level");
    }
    if (!j["type"].is_string()) {
	throw VampJson::Failure("string expected for type field");
    }
    if (!j["content"].is_null() && !j["content"].is_object()) {
	throw VampJson::Failure("object expected for content field");
    }
    return j;
}

Json
convertResponseJson(string input)
{
    string err;
    Json j = Json::parse(input, err);
    if (err != "") {
	throw VampJson::Failure("invalid json: " + err);
    }
    if (!j.is_object()) {
	throw VampJson::Failure("object expected at top level");
    }
    if (!j["success"].is_bool()) {
	throw VampJson::Failure("bool expected for success field");
    }
    if (!j["content"].is_object()) {
	throw VampJson::Failure("object expected for content field");
    }
    return j;
}

//!!! Lots of potential for refactoring the conversion classes based
//!!! on the common matter in the following eight functions...

PreservingPluginHandleMapper mapper;

RequestOrResponse
readRequestJson()
{
    RequestOrResponse rr;
    rr.direction = RequestOrResponse::Request;

    string input;
    if (!getline(cin, input)) {
	rr.type = RRType::NotValid;
	return rr;
    }
    
    Json j = convertRequestJson(input);

    rr.type = VampJson::getRequestResponseType(j);
    VampJson::BufferSerialisation serialisation = VampJson::BufferSerialisation::Text;

    switch (rr.type) {

    case RRType::List:
	VampJson::toVampRequest_List(j); // type check only
	break;
    case RRType::Load:
	rr.loadRequest = VampJson::toVampRequest_Load(j);
	break;
    case RRType::Configure:
	rr.configurationRequest = VampJson::toVampRequest_Configure(j, mapper);
	break;
    case RRType::Process:
	rr.processRequest = VampJson::toVampRequest_Process(j, mapper, serialisation);
	break;
    case RRType::Finish:
	rr.finishRequest = VampJson::toVampRequest_Finish(j, mapper);
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
         VampJson::BufferSerialisation::Text);

    switch (rr.type) {

    case RRType::List:
	j = VampJson::fromVampRequest_List();
	break;
    case RRType::Load:
	j = VampJson::fromVampRequest_Load(rr.loadRequest);
	break;
    case RRType::Configure:
	j = VampJson::fromVampRequest_Configure(rr.configurationRequest, mapper);
	break;
    case RRType::Process:
	j = VampJson::fromVampRequest_Process
	    (rr.processRequest, mapper, serialisation);
	break;
    case RRType::Finish:
	j = VampJson::fromVampRequest_Finish(rr.finishRequest, mapper);
	break;
    case RRType::NotValid:
	break;
    }

    cout << j.dump() << endl;
}

RequestOrResponse
readResponseJson()
{
    RequestOrResponse rr;
    rr.direction = RequestOrResponse::Response;

    string input;
    if (!getline(cin, input)) {
	rr.type = RRType::NotValid;
	return rr;
    }

    Json j = convertResponseJson(input);

    rr.type = VampJson::getRequestResponseType(j);
    VampJson::BufferSerialisation serialisation = VampJson::BufferSerialisation::Text;

    rr.success = j["success"].bool_value();
    rr.errorText = j["errorText"].string_value();

    switch (rr.type) {

    case RRType::List:
	rr.listResponse = VampJson::toVampResponse_List(j);
	break;
    case RRType::Load:
	rr.loadResponse = VampJson::toVampResponse_Load(j, mapper);
	break;
    case RRType::Configure:
	rr.configurationResponse = VampJson::toVampResponse_Configure(j, mapper);
	break;
    case RRType::Process: 
	rr.processResponse = VampJson::toVampResponse_Process(j, mapper, serialisation);
	break;
    case RRType::Finish:
	rr.finishResponse = VampJson::toVampResponse_Finish(j, mapper, serialisation);
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
         VampJson::BufferSerialisation::Text);

    if (!rr.success) {

	j = VampJson::fromError(rr.errorText, rr.type);

    } else {
    
	switch (rr.type) {

	case RRType::List:
	    j = VampJson::fromVampResponse_List("", rr.listResponse);
	    break;
	case RRType::Load:
	    j = VampJson::fromVampResponse_Load(rr.loadResponse, mapper);
	    break;
	case RRType::Configure:
	    j = VampJson::fromVampResponse_Configure(rr.configurationResponse,
                                                     mapper);
	    break;
	case RRType::Process:
	    j = VampJson::fromVampResponse_Process
		(rr.processResponse, mapper, serialisation);
	    break;
	case RRType::Finish:
	    j = VampJson::fromVampResponse_Finish
		(rr.finishResponse, mapper, serialisation);
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
    VampRequest::Reader reader = message.getRoot<VampRequest>();
    
    rr.type = VampnProto::getRequestResponseType(reader);

    switch (rr.type) {

    case RRType::List:
	VampnProto::readVampRequest_List(reader); // type check only
	break;
    case RRType::Load:
	VampnProto::readVampRequest_Load(rr.loadRequest, reader);
	break;
    case RRType::Configure:
	VampnProto::readVampRequest_Configure(rr.configurationRequest,
					      reader, mapper);
	break;
    case RRType::Process:
	VampnProto::readVampRequest_Process(rr.processRequest, reader, mapper);
	break;
    case RRType::Finish:
	VampnProto::readVampRequest_Finish(rr.finishRequest, reader, mapper);
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
    VampRequest::Builder builder = message.initRoot<VampRequest>();

    switch (rr.type) {

    case RRType::List:
	VampnProto::buildVampRequest_List(builder);
	break;
    case RRType::Load:
	VampnProto::buildVampRequest_Load(builder, rr.loadRequest);
	break;
    case RRType::Configure:
	VampnProto::buildVampRequest_Configure(builder,
					       rr.configurationRequest, mapper);
	break;
    case RRType::Process:
	VampnProto::buildVampRequest_Process(builder, rr.processRequest, mapper);
	break;
    case RRType::Finish:
	VampnProto::buildVampRequest_Finish(builder, rr.finishRequest, mapper);
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
    VampResponse::Reader reader = message.getRoot<VampResponse>();
    
    rr.type = VampnProto::getRequestResponseType(reader);
    rr.success = reader.getSuccess();
    rr.errorText = reader.getErrorText();

    switch (rr.type) {

    case RRType::List:
	VampnProto::readVampResponse_List(rr.listResponse, reader);
	break;
    case RRType::Load:
	VampnProto::readVampResponse_Load(rr.loadResponse, reader, mapper);
	break;
    case RRType::Configure:
	VampnProto::readVampResponse_Configure(rr.configurationResponse,
					       reader, mapper);
	break;
    case RRType::Process:
	VampnProto::readVampResponse_Process(rr.processResponse, reader, mapper);
	break;
    case RRType::Finish:
	VampnProto::readVampResponse_Finish(rr.finishResponse, reader, mapper);
	break;
    case RRType::NotValid:
	break;
    }

    return rr;
}

void
writeResponseCapnp(RequestOrResponse &rr)
{
    ::capnp::MallocMessageBuilder message;
    VampResponse::Builder builder = message.initRoot<VampResponse>();

    if (!rr.success) {

	VampnProto::buildVampResponse_Error(builder, rr.errorText, rr.type);

    } else {
	
	switch (rr.type) {

	case RRType::List:
	    VampnProto::buildVampResponse_List(builder, rr.listResponse);
	    break;
	case RRType::Load:
	    VampnProto::buildVampResponse_Load(builder, rr.loadResponse, mapper);
	    break;
	case RRType::Configure:
	    VampnProto::buildVampResponse_Configure(builder, rr.configurationResponse, mapper);
	    break;
	case RRType::Process:
	    VampnProto::buildVampResponse_Process(builder, rr.processResponse, mapper);
	    break;
	case RRType::Finish:
	    VampnProto::buildVampResponse_Finish(builder, rr.finishResponse, mapper);
	    break;
	case RRType::NotValid:
	    break;
	}
    }
    
    writeMessageToFd(1, message);
}

RequestOrResponse
readInputJson(RequestOrResponse::Direction direction)
{
    if (direction == RequestOrResponse::Request) {
	return readRequestJson();
    } else {
	return readResponseJson();
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
	return readInputJson(direction);
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


