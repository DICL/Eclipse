vpath %.cc ../
vpath %.cc ../../DHT/

#CXX := colorgcc -lstdc++
CXX := g++
LIBDIR  ?= ../../../lib
LIBDIR2 := ../../common
INCLUDE := -I ./../
INCLUDE += -I ./../../common
INCLUDE += -I ./../../DHT
INCLUDE += -I $(LIBDIR)/unittest++/

CXXFLAGS := -ggdb3 -Wall -Wextra $(INCLUDE) -std=gnu++11 -fopenmp #-D_DEBUG
LDFLAGS := -L $(LIBDIR) -L $(LIBDIR2) -lUnitTest++ -lm -lpthread -lmrr

SOURCES := main.cc diskpage_test.cc local_cache.cc local_cache_test.cc 
#main.cc orthrus.cc orthrus_test.cc DHTclient.cc
OBJECTS := $(patsubst %.cc, %.o, $(SOURCES))

%.o : %.cc 
	$(CXX) -c $(CXXFLAGS) $^

all: unit_test _clean  

unit_test: $(OBJECTS) depend
	$(CXX) $(CXXFLAGS) -o ./all_test $(OBJECTS) $(LDFLAGS) 

test: all
	./all_test

depend:
	@if ! [ $(LIBDIR)/libUnitTest++.a ]; then \
		make -C $(LIBDIR);                       \
	fi;

_clean:
	-rm *.o

clean:						
	-rm *.o ./all_test
