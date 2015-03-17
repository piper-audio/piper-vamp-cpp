
CXX		:= g++

OBJECTS		:= test.o vampipe_apply.o vampipe_types.o

CXXFLAGS	:= -Wno-unknown-pragmas -Iext -g -std=c++11 -Wall -Werror

LDFLAGS		+= -lvamp-hostsdk -ldl

test:	$(OBJECTS)
	$(CXX) -o $@ $^ $(LDFLAGS)

clean:
	rm -f $(OBJECTS)
