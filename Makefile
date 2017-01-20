
VAMPSDK_DIR	:= ../vamp-plugin-sdk
PIPER_DIR	:= ../piper

CXXFLAGS	:= -Wall -Wextra -Werror -g3 -std=c++11
INCFLAGS	:= -Iext -I$(VAMPSDK_DIR) -I. -I/usr/local/include

#LDFLAGS		:= -L$(VAMPSDK_DIR) -L/usr/local/lib -lvamp-hostsdk -lcapnp -lkj 
LDFLAGS		:= $(VAMPSDK_DIR)/libvamp-hostsdk.a -lcapnp -lkj 

LDFLAGS		+= -ldl

all:	o bin bin/piper-convert bin/piper-vamp-simple-server

bin:
	mkdir bin

o:
	mkdir o

bin/piper-convert: o/convert.o o/json11.o o/piper.capnp.o
	c++ $(CXXFLAGS) $^ -o $@ $(LDFLAGS)

bin/piper-vamp-simple-server: o/simple-server.o o/json11.o o/piper.capnp.o
	c++ $(CXXFLAGS) $^ -o $@ $(LDFLAGS)

o/piper.capnp.o:	vamp-capnp/piper.capnp.c++ vamp-capnp/piper.capnp.h
	c++ $(CXXFLAGS) $(INCFLAGS) -c $< -o $@

o/json11.o:	ext/json11/json11.cpp
	c++ $(CXXFLAGS) -c $< -o $@

o/convert.o:	vamp-server/convert.cpp vamp-capnp/piper.capnp.h vamp-capnp/VampnProto.h vamp-json/VampJson.h
	c++ $(CXXFLAGS) $(INCFLAGS) -c $< -o $@

o/simple-server.o:	vamp-server/simple-server.cpp vamp-capnp/piper.capnp.h vamp-capnp/VampnProto.h vamp-json/VampJson.h
	c++ $(CXXFLAGS) $(INCFLAGS) -c $< -o $@

test:	all
	vamp-server/test.sh

clean:
	rm -f */*.o

distclean:	clean
	rm -f bin/*

# cancel implicit rule which otherwise could try to link %.capnp
%:	%.o
