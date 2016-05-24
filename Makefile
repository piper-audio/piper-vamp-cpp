
CXXFLAGS	:= -Wall -Werror -O3 -std=c++11
INCFLAGS	:= -Ivamp-plugin-sdk -Ijson -Icapnproto -I.
LDFLAGS		:= -Lvamp-plugin-sdk -Wl,-Bstatic -lvamp-hostsdk -Wl,-Bdynamic -lcapnp -lkj -ldl

all:	bin/vamp-json-cli bin/vamp-json-to-capnp bin/vampipe-convert

bin/vampipe-convert: o/vampipe-convert.o o/json11.o o/vamp.capnp.o
	c++ $(CXXFLAGS) $^ -o $@ $(LDFLAGS)

bin/vamp-json-to-capnp:	o/json-to-capnp.o o/json11.o
	c++ $(CXXFLAGS) $^ -o $@ $(LDFLAGS)

bin/vamp-json-cli:	o/json-cli.o o/json11.o
	c++ $(CXXFLAGS) $^ -o $@ $(LDFLAGS)

capnproto/vamp.capnp.h:	capnproto/vamp.capnp
	capnp compile $< -oc++

o/vamp.capnp.o:	capnproto/vamp.capnp.c++ capnproto/vamp.capnp.h
	c++ $(CXXFLAGS) -c $< -o $@

o/json11.o:	json/json11/json11.cpp
	c++ $(CXXFLAGS) -c $< -o $@

o/vampipe-convert.o:	utilities/vampipe-convert.cpp capnproto/vamp.capnp.h capnproto/VampnProto.h json/VampJson.h
	c++ $(CXXFLAGS) $(INCFLAGS) -c $< -o $@

o/json-to-capnp.o:	utilities/json-to-capnp.cpp capnproto/vamp.capnp.h capnproto/VampnProto.h json/VampJson.h
	c++ $(CXXFLAGS) $(INCFLAGS) -c $< -o $@

o/json-cli.o:	utilities/json-cli.cpp json/VampJson.h
	c++ $(CXXFLAGS) $(INCFLAGS) -c $< -o $@

test:	all
	VAMP_PATH=./vamp-plugin-sdk/examples test/test-json-cli.sh
	VAMP_PATH=./vamp-plugin-sdk/examples test/test-json-to-capnp.sh

clean:
	rm -f */*.o capnproto/vamp.capnp.h capnproto/vamp.capnp.c++

distclean:	clean
	rm -f bin/*

