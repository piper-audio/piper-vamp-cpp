#!/bin/bash 

set -eu

piperdir=../piper
vampsdkdir=../vamp-plugin-sdk
schemadir="$piperdir"/json/schema

tmpdir=$(mktemp -d)

if [ ! -d "$tmpdir" ]; then
    echo "Temp directory creation failed" 1>&2
    exit 1
fi

trap "rm -rf $tmpdir" 0

reqfile="$tmpdir/req.json"
respfile="$tmpdir/resp.json"
allrespfile="$tmpdir/resp.all"
input="$tmpdir/input"
expected="$tmpdir/expected"
obtained="$tmpdir/obtained"

validate() {
    local file="$1"
    local schemaname="$2"
    jsonschema -i "$file" "$schemadir/$schemaname.json" 1>&2 && \
        echo "validated $schemaname" 1>&2 || \
        echo "failed to validate $schemaname" 1>&2
}

validate_request() {
    local json="$1"
    echo "$json" > "$reqfile"
    validate "$reqfile" "rpcrequest"
}

validate_response() {
    local json="$1"
    echo "$json" > "$respfile"
    validate "$respfile" "rpcresponse"
}

cat > "$input" <<EOF
{"method":"list"}
{"method":"list","params": {"from":["vamp-example-plugins","something-nonexistent"]}}
{"method":"list","params": {"from":["something-nonexistent"]}}
{"method":"load","id":6,"params": {"key":"vamp-example-plugins:percussiononsets","inputSampleRate":44100,"adapterFlags":["AdaptInputDomain","AdaptBufferSize"]}}
{"method":"configure","id":"weevil","params":{"handle":1,"configuration":{"blockSize": 8, "channelCount": 1, "parameterValues": {"sensitivity": 40, "threshold": 3}, "stepSize": 8}}}
{"method":"process","params": {"handle": 1, "processInput": { "timestamp": {"s": 0, "n": 0}, "inputBuffers": [ [1,2,3,4,5,6,7,8] ]}}}
{"method":"finish","params": {"handle": 1}}
EOF

# Expected output, apart from the plugin list which seems a bit
# fragile to check here
cat > "$expected" <<EOF
{"id": 6, "jsonrpc": "2.0", "method": "load", "result": {"defaultConfiguration": {"blockSize": 1024, "channelCount": 1, "parameterValues": {"sensitivity": 40, "threshold": 3}, "stepSize": 1024}, "handle": 1, "staticData": {"basic": {"description": "Detect percussive note onsets by identifying broadband energy rises", "identifier": "percussiononsets", "name": "Simple Percussion Onset Detector"}, "basicOutputInfo": [{"description": "Percussive note onset locations", "identifier": "onsets", "name": "Onsets"}, {"description": "Broadband energy rise detection function", "identifier": "detectionfunction", "name": "Detection Function"}], "category": ["Time", "Onsets"], "copyright": "Code copyright 2006 Queen Mary, University of London, after Dan Barry et al 2005.  Freely redistributable (BSD license)", "inputDomain": "TimeDomain", "key": "vamp-example-plugins:percussiononsets", "maker": "Vamp SDK Example Plugins", "maxChannelCount": 1, "minChannelCount": 1, "parameters": [{"basic": {"description": "Energy rise within a frequency bin necessary to count toward broadband total", "identifier": "threshold", "name": "Energy rise threshold"}, "defaultValue": 3, "extents": {"max": 20, "min": 0}, "unit": "dB", "valueNames": []}, {"basic": {"description": "Sensitivity of peak detector applied to broadband detection function", "identifier": "sensitivity", "name": "Sensitivity"}, "defaultValue": 40, "extents": {"max": 100, "min": 0}, "unit": "%", "valueNames": []}], "programs": [], "version": 2}}}
{"id": "weevil", "jsonrpc": "2.0", "method": "configure", "result": {"handle": 1, "outputList": [{"basic": {"description": "Percussive note onset locations", "identifier": "onsets", "name": "Onsets"}, "configured": {"binCount": 0, "binNames": [], "hasDuration": false, "sampleRate": 44100, "sampleType": "VariableSampleRate", "unit": ""}}, {"basic": {"description": "Broadband energy rise detection function", "identifier": "detectionfunction", "name": "Detection Function"}, "configured": {"binCount": 1, "binNames": [""], "hasDuration": false, "quantizeStep": 1, "sampleRate": 86.1328125, "sampleType": "FixedSampleRate", "unit": ""}}]}}
{"jsonrpc": "2.0", "method": "process", "result": {"features": {}, "handle": 1}}
{"jsonrpc": "2.0", "method": "finish", "result": {"features": {"detectionfunction": [{"featureValues": [0], "timestamp": {"n": 11609977, "s": 0}}]}, "handle": 1}}
EOF

# We run the whole test twice, once with the server in Capnp mode
# (converting to JSON using piper-convert) and once with it directly
# in JSON mode

for format in json capnp ; do

    ( export VAMP_PATH="$vampsdkdir"/examples ;
      while read request ; do
          validate_request "$request"
          echo "$request"
      done |
          if [ "$format" = "json" ]; then
              bin/piper-vamp-simple-server -d json
          else
              bin/piper-convert request -i json -o capnp |
                  bin/piper-vamp-simple-server -d capnp |
                  bin/piper-convert response -i capnp -o json
          fi |
          while read response ; do
              echo "$response" >> "$allrespfile"
              validate_response "$response"
          done
    ) < "$input"

    # Skip plugin lists
    tail -n +4 "$allrespfile" > "$obtained"

    echo "Checking response contents against expected contents..."
    if ! cmp "$obtained" "$expected"; then
        diff -u1 "$obtained" "$expected"
    else
        echo "OK"
    fi

    echo "Checking plugin counts from list responses..."
    
    # Now check the plugin lists, but as the descriptions etc are
    # probably a bit fragile, let's just count the number of plugins

    # First, with no "from" arg to the list call
    list_no_from=$(head -n +1 "$allrespfile" | fmt -1 | grep '"key"' | wc -l)

    # Now with a "from" arg that includes the library that exists
    list_with_good_from=$(tail -n +2 "$allrespfile" | head -n +1 | fmt -1 |
                              grep '"key"' | wc -l)
    
    # Now with a "from" arg that doesn't include any real library
    list_with_bad_from=$(tail -n +3 "$allrespfile" | head -n +1 | fmt -1 |
                             grep '"key"' | wc -l)

    if [ "$list_no_from" != "6" ]; then
        echo "Wrong number of plugins from list response without \"from\" arg"
        echo "Expected 6, obtained $list_no_from"
        false
    fi
    if [ "$list_with_good_from" != "6" ]; then
        echo "Wrong number of plugins from list response with good \"from\" arg"
        echo "Expected 6, obtained $list_with_good_from"
        false
    fi
    if [ "$list_with_bad_from" != "0" ]; then
        echo "Wrong number of plugins from list response with bad \"from\" arg"
        echo "Expected 0, obtained $list_with_bad_from"
        false
    fi
    echo OK
    
    rm "$allrespfile"

done

echo "Tests succeeded"  # set -e at top should ensure we don't get here otherwise
