
VAMPSDK_DIR	:= ../vamp-plugin-sdk
PIPER_DIR	:= ../piper

CXXFLAGS	:= -Wall -Wextra -Werror -g3 -std=c++11
INCFLAGS	:= -Iext -I$(VAMPSDK_DIR) -I. -I/usr/local/include

#LDFLAGS		:= -L$(VAMPSDK_DIR) -L/usr/local/lib -lvamp-hostsdk -lcapnp -lkj 
LDFLAGS		:= $(VAMPSDK_DIR)/libvamp-hostsdk.a -lcapnp -lkj 

LDFLAGS		+= -ldl

TEST_SRCS := test/vamp-client/tst_PluginStub.cpp

all:	o bin bin/piper-convert bin/piper-vamp-simple-server bin/test-suite

bin:
	mkdir bin

o:
	mkdir o

bin/piper-convert: o/convert.o o/json11.o o/piper.capnp.o
	c++ $(CXXFLAGS) $^ -o $@ $(LDFLAGS)

bin/piper-vamp-simple-server: o/simple-server.o o/json11.o o/piper.capnp.o
	c++ $(CXXFLAGS) $^ -o $@ $(LDFLAGS)
	
bin/test-suite: test/main.cpp $(TEST_SRCS)
	c++ $(CXXFLAGS) $(INCFLAGS) $< $(TEST_SRCS) -o $@ $(LDFLAGS)

o/piper.capnp.o:	vamp-capnp/piper.capnp.c++
	c++ $(CXXFLAGS) $(INCFLAGS) -c $< -o $@

vamp-capnp/piper.capnp.h:	vamp-capnp/piper.capnp.c++

vamp-capnp/piper.capnp.c++: $(PIPER_DIR)/capnp/piper.capnp
	capnpc --src-prefix=$(PIPER_DIR)/capnp -oc++:vamp-capnp $<

o/json11.o:	ext/json11/json11.cpp
	c++ $(CXXFLAGS) -c $< -o $@

o/convert.o:	vamp-server/convert.cpp vamp-capnp/piper.capnp.h vamp-capnp/VampnProto.h vamp-json/VampJson.h
	c++ $(CXXFLAGS) $(INCFLAGS) -c $< -o $@

o/simple-server.o:	vamp-server/simple-server.cpp vamp-capnp/piper.capnp.h vamp-capnp/VampnProto.h vamp-json/VampJson.h
	c++ $(CXXFLAGS) $(INCFLAGS) -c $< -o $@

test:	all
	bin/test-suite -s -d yes
	vamp-server/test.sh

clean:
	rm -f */*.o

distclean:	clean
	rm -rf bin/*

# cancel implicit rule which otherwise could try to link %.capnp
%:	%.o
