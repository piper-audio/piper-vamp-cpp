
CXXFLAGS	:= -Wall -Werror -g3 -std=c++11
INCFLAGS	:= -Ivamp-plugin-sdk -Ijson -I/usr/local/include -Icapnproto -I.

LDFLAGS		:= vamp-plugin-sdk/libvamp-hostsdk.a -L/usr/local/lib -lcapnp -lkj -ldl

#!!! todo: proper dependencies

all:	bin/vampipe-convert bin/vampipe-server

bin/vampipe-convert: o/vampipe-convert.o o/json11.o o/vamp.capnp.o
	c++ $(CXXFLAGS) $^ -o $@ $(LDFLAGS)

bin/vampipe-server: o/vampipe-server.o o/vamp.capnp.o
	c++ $(CXXFLAGS) $^ -o $@ $(LDFLAGS)

capnproto/vamp.capnp.h:	capnproto/vamp.capnp
	capnp compile $< -oc++

o/vamp.capnp.o:	capnproto/vamp.capnp.c++ capnproto/vamp.capnp.h
	c++ $(CXXFLAGS) $(INCFLAGS) -c $< -o $@

o/json11.o:	json/json11/json11.cpp
	c++ $(CXXFLAGS) -c $< -o $@

o/vampipe-convert.o:	utilities/vampipe-convert.cpp capnproto/vamp.capnp.h capnproto/VampnProto.h json/VampJson.h
	c++ $(CXXFLAGS) $(INCFLAGS) -c $< -o $@

o/vampipe-server.o:	utilities/vampipe-server.cpp capnproto/vamp.capnp.h capnproto/VampnProto.h 
	c++ $(CXXFLAGS) $(INCFLAGS) -c $< -o $@

test:	all
	test/test-vampipe-server.sh

clean:
	rm -f */*.o capnproto/vamp.capnp.h capnproto/vamp.capnp.c++

distclean:	clean
	rm -f bin/*

