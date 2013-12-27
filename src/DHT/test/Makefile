vpath %.cc ../

#CXX := colorgcc -lstdc++
CXX := g++
LIBDIR  ?= ../../../lib
INCLUDE := -I ./../
INCLUDE += -I ./../../common
INCLUDE += -I $(LIBDIR)/unittest++/

CXXFLAGS := -g -Wall $(INCLUDE) -std=gnu++98 
LDFLAGS := -L $(LIBDIR) -lUnitTest++ -lm

SOURCES := DHTserver.cc DHTclient.cc
OBJECTS := $(patsubst %.cc, %.o, $(SOURCES))

%.o : %.cc 
	$(CXX) -c $(CXXFLAGS) $^

all: $(OBJECTS) depend
	$(CXX) $(CXXFLAGS) -o ./all_test $(OBJECTS) $(LDFLAGS) 

test: all
	./all_test

depend:
	@if ! [ $(LIBDIR)/libUnitTest++.a ]; then \
		make -C $(LIBDIR);                       \
	fi;

clean:						
	-rm *.o all_test
