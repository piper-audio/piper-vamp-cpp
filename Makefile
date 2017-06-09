
VAMPSDK_DIR	:= ../vamp-plugin-sdk
PIPER_DIR	:= ../piper

INCFLAGS	:= -Iext -Iext/sord -Iext/serd -I$(VAMPSDK_DIR) -I. -I/usr/local/include
CXXFLAGS	:= -Wall -Wextra -Werror -Wno-error=unused-parameter -g3 -std=c++11 $(INCFLAGS)

#LDFLAGS		:= -L$(VAMPSDK_DIR) -L/usr/local/lib -lvamp-hostsdk -lcapnp -lkj 
LDFLAGS		:= $(VAMPSDK_DIR)/libvamp-hostsdk.a -lcapnp -lkj 

LDFLAGS		+= -ldl

COMMON_OBJS	:= ext/json11/json11.o vamp-capnp/piper.capnp.o

TEST_SRCS 	:= test/main.cpp test/vamp-client/tst_PluginStub.cpp
TEST_OBJS	:= $(TEST_SRCS:.cpp=.o)

all:	bin bin/piper-convert bin/piper-vamp-simple-server bin/test-suite

bin:
	mkdir bin

bin/piper-convert: vamp-server/convert.o $(COMMON_OBJS)
	c++ $(CXXFLAGS) $^ -o $@ $(LDFLAGS)

bin/piper-vamp-simple-server: vamp-server/simple-server.o $(COMMON_OBJS)
	c++ $(CXXFLAGS) $^ -o $@ $(LDFLAGS)

bin/test-suite: $(TEST_OBJS)
	c++ $(CXXFLAGS) $^ -o $@ $(LDFLAGS)
	bin/test-suite

vamp-capnp/piper.capnp.o:	vamp-capnp/piper.capnp.c++
	c++ $(CXXFLAGS) $(INCFLAGS) -c $< -o $@

vamp-capnp/piper.capnp.h:	vamp-capnp/piper.capnp.c++

vamp-capnp/piper.capnp.c++: $(PIPER_DIR)/capnp/piper.capnp
	capnpc --src-prefix=$(PIPER_DIR)/capnp -oc++:vamp-capnp $<

test:	all
	bin/test-suite -s -d yes
	vamp-server/test.sh

clean:
	rm -f */*.o

distclean:	clean
	rm -rf bin/*

depend:
	makedepend -Y. */*.cpp */*/*.cpp */*/*/*.cpp */*.c++

# cancel implicit rule which otherwise could try to link %.capnp
%:	%.o

# DO NOT DELETE

