
# Piper C++

Supporting code in C++ for the Piper audio feature extractor protocol,
primarily for making Vamp plugins work with the Piper protocol.

## Contents

 * code to adapt Piper messages to the classes used in the Vamp SDK
 * a command-line converter between Piper serialisations (via Vamp SDK
   classes)
 * a server that makes Vamp plugins available via Piper messages

## Directory index

*vamp-json* - convert between Piper JSON messages and Vamp SDK classes

*vamp-capnp* - convert between Piper Cap'n Proto messages and Vamp
abstractions

*vamp-support* - support classes for the above

*vamp-server* - main programs for command-line converter and server

*vamp-client* - logic to make Piper servers available to Vamp hosts
through a Vamp-like API

*vamp-client/qt* - logic specific to hosts written with Qt

[![Build Status](https://travis-ci.org/piper-audio/piper-cpp.svg?branch=master)](https://travis-ci.org/piper-audio/piper-cpp)

## Authors and licensing

Written by Chris Cannam at the Centre for Digital Music, Queen Mary,
University of London.

Copyright (c) 2015-2017 Queen Mary, University of London, provided
under a BSD-style licence. See the file COPYING for details.

