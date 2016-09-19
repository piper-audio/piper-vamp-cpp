/* -*- c-basic-offset: 4 indent-tabs-mode: nil -*-  vi:set ts=8 sts=4 sw=4: */

/*
    Vampipe

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

#ifndef VAMPIPE_REQUEST_OR_RESPONSE_H
#define VAMPIPE_REQUEST_OR_RESPONSE_H

#include "RequestResponseType.h"

#include <vamp-hostsdk/PluginStaticData.h>
#include <vamp-hostsdk/RequestResponse.h>

#include <string>
#include <vector>

namespace vampipe {

class RequestOrResponse
{
public:
    enum Direction {
	Request, Response
    };
    
    RequestOrResponse() : // nothing by default
	direction(Request),
	type(RRType::NotValid),
	success(false) { }

    Direction direction;
    RRType type;
    bool success;
    std::string errorText;

    Vamp::HostExt::ListResponse listResponse;
    Vamp::HostExt::LoadRequest loadRequest;
    Vamp::HostExt::LoadResponse loadResponse;
    Vamp::HostExt::ConfigurationRequest configurationRequest;
    Vamp::HostExt::ConfigurationResponse configurationResponse;
    Vamp::HostExt::ProcessRequest processRequest;
    Vamp::HostExt::ProcessResponse processResponse;
    Vamp::HostExt::FinishRequest finishRequest;
    Vamp::HostExt::ProcessResponse finishResponse;
};

}

#endif
