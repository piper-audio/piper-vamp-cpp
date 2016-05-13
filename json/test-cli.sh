#!/bin/bash

./json-cli <<EOF
{"verb":"list"}
{"verb":"load","content": {"pluginKey":"vamp-example-plugins:percussiononsets","inputSampleRate":44100,"adapterFlags":["AdaptInputDomain"]}}
{"verb":"configure","content":{"pluginHandle":1,"configuration":{"blockSize": 512, "channelCount": 1, "parameterValues": {"sensitivity": 40, "threshold": 3}, "stepSize": 1024}}}
EOF

