
CXXFLAGS	:= -Wall -Werror -std=c++11
INCFLAGS	:= -Ivamp-plugin-sdk -Ijson -Icapnproto -I.
LDFLAGS		:= -Lvamp-plugin-sdk -Wl,-Bstatic -lvamp-hostsdk -Wl,-Bdynamic -lcapnp -lkj -ldl

all:	bin/vamp-json-cli bin/vamp-json-to-capnp bin/vampipe-convert

bin/vampipe-convert: utilities/vampipe-convert.o json/json11/json11.o
	c++ $(CXXFLAGS) $^ -o $@ $(LDFLAGS)

bin/vamp-json-to-capnp:	utilities/json-to-capnp.o json/json11/json11.o
	c++ $(CXXFLAGS) $^ -o $@ $(LDFLAGS)

bin/vamp-json-cli:	utilities/json-cli.o json/json11/json11.o
	c++ $(CXXFLAGS) $^ -o $@ $(LDFLAGS)

capnproto/vamp.capnp.h:	capnproto/vamp.capnp
	capnp compile $< -oc++

utilities/vampipe-convert.o:	utilities/vampipe-convert.cpp capnproto/vamp.capnp.h capnproto/VampnProto.h json/VampJson.h
	c++ $(CXXFLAGS) $(INCFLAGS) -c $< -o $@

utilities/json-to-capnp.o:	utilities/json-to-capnp.cpp capnproto/vamp.capnp.h capnproto/VampnProto.h json/VampJson.h
	c++ $(CXXFLAGS) $(INCFLAGS) -c $< -o $@

utilities/json-cli.o:	utilities/json-cli.cpp json/VampJson.h
	c++ $(CXXFLAGS) $(INCFLAGS) -c $< -o $@

test:	all
	VAMP_PATH=./vamp-plugin-sdk/examples test/test-json-cli.sh
	VAMP_PATH=./vamp-plugin-sdk/examples test/test-json-to-capnp.sh

clean:
	rm -f */*.o capnp/vamp.capnp.h capnp/vamp.capnp.c++

distclean:	clean
	rm -f bin/*

