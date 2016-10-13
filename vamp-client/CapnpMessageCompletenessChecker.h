
#ifndef CAPNP_MESSAGE_COMPLETENESS_CHECKER_H
#define CAPNP_MESSAGE_COMPLETENESS_CHECKER_H

#include "SynchronousTransport.h" //!!!

#include <capnp/serialize.h>

#include <iostream>

namespace piper { //!!! change

class CapnpMessageCompletenessChecker : public MessageCompletenessChecker
{
public:
    bool isComplete(const std::vector<char> &message) const override {

        // a bit liberal with the copies here
        size_t wordSize = sizeof(capnp::word);
	size_t words = message.size() / wordSize;
	kj::Array<capnp::word> karr(kj::heapArray<capnp::word>(words));
	memcpy(karr.begin(), message.data(), words * wordSize);

        size_t expected = capnp::expectedSizeInWordsFromPrefix(karr);

        if (words > expected) {
            std::cerr << "WARNING: obtained more data than expected ("
                      << words << " " << wordSize << "-byte words, expected "
                      << expected << ")" << std::endl;
        }
        
        return words >= expected;
    }
};

}

#endif
