
#ifndef PIPER_SYNCHRONOUS_TRANSPORT_H
#define PIPER_SYNCHRONOUS_TRANSPORT_H

#include <vector>

namespace piper {

class SynchronousTransport // interface
{
public:
    //!!! how to handle errors -- exception or return value? often an
    //!!! error (e.g. server has exited) may mean the transport can no
    //!!! longer be used at all
    virtual std::vector<char> call(const char *data, size_t bytes) = 0;

    virtual bool isOK() const = 0;
};

class MessageCompletenessChecker // interface
{
public:
    virtual bool isComplete(const std::vector<char> &message) const = 0;
};

}

#endif
