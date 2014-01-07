vpath %.cc ../

#CXX := colorgcc -lstdc++
CXX := g++
LIBDIR  ?= ../../../lib
LIBDIR2 := ../../common
INCLUDE := -I ./../
INCLUDE += -I ./../../common
INCLUDE += -I $(LIBDIR)/unittest++/

CXXFLAGS := -g -Wall $(INCLUDE) -std=gnu++98 -fopenmp
LDFLAGS := -L $(LIBDIR) -L $(LIBDIR2) -lUnitTest++ -lm -lpthread -lmrr

SOURCES := DHTserver.cc DHTclient.cc DHTserver_test.cc DHTclient_test.cc main.cc
OBJECTS := $(patsubst %.cc, %.o, $(SOURCES))

SOURCES_INT := DHTserver.cc DHTclient.cc main_integration.cc client.cc server.cc 
OBJECTS_INT := $(patsubst %.cc, %.o, $(SOURCES_INT))

%.o : %.cc 
	$(CXX) -c $(CXXFLAGS) $^

all: unit_test integration_test

integration_test: $(OBJECTS_INT) depend
	$(CXX) $(CXXFLAGS) -o ./all_test $(OBJECTS_INT) $(LDFLAGS) 

unit_test: $(OBJECTS) depend
	$(CXX) $(CXXFLAGS) -o ./all_test $(OBJECTS) $(LDFLAGS) 

test: all
	./all_test


depend:
	@if ! [ $(LIBDIR)/libUnitTest++.a ]; then \
		make -C $(LIBDIR);                       \
	fi;

clean:						
	-rm *.o all_test
