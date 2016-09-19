#!/bin/bash

( bin/vampipe-convert request -i json -o capnp |
	VAMP_PATH=./vamp-plugin-sdk/examples bin/vampipe-server |

#	capnp decode capnproto/vamp.capnp VampResponse

	bin/vampipe-convert response -i capnp -o json
) <<EOF
{"type":"list"}
{"type":"load","content": {"pluginKey":"vamp-example-plugins:percussiononsets","inputSampleRate":44100,"adapterFlags":["AdaptInputDomain","AdaptBufferSize"]}}
{"type":"configure","content":{"pluginHandle":1,"configuration":{"blockSize": 8, "channelCount": 1, "parameterValues": {"sensitivity": 40, "threshold": 3}, "stepSize": 8}}}
{"type":"process","content": {"pluginHandle": 1, "processInput": { "timestamp": {"s": 0, "n": 0}, "inputBuffers": [{"values": [1,2,3,4,5,6,7,8]}]}}}
{"type":"finish","content": {"pluginHandle": 1}}
EOF

