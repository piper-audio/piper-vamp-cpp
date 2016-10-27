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

#ifndef PIPER_SYNCHRONOUS_TRANSPORT_H
#define PIPER_SYNCHRONOUS_TRANSPORT_H

#include <vector>
#include <cstdlib>
#include <stdexcept>

namespace piper_vamp {
namespace client {

class MessageCompletenessChecker // interface
{
public:
    virtual ~MessageCompletenessChecker() = default;
    
    virtual bool isComplete(const std::vector<char> &message) const = 0;
};

class ServerCrashed : public std::runtime_error
{
public:
    ServerCrashed() : std::runtime_error("Piper server exited unexpectedly") {}
};

class SynchronousTransport // interface
{
public:
    virtual ~SynchronousTransport() = default;
    
    //!!! I do not take ownership
    virtual void setCompletenessChecker(MessageCompletenessChecker *) = 0;
    
    //!!! how to handle errors -- exception or return value? often an
    //!!! error (e.g. server has exited) may mean the transport can no
    //!!! longer be used at all
    virtual std::vector<char> call(const char *data, size_t bytes) = 0;

    virtual bool isOK() const = 0;
};

}
}

#endif
