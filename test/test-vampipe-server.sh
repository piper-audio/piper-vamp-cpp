#!/bin/bash 

set -eu

reqfile="/tmp/$$.req.json"
respfile="/tmp/$$.resp.json"
trap "rm -f $reqfile $respfile" 0

schema=vamp-json-schema/schema

validate() {
    local file="$1"
    local schemaname="$2"
    jsonschema -i "$file" "$schema/$schemaname.json" 1>&2 && \
        echo "validated $schemaname" 1>&2 || \
        echo "failed to validate $schemaname" 1>&2
}

validate_request() {
    local json="$1"
    echo "$json" > "$reqfile"
    validate "$reqfile" "request"
}

validate_response() {
    local json="$1"
    echo "$json" > "$respfile"
    validate "$respfile" "response"
}

( while read request ; do
      validate_request "$request"
      echo "$request"
  done |
      bin/vampipe-convert request -i json -o capnp |
      VAMP_PATH=./vamp-plugin-sdk/examples bin/vampipe-server |
      bin/vampipe-convert response -i capnp -o json |
      while read response ; do
          validate_response "$response"
      done
) <<EOF
{"method":"list"}
{"method":"load","params": {"pluginKey":"vamp-example-plugins:percussiononsets","inputSampleRate":44100,"adapterFlags":["AdaptInputDomain","AdaptBufferSize"]}}
{"method":"configure","params":{"pluginHandle":1,"configuration":{"blockSize": 8, "channelCount": 1, "parameterValues": {"sensitivity": 40, "threshold": 3}, "stepSize": 8}}}
{"method":"process","params": {"pluginHandle": 1, "processInput": { "timestamp": {"s": 0, "n": 0}, "inputBuffers": [ [1,2,3,4,5,6,7,8] ]}}}
{"method":"finish","params": {"pluginHandle": 1}}
EOF