vamp-capnp/piper-capnp.o: vamp-capnp/piper.capnp.c++ vamp-capnp/piper.capnp.h
vamp-server/convert.o: vamp-json/VampJson.h vamp-support/PluginStaticData.h
vamp-server/convert.o: vamp-support/PluginConfiguration.h
vamp-server/convert.o: vamp-support/RequestResponse.h
vamp-server/convert.o: vamp-support/PluginStaticData.h
vamp-server/convert.o: vamp-support/PluginConfiguration.h
vamp-server/convert.o: vamp-support/PluginHandleMapper.h
vamp-server/convert.o: vamp-support/PluginOutputIdMapper.h
vamp-server/convert.o: vamp-support/PluginOutputIdMapper.h
vamp-server/convert.o: vamp-support/RequestResponseType.h
vamp-server/convert.o: vamp-capnp/VampnProto.h vamp-capnp/piper.capnp.h
vamp-server/convert.o: vamp-support/RequestOrResponse.h
vamp-server/convert.o: vamp-support/RequestResponseType.h
vamp-server/convert.o: vamp-support/RequestResponse.h
vamp-server/convert.o: vamp-support/PreservingPluginHandleMapper.h
vamp-server/convert.o: vamp-support/PluginHandleMapper.h
vamp-server/convert.o: vamp-support/PreservingPluginOutputIdMapper.h
vamp-server/simple-server.o: vamp-json/VampJson.h
vamp-server/simple-server.o: vamp-support/PluginStaticData.h
vamp-server/simple-server.o: vamp-support/PluginConfiguration.h
vamp-server/simple-server.o: vamp-support/RequestResponse.h
vamp-server/simple-server.o: vamp-support/PluginStaticData.h
vamp-server/simple-server.o: vamp-support/PluginConfiguration.h
vamp-server/simple-server.o: vamp-support/PluginHandleMapper.h
vamp-server/simple-server.o: vamp-support/PluginOutputIdMapper.h
vamp-server/simple-server.o: vamp-support/PluginOutputIdMapper.h
vamp-server/simple-server.o: vamp-support/RequestResponseType.h
vamp-server/simple-server.o: vamp-capnp/VampnProto.h vamp-capnp/piper.capnp.h
vamp-server/simple-server.o: vamp-support/RequestOrResponse.h
vamp-server/simple-server.o: vamp-support/RequestResponseType.h
vamp-server/simple-server.o: vamp-support/RequestResponse.h
vamp-server/simple-server.o: vamp-support/CountingPluginHandleMapper.h
vamp-server/simple-server.o: vamp-support/PluginHandleMapper.h
vamp-server/simple-server.o: vamp-support/AssignedPluginHandleMapper.h
vamp-server/simple-server.o: vamp-support/DefaultPluginOutputIdMapper.h
vamp-server/simple-server.o: vamp-support/LoaderRequests.h
ext/json11/json11.o: ext/json11/json11.hpp
ext/json11/test.o: ext/json11/json11.hpp
test/vamp-client/tst_PluginStub.o: vamp-client/Loader.h
test/vamp-client/tst_PluginStub.o: vamp-support/RequestResponse.h
test/vamp-client/tst_PluginStub.o: vamp-support/PluginStaticData.h
test/vamp-client/tst_PluginStub.o: vamp-support/PluginConfiguration.h
test/vamp-client/tst_PluginStub.o: vamp-client/PluginClient.h
test/vamp-client/tst_PluginStub.o: vamp-support/PluginConfiguration.h
test/vamp-client/tst_PluginStub.o: vamp-client/PiperVampPlugin.h
test/vamp-client/tst_PluginStub.o: vamp-support/PluginStaticData.h
test/vamp-client/tst_PluginStub.o: vamp-client/PluginClient.h
vamp-client/qt/test.o: vamp-client/qt/ProcessQtTransport.h
vamp-client/qt/test.o: vamp-client/SynchronousTransport.h
vamp-client/qt/test.o: vamp-client/Exceptions.h
vamp-client/qt/test.o: vamp-client/qt/PiperAutoPlugin.h
vamp-client/qt/test.o: vamp-client/CapnpRRClient.h vamp-client/Loader.h
vamp-client/qt/test.o: vamp-support/RequestResponse.h
vamp-client/qt/test.o: vamp-support/PluginStaticData.h
vamp-client/qt/test.o: vamp-support/PluginConfiguration.h
vamp-client/qt/test.o: vamp-client/PluginClient.h
vamp-client/qt/test.o: vamp-client/PiperVampPlugin.h
vamp-client/qt/test.o: vamp-support/PluginStaticData.h
vamp-client/qt/test.o: vamp-support/PluginConfiguration.h
vamp-client/qt/test.o: vamp-client/SynchronousTransport.h
vamp-client/qt/test.o: vamp-support/AssignedPluginHandleMapper.h
vamp-client/qt/test.o: vamp-support/PluginHandleMapper.h
vamp-client/qt/test.o: vamp-support/PluginOutputIdMapper.h
vamp-client/qt/test.o: vamp-support/DefaultPluginOutputIdMapper.h
vamp-client/qt/test.o: vamp-capnp/VampnProto.h vamp-capnp/piper.capnp.h
vamp-client/qt/test.o: vamp-support/PluginHandleMapper.h
vamp-client/qt/test.o: vamp-support/PluginOutputIdMapper.h
vamp-client/qt/test.o: vamp-support/RequestResponseType.h
vamp-capnp/piper.capnp.o: vamp-capnp/piper.capnp.h
